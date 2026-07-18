#include "chronos/core/market_state/listing_aux_state.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <vector>

namespace {
namespace contracts = chronos::contracts;
namespace market = chronos::core::market_state;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), 1)
      .value();
}

market::ListingAuxConfig config(std::size_t capacity = 3) {
  return {
      .listing_id = id<contracts::ListingId>(1),
      .price_definition = version(2),
      .quantity_definition = version(3),
      .trade_stream_id = id<contracts::StreamId>(4),
      .trade_stream_epoch = 1,
      .initial_run_input_sequence = 0,
      .initial_logical_time_nanoseconds = 100,
      .book_freshness_deadline_nanoseconds = 10,
      .trade_freshness_deadline_nanoseconds = 5,
      .freshness_policy_version = version(5),
      .recent_trade_capacity = capacity,
  };
}

contracts::StreamCursor cursor(std::uint64_t epoch, std::uint64_t sequence) {
  return contracts::StreamCursor::at_sequence(id<contracts::StreamId>(4), epoch,
                                              sequence)
      .value();
}

contracts::StreamCursor origin(std::uint64_t epoch) {
  return contracts::StreamCursor::at_origin(id<contracts::StreamId>(4), epoch)
      .value();
}

contracts::TimePoint source_time(std::int64_t nanoseconds) {
  return contracts::TimePoint::from(nanoseconds,
                                    id<contracts::ClockDomainId>(6),
                                    contracts::ClockClass::source_wall, 1)
      .value();
}

market::RecentTrade trade(std::uint8_t seed, std::uint64_t stream_sequence,
                          std::uint64_t run_sequence,
                          std::int64_t logical_time) {
  return {
      .event_id = id<contracts::EventId>(seed),
      .source_event_id = id<contracts::SourceEventId>(seed),
      .listing_id = id<contracts::ListingId>(1),
      .cursor = cursor(1, stream_sequence),
      .source_event_time = source_time(logical_time),
      .price = contracts::Price::from_units(100 + seed, version(2)).value(),
      .quantity = contracts::Quantity::from_units(seed, version(3)).value(),
      .aggressor_side = seed % 2 == 0 ? market::TradeAggressorSide::Buy
                                      : market::TradeAggressorSide::Sell,
      .run_input_sequence = run_sequence,
      .logical_time_nanoseconds = logical_time,
  };
}

market::ListingQualityInput quality_input(market::ListingQualityInputKind kind,
                                          std::uint64_t run_sequence,
                                          std::int64_t logical_time) {
  return {
      .listing_id = id<contracts::ListingId>(1),
      .kind = kind,
      .run_input_sequence = run_sequence,
      .logical_time_nanoseconds = logical_time,
  };
}

std::vector<contracts::AmountUnits>
trade_prices(std::span<const market::RecentTrade> trades) {
  std::vector<contracts::AmountUnits> result;
  for (const auto &entry : trades)
    result.push_back(entry.price.units());
  return result;
}

} // namespace

TEST_CASE("recent trade window retains configured accepted-order tail") {
  auto state = market::ListingAuxState::create(config()).value();
  auto synchronized =
      quality_input(market::ListingQualityInputKind::TradeSynchronized, 1, 100);
  synchronized.recovered_trade_cursor = origin(1);
  CHECK(state.apply_quality_input(synchronized).ok());
  const auto reserved_capacity = state.trade_storage_capacity();

  CHECK(state.apply_trade(trade(1, 0, 2, 101)).ok());
  CHECK(state.apply_trade(trade(2, 1, 3, 102)).ok());
  CHECK(state.quality().trade_window_status ==
        market::TradeWindowStatus::Incomplete);
  CHECK(state.apply_trade(trade(3, 2, 4, 103)).ok());
  CHECK(state.quality().trade_window_status ==
        market::TradeWindowStatus::Complete);
  CHECK(state.apply_trade(trade(4, 3, 5, 104)).ok());

  CHECK(trade_prices(state.recent_trades()) ==
        (std::vector<contracts::AmountUnits>{102, 103, 104}));
  CHECK(state.trade_storage_capacity() == reserved_capacity);
  CHECK(state.trade_cursor() == cursor(1, 3));
  CHECK(state.quality().trade_freshness == market::FreshnessStatus::Fresh);
}

