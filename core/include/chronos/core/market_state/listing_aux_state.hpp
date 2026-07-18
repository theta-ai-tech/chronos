#pragma once

#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace chronos::core::market_state {

class L2Book;

enum class TradeAggressorSide : std::uint8_t { Buy, Sell };
enum class TradeWindowPolicy : std::uint8_t { AcceptedCount };
enum class SourceTimeQuality : std::uint8_t { Exact, Approximate, Unknown };
enum class TradeFidelity : std::uint8_t { Lossless, Lossy, Unknown };
enum class TradeCorrectionPolicy : std::uint8_t { Reject, RetainProspective };

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
  contracts::StreamId trade_continuity_stream_id;
  std::uint64_t trade_continuity_stream_epoch{};
  std::optional<std::uint64_t> initial_trade_continuity_sequence;
  contracts::StreamId book_stream_id;
  std::uint64_t book_stream_epoch{};
  std::optional<std::uint64_t> initial_book_sequence;
  std::uint64_t initial_run_input_sequence{};
  std::int64_t initial_logical_time_nanoseconds{};
  std::int64_t book_freshness_deadline_nanoseconds{};
  std::int64_t trade_freshness_deadline_nanoseconds{};
  contracts::VersionRef freshness_policy_version;
  contracts::VersionRef trade_window_policy_version;
  contracts::ClockDomainId accepted_source_clock_domain;
  contracts::ClockClass accepted_source_clock_class{
      contracts::ClockClass::source_wall};
  SourceTimeQuality required_source_time_quality{SourceTimeQuality::Exact};
  TradeCorrectionPolicy correction_policy{TradeCorrectionPolicy::Reject};
  TradeWindowPolicy trade_window_policy{TradeWindowPolicy::AcceptedCount};
  std::size_t recent_trade_capacity{};
};

struct RecentTrade final {
  contracts::EventId event_id;
  contracts::SourceEventId source_event_id;
  contracts::ListingId listing_id;
  contracts::StreamCursor cursor;
  contracts::TimePoint source_event_time;
  SourceTimeQuality source_time_quality{SourceTimeQuality::Unknown};
  TradeFidelity fidelity{TradeFidelity::Unknown};
  std::optional<contracts::EventId> corrects_event_id;
  contracts::Price price;
  contracts::Quantity quantity;
  TradeAggressorSide aggressor_side{TradeAggressorSide::Buy};
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time_nanoseconds{};

  bool operator==(const RecentTrade &) const = default;
};

struct BookSynchronizationProof final {
  contracts::EventId snapshot_event_id;
  contracts::StreamCursor snapshot_cursor;
  contracts::StreamCursor applied_through_cursor;
  std::uint64_t l2_transition_sequence{};
  bool bridge_complete{};
  bool reference_compatible{};

  bool operator==(const BookSynchronizationProof &) const = default;
};

struct TradeContinuityProof final {
  contracts::EventId boundary_event_id;
  contracts::StreamCursor boundary_cursor;
  contracts::StreamCursor prior_trade_cursor;
  contracts::StreamCursor recovered_trade_cursor;
  TradeFidelity fidelity{TradeFidelity::Unknown};

  bool operator==(const TradeContinuityProof &) const = default;
};

struct ListingQualityInput final {
  contracts::ListingId listing_id;
  ListingQualityInputKind kind{ListingQualityInputKind::LogicalTimerAdvanced};
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time_nanoseconds{};
  std::optional<BookSynchronizationProof> book_proof;
  std::optional<TradeContinuityProof> trade_proof;
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
  contracts::VersionRef trade_window_policy_version;
  TradeWindowPolicy trade_window_policy{TradeWindowPolicy::AcceptedCount};
  contracts::StreamCursor book_cursor;
  contracts::StreamCursor trade_cursor;
  contracts::StreamCursor trade_continuity_cursor;
  std::optional<BookSynchronizationProof> last_book_proof;
  std::optional<TradeContinuityProof> last_trade_boundary;

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
  apply_quality_input(const ListingQualityInput &input,
                      const L2Book *book = nullptr);

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
