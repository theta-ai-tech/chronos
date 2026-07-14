#include "chronos/runtime/datasets/replay.hpp"

#include "microtest.hpp"

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace replay = chronos::runtime::datasets;
namespace adapter = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type bytes{};
  bytes.front() = seed;
  return Id::from_bytes(bytes).value();
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

std::uint8_t nibble(char value) {
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  return static_cast<std::uint8_t>(value - 'a' + 10);
}

contracts::Sha256Digest digest(std::string_view hex) {
  contracts::Sha256Digest result;
  for (std::size_t index = 0; index < result.bytes.size(); ++index) {
    result.bytes[index] = static_cast<std::uint8_t>(
        (nibble(hex[index * 2]) << 4U) | nibble(hex[index * 2 + 1]));
  }
  return result;
}

sdk::SourceCaptureContext capture_context() {
  return {
      .adapter_id = "chronos.bybit.public-market-data",
      .adapter_version = "m2.5",
      .build_version = "test",
      .venue = "bybit",
      .environment = sdk::EnvironmentClass::Test,
      .market = sdk::MarketClass::LinearPerpetual,
      .endpoint = sdk::EndpointClass::PublicMarketData,
      .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
      .capture_session_id = id<sdk::CaptureSessionId>(1),
      .runtime_id = id<contracts::RuntimeId>(2),
      .capture_partition_id = id<sdk::CapturePartitionId>(3),
      .framing_version = "websocket-rfc6455-v1",
      .static_configuration_version = "replay-test-v1",
      .capability_manifest_version = "bybit-v5-v1",
      .schema_policy_version = "bybit-v5-public-v1",
      .data_classification = sdk::DataClassification::PublicMarketData,
      .access_restriction = sdk::AccessRestriction::ChronosInternal,
      .maximum_retained_payload_bytes = 4096,
      .maximum_source_events = 10,
  };
}

adapter::DatasetReadResult capture_dataset(std::string_view first,
                                           std::string_view second) {
  static std::uint64_t sequence{};
  const auto path = std::filesystem::temp_directory_path() /
                    ("chronos-replay-provider-" + std::to_string(++sequence));
  std::filesystem::remove_all(path);
  std::filesystem::remove_all(path.string() + ".partial");
  const auto context = capture_context();
  auto recorder = sdk::SourceCaptureRecorder::create(context).value();
  auto writer = adapter::CaptureDatasetWriter::create(path, context).value();
  const auto capture = [&](std::uint8_t seed, std::string_view payload) {
    const auto raw = bytes(payload);
    return recorder.capture({
        .source_event_id = id<contracts::SourceEventId>(seed),
        .chronos_receive_time =
            contracts::TimePoint::from(seed, id<contracts::ClockDomainId>(4),
                                       contracts::ClockClass::monotonic, 1)
                .value(),
        .raw_payload = raw,
        .framing_protocol = sdk::FramingProtocol::WebSocket,
        .frame_kind = sdk::SourceFrameKind::Text,
        .framing_status = sdk::FramingStatus::Complete,
        .integrity_status = sdk::CaptureIntegrityStatus::Complete,
        .content_encoding = sdk::ContentEncoding::Utf8Text,
        .compression_disposition = sdk::CompressionDisposition::NotCompressed,
    });
  };
  const auto one = capture(10, first);
  const auto two = capture(11, second);
  if (!one.ok() || !two.ok() ||
      writer.append(*one.event) != adapter::DatasetFailure::None ||
      writer.append(*two.event) != adapter::DatasetFailure::None ||
      !writer.seal().manifest.has_value()) {
    throw std::runtime_error("failed to construct replay capture fixture");
  }
  auto result = adapter::read_capture_dataset(path);
  std::filesystem::remove_all(path);
  return result;
}

replay::ReplayVersionPins faithful_pins() {
  return {
      .provider_version = "capture-order-provider-v1",
      .merge_policy_version = "single-stream-capture-order-v1",
      .schema_registry_version = "event-registry-v1",
      .canonicalization_version = "chronos-source-enrichment-v1",
      .normalizer_version = "chronos-book-normalizer-v1",
      .reference_lineage_version =
          contracts::VersionRef::from(id<contracts::DefinitionId>(5), 1),
      .expected_normalized_dataset_identity =
          contracts::sha256(bytes("expected-normalized-output")),
  };
}