TEST_CASE("trade gap is first-class and recovery starts a new window") {
  auto state = market::ListingAuxState::create(config(2)).value();
  auto synchronized =
      quality_input(market::ListingQualityInputKind::TradeSynchronized, 1, 100);
  synchronized.recovered_trade_cursor = origin(1);
  CHECK(state.apply_quality_input(synchronized).ok());
  CHECK(state.apply_trade(trade(1, 0, 2, 101)).ok());

  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::TradeGapDetected, 3, 102))
            .ok());
  CHECK(state.recent_trades().empty());
  CHECK(state.quality().trade_continuity == market::TradeContinuity::Gapped);
  CHECK(state.quality().trade_window_status ==
        market::TradeWindowStatus::Gapped);
  CHECK(state.apply_trade(trade(2, 1, 4, 103)).failure ==
        market::ListingAuxFailure::InvalidTransition);

  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::TradeRecoveryStarted, 4, 103))
            .ok());
  auto recovered =
      quality_input(market::ListingQualityInputKind::TradeSynchronized, 5, 104);
  recovered.recovered_trade_cursor = origin(2);
  CHECK(state.apply_quality_input(recovered).ok());
  CHECK(state.quality().trade_freshness == market::FreshnessStatus::Unknown);
  auto first_recovered = trade(3, 0, 6, 105);
  first_recovered.cursor = cursor(2, 0);
  CHECK(state.apply_trade(first_recovered).ok());
  CHECK(state.quality().trade_window_status ==
        market::TradeWindowStatus::Incomplete);
}

TEST_CASE("ordered logical timers make freshness stale without hiding gaps") {
  auto state = market::ListingAuxState::create(config()).value();
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::BookSynchronized, 1, 100))
            .ok());
  auto trade_sync =
      quality_input(market::ListingQualityInputKind::TradeSynchronized, 2, 100);
  trade_sync.recovered_trade_cursor = origin(1);
  CHECK(state.apply_quality_input(trade_sync).ok());
  CHECK(state.apply_trade(trade(1, 0, 3, 101)).ok());

  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::LogicalTimerAdvanced, 4, 106))
            .ok());
  CHECK(state.quality().book_freshness == market::FreshnessStatus::Fresh);
  CHECK(state.quality().trade_freshness == market::FreshnessStatus::Fresh);
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::BookGapDetected, 5, 107))
            .ok());
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::LogicalTimerAdvanced, 6, 112))
            .ok());

  const auto quality = state.quality();
  CHECK(quality.book_synchronization == market::BookSynchronization::Gapped);
  CHECK(quality.book_freshness == market::FreshnessStatus::Stale);
  CHECK(quality.trade_freshness == market::FreshnessStatus::Stale);
  CHECK(quality.book_age_nanoseconds == 12);
  CHECK(quality.trade_age_nanoseconds == 11);
}

TEST_CASE("freshness recovers only from ordered qualifying evidence") {
  auto state = market::ListingAuxState::create(config()).value();
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::BookSynchronized, 1, 100))
            .ok());
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::LogicalTimerAdvanced, 2, 111))
            .ok());
  CHECK(state.quality().book_freshness == market::FreshnessStatus::Stale);
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::BookEvidenceObserved, 3, 111))
            .ok());
  CHECK(state.quality().book_freshness == market::FreshnessStatus::Fresh);
  CHECK(state.quality().book_age_nanoseconds == 0);
}

TEST_CASE("invalid order time cursor and trade fail atomically") {
  auto state = market::ListingAuxState::create(config()).value();
  auto synchronized =
      quality_input(market::ListingQualityInputKind::TradeSynchronized, 1, 100);
  synchronized.recovered_trade_cursor = origin(1);
  CHECK(state.apply_quality_input(synchronized).ok());
  const auto before = state.quality();
  const auto before_cursor = state.trade_cursor();

  CHECK(state.apply_trade(trade(1, 0, 3, 101)).failure ==
        market::ListingAuxFailure::InvalidRunInputSequence);
  auto wrong_cursor = trade(1, 2, 2, 101);
  CHECK(state.apply_trade(wrong_cursor).failure ==
        market::ListingAuxFailure::InvalidCursor);
  auto negative = trade(1, 0, 2, 101);
  negative.quantity = contracts::Quantity::from_units(-1, version(3)).value();
  CHECK(state.apply_trade(negative).failure ==
        market::ListingAuxFailure::InvalidTrade);
  auto invalid_side = trade(1, 0, 2, 101);
  invalid_side.aggressor_side = static_cast<market::TradeAggressorSide>(99);
  CHECK(state.apply_trade(invalid_side).failure ==
        market::ListingAuxFailure::InvalidTrade);
  CHECK(state
            .apply_quality_input(quality_input(
                market::ListingQualityInputKind::LogicalTimerAdvanced, 2, 99))
            .failure == market::ListingAuxFailure::InvalidLogicalTime);
  auto unknown_input =
      quality_input(static_cast<market::ListingQualityInputKind>(99), 2, 101);
  CHECK(state.apply_quality_input(unknown_input).failure ==
        market::ListingAuxFailure::InvalidTransition);

  CHECK(state.quality() == before);
  CHECK(state.trade_cursor() == before_cursor);
  CHECK(state.recent_trades().empty());
}

TEST_CASE("invalid auxiliary configuration is rejected") {
  auto invalid = config();
  invalid.recent_trade_capacity = 0;
  CHECK(!market::ListingAuxState::create(invalid));
  invalid = config();
  invalid.trade_freshness_deadline_nanoseconds = 0;
  CHECK(!market::ListingAuxState::create(invalid));
}
