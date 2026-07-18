#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"
#include "chronos/normalization/market_data/trade_normalizer.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace adapter = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;
namespace reference = chronos::core::reference_data;
namespace trade = chronos::normalization::market_data;

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
  sdk::MarketClass market{sdk::MarketClass::LinearPerpetual};
  sdk::CaptureIntegrityStatus integrity_status{
      sdk::CaptureIntegrityStatus::Complete};
};

sdk::SourceCaptureContext capture_context(const DatasetOptions &options = {}) {
  return {
      .adapter_id = "chronos.bybit.public-market-data",
      .adapter_version = "m2.5",
      .build_version = "test",
      .venue = "bybit",
      .environment = options.environment,
      .market = options.market,
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
      .schema_policy_version = "bybit-v5-public-v1",
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
                    ("chronos-trade-normalizer-" + std::to_string(++sequence));
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

adapter::CaptureDatasetManifest manifest() {
  return verified_dataset("{}").manifest().value();
}

adapter::CaptureDatasetRecord record(std::string_view payload) {
  return verified_dataset(payload).records().front();
}

adapter::BybitTradeDecodeResult
decode(std::string_view payload, const DatasetOptions &options = {},
       const adapter::BybitTradeDecodeLimits &limits = {}) {
  const auto dataset = verified_dataset(payload, options);
  return adapter::decode_bybit_v5_trades(dataset, 0, limits);
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

reference::ReferenceConfigurationLineage reference_lineage(
    reference::ListingStatus status = reference::ListingStatus::Active) {
  return reference::ReferenceConfigurationLineage::create(
             1, "reference-lineage-v1", "bybit-semantic-key-v1",
             "capture-sequence-v1", "exact-single-match-v1",
             {reference_snapshot(status)})
      .value();
}

const trade::TradeNormalizerVersions kVersions{
    .normalizer_version = "chronos-trade-normalizer-v1",
};

trade::TradeNormalizationResult normalize(std::string_view payload) {
  const auto decoded = decode(payload);
  if (!decoded.ok()) {
    return {.failure = decoded.failure};
  }
  const auto lineage = reference_lineage();
  return trade::normalize_trades(*decoded.enrichment, lineage,
                                 id<contracts::ClockDomainId>(12), kVersions);
}

constexpr std::string_view kTrades = R"({
  "topic":"publicTrade.BTCUSDT","type":"snapshot","ts":1672304486868,
  "data":[
    {"T":1672304486867,"s":"BTCUSDT","S":"Sell","v":"0.002",
     "p":"16578.60","L":"MinusTick","i":"trade-b","BT":false,
     "RPI":true,"seq":1783284618},
    {"T":1672304486865,"s":"BTCUSDT","S":"Buy","v":"0.001",
     "p":"16578.50","L":"PlusTick","i":"trade-a","BT":true}
  ]
})";

} // namespace

TEST_CASE("all Bybit trade members normalize in source order with lineage") {
  const auto result = normalize(kTrades);
  CHECK(result.ok());
  CHECK(result.facts.size() == 2);
  const auto &first = result.facts[0];
  const auto &second = result.facts[1];
  CHECK(first.event_type() == "market.trade.observation.executed");
  CHECK(first.source_assertions.source_trade_id == "trade-b");
  CHECK(second.source_assertions.source_trade_id == "trade-a");
  CHECK(first.source_assertions.member_index == 0);
  CHECK(second.source_assertions.member_index == 1);
  CHECK(first.source_lineage == second.source_lineage);
  CHECK(first.source_lineage.source_event_id ==
        record(kTrades).source_event_id);
  CHECK(first.source_lineage.capture_session_id ==
        manifest().capture_session_id);
  CHECK(first.source_lineage.runtime_id == manifest().runtime_id);
  CHECK(first.source_lineage.connection_id == manifest().connection_id);
  CHECK(first.source_lineage.subscription_id == manifest().subscription_id);
  CHECK(first.source_lineage.capture_partition_id ==
        manifest().capture_partition_id);
  CHECK(first.source_lineage.capture_sequence == 1);
  CHECK(first.source_lineage.chronos_receive_time ==
        record(kTrades).chronos_receive_time);
  CHECK(first.source_lineage.payload_digest == record(kTrades).payload_digest);
  CHECK(first.canonical_instrument_id ==
        reference_snapshot().instrument().instrument_id);
  CHECK(first.listing_id == reference_snapshot().listing().listing_id);
  CHECK(first.reference_snapshot_version == reference_snapshot().version());
  CHECK(first.instrument_version == reference_snapshot().instrument().version);
  CHECK(first.listing_version == reference_snapshot().listing().version);
  CHECK(first.reference_selection.reference_configuration_lineage_version ==
        reference_lineage().version());
  CHECK(first.reference_selection.semantic_key.product_class ==
        trade::SourceProductClass::LinearPerpetual);
  CHECK(first.source_decode_evidence.source_member_index == 0);
  CHECK(second.source_decode_evidence.source_member_index == 1);
  CHECK(first.source_decode_evidence.source_decode_enrichment_id !=
        second.source_decode_evidence.source_decode_enrichment_id);
  CHECK(first.aggressor_side == trade::AggressorSide::Sell);
  CHECK(second.aggressor_side == trade::AggressorSide::Buy);
  CHECK(first.price.units() == 165786);
  CHECK(first.quantity.units() == 2);
  CHECK(first.source_event_time.nanoseconds() == 1672304486867000000LL);
  CHECK(first.source_assertions.system_timestamp_milliseconds ==
        1672304486868ULL);
  CHECK(first.source_assertions.tick_direction == "MinusTick");
  CHECK(!first.source_assertions.block_trade);
  CHECK(first.source_assertions.rpi_trade == true);
  CHECK(first.source_assertions.sequence == 1783284618ULL);
  CHECK(!second.source_assertions.rpi_trade.has_value());
  CHECK(!second.source_assertions.sequence.has_value());
  CHECK(first.decoder_version == "bybit-trade-decoder-v1");
  CHECK(first.source_schema_version == "bybit-v5-public-trade-v1");
  CHECK(first.normalizer_version == kVersions.normalizer_version);
}

