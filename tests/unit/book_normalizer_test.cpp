#include "chronos/adapters/market_data/bybit_book_decoder.hpp"
#include "chronos/normalization/market_data/book_normalizer.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
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

struct DatasetOptions final {
  sdk::EnvironmentClass environment{sdk::EnvironmentClass::Test};
  sdk::CaptureIntegrityStatus integrity_status{
      sdk::CaptureIntegrityStatus::Complete};
  std::string adapter_id{"chronos.bybit.public-market-data"};
  std::string schema_policy_version{"bybit-v5-public-v1"};
  bool fragmented{};
};

sdk::SourceCaptureContext capture_context(const DatasetOptions &options = {}) {
  return {
      .adapter_id = options.adapter_id,
      .adapter_version = "m2.5",
      .build_version = "test",
      .venue = "bybit",
      .environment = options.environment,
      .endpoint = sdk::EndpointClass::PublicMarketData,
      .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
      .capture_session_id = id<sdk::CaptureSessionId>(1),
      .runtime_id = id<contracts::RuntimeId>(3),
      .connection_id = id<sdk::SourceConnectionId>(13),
      .subscription_id = id<sdk::SourceSubscriptionId>(14),
      .capture_partition_id = id<sdk::CapturePartitionId>(2),
      .framing_version = "websocket-rfc6455-v1",
      .static_configuration_version = "test-v1",
      .capability_manifest_version = "bybit-v5-v1",
      .schema_policy_version = options.schema_policy_version,
      .data_classification = sdk::DataClassification::PublicMarketData,
      .access_restriction = sdk::AccessRestriction::ChronosInternal,
      .maximum_retained_payload_bytes = 1U << 20U,
      .maximum_source_events = 10,
  };
}

adapter::DatasetReadResult
verified_dataset(std::string_view payload, const DatasetOptions &options = {}) {
  static std::uint64_t sequence{};
  const auto path = std::filesystem::temp_directory_path() /
                    ("chronos-book-normalizer-" + std::to_string(++sequence));
  std::filesystem::remove_all(path);
  std::filesystem::remove_all(path.string() + ".partial");
  const auto context = capture_context(options);
  auto recorder = sdk::SourceCaptureRecorder::create(context).value();
  auto writer = adapter::CaptureDatasetWriter::create(path, context).value();
  const auto raw_payload = bytes(payload);
  const auto captured = recorder.capture({
      .source_event_id = id<contracts::SourceEventId>(4),
      .chronos_receive_time =
          contracts::TimePoint::from(900, id<contracts::ClockDomainId>(5),
                                     contracts::ClockClass::monotonic, 1)
              .value(),
      .raw_payload = raw_payload,
      .complete_payload_available =
          options.integrity_status == sdk::CaptureIntegrityStatus::Complete,
      .framing_protocol = sdk::FramingProtocol::WebSocket,
      .frame_kind = sdk::SourceFrameKind::Text,
      .framing_status =
          options.integrity_status == sdk::CaptureIntegrityStatus::Complete
              ? sdk::FramingStatus::Complete
              : sdk::FramingStatus::Incomplete,
      .integrity_status = options.integrity_status,
      .content_encoding = sdk::ContentEncoding::Utf8Text,
      .compression_disposition = sdk::CompressionDisposition::NotCompressed,
      .fragmented = options.fragmented,
  });
  if (!captured.ok() ||
      writer.append(*captured.event) != adapter::DatasetFailure::None ||
      !writer.seal().manifest.has_value()) {
    std::abort();
  }
  auto result = adapter::read_capture_dataset(path);
  std::filesystem::remove_all(path);
  return result;
}

adapter::BybitBookDecodeResult
decode(std::string_view payload, const DatasetOptions &options = {},
       book::SourceProductClass product_class =
           book::SourceProductClass::LinearPerpetual,
       const adapter::BybitBookDecodeLimits &limits = {}) {
  const auto dataset = verified_dataset(payload, options);
  return adapter::decode_bybit_v5_book(
      dataset, 0, {.product_class = product_class}, limits);
}

const book::ReferenceSelectionPolicy kSelectionPolicy{
    .reference_configuration_lineage_id = id<contracts::DefinitionId>(15),
    .lineage_schema_version = "reference-lineage-v1",
    .semantic_key_policy_version = "bybit-semantic-key-v1",
    .effective_basis_policy_version = "capture-sequence-v1",
    .selection_policy_version = "exact-single-match-v1",
};

adapter::CaptureDatasetRecord record(std::string_view payload) {
  const auto dataset = verified_dataset(payload);
  return dataset.records().front();
}

adapter::CaptureDatasetManifest manifest() {
  const auto dataset = verified_dataset("{}");
  return dataset.manifest().value();
}

