#include "chronos/adapters/market_data/source_capture.hpp"
#include "chronos/adapters/sdk/source_event.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace market_data = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type bytes{};
  bytes.front() = seed;
  return *Id::from_bytes(bytes);
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

sdk::SourceCaptureContext context(std::size_t maximum_payload = 4096,
                                  std::size_t maximum_events = 100) {
  return {.adapter_id = "chronos.bybit.public-market-data",
          .adapter_version = "m2.3",
          .build_version = "test",
          .venue = "bybit",
          .environment = sdk::EnvironmentClass::Test,
          .endpoint = sdk::EndpointClass::PublicMarketData,
          .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
          .capture_session_id = id<sdk::CaptureSessionId>(1),
          .runtime_id = id<contracts::RuntimeId>(2),
          .connection_id = id<sdk::SourceConnectionId>(3),
          .subscription_id = id<sdk::SourceSubscriptionId>(4),
          .capture_partition_id = id<sdk::CapturePartitionId>(5),
          .framing_version = "websocket-rfc6455-v1",
          .static_configuration_version = "capture-test-v1",
          .capability_manifest_version = "adapter-sdk-v1",
          .schema_policy_version = "bybit-v5-public-v1",
          .data_classification = sdk::DataClassification::PublicMarketData,
          .access_restriction = sdk::AccessRestriction::ChronosInternal,
          .maximum_retained_payload_bytes = maximum_payload,
          .maximum_source_events = maximum_events};
}

contracts::TimePoint receive_time(std::int64_t nanoseconds = 100) {
  return *contracts::TimePoint::from(nanoseconds,
                                     id<contracts::ClockDomainId>(6),
                                     contracts::ClockClass::monotonic, 1);
}

sdk::SourceCaptureInput input(contracts::SourceEventId event_id,
                              std::span<const std::byte> payload) {
  return {.source_event_id = event_id,
          .chronos_receive_time = receive_time(),
          .raw_payload = payload,
          .framing_protocol = sdk::FramingProtocol::WebSocket,
          .frame_kind = sdk::SourceFrameKind::Text,
          .framing_status = sdk::FramingStatus::Complete,
          .integrity_status = sdk::CaptureIntegrityStatus::Complete,
          .content_encoding = sdk::ContentEncoding::Utf8Text,
          .compression_disposition = sdk::CompressionDisposition::NotCompressed,
          .fragmented = false};
}

class SequentialIdentitySource final
    : public market_data::SourceEventIdentitySource {
public:
  std::optional<contracts::SourceEventId> next() override {
    return id<contracts::SourceEventId>(next_++);
  }

private:
  std::uint8_t next_{30};
};

class CollectingConsumer final : public market_data::SourceEventConsumer {
public:
  bool accept(const sdk::SourceEvent &event) override {
    events.push_back(event);
    return true;
  }

  std::vector<sdk::SourceEvent> events;
};

class RetryConsumer final : public market_data::SourceEventConsumer {
public:
  bool accept(const sdk::SourceEvent &event) override {
    seen_ids.push_back(event.source_event_id());
    return accept_now;
  }

  bool accept_now{};
  std::vector<contracts::SourceEventId> seen_ids;
};

bool equal_payload(std::span<const std::byte> left,
                   std::span<const std::byte> right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin());
}
} // namespace

TEST_CASE("source capture assigns immutable partition-local occurrence order") {
  static_assert(!std::is_copy_assignable_v<sdk::SourceEvent>);
  static_assert(!std::is_move_assignable_v<sdk::SourceEvent>);
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  CHECK(recorder.has_value());
  const auto first_payload = bytes("first");
  const auto second_payload = bytes("second");
  auto first =
      recorder->capture(input(id<contracts::SourceEventId>(10), first_payload));
  auto second = recorder->capture(
      input(id<contracts::SourceEventId>(11), second_payload));
  CHECK(first.ok());
  CHECK(second.ok());
  CHECK(first.event->capture_sequence() == 1);
  CHECK(second.event->capture_sequence() == 2);
  CHECK(first.event->capture_partition_id() == id<sdk::CapturePartitionId>(5));
  CHECK(first.event->parse_status() == sdk::ParseStatus::NotAttempted);
  CHECK(equal_payload(first.event->raw_payload(), first_payload));
}

