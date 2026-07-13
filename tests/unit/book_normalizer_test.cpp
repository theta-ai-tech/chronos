#include "chronos/adapters/market_data/bybit_book_decoder.hpp"
#include "chronos/normalization/market_data/book_normalizer.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace {
namespace adapter = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;
namespace reference = chronos::core::reference_data;
namespace book = chronos::normalization::market_data;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

adapter::CaptureDatasetManifest manifest() {
  return {
      .format_version = "chronos.capture-dataset.v1",
      .dataset_id = "dataset",
      .records_sha256 = "records",
      .capture_session_id = id<sdk::CaptureSessionId>(1),
      .capture_partition_id = id<sdk::CapturePartitionId>(2),
      .runtime_id = id<contracts::RuntimeId>(3),
      .connection_id = id<sdk::SourceConnectionId>(13),
      .subscription_id = id<sdk::SourceSubscriptionId>(14),
      .adapter_id = "chronos.bybit.public-market-data",
      .adapter_version = "m2.5",
      .build_version = "test",
      .venue = "bybit",
      .environment = sdk::EnvironmentClass::Test,
      .endpoint = sdk::EndpointClass::PublicMarketData,
      .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
      .framing_version = "websocket-rfc6455-v1",
      .static_configuration_version = "test-v1",
      .capability_manifest_version = "bybit-v5-v1",
      .schema_policy_version = "bybit-v5-public-v1",
      .data_classification = sdk::DataClassification::PublicMarketData,
      .access_restriction = sdk::AccessRestriction::ChronosInternal,
      .dataset_class = "raw_source_capture",
      .replay_admissible = false,
      .records_bytes = 1,
      .maximum_retained_payload_bytes = 1U << 20U,
      .maximum_source_events = 10,
      .record_count = 1,
      .first_capture_sequence = 10,
      .last_capture_sequence = 10,
  };
}

adapter::CaptureDatasetRecord record(std::string_view payload) {
  auto raw_payload = bytes(payload);
  return {
      .source_event_id = id<contracts::SourceEventId>(4),
      .capture_sequence = 10,
      .chronos_receive_time =
          contracts::TimePoint::from(900, id<contracts::ClockDomainId>(5),
                                     contracts::ClockClass::monotonic, 1)
              .value(),
      .raw_payload = raw_payload,
      .original_payload_size = raw_payload.size(),
      .payload_digest =
          sdk::sha256(raw_payload, sdk::DigestCoverage::CompletePayload),
      .framing_protocol = sdk::FramingProtocol::WebSocket,
      .frame_kind = sdk::SourceFrameKind::Text,
      .framing_status = sdk::FramingStatus::Complete,
      .integrity_status = sdk::CaptureIntegrityStatus::Complete,
      .parse_status = sdk::ParseStatus::NotAttempted,
      .content_encoding = sdk::ContentEncoding::Utf8Text,
      .compression_disposition = sdk::CompressionDisposition::NotCompressed,
      .fragmented = false,
  };
}

reference::EffectiveDomainId effective_domain() {
  return id<reference::EffectiveDomainId>(6);
}

reference::ReferenceSnapshot reference_snapshot(
    reference::ListingStatus status = reference::ListingStatus::Active) {
  const auto interval = reference::EffectiveInterval::from_capture_sequence(
                            effective_domain(), 1, 20)
                            .value();
  return reference::ReferenceSnapshot::create(
             version(7, 2),
             {.instrument_id = id<contracts::CanonicalInstrumentId>(8),
              .version = version(9, 3),
              .base_asset = "BTC",
              .quote_asset = "USDT",
              .product_class = reference::ProductClass::Spot,
              .effective_interval = interval},
             {.listing_id = id<contracts::ListingId>(10),
              .instrument_id = id<contracts::CanonicalInstrumentId>(8),
              .version = version(11, 4),
              .venue = "bybit",
              .source_symbol = "BTCUSDT",
              .status = status,
              .price_tick = reference::DecimalIncrement::parse("0.10").value(),
              .quantity_step =
                  reference::DecimalIncrement::parse("0.001").value(),
              .amount_definition_ref = 7001,
              .effective_interval = interval})
      .value();
}