TEST_CASE("trade normalization is deterministic") {
  const auto first = normalize(kTrades);
  const auto second = normalize(kTrades);
  CHECK(first.ok());
  CHECK(second.ok());
  CHECK(first.facts == second.facts);
}

TEST_CASE("trade product class is capture-bound and drives resolution") {
  const auto linear = decode(kTrades);
  const auto spot = decode(kTrades, {.market = sdk::MarketClass::Spot});
  CHECK(linear.ok());
  CHECK(spot.ok());
  CHECK(linear.enrichment->message().members[0].product_class ==
        trade::SourceProductClass::LinearPerpetual);
  CHECK(spot.enrichment->message().members[0].product_class ==
        trade::SourceProductClass::Spot);
  CHECK(linear.enrichment->decode_evidence()[0].source_decode_enrichment_id !=
        spot.enrichment->decode_evidence()[0].source_decode_enrichment_id);
  const auto lineage = reference_lineage();
  CHECK(trade::normalize_trades(*spot.enrichment, lineage,
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::ReferenceUnavailable);
}

TEST_CASE("trade decoder rejects malformed ineligible and bounded input") {
  CHECK(decode(kTrades,
               {.integrity_status = sdk::CaptureIntegrityStatus::Truncated})
            .failure == trade::TradeNormalizationFailure::IntegrityIneligible);
  CHECK(decode("{bad").failure ==
        trade::TradeNormalizationFailure::MalformedPayload);
  CHECK(decode(R"({"topic":"a","topic":"b"})").failure ==
        trade::TradeNormalizationFailure::AmbiguousDuplicate);
  const auto dataset = verified_dataset(kTrades);
  CHECK(adapter::decode_bybit_v5_trades(dataset, 1).failure ==
        trade::TradeNormalizationFailure::IntegrityIneligible);

  auto limits = adapter::BybitTradeDecodeLimits{};
  limits.maximum_payload_bytes = 8;
  CHECK(decode(kTrades, {}, limits).failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_trades_per_message = 1;
  CHECK(decode(kTrades, {}, limits).failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_json_depth = 1;
  CHECK(decode(kTrades, {}, limits).failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_normalized_output_bytes = 1024;
  const auto amplified = decode(kTrades, {}, limits);
  CHECK(amplified.failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
  CHECK(!amplified.enrichment.has_value());
}

TEST_CASE("trade topic symbol and message family mismatches fail visibly") {
  CHECK(normalize(R"({"topic":"publicTrades.BTCUSDT","type":"snapshot",
    "ts":1,"data":[]})")
            .failure == trade::TradeNormalizationFailure::WrongTopicOrSymbol);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"delta",
    "ts":1,"data":[]})")
            .failure == trade::TradeNormalizationFailure::UnsupportedMessage);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"ETHUSDT","S":"Buy","v":"1",
    "p":"1","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::WrongTopicOrSymbol);
}

TEST_CASE("duplicate IDs and fields fail atomically") {
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[
    {"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001","p":"1.0",
     "L":"PlusTick","i":"same","BT":false},
    {"T":2,"s":"BTCUSDT","S":"Sell","v":"0.001","p":"1.0",
     "L":"MinusTick","i":"same","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::AmbiguousDuplicate);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Buy","S":"Sell",
    "v":"0.001","p":"1.0","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::AmbiguousDuplicate);
}

TEST_CASE(
    "unknown envelope and member fields are preserved deterministically") {
  const auto result = normalize(R"({"topic":"publicTrade.BTCUSDT",
    "type":"snapshot","ts":1,"id":"message-1","future":{"z":2,"a":1},
    "data/0/new":4,
    "data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001","p":"1.0",
    "L":"PlusTick","i":"x","BT":false,"new":[true,"value"]}]})");
  CHECK(result.ok());
  CHECK(result.facts.size() == 1);
  CHECK(result.facts[0].source_message_extensions.size() == 3);
  CHECK(result.facts[0].source_message_extensions[0].json_pointer ==
        "/data~10~1new");
  CHECK(result.facts[0].source_message_extensions[0].canonical_json == "4");
  CHECK(result.facts[0].source_message_extensions[1].json_pointer == "/future");
  CHECK(result.facts[0].source_message_extensions[1].canonical_json ==
        "{\"a\":1,\"z\":2}");
  CHECK(result.facts[0].source_message_extensions[2].json_pointer == "/id");
  CHECK(result.facts[0].source_message_extensions[2].canonical_json ==
        "\"message-1\"");
  CHECK(result.facts[0].source_assertions.extensions.size() == 1);
  CHECK(result.facts[0].source_assertions.extensions[0].json_pointer ==
        "/data/0/new");
  CHECK(result.facts[0].source_assertions.extensions[0].canonical_json ==
        "[true,\"value\"]");
}