TEST_CASE(
    "duplicate occurrence identity is rejected without advancing capture") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  const auto payload = bytes("same bytes may be separate occurrences");
  const auto event_id = id<contracts::SourceEventId>(12);
  CHECK(recorder->capture(input(event_id, payload)).ok());
  const auto duplicate = recorder->capture(input(event_id, payload));
  CHECK(duplicate.failure == sdk::CaptureFailure::DuplicateSourceEventId);
  CHECK(recorder->next_capture_sequence() == 2);
  CHECK(recorder->capture(input(id<contracts::SourceEventId>(13), payload))
            .event->capture_sequence() == 2);
}

TEST_CASE("capture session capacity fails explicitly without advancing") {
  auto recorder = sdk::SourceCaptureRecorder::create(context(4096, 1));
  const auto payload = bytes("bounded");
  CHECK(
      recorder->capture(input(id<contracts::SourceEventId>(40), payload)).ok());
  const auto full =
      recorder->capture(input(id<contracts::SourceEventId>(41), payload));
  CHECK(full.failure == sdk::CaptureFailure::CapacityExceeded);
  CHECK(recorder->next_capture_sequence() == 2);
}

TEST_CASE("malformed and unsupported source bytes are retained unchanged") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  const auto malformed_payload = bytes("{not-json");
  auto malformed_input =
      input(id<contracts::SourceEventId>(14), malformed_payload);
  malformed_input.integrity_status = sdk::CaptureIntegrityStatus::Malformed;
  malformed_input.framing_status = sdk::FramingStatus::Invalid;
  auto malformed = recorder->capture(malformed_input);
  CHECK(malformed.ok());
  CHECK(malformed.event->integrity_status() ==
        sdk::CaptureIntegrityStatus::Malformed);
  CHECK(equal_payload(malformed.event->raw_payload(), malformed_payload));

  const auto unsupported_payload = bytes("opaque-vendor-message");
  auto unsupported_input =
      input(id<contracts::SourceEventId>(15), unsupported_payload);
  unsupported_input.integrity_status = sdk::CaptureIntegrityStatus::Unsupported;
  unsupported_input.framing_status = sdk::FramingStatus::Unsupported;
  auto unsupported = recorder->capture(unsupported_input);
  CHECK(unsupported.ok());
  CHECK(equal_payload(unsupported.event->raw_payload(), unsupported_payload));
}

TEST_CASE("contradictory framing and integrity metadata fails closed") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  const auto payload = bytes("partial");
  auto contradictory = input(id<contracts::SourceEventId>(42), payload);
  contradictory.integrity_status = sdk::CaptureIntegrityStatus::Truncated;
  CHECK(recorder->capture(contradictory).failure ==
        sdk::CaptureFailure::InvalidInput);
}

TEST_CASE("resource limit retains a verifiable prefix and original length") {
  auto recorder = sdk::SourceCaptureRecorder::create(context(4));
  const auto payload = bytes("abcdefgh");
  auto result =
      recorder->capture(input(id<contracts::SourceEventId>(16), payload));
  CHECK(result.ok());
  CHECK(equal_payload(result.event->raw_payload(),
                      std::span<const std::byte>(payload.data(), 4)));
  CHECK(result.event->original_payload_size() == 8);
  CHECK(result.event->integrity_status() ==
        sdk::CaptureIntegrityStatus::ResourceLimitExceeded);
  CHECK(result.event->framing_status() == sdk::FramingStatus::Incomplete);
  CHECK(result.event->payload_digest().coverage ==
        sdk::DigestCoverage::CompletePayload);
  auto second = recorder->capture(
      input(id<contracts::SourceEventId>(18), bytes("abcdWXYZ")));
  CHECK(second.ok());
  CHECK(second.event->payload_digest().bytes !=
        result.event->payload_digest().bytes);
}