const book::BookNormalizerVersions kVersions{
    .decoder_version = "bybit-book-decoder-v1",
    .source_schema_version = "bybit-v5-orderbook-v1",
    .normalizer_version = "chronos-book-normalizer-v1",
};

book::BookNormalizationResult normalize(std::string_view payload) {
  const auto decoded =
      adapter::decode_bybit_v5_book(manifest(), record(payload));
  if (!decoded.ok()) {
    return {.failure = decoded.failure};
  }
  return book::normalize_book(*decoded.message, *decoded.source_lineage,
                              "bybit", reference_snapshot(), effective_domain(),
                              id<contracts::ClockDomainId>(12), kVersions);
}

constexpr std::string_view kSnapshot = R"({
  "data":{"a":[["42000.50","0.200"],["42000.30","0.100"]],
          "seq":7961638724,"s":"BTCUSDT","cts":1672304486868,
          "b":[["42000.00","1.000"],["42000.20","0.500"]],"u":18521288},
  "ts":1672304486869,"type":"snapshot","topic":"orderbook.50.BTCUSDT"
})";

constexpr std::string_view kDelta = R"({
  "topic":"orderbook.50.BTCUSDT","type":"delta","ts":1672304486870,
  "data":{"s":"BTCUSDT","b":[["42000.00","0"],["42000.20","0.750"]],
          "a":[["42000.30","0.300"]],"u":18521289,"seq":7961638725,
          "cts":1672304486869}
})";

} // namespace

TEST_CASE("Bybit snapshots map to canonical fully-lineaged observations") {
  const auto result = normalize(kSnapshot);
  CHECK(result.ok());
  const auto &fact = *result.fact;
  CHECK(fact.event_type() == "market.book.snapshot_observed");
  CHECK(fact.canonical_instrument_id ==
        reference_snapshot().instrument().instrument_id);
  CHECK(fact.listing_id == reference_snapshot().listing().listing_id);
  CHECK(fact.reference_snapshot_version == reference_snapshot().version());
  CHECK(fact.listing_version == reference_snapshot().listing().version);
  CHECK(fact.source_lineage.source_event_id ==
        record(kSnapshot).source_event_id);
  CHECK(fact.source_lineage.capture_session_id ==
        manifest().capture_session_id);
  CHECK(fact.source_lineage.runtime_id == manifest().runtime_id);
  CHECK(fact.source_lineage.connection_id == manifest().connection_id);
  CHECK(fact.source_lineage.subscription_id == manifest().subscription_id);
  CHECK(fact.source_lineage.capture_partition_id ==
        manifest().capture_partition_id);
  CHECK(fact.source_lineage.capture_sequence == 10);
  CHECK(fact.source_lineage.chronos_receive_time ==
        record(kSnapshot).chronos_receive_time);
  CHECK(fact.source_lineage.payload_digest == record(kSnapshot).payload_digest);
  CHECK(fact.source_assertions.sequence == 7961638724ULL);
  CHECK(fact.source_assertions.sequence_scope ==
        sdk::SourceSequenceScope::VenueCrossSequence);
  CHECK(fact.source_assertions.update_id == 18521288ULL);
  CHECK(fact.source_assertions.update_id_scope ==
        sdk::SourceSequenceScope::ListingChannel);
  CHECK(fact.source_assertions.system_timestamp_milliseconds ==
        1672304486869ULL);
  CHECK(fact.source_assertions.timestamp_unit ==
        book::SourceTimestampUnit::Milliseconds);
  CHECK(fact.source_event_time.nanoseconds() == 1672304486868000000LL);

  const auto &snapshot = std::get<book::BookSnapshotObservation>(fact.payload);
  CHECK(snapshot.bids.size() == 2);
  CHECK(snapshot.bids[0].price.units() == 420002);
  CHECK(snapshot.bids[1].price.units() == 420000);
  CHECK(snapshot.asks[0].price.units() == 420003);
  CHECK(snapshot.asks[1].price.units() == 420005);
  CHECK(snapshot.bids[0].quantity.units() == 500);
}