TEST_CASE("required and optional Bybit trade fields follow explicit policy") {
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001",
    "p":"1.0","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::SchemaViolation);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001",
    "p":"1.0","L":"PlusTick","i":"x"}]})")
            .failure == trade::TradeNormalizationFailure::SchemaViolation);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001",
    "p":"1.0","L":"PlusTick","i":"x","BT":false,
    "RPI":"false"}]})")
            .failure == trade::TradeNormalizationFailure::SchemaViolation);
}

TEST_CASE("invalid side numeric and time values have typed failures") {
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Unknown","v":"0.001",
    "p":"1.0","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::InvalidSide);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.0015",
    "p":"1.0","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::InvalidNumeric);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001",
    "p":"1.05","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::InvalidNumeric);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":0,"data":[]})")
            .failure == trade::TradeNormalizationFailure::InvalidTime);

  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":1,"data":[{"T":18446744073709551615,"s":"BTCUSDT","S":"Buy",
    "v":"0.001","p":"1.0","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::InvalidTime);
  CHECK(normalize(R"({"topic":"publicTrade.BTCUSDT","type":"snapshot",
    "ts":18446744073709551615,"data":[{"T":1,"s":"BTCUSDT","S":"Buy",
    "v":"0.001","p":"1.0","L":"PlusTick","i":"x","BT":false}]})")
            .failure == trade::TradeNormalizationFailure::InvalidTime);
}

TEST_CASE("trade normalization rejects unavailable or mismatched reference") {
  const auto decoded = decode(kTrades);
  CHECK(decoded.ok());
  const auto inactive_lineage =
      reference_lineage(reference::ListingStatus::Inactive);
  CHECK(trade::normalize_trades(*decoded.enrichment, inactive_lineage,
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::ReferenceUnavailable);

  const auto production =
      decode(kTrades, {.environment = sdk::EnvironmentClass::Production});
  CHECK(production.ok());
  const auto active_lineage = reference_lineage();
  CHECK(trade::normalize_trades(*production.enrichment, active_lineage,
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::ReferenceUnavailable);
}

TEST_CASE("trade normalization rejects ambiguous reference authority") {
  const auto decoded = decode(kTrades);
  CHECK(decoded.ok());
  const auto primary = reference_snapshot();
  const auto duplicate =
      reference::ReferenceSnapshot::create(version(15, 2), primary.instrument(),
                                           primary.listing())
          .value();
  const auto ambiguous =
      reference::ReferenceConfigurationLineage::create(
          1, "reference-lineage-v1", "bybit-semantic-key-v1",
          "capture-sequence-v1", "exact-single-match-v1", {primary, duplicate})
          .value();
  CHECK(trade::normalize_trades(*decoded.enrichment, ambiguous,
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::ReferenceAmbiguous);
}