TEST_CASE(
    "SHA-256 digest is canonical and WebSocket ingress time is preserved") {
  const auto empty = bytes("");
  CHECK(sdk::sha256(empty, sdk::DigestCoverage::CompletePayload).hex() ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  const auto payload = bytes("abc");
  CHECK(sdk::sha256(payload, sdk::DigestCoverage::CompletePayload).hex() ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  auto recorder = sdk::SourceCaptureRecorder::create(context());
  market_data::WebSocketMessage message{
      .kind = market_data::WebSocketMessageKind::Text,
      .payload = payload,
      .monotonic_receive_time_nanoseconds = 987654321,
      .fragmented = true,
      .integrity = market_data::WebSocketIngressIntegrity::Malformed,
      .original_payload_size = payload.size()};
  auto result = market_data::capture_websocket_message(
      *recorder, id<contracts::SourceEventId>(17),
      id<contracts::ClockDomainId>(6), message);
  CHECK(result.ok());
  CHECK(result.event->chronos_receive_time().nanoseconds() == 987654321);
  CHECK(result.event->fragmented());
  CHECK(result.event->integrity_status() ==
        sdk::CaptureIntegrityStatus::Malformed);
}

TEST_CASE(
    "WebSocket capture pipeline assigns identity and hands off each event") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  SequentialIdentitySource identities;
  CollectingConsumer consumer;
  market_data::WebSocketSourceCapture capture(std::move(*recorder),
                                              id<contracts::ClockDomainId>(6),
                                              identities, consumer);
  const auto payload = bytes("raw acknowledgement or market frame");
  market_data::WebSocketMessage message{
      .kind = market_data::WebSocketMessageKind::Text,
      .payload = payload,
      .monotonic_receive_time_nanoseconds = 444,
      .fragmented = false};
  CHECK(capture.capture(message));
  CHECK(consumer.events.size() == 1);
  CHECK(consumer.events.front().capture_sequence() == 1);
  CHECK(equal_payload(consumer.events.front().raw_payload(), payload));
}

TEST_CASE("consumer rejection retains the accepted identity for exact retry") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  SequentialIdentitySource identities;
  RetryConsumer consumer;
  market_data::WebSocketSourceCapture capture(std::move(*recorder),
                                              id<contracts::ClockDomainId>(6),
                                              identities, consumer);
  const auto payload = bytes("retry me exactly");
  market_data::WebSocketMessage message{
      .kind = market_data::WebSocketMessageKind::Text,
      .payload = payload,
      .monotonic_receive_time_nanoseconds = 555};
  CHECK(!capture.capture(message));
  CHECK(capture.has_pending());
  CHECK(capture.last_failure() == sdk::CaptureFailure::ConsumerRejected);
  consumer.accept_now = true;
  CHECK(capture.retry_pending());
  CHECK(!capture.has_pending());
  CHECK(consumer.seen_ids.size() == 2);
  CHECK(consumer.seen_ids.front() == consumer.seen_ids.back());
}

TEST_CASE(
    "partial transport evidence retains prefix identity and reported size") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  const auto prefix = bytes("abcd");
  market_data::WebSocketMessage evidence{
      .kind = market_data::WebSocketMessageKind::Text,
      .payload = prefix,
      .monotonic_receive_time_nanoseconds = 666,
      .fragmented = true,
      .integrity =
          market_data::WebSocketIngressIntegrity::ResourceLimitExceeded,
      .original_payload_size = 9};
  auto result = market_data::capture_websocket_message(
      *recorder, id<contracts::SourceEventId>(43),
      id<contracts::ClockDomainId>(6), evidence);
  CHECK(result.ok());
  CHECK(result.event->original_payload_size() == 9);
  CHECK(result.event->framing_status() == sdk::FramingStatus::Incomplete);
  CHECK(result.event->payload_digest().coverage ==
        sdk::DigestCoverage::RetainedPrefix);
}

TEST_CASE("unsupported transport evidence preserves unknown frame kind") {
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  const auto raw = bytes("unknown-frame");
  market_data::WebSocketMessage evidence{
      .kind = market_data::WebSocketMessageKind::Unknown,
      .payload = raw,
      .monotonic_receive_time_nanoseconds = 777,
      .integrity = market_data::WebSocketIngressIntegrity::Unsupported,
      .original_payload_size = raw.size()};
  auto result = market_data::capture_websocket_message(
      *recorder, id<contracts::SourceEventId>(44),
      id<contracts::ClockDomainId>(6), evidence);
  CHECK(result.ok());
  CHECK(result.event->frame_kind() == sdk::SourceFrameKind::Unknown);
  CHECK(result.event->framing_status() == sdk::FramingStatus::Unsupported);
}