TEST_CASE("Bybit deltas retain absolute set and delete semantics") {
  const auto first = normalize(kDelta);
  const auto second = normalize(kDelta);
  CHECK(first.ok());
  CHECK(second.ok());
  CHECK(first.fact == second.fact);
  CHECK(first.fact->event_type() == "market.book.delta_observed");
  const auto &delta = std::get<book::BookDeltaObservation>(first.fact->payload);
  CHECK(delta.bid_changes.size() == 2);
  CHECK(delta.bid_changes[0].price.units() == 420002);
  CHECK(delta.bid_changes[0].operation ==
        book::BookLevelOperation::SetAbsolute);
  CHECK(delta.bid_changes[1].operation == book::BookLevelOperation::Delete);
  CHECK(delta.bid_changes[1].quantity.units() == 0);
}

TEST_CASE("decoder returns typed failures for malformed and ineligible input") {
  auto bad_record = record(kSnapshot);
  bad_record.integrity_status = sdk::CaptureIntegrityStatus::Truncated;
  CHECK(adapter::decode_bybit_v5_book(manifest(), bad_record).failure ==
        book::BookNormalizationFailure::IntegrityIneligible);
  CHECK(
      adapter::decode_bybit_v5_book(manifest(), record("{not-json")).failure ==
      book::BookNormalizationFailure::MalformedPayload);
  CHECK(adapter::decode_bybit_v5_book(
            manifest(),
            record(
                R"({"topic":"x","topic":"y","type":"delta","ts":1,"data":{}})"))
            .failure == book::BookNormalizationFailure::AmbiguousDuplicate);

  auto limits = adapter::BybitBookDecodeLimits{};
  limits.maximum_payload_bytes = 8;
  CHECK(adapter::decode_bybit_v5_book(manifest(), record(kSnapshot), limits)
            .failure == book::BookNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_levels_per_side = 1;
  CHECK(adapter::decode_bybit_v5_book(manifest(), record(kSnapshot), limits)
            .failure == book::BookNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_json_depth = 1;
  CHECK(adapter::decode_bybit_v5_book(manifest(), record(kSnapshot), limits)
            .failure == book::BookNormalizationFailure::ResourceLimitExceeded);

  std::string invalid_utf8 = "{\"bad\":\"";
  invalid_utf8.push_back(static_cast<char>(0xFF));
  invalid_utf8 += "\"}";
  CHECK(
      adapter::decode_bybit_v5_book(manifest(), record(invalid_utf8)).failure ==
      book::BookNormalizationFailure::MalformedPayload);
}

TEST_CASE("topic symbol and message family mismatches fail visibly") {
  CHECK(normalize(R"({"topic":"orderbook.50.ETHUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);
  CHECK(normalize(R"({"topic":"orderbook.50.ETHUSDT","type":"snapshot","ts":1,
    "data":{"s":"ETHUSDT","b":[],"a":[],"u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"trade","ts":1,
    "data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::UnsupportedMessage);
}

TEST_CASE("inexact and duplicate canonical levels fail without repair") {
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[["42000.15","1.000"]],"a":[],
    "u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::InvalidNumeric);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"delta","ts":1,
    "data":{"s":"BTCUSDT","b":[["42000.10","1.000"],["42000.100","2.000"]],
    "a":[],"u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::AmbiguousDuplicate);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[["42000.10","0"]],"a":[],
    "u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::InvalidNumeric);
}

TEST_CASE("reference and timestamp eligibility are explicit failures") {
  const auto decoded =
      adapter::decode_bybit_v5_book(manifest(), record(kSnapshot));
  CHECK(decoded.ok());
  CHECK(book::normalize_book(
            *decoded.message, *decoded.source_lineage, "bybit",
            reference_snapshot(reference::ListingStatus::Inactive),
            effective_domain(), id<contracts::ClockDomainId>(12), kVersions)
            .failure == book::BookNormalizationFailure::ReferenceUnavailable);

  auto overflow = *decoded.message;
  overflow.assertions.matching_timestamp_milliseconds =
      std::numeric_limits<std::uint64_t>::max();
  CHECK(book::normalize_book(overflow, *decoded.source_lineage, "bybit",
                             reference_snapshot(), effective_domain(),
                             id<contracts::ClockDomainId>(12), kVersions)
            .failure == book::BookNormalizationFailure::InvalidNumeric);
}