replay::ReplayVersionPins normalized_pins() {
  return {
      .provider_version = "normalized-fact-provider-v1",
      .merge_policy_version = "single-stream-normalized-order-v1",
      .schema_registry_version = "event-registry-v1",
      .canonicalization_version = "chronos-normalized-fact-v1",
  };
}

replay::NormalizedFactRecord fact(std::uint64_t position, std::string_view type,
                                  std::string_view payload) {
  auto semantic_payload = bytes(payload);
  return {
      .normalized_position = position,
      .normalized_stream_id = id<contracts::StreamId>(20),
      .normalized_stream_epoch = 1,
      .source_dataset_identity = contracts::sha256(bytes("source-dataset")),
      .source_event_id = id<contracts::SourceEventId>(21),
      .source_decode_enrichment_id =
          id<contracts::SourceDecodeEnrichmentId>(22),
      .normalizer_version = "chronos-market-normalizer-v1",
      .reference_lineage_version =
          contracts::VersionRef::from(id<contracts::DefinitionId>(23), 1)
              .value(),
      .event_type = std::string(type),
      .semantic_payload = semantic_payload,
      .semantic_checksum = contracts::sha256(semantic_payload),
  };
}

struct ObservedInput final {
  replay::ReplayClass replay_class;
  std::uint64_t ordinal;
  std::string event_type;
  std::vector<std::byte> payload;
  contracts::Sha256Digest checksum;
  std::optional<contracts::SourceEventId> source_event_id;
  std::optional<std::uint64_t> capture_sequence;
  std::optional<std::uint64_t> normalized_position;

  bool operator==(const ObservedInput &) const = default;
};

class RecordingSink final : public replay::ReplayDispatchSink {
public:
  bool accept(const replay::ReplayDispatchInput &input) override {
    if (reject_at != 0 && input.replay_ordinal == reject_at)
      return false;
    observed.push_back({
        .replay_class = input.replay_class,
        .ordinal = input.replay_ordinal,
        .event_type = std::string(input.event_type),
        .payload = {input.semantic_payload.begin(),
                    input.semantic_payload.end()},
        .checksum = input.semantic_checksum,
        .source_event_id = input.source_event_id,
        .capture_sequence = input.capture_sequence,
        .normalized_position = input.normalized_position,
    });
    return true;
  }

  std::uint64_t reject_at{};
  std::vector<ObservedInput> observed;
};

} // namespace

TEST_CASE("run manifest names exactly one replay class") {
  const auto dataset_identity = contracts::sha256(bytes("dataset"));
  const auto faithful = replay::ReplayRunManifest::create(
      id<contracts::RunId>(1), replay::ReplayClass::FaithfulCaptureOrder,
      dataset_identity, faithful_pins());
  const auto normalized = replay::ReplayRunManifest::create(
      id<contracts::RunId>(1), replay::ReplayClass::NormalizedFact,
      dataset_identity, normalized_pins());
  CHECK(faithful.has_value());
  CHECK(normalized.has_value());
  CHECK(faithful->replay_class() == replay::ReplayClass::FaithfulCaptureOrder);
  CHECK(normalized->replay_class() == replay::ReplayClass::NormalizedFact);
  CHECK(faithful->identity() != normalized->identity());
  auto changed_pins = faithful_pins();
  changed_pins.normalizer_version = "chronos-book-normalizer-v2";
  const auto changed = replay::ReplayRunManifest::create(
      id<contracts::RunId>(1), replay::ReplayClass::FaithfulCaptureOrder,
      dataset_identity, std::move(changed_pins));
  CHECK(changed.has_value());
  CHECK(changed->identity() != faithful->identity());

  CHECK(!replay::ReplayRunManifest::create(
             id<contracts::RunId>(1), replay::ReplayClass::FaithfulCaptureOrder,
             dataset_identity, normalized_pins())
             .has_value());
  CHECK(!replay::ReplayRunManifest::create(id<contracts::RunId>(1),
                                           replay::ReplayClass::NormalizedFact,
                                           dataset_identity, faithful_pins())
             .has_value());
}