reference::ReferenceSnapshot reference_snapshot(
    reference::ListingStatus status = reference::ListingStatus::Active) {
  const auto interval = reference::EffectiveInterval::from_capture_sequence(
                            manifest().capture_partition_id, 1, 20)
                            .value();
  return reference::ReferenceSnapshot::create(
             version(7, 2),
             {.instrument_id = id<contracts::CanonicalInstrumentId>(8),
              .version = version(9, 3),
              .base_asset = "BTC",
              .quote_asset = "USDT",
              .product_class = reference::ProductClass::LinearPerpetual,
              .effective_interval = interval},
             {.listing_id = id<contracts::ListingId>(10),
              .instrument_id = id<contracts::CanonicalInstrumentId>(8),
              .version = version(11, 4),
              .venue = "bybit",
              .environment = reference::VenueEnvironment::Test,
              .source_symbol = "BTCUSDT",
              .status = status,
              .price_tick = reference::DecimalIncrement::parse("0.10").value(),
              .quantity_step =
                  reference::DecimalIncrement::parse("0.001").value(),
              .effective_interval = interval})
      .value();
}

const book::BookNormalizerVersions kVersions{
    .decoder_version = "bybit-book-decoder-v1",
    .source_schema_version = "bybit-v5-orderbook-v1",
    .normalizer_version = "chronos-book-normalizer-v1",
};

book::BookNormalizationResult normalize(std::string_view payload) {
  const auto decoded = decode(payload);
  if (!decoded.ok()) {
    return {.failure = decoded.failure};
  }
  return book::normalize_book(*decoded.enrichment, reference_snapshot(),
                              id<contracts::ClockDomainId>(12),
                              kSelectionPolicy, kVersions);
}

constexpr std::string_view kSnapshot = R"({
  "data":{"a":[["42000.50","0.200"],["42000.30","0.100"]],
          "seq":7961638724,"s":"BTCUSDT",
          "b":[["42000.00","1.000"],["42000.20","0.500"]],"u":18521288},
  "cts":1672304486868,"ts":1672304486869,"type":"snapshot",
  "topic":"orderbook.50.BTCUSDT"
})";

constexpr std::string_view kDelta = R"({
  "topic":"orderbook.50.BTCUSDT","type":"delta","ts":1672304486870,
  "data":{"s":"BTCUSDT","b":[["42000.00","0"],["42000.20","0.750"]],
          "a":[["42000.30","0.300"]],"u":18521289,"seq":7961638725},
  "cts":1672304486869
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
  CHECK(fact.reference_selection.reference_configuration_lineage_id ==
        kSelectionPolicy.reference_configuration_lineage_id);
  CHECK(fact.reference_selection.semantic_key.venue == "bybit");
  CHECK(fact.reference_selection.semantic_key.environment ==
        sdk::EnvironmentClass::Test);
  CHECK(fact.reference_selection.semantic_key.product_class ==
        book::SourceProductClass::LinearPerpetual);
  CHECK(fact.reference_selection.semantic_key.source_listing_key == "BTCUSDT");
  CHECK(fact.reference_selection.effective_capture_sequence == 1);
  CHECK(fact.source_lineage.dataset_format_version ==
        "chronos-source-capture-v1");
  CHECK(fact.source_lineage.dataset_id.size() == 64);
  CHECK(fact.source_lineage.records_sha256.size() == 64);
  CHECK(fact.source_lineage.dataset_record_index == 0);
  CHECK(fact.source_lineage.source_event_id ==
        record(kSnapshot).source_event_id);
  CHECK(fact.source_lineage.capture_session_id ==
        manifest().capture_session_id);
  CHECK(fact.source_lineage.runtime_id == manifest().runtime_id);
  CHECK(fact.source_lineage.connection_id == manifest().connection_id);
  CHECK(fact.source_lineage.subscription_id == manifest().subscription_id);
  CHECK(fact.source_lineage.capture_partition_id ==
        manifest().capture_partition_id);
  CHECK(fact.source_lineage.capture_sequence == 1);
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

TEST_CASE("source environment and product class participate in resolution") {
  const auto production =
      decode(kSnapshot, {.environment = sdk::EnvironmentClass::Production});
  CHECK(production.ok());
  CHECK(book::normalize_book(*production.enrichment, reference_snapshot(),
                             id<contracts::ClockDomainId>(12), kSelectionPolicy,
                             kVersions)
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);

  const auto spot = decode(kSnapshot, {}, book::SourceProductClass::Spot);
  CHECK(spot.ok());
  CHECK(book::normalize_book(*spot.enrichment, reference_snapshot(),
                             id<contracts::ClockDomainId>(12), kSelectionPolicy,
                             kVersions)
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);
}

