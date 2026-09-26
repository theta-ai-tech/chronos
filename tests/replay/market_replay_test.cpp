#include "chronos/adapters/market_data/capture_dataset.hpp"
#include "chronos/applications/replay_runner/market_replay.hpp"
#include "chronos/core/market_state/listing_aux_state.hpp"
#include "microtest.hpp"
#include <array>
#include <filesystem>
#include <string_view>
#include <vector>
namespace {
namespace app = chronos::applications::replay_runner;
namespace sdk = chronos::adapters::sdk;
namespace adapter = chronos::adapters::market_data;
namespace contracts = chronos::contracts;
namespace market = chronos::core::market_state;
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

sdk::SourceCaptureContext capture_context() {
  return {
      .adapter_id = "chronos.bybit.public-market-data",
      .adapter_version = "m2.5",
      .build_version = "m3.5-golden",
      .venue = "bybit",
      .environment = sdk::EnvironmentClass::Test,
      .market = sdk::MarketClass::LinearPerpetual,
      .endpoint = sdk::EndpointClass::PublicMarketData,
      .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
      .capture_session_id = id<sdk::CaptureSessionId>(1),
      .runtime_id = id<contracts::RuntimeId>(2),
      .connection_id = id<sdk::SourceConnectionId>(3),
      .subscription_id = id<sdk::SourceSubscriptionId>(4),
      .capture_partition_id = id<sdk::CapturePartitionId>(5),
      .framing_version = "websocket-rfc6455-v1",
      .static_configuration_version = "m3.5-golden-v1",
      .capability_manifest_version = "bybit-v5-v1",
      .schema_policy_version = "bybit-v5-public-v1",
      .data_classification = sdk::DataClassification::PublicMarketData,
      .access_restriction = sdk::AccessRestriction::ChronosInternal,
      .maximum_retained_payload_bytes = 1U << 20U,
      .maximum_source_events = 100,
  };
}

struct CapturedFixture {
  std::filesystem::path path;
  struct Frame final {
    std::string_view payload;
    sdk::SourceFrameKind kind{sdk::SourceFrameKind::Text};

    Frame(std::string_view payload) : payload(payload) {}
    Frame(std::string_view payload, sdk::SourceFrameKind kind)
        : payload(payload), kind(kind) {}
  };

  explicit CapturedFixture(std::vector<std::string_view> payloads)
      : CapturedFixture(text_frames(payloads)) {}

