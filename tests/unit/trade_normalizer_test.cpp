#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"
#include "chronos/normalization/market_data/trade_normalizer.hpp"

#include "microtest.hpp"

#include <cstdint>
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
              .source_symbol = "BTCUSDT",
              .status = status,
              .price_tick = reference::DecimalIncrement::parse("0.10").value(),
              .quantity_step =
                  reference::DecimalIncrement::parse("0.001").value(),
              .effective_interval = interval})
      .value();
}

const trade::TradeNormalizerVersions kVersions{
    .decoder_version = "bybit-trade-decoder-v1",
    .source_schema_version = "bybit-v5-public-trade-linear-v1",
    .normalizer_version = "chronos-trade-normalizer-v1",
};

trade::TradeNormalizationResult normalize(std::string_view payload) {
  const auto decoded =
      adapter::decode_bybit_v5_trades(manifest(), record(payload));
  if (!decoded.ok()) {
    return {.failure = decoded.failure};
  }
  return trade::normalize_trades(*decoded.decoded, "bybit",
                                 reference_snapshot(),
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
  CHECK(first.event_type() == "market.trade.observed");
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
  CHECK(first.source_lineage.capture_sequence == 10);
  CHECK(first.source_lineage.chronos_receive_time ==
        record(kTrades).chronos_receive_time);
  CHECK(first.source_lineage.payload_digest == record(kTrades).payload_digest);
  CHECK(first.canonical_instrument_id ==
        reference_snapshot().instrument().instrument_id);
  CHECK(first.listing_id == reference_snapshot().listing().listing_id);
  CHECK(first.reference_snapshot_version == reference_snapshot().version());
  CHECK(first.instrument_version == reference_snapshot().instrument().version);
  CHECK(first.listing_version == reference_snapshot().listing().version);
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
  CHECK(first.decoder_version == kVersions.decoder_version);
  CHECK(first.source_schema_version == kVersions.source_schema_version);
  CHECK(first.normalizer_version == kVersions.normalizer_version);
}

TEST_CASE("trade normalization is deterministic") {
  const auto first = normalize(kTrades);
  const auto second = normalize(kTrades);
  CHECK(first.ok());
  CHECK(second.ok());
  CHECK(first.facts == second.facts);
}

TEST_CASE("trade decoder rejects malformed ineligible and bounded input") {
  auto ineligible = record(kTrades);
  ineligible.integrity_status = sdk::CaptureIntegrityStatus::Truncated;
  CHECK(adapter::decode_bybit_v5_trades(manifest(), ineligible).failure ==
        trade::TradeNormalizationFailure::IntegrityIneligible);
  CHECK(adapter::decode_bybit_v5_trades(manifest(), record("{bad")).failure ==
        trade::TradeNormalizationFailure::MalformedPayload);
  CHECK(adapter::decode_bybit_v5_trades(manifest(),
                                        record(R"({"topic":"a","topic":"b"})"))
            .failure == trade::TradeNormalizationFailure::AmbiguousDuplicate);

  auto limits = adapter::BybitTradeDecodeLimits{};
  limits.maximum_payload_bytes = 8;
  CHECK(adapter::decode_bybit_v5_trades(manifest(), record(kTrades), limits)
            .failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_trades_per_message = 1;
  CHECK(adapter::decode_bybit_v5_trades(manifest(), record(kTrades), limits)
            .failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
  limits = {};
  limits.maximum_json_depth = 1;
  CHECK(adapter::decode_bybit_v5_trades(manifest(), record(kTrades), limits)
            .failure ==
        trade::TradeNormalizationFailure::ResourceLimitExceeded);
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
    "data":[{"T":1,"s":"BTCUSDT","S":"Buy","v":"0.001","p":"1.0",
    "L":"PlusTick","i":"x","BT":false,"new":[true,"value"]}]})");
  CHECK(result.ok());
  CHECK(result.facts.size() == 1);
  CHECK(result.facts[0].source_message_extensions.size() == 2);
  CHECK(result.facts[0].source_message_extensions[0].name == "id");
  CHECK(result.facts[0].source_message_extensions[0].canonical_json ==
        "\"message-1\"");
  CHECK(result.facts[0].source_message_extensions[1].canonical_json ==
        "{\"z\":2,\"a\":1}");
  CHECK(result.facts[0].source_assertions.extensions.size() == 1);
  CHECK(result.facts[0].source_assertions.extensions[0].name == "new");
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

  const auto decoded =
      adapter::decode_bybit_v5_trades(manifest(), record(kTrades));
  CHECK(decoded.ok());
  auto overflow = *decoded.decoded;
  overflow.message.members[0].trade_timestamp_milliseconds =
      std::numeric_limits<std::uint64_t>::max();
  CHECK(trade::normalize_trades(overflow, "bybit", reference_snapshot(),
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::InvalidTime);
  overflow = *decoded.decoded;
  overflow.message.members[0].system_timestamp_milliseconds =
      std::numeric_limits<std::uint64_t>::max();
  CHECK(trade::normalize_trades(overflow, "bybit", reference_snapshot(),
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::InvalidTime);
}

TEST_CASE("trade normalization rejects unavailable or mismatched reference") {
  const auto decoded =
      adapter::decode_bybit_v5_trades(manifest(), record(kTrades));
  CHECK(decoded.ok());
  CHECK(trade::normalize_trades(
            *decoded.decoded, "bybit",
            reference_snapshot(reference::ListingStatus::Inactive),
            id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::ReferenceUnavailable);
  CHECK(trade::normalize_trades(*decoded.decoded, "other", reference_snapshot(),
                                id<contracts::ClockDomainId>(12), kVersions)
            .failure == trade::TradeNormalizationFailure::WrongTopicOrSymbol);
}