TEST_CASE("registered unknown fields are preserved as canonical extensions") {
  constexpr std::string_view payload = R"({
    "z":{"b":2,"a":"x"},"topic":"orderbook.50.BTCUSDT",
    "type":"snapshot","ts":1672304486869,
    "data":{"s":"BTCUSDT","b":[],"extra":[3,true],"a":[],
            "u":18521288,"seq":7961638724},"cts":1672304486868
  })";
  const auto decoded = decode(payload);
  CHECK(decoded.ok());
  const auto &extensions = decoded.enrichment->message().extensions;
  CHECK(extensions.size() == 2);
  CHECK(extensions[0].path == "$.data.extra");
  CHECK(extensions[0].canonical_json == "[3,true]");
  CHECK(extensions[1].path == "$.z");
  CHECK(extensions[1].canonical_json == R"({"a":"x","b":2})");

  const auto normalized = book::normalize_book(
      *decoded.enrichment, reference_snapshot(),
      id<contracts::ClockDomainId>(12), kSelectionPolicy, kVersions);
  CHECK(normalized.ok());
  CHECK(normalized.fact->source_extensions == extensions);
}

TEST_CASE("decoder only accepts records from a verified dataset result") {
  const auto dataset = verified_dataset(kSnapshot);
  CHECK(adapter::decode_bybit_v5_book(
            dataset, 1,
            {.product_class = book::SourceProductClass::LinearPerpetual})
            .failure == book::BookNormalizationFailure::IntegrityIneligible);
}

TEST_CASE("decoder returns typed failures for malformed and ineligible input") {
  CHECK(decode(kSnapshot,
               {.integrity_status = sdk::CaptureIntegrityStatus::Truncated})
            .failure == book::BookNormalizationFailure::IntegrityIneligible);
  CHECK(decode(kSnapshot, {.fragmented = true}).ok());
  CHECK(decode(kSnapshot, {.adapter_id = "other.bybit.adapter"}).failure ==
        book::BookNormalizationFailure::IntegrityIneligible);
  CHECK(
      decode(kSnapshot, {.schema_policy_version = "unknown-policy"}).failure ==
      book::BookNormalizationFailure::IntegrityIneligible);
  CHECK(decode("{not-json").failure ==
        book::BookNormalizationFailure::MalformedPayload);
  CHECK(decode(R"({"topic":"x","topic":"y","type":"delta","ts":1,"data":{}})")
            .failure == book::BookNormalizationFailure::AmbiguousDuplicate);

  auto limits = adapter::BybitBookDecodeLimits{};
  limits.maximum_payload_bytes = 8;
  CHECK(decode(kSnapshot, {}, book::SourceProductClass::LinearPerpetual, limits)
            .failure == book::BookNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_levels_per_side = 1;
  CHECK(decode(kSnapshot, {}, book::SourceProductClass::LinearPerpetual, limits)
            .failure == book::BookNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_json_depth = 1;
  CHECK(decode(kSnapshot, {}, book::SourceProductClass::LinearPerpetual, limits)
            .failure == book::BookNormalizationFailure::ResourceLimitExceeded);

  std::string invalid_utf8 = "{\"bad\":\"";
  invalid_utf8.push_back(static_cast<char>(0xFF));
  invalid_utf8 += "\"}";
  CHECK(decode(invalid_utf8).failure ==
        book::BookNormalizationFailure::MalformedPayload);
}

TEST_CASE("topic symbol and message family mismatches fail visibly") {
  CHECK(normalize(R"({"topic":"orderbook.200.BTCUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);
  CHECK(normalize(R"({"topic":"orderbook.50.ETHUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);
  CHECK(normalize(R"({"topic":"orderbook.50.ETHUSDT","type":"snapshot","ts":1,
    "data":{"s":"ETHUSDT","b":[],"a":[],"u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::WrongTopicOrSymbol);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"trade","ts":1,
    "data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::UnsupportedMessage);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1,"cts":1}})")
            .failure == book::BookNormalizationFailure::InvalidNumeric);
}

TEST_CASE("inexact and duplicate canonical levels fail without repair") {
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[["42000.15","1.000"]],"a":[],
    "u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::InvalidNumeric);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"delta","ts":1,
    "data":{"s":"BTCUSDT","b":[["42000.10","1.000"],["42000.100","2.000"]],
    "a":[],"u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::AmbiguousDuplicate);
  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1,
    "data":{"s":"BTCUSDT","b":[["42000.10","0"]],"a":[],
    "u":1,"seq":1},"cts":1})")
            .failure == book::BookNormalizationFailure::InvalidNumeric);
}

TEST_CASE("reference and timestamp eligibility are explicit failures") {
  const auto decoded = decode(kSnapshot);
  CHECK(decoded.ok());
  CHECK(book::normalize_book(
            *decoded.enrichment,
            reference_snapshot(reference::ListingStatus::Inactive),
            id<contracts::ClockDomainId>(12), kSelectionPolicy, kVersions)
            .failure == book::BookNormalizationFailure::ReferenceUnavailable);

  CHECK(normalize(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot",
    "ts":1,"data":{"s":"BTCUSDT","b":[],"a":[],"u":1,"seq":1},
    "cts":18446744073709551615})")
            .failure == book::BookNormalizationFailure::InvalidNumeric);
}