TEST_CASE("faithful provider dispatches verified capture order exactly") {
  const auto dataset = capture_dataset("first", "second");
  CHECK(dataset.ok());
  const auto manifest =
      replay::ReplayRunManifest::create(
          id<contracts::RunId>(2), replay::ReplayClass::FaithfulCaptureOrder,
          digest(dataset.manifest()->dataset_id), faithful_pins())
          .value();
  RecordingSink first;
  RecordingSink second;
  CHECK(replay::replay_capture_order(manifest, dataset, first).ok());
  CHECK(replay::replay_capture_order(manifest, dataset, second).ok());
  CHECK(first.observed == second.observed);
  CHECK(first.observed.size() == 2);
  CHECK(first.observed[0].ordinal == 1);
  CHECK(first.observed[0].capture_sequence == 1);
  CHECK(first.observed[1].capture_sequence == 2);
  CHECK(first.observed[0].payload == bytes("first"));
  CHECK(first.observed[1].payload == bytes("second"));
  CHECK(first.observed[0].source_event_id == id<contracts::SourceEventId>(10));

  RecordingSink rejected;
  rejected.reject_at = 2;
  const auto failure =
      replay::replay_capture_order(manifest, dataset, rejected);
  CHECK(failure.failure == replay::ReplayFailure::DispatchRejected);
  CHECK(failure.dispatched_count == 1);
  CHECK(rejected.observed.size() == 1);

  const auto wrong_manifest =
      replay::ReplayRunManifest::create(
          id<contracts::RunId>(2), replay::ReplayClass::FaithfulCaptureOrder,
          contracts::sha256(bytes("wrong-dataset")), faithful_pins())
          .value();
  RecordingSink mismatch;
  CHECK(
      replay::replay_capture_order(wrong_manifest, dataset, mismatch).failure ==
      replay::ReplayFailure::DatasetMismatch);
  CHECK(mismatch.observed.empty());
}

TEST_CASE(
    "normalized provider preserves accepted fact order without decoding") {
  const auto dataset = replay::NormalizedFactDataset::create(
                           {fact(1, "market.book.snapshot_observed", "book"),
                            fact(2, "market.trade.observed", "trade")})
                           .value();
  const auto manifest =
      replay::ReplayRunManifest::create(id<contracts::RunId>(3),
                                        replay::ReplayClass::NormalizedFact,
                                        dataset.identity(), normalized_pins())
          .value();
  RecordingSink sink;
  const auto result = replay::replay_normalized_facts(manifest, dataset, sink);
  CHECK(result.ok());
  CHECK(result.dispatched_count == 2);
  CHECK(sink.observed[0].event_type == "market.book.snapshot_observed");
  CHECK(sink.observed[1].event_type == "market.trade.observed");
  CHECK(sink.observed[0].normalized_position == 1);
  CHECK(sink.observed[1].normalized_position == 2);
  CHECK(!sink.observed[0].source_event_id.has_value());

  const auto wrong_manifest =
      replay::ReplayRunManifest::create(
          id<contracts::RunId>(3), replay::ReplayClass::NormalizedFact,
          contracts::sha256(bytes("wrong-dataset")), normalized_pins())
          .value();
  RecordingSink mismatch;
  CHECK(replay::replay_normalized_facts(wrong_manifest, dataset, mismatch)
            .failure == replay::ReplayFailure::DatasetMismatch);
  CHECK(mismatch.observed.empty());

  RecordingSink wrong_provider;
  CHECK(replay::replay_capture_order(manifest, capture_dataset("a", "b"),
                                     wrong_provider)
            .failure == replay::ReplayFailure::InvalidManifest);
}

TEST_CASE("normalized datasets reject gaps and semantic corruption") {
  CHECK(!replay::NormalizedFactDataset::create(
             {fact(2, "market.trade.observed", "trade")})
             .has_value());
  auto corrupted = fact(1, "market.trade.observed", "trade");
  corrupted.semantic_payload[0] = std::byte{'x'};
  CHECK(!replay::NormalizedFactDataset::create({std::move(corrupted)})
             .has_value());

  auto missing_epoch = fact(1, "market.trade.observed", "trade");
  missing_epoch.normalized_stream_epoch = 0;
  CHECK(!replay::NormalizedFactDataset::create({std::move(missing_epoch)})
             .has_value());

  CHECK(!replay::NormalizedFactDataset::create(
             {fact(1, "market.trade.observed", "too-large")},
             {.maximum_records = 1,
              .maximum_payload_bytes = 4,
              .maximum_total_payload_bytes = 4})
             .has_value());
  CHECK(!replay::NormalizedFactDataset::create(
             {fact(1, "market.trade.observed", "1234"),
              fact(2, "market.trade.observed", "5678")},
             {.maximum_records = 2,
              .maximum_payload_bytes = 4,
              .maximum_total_payload_bytes = 7})
             .has_value());
}