  explicit CapturedFixture(std::vector<Frame> frames) {
    static std::uint64_t sequence{};
    path = std::filesystem::temp_directory_path() /
           ("chronos-market-replay-regression-" + std::to_string(++sequence));
    std::filesystem::remove_all(path);
    std::filesystem::remove_all(path.string() + ".partial");
    const auto context = capture_context();
    auto recorder = sdk::SourceCaptureRecorder::create(context).value();
    auto writer = adapter::CaptureDatasetWriter::create(path, context).value();
    for (std::size_t i = 0; i < frames.size(); ++i) {
      const auto payload = bytes(frames[i].payload);
      auto captured = recorder.capture(
          {.source_event_id =
               id<contracts::SourceEventId>(static_cast<std::uint8_t>(20 + i)),
           .chronos_receive_time =
               contracts::TimePoint::from(
                   static_cast<std::int64_t>(900 + i * 1000000),
                   id<contracts::ClockDomainId>(6),
                   contracts::ClockClass::monotonic, 1)
                   .value(),
           .raw_payload = payload,
           .framing_protocol = sdk::FramingProtocol::WebSocket,
           .frame_kind = frames[i].kind,
           .framing_status = sdk::FramingStatus::Complete,
           .integrity_status = sdk::CaptureIntegrityStatus::Complete,
           .content_encoding = frames[i].kind == sdk::SourceFrameKind::Text
                                   ? sdk::ContentEncoding::Utf8Text
                                   : sdk::ContentEncoding::OpaqueBinary,
           .compression_disposition =
               sdk::CompressionDisposition::NotCompressed});
      if (!captured.ok() ||
          writer.append(*captured.event) != adapter::DatasetFailure::None)
        throw std::runtime_error("capture fixture append failed");
    }
    if (!writer.seal().manifest)
      throw std::runtime_error("capture fixture seal failed");
  }
  ~CapturedFixture() { std::filesystem::remove_all(path); }

private:
  static std::vector<Frame>
  text_frames(const std::vector<std::string_view> &payloads) {
    std::vector<Frame> frames;
    frames.reserve(payloads.size());
    for (const auto payload : payloads)
      frames.emplace_back(payload);
    return frames;
  }
};
market::ListingAuxConfig auxiliary_config() {
  return {.listing_id = id<contracts::ListingId>(1),
          .price_definition = version(2, 1),
          .quantity_definition = version(3, 1),
          .trade_stream_id = id<contracts::StreamId>(4),
          .trade_stream_epoch = 1,
          .trade_continuity_stream_id = id<contracts::StreamId>(7),
          .trade_continuity_stream_epoch = 1,
          .book_stream_id = id<contracts::StreamId>(8),
          .book_stream_epoch = 1,
          .initial_logical_time_nanoseconds = 0,
          .book_freshness_deadline_nanoseconds = 10,
          .trade_freshness_deadline_nanoseconds = 10,
          .freshness_policy_version = version(5, 1),
          .trade_window_policy_version = version(9, 1),
          .accepted_source_clock_domain = id<contracts::ClockDomainId>(6),
          .required_source_time_quality = market::SourceTimeQuality::Exact,
          .recent_trade_capacity = 8};
}
} // namespace
TEST_CASE("multi-level capture feeds generated strategy and preserves one "
          "honest observation sequence") {
  CapturedFixture data({R"({"op":"subscribe","success":true})", kSnapshot,
                        kTrades, kDelta,
                        R"({"topic":"ticker.BTCUSDT","data":{}})"});
  std::vector<contracts::TradeRecommendationId> first, second;
  const auto run = [&](auto &ids) {
    return app::run_market_replay(
        data.path, [&](const app::MarketReplayCut &cut) {
          ids.push_back(cut.recommendation.recommendation_id());
          CHECK(cut.view.quality.trade_continuity ==
                market::TradeContinuity::Unavailable);
          CHECK(cut.view.quality.trade_window_status ==
                market::TradeWindowStatus::Unavailable);
          CHECK(cut.view.recent_trades.empty());
          CHECK(cut.view.bids.back() == *cut.view.top.best_bid);
          CHECK(cut.view.asks.front() == *cut.view.top.best_ask);
          CHECK(cut.profile.price_scale.exponent() == 1);
          CHECK(cut.profile.quantity_scale.exponent() == 3);
          CHECK(cut.view.top.best_bid->price.units() == 420002);
          CHECK(cut.bundle.run_input_sequence == cut.run_input_sequence);
          CHECK(cut.view.lineage.run_input_sequence() ==
                cut.run_input_sequence);
          return true;
        });
  };
  const auto a = run(first), b = run(second);
  CHECK(a.completed);
  CHECK(b.completed);
  CHECK(a.error.empty());
  CHECK(a.capture_records == 5);
  CHECK(a.normalized_book_facts == 2);
  CHECK(a.normalized_trade_facts == 2);
  CHECK(a.control_frames == 1);
  CHECK(a.unsupported_frames == 1);
  CHECK(a.run_inputs == 6);
  CHECK(a.published_views == 6);
  CHECK(a.abstentions == 1);
  CHECK(a.recommendations == 5);
  CHECK(a.recommendation_cardinality_proven);
  CHECK(a.trade_observations_without_continuity == 2);
  CHECK(first == second);
  CHECK(a.semantic_checksum == b.semantic_checksum);
  CHECK(a.final_bundle->logical_time_nanoseconds == 4000000);
}
TEST_CASE("market replay rejects malformed relevant data and missing datasets "
          "visibly") {
  CapturedFixture data(
      {kSnapshot,
       R"({"topic":"orderbook.50.BTCUSDT","type":"delta","data":{}})"});
  const auto result = app::run_market_replay(data.path, {});
  CHECK(!result.completed);
  CHECK(!result.error.empty());
  CHECK(result.capture_records == 2);
  CHECK(!app::run_market_replay(data.path / "missing", {}).completed);
  const auto stopped =
      app::run_market_replay(data.path, [](const auto &) { return false; });
  CHECK(!stopped.completed);
  CHECK(stopped.error == "downstream callback rejected market cut");
}
TEST_CASE("market replay rejects malformed relevant data with escaped topics") {
  for (
      const auto malformed :
      {R"({"topic":"order\u0062ook.50.BTCUSDT","type":"delta","data":{}})",
       R"({"topic":"public\u0054rade.BTCUSDT","type":"snapshot","data":{}})"}) {
    CapturedFixture data({kSnapshot, malformed});
    const auto result = app::run_market_replay(data.path, {});
    CHECK(!result.completed);
    CHECK(!result.error.empty());
    CHECK(result.capture_records == 2);
    CHECK(result.unsupported_frames == 0);
  }
}
TEST_CASE("market replay preserves captured WebSocket ping and pong controls") {
  CapturedFixture data({kSnapshot,
                        {"", sdk::SourceFrameKind::Ping},
                        {"keepalive", sdk::SourceFrameKind::Pong},
                        kDelta});
  const auto result = app::run_market_replay(data.path, {});
  CHECK(result.completed);
  CHECK(result.error.empty());
  CHECK(result.capture_records == 4);
  CHECK(result.control_frames == 2);
  CHECK(result.unsupported_frames == 0);
}
TEST_CASE("unadmitted observation consumes cursor without minting continuity "
          "or freshness") {
  auto aux = market::ListingAuxState::create(auxiliary_config()).value();
  market::ListingQualityInput input{
      .event_id = id<contracts::EventId>(20),
      .input_semantic_checksum = contracts::sha256(bytes("unadmitted")),
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::TradeObservationUnadmitted,
      .run_input_sequence = 1,
      .logical_time_nanoseconds = 1,
      .event_cursor =
          contracts::StreamCursor::at_sequence(id<contracts::StreamId>(4), 1, 0)
              .value()};
  CHECK(aux.apply_quality_input(input).ok());
  CHECK(aux.quality().trade_continuity == market::TradeContinuity::Unavailable);
  CHECK(aux.quality().trade_freshness == market::FreshnessStatus::Unknown);
  CHECK(aux.recent_trades().empty());
  const auto prior = aux.quality();
  input.run_input_sequence = 2;
  CHECK(!aux.apply_quality_input(input).ok());
  CHECK(aux.quality() == prior);
  input.event_cursor =
      contracts::StreamCursor::at_sequence(id<contracts::StreamId>(4), 1, 2)
          .value();
  CHECK(!aux.apply_quality_input(input).ok());
  CHECK(aux.quality() == prior);
  input.event_cursor =
      contracts::StreamCursor::at_sequence(id<contracts::StreamId>(4), 1, 1)
          .value();
  CHECK(aux.apply_quality_input(input).ok());
  input.kind = market::ListingQualityInputKind::SourceObservation;
  input.run_input_sequence = 3;
  input.event_cursor.reset();
  input.logical_time_nanoseconds = 100;
  CHECK(aux.apply_quality_input(input).ok());
  CHECK(aux.quality().trade_freshness == market::FreshnessStatus::Unknown);
  CHECK(aux.quality().book_freshness == market::FreshnessStatus::Unknown);
}
