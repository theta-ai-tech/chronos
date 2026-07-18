#pragma once

#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace chronos::core::market_state {

enum class TradeAggressorSide : std::uint8_t { Buy, Sell };

enum class BookSynchronization : std::uint8_t {
  Unavailable,
  Starting,
  Recovering,
  Synchronized,
  Gapped,
  Invalid,
  Closed,
};

enum class TradeContinuity : std::uint8_t {
  Unavailable,
  Continuous,
  Gapped,
  Recovering,
  Closed,
};

enum class FreshnessStatus : std::uint8_t { Unknown, Fresh, Stale, Closed };

enum class TradeWindowStatus : std::uint8_t {
  Incomplete,
  Complete,
  Gapped,
  Recovering,
  Unavailable,
  Closed,
};

enum class ListingQualityInputKind : std::uint8_t {
  BookSynchronized,
  BookGapDetected,
  BookRecoveryStarted,
  BookEvidenceObserved,
  BookInvalidated,
  TradeSynchronized,
  TradeGapDetected,
  TradeRecoveryStarted,
  LogicalTimerAdvanced,
  ListingUnavailable,
  ListingClosed,
};

enum class ListingAuxFailure : std::uint8_t {
  None,
  WrongListing,
  WrongDefinition,
  InvalidTrade,
  InvalidCursor,
  InvalidRunInputSequence,
  InvalidLogicalTime,
  InvalidTransition,
  SequenceExhausted,
};

struct ListingAuxConfig final {
  contracts::ListingId listing_id;
  contracts::VersionRef price_definition;
  contracts::VersionRef quantity_definition;
  contracts::StreamId trade_stream_id;
  std::uint64_t trade_stream_epoch{};
  std::optional<std::uint64_t> initial_trade_sequence;
  std::uint64_t initial_run_input_sequence{};
  std::int64_t initial_logical_time_nanoseconds{};
  std::int64_t book_freshness_deadline_nanoseconds{};
  std::int64_t trade_freshness_deadline_nanoseconds{};
  contracts::VersionRef freshness_policy_version;
  std::size_t recent_trade_capacity{};
};

struct RecentTrade final {
  contracts::EventId event_id;
  contracts::SourceEventId source_event_id;
  contracts::ListingId listing_id;
  contracts::StreamCursor cursor;
  contracts::TimePoint source_event_time;
  contracts::Price price;
  contracts::Quantity quantity;
  TradeAggressorSide aggressor_side{TradeAggressorSide::Buy};
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time_nanoseconds{};

  bool operator==(const RecentTrade &) const = default;
};

struct ListingQualityInput final {
  contracts::ListingId listing_id;
  ListingQualityInputKind kind{ListingQualityInputKind::LogicalTimerAdvanced};
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time_nanoseconds{};
  std::optional<contracts::StreamCursor> recovered_trade_cursor;
};

struct ListingQualityState final {
  BookSynchronization book_synchronization{BookSynchronization::Starting};
  TradeContinuity trade_continuity{TradeContinuity::Unavailable};
  FreshnessStatus book_freshness{FreshnessStatus::Unknown};
  FreshnessStatus trade_freshness{FreshnessStatus::Unknown};
  TradeWindowStatus trade_window_status{TradeWindowStatus::Unavailable};
  std::optional<std::int64_t> book_age_nanoseconds;
  std::optional<std::int64_t> trade_age_nanoseconds;
  std::int64_t logical_time_nanoseconds{};
  std::uint64_t run_input_sequence{};
  contracts::VersionRef freshness_policy_version;

  bool operator==(const ListingQualityState &) const = default;
};

struct ListingAuxResult final {
  ListingAuxFailure failure{ListingAuxFailure::None};
  bool content_changed{};
  std::uint64_t run_input_sequence{};

  [[nodiscard]] bool ok() const noexcept {
    return failure == ListingAuxFailure::None;
  }
};

class ListingAuxState final {
public:
  [[nodiscard]] static std::optional<ListingAuxState>
  create(ListingAuxConfig config);

  ListingAuxState(ListingAuxState &&) noexcept;
  ListingAuxState &operator=(ListingAuxState &&) noexcept;
  ~ListingAuxState();

  [[nodiscard]] ListingAuxResult apply_trade(const RecentTrade &trade);
  [[nodiscard]] ListingAuxResult
  apply_quality_input(const ListingQualityInput &input);

  [[nodiscard]] contracts::ListingId listing_id() const noexcept;
  [[nodiscard]] std::span<const RecentTrade> recent_trades() const noexcept;
  [[nodiscard]] ListingQualityState quality() const noexcept;
  [[nodiscard]] contracts::StreamCursor trade_cursor() const noexcept;
  [[nodiscard]] std::size_t trade_storage_capacity() const noexcept;

private:
  struct State;
  explicit ListingAuxState(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

} // namespace chronos::core::market_state
