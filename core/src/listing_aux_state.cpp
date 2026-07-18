#include "chronos/core/market_state/listing_aux_state.hpp"

#include "chronos/core/market_state/l2_book.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace chronos::core::market_state {
namespace {

std::optional<std::int64_t>
evaluated_age(std::optional<std::int64_t> evidence_time,
              std::int64_t logical_time) {
  if (!evidence_time)
    return std::nullopt;
  std::int64_t result{};
  if (__builtin_sub_overflow(logical_time, *evidence_time, &result))
    return std::nullopt;
  return result;
}

FreshnessStatus freshness(std::optional<std::int64_t> evidence_time,
                          std::int64_t logical_time, std::int64_t deadline) {
  const auto age = evaluated_age(evidence_time, logical_time);
  if (!age)
    return FreshnessStatus::Unknown;
  return *age <= deadline ? FreshnessStatus::Fresh : FreshnessStatus::Stale;
}

bool next_sequence(std::uint64_t current, std::uint64_t candidate) {
  return current != std::numeric_limits<std::uint64_t>::max() &&
         candidate == current + 1;
}

bool valid_next_cursor(const contracts::StreamCursor &current,
                       const contracts::StreamCursor &candidate) {
  if (candidate.stream_id() != current.stream_id() ||
      candidate.stream_epoch() != current.stream_epoch() ||
      !candidate.last_consumed_sequence()) {
    return false;
  }
  if (current.is_origin())
    return *candidate.last_consumed_sequence() == 0;
  return next_sequence(*current.last_consumed_sequence(),
                       *candidate.last_consumed_sequence());
}

contracts::StreamCursor
configured_cursor(contracts::StreamId stream_id, std::uint64_t epoch,
                  std::optional<std::uint64_t> sequence) {
  if (sequence)
    return *contracts::StreamCursor::at_sequence(stream_id, epoch, *sequence);
  return *contracts::StreamCursor::at_origin(stream_id, epoch);
}

bool known_top_side(L2SideCompleteness completeness) {
  return completeness == L2SideCompleteness::Complete ||
         completeness == L2SideCompleteness::BoundedWithProvenTop;
}

bool valid_book_proof(const ListingAuxConfig &config,
                      const contracts::StreamCursor &current_cursor,
                      BookSynchronization current_status,
                      const BookSynchronizationProof &proof,
                      const L2Book *book) {
  if (!book || book->listing_id() != config.listing_id ||
      book->transition_sequence() == 0 ||
      book->transition_sequence() != proof.l2_transition_sequence ||
      !proof.bridge_complete || !proof.reference_compatible) {
    return false;
  }
  const auto top = book->top_of_book();
  if (!known_top_side(top.bid_completeness) ||
      !known_top_side(top.ask_completeness)) {
    return false;
  }
  if (proof.snapshot_cursor.stream_id() != config.book_stream_id ||
      proof.applied_through_cursor.stream_id() != config.book_stream_id ||
      proof.snapshot_cursor.stream_epoch() !=
          proof.applied_through_cursor.stream_epoch() ||
      !proof.snapshot_cursor.last_consumed_sequence() ||
      !proof.applied_through_cursor.last_consumed_sequence() ||
      *proof.snapshot_cursor.last_consumed_sequence() >
          *proof.applied_through_cursor.last_consumed_sequence()) {
    return false;
  }
  if (current_status == BookSynchronization::Starting)
    return proof.applied_through_cursor.stream_epoch() ==
           current_cursor.stream_epoch();
  return current_status == BookSynchronization::Recovering &&
         proof.applied_through_cursor.stream_epoch() !=
             current_cursor.stream_epoch();
}

bool valid_trade_proof(const ListingAuxConfig &config,
                       const contracts::StreamCursor &current_trade_cursor,
                       const contracts::StreamCursor &continuity_cursor,
                       const TradeContinuityProof &proof) {
  if (!valid_next_cursor(continuity_cursor, proof.boundary_cursor) ||
      proof.boundary_cursor.stream_id() != config.trade_continuity_stream_id ||
      proof.prior_trade_cursor != current_trade_cursor ||
      proof.recovered_trade_cursor.stream_id() != config.trade_stream_id ||
      proof.fidelity == TradeFidelity::Unknown) {
    return false;
  }
  if (proof.recovered_trade_cursor.stream_epoch() ==
      proof.prior_trade_cursor.stream_epoch()) {
    return proof.recovered_trade_cursor == proof.prior_trade_cursor &&
           proof.fidelity == TradeFidelity::Lossless;
  }
  return proof.recovered_trade_cursor.is_origin();
}

bool correction_is_supported(const ListingAuxConfig &config,
                             std::span<const RecentTrade> retained,
                             const RecentTrade &trade) {
  if (!trade.corrects_event_id)
    return true;
  if (config.correction_policy != TradeCorrectionPolicy::RetainProspective)
    return false;
  return std::any_of(retained.begin(), retained.end(), [&](const auto &prior) {
    return prior.event_id == *trade.corrects_event_id;
  });
}

} // namespace

struct ListingAuxState::State final {
  explicit State(ListingAuxConfig initial_config)
      : config(std::move(initial_config)),
        trade_cursor(configured_cursor(config.trade_stream_id,
                                       config.trade_stream_epoch,
                                       config.initial_trade_sequence)),
        trade_continuity_cursor(
            configured_cursor(config.trade_continuity_stream_id,
                              config.trade_continuity_stream_epoch,
                              config.initial_trade_continuity_sequence)),
        book_cursor(configured_cursor(config.book_stream_id,
                                      config.book_stream_epoch,
                                      config.initial_book_sequence)),
        run_input_sequence(config.initial_run_input_sequence),
        logical_time(config.initial_logical_time_nanoseconds) {
    trades.reserve(config.recent_trade_capacity);
  }

  ListingAuxConfig config;
  contracts::StreamCursor trade_cursor;
  contracts::StreamCursor trade_continuity_cursor;
  contracts::StreamCursor book_cursor;
  std::vector<RecentTrade> trades;
  BookSynchronization book_synchronization{BookSynchronization::Starting};
  TradeContinuity trade_continuity{TradeContinuity::Unavailable};
  std::optional<std::int64_t> last_book_evidence;
  std::optional<std::int64_t> last_trade_evidence;
  std::optional<BookSynchronizationProof> last_book_proof;
  std::optional<TradeContinuityProof> last_trade_boundary;
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time{};
};

ListingAuxState::ListingAuxState(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
ListingAuxState::ListingAuxState(ListingAuxState &&) noexcept = default;
ListingAuxState &
ListingAuxState::operator=(ListingAuxState &&) noexcept = default;
ListingAuxState::~ListingAuxState() = default;

std::optional<ListingAuxState>
ListingAuxState::create(ListingAuxConfig config) {
  if (config.trade_stream_epoch == 0 ||
      config.trade_continuity_stream_epoch == 0 ||
      config.book_stream_epoch == 0 || config.recent_trade_capacity == 0 ||
      config.book_freshness_deadline_nanoseconds <= 0 ||
      config.trade_freshness_deadline_nanoseconds <= 0 ||
      !contracts::is_valid(config.accepted_source_clock_class) ||
      config.required_source_time_quality == SourceTimeQuality::Unknown ||
      (config.correction_policy != TradeCorrectionPolicy::Reject &&
       config.correction_policy != TradeCorrectionPolicy::RetainProspective) ||
      config.trade_window_policy != TradeWindowPolicy::AcceptedCount) {
    return std::nullopt;
  }
  return ListingAuxState(std::make_unique<State>(std::move(config)));
}

ListingAuxResult ListingAuxState::apply_trade(const RecentTrade &trade) {
  if (trade.listing_id != state_->config.listing_id)
    return {.failure = ListingAuxFailure::WrongListing};
  if (!next_sequence(state_->run_input_sequence, trade.run_input_sequence)) {
    return {.failure = state_->run_input_sequence ==
                               std::numeric_limits<std::uint64_t>::max()
                           ? ListingAuxFailure::SequenceExhausted
                           : ListingAuxFailure::InvalidRunInputSequence};
  }
  if (trade.logical_time_nanoseconds < state_->logical_time)
    return {.failure = ListingAuxFailure::InvalidLogicalTime};
  if (state_->trade_continuity != TradeContinuity::Continuous)
    return {.failure = ListingAuxFailure::InvalidTransition};
  if (trade.price.definition_ref() != state_->config.price_definition ||
      trade.quantity.definition_ref() != state_->config.quantity_definition) {
    return {.failure = ListingAuxFailure::WrongDefinition};
  }
  if (trade.price.units() <= 0 || trade.quantity.units() <= 0)
    return {.failure = ListingAuxFailure::InvalidTrade};
  if (trade.aggressor_side != TradeAggressorSide::Buy &&
      trade.aggressor_side != TradeAggressorSide::Sell) {
    return {.failure = ListingAuxFailure::InvalidTrade};
  }
  if (trade.source_event_time.clock_domain_id() !=
          state_->config.accepted_source_clock_domain ||
      trade.source_event_time.clock_class() !=
          state_->config.accepted_source_clock_class ||
      trade.source_time_quality !=
          state_->config.required_source_time_quality ||
      trade.fidelity == TradeFidelity::Unknown ||
      !correction_is_supported(state_->config, state_->trades, trade)) {
    return {.failure = ListingAuxFailure::InvalidTrade};
  }
  if (!valid_next_cursor(state_->trade_cursor, trade.cursor))
    return {.failure = ListingAuxFailure::InvalidCursor};

  if (state_->trades.size() == state_->config.recent_trade_capacity)
    state_->trades.erase(state_->trades.begin());
  state_->trades.push_back(trade);
  state_->trade_cursor = trade.cursor;
  state_->last_trade_evidence = trade.logical_time_nanoseconds;
  state_->logical_time = trade.logical_time_nanoseconds;
  state_->run_input_sequence = trade.run_input_sequence;
  return {.content_changed = true,
          .run_input_sequence = state_->run_input_sequence};
}

ListingAuxResult
ListingAuxState::apply_quality_input(const ListingQualityInput &input,
                                     const L2Book *l2_book) {
  if (input.listing_id != state_->config.listing_id)
    return {.failure = ListingAuxFailure::WrongListing};
  if (!next_sequence(state_->run_input_sequence, input.run_input_sequence)) {
    return {.failure = state_->run_input_sequence ==
                               std::numeric_limits<std::uint64_t>::max()
                           ? ListingAuxFailure::SequenceExhausted
                           : ListingAuxFailure::InvalidRunInputSequence};
  }
  if (input.logical_time_nanoseconds < state_->logical_time)
    return {.failure = ListingAuxFailure::InvalidLogicalTime};

  auto book = state_->book_synchronization;
  auto trade = state_->trade_continuity;
  auto trade_cursor = state_->trade_cursor;
  auto trade_continuity_cursor = state_->trade_continuity_cursor;
  auto book_cursor = state_->book_cursor;
  auto book_evidence = state_->last_book_evidence;
  auto trade_evidence = state_->last_trade_evidence;
  auto book_proof = state_->last_book_proof;
  auto trade_boundary = state_->last_trade_boundary;
  bool clear_trades = false;

  switch (input.kind) {
  case ListingQualityInputKind::BookSynchronized:
    if (book != BookSynchronization::Starting &&
        book != BookSynchronization::Recovering)
      return {.failure = ListingAuxFailure::InvalidTransition};
    if (!input.book_proof ||
        !valid_book_proof(state_->config, state_->book_cursor, book,
                          *input.book_proof, l2_book)) {
      return {.failure = ListingAuxFailure::InvalidTransition};
    }
    book = BookSynchronization::Synchronized;
    book_cursor = input.book_proof->applied_through_cursor;
    book_proof = input.book_proof;
    book_evidence = input.logical_time_nanoseconds;
    break;
  case ListingQualityInputKind::BookGapDetected:
    if (book != BookSynchronization::Synchronized)
      return {.failure = ListingAuxFailure::InvalidTransition};
    book = BookSynchronization::Gapped;
    break;
  case ListingQualityInputKind::BookRecoveryStarted:
    if (book != BookSynchronization::Starting &&
        book != BookSynchronization::Gapped &&
        book != BookSynchronization::Invalid)
      return {.failure = ListingAuxFailure::InvalidTransition};
    book = BookSynchronization::Recovering;
    break;
  case ListingQualityInputKind::BookEvidenceObserved:
    if (book != BookSynchronization::Synchronized)
      return {.failure = ListingAuxFailure::InvalidTransition};
    book_evidence = input.logical_time_nanoseconds;
    break;
  case ListingQualityInputKind::BookInvalidated:
    if (book == BookSynchronization::Closed)
      return {.failure = ListingAuxFailure::InvalidTransition};
    book = BookSynchronization::Invalid;
    break;
  case ListingQualityInputKind::TradeSynchronized:
    if (trade != TradeContinuity::Unavailable &&
        trade != TradeContinuity::Recovering)
      return {.failure = ListingAuxFailure::InvalidTransition};
    if (!input.trade_proof ||
        !valid_trade_proof(state_->config, state_->trade_cursor,
                           state_->trade_continuity_cursor,
                           *input.trade_proof)) {
      return {.failure = ListingAuxFailure::InvalidCursor};
    }
    trade = TradeContinuity::Continuous;
    trade_cursor = input.trade_proof->recovered_trade_cursor;
    trade_continuity_cursor = input.trade_proof->boundary_cursor;
    trade_boundary = input.trade_proof;
    trade_evidence.reset();
    clear_trades = true;
    break;
  case ListingQualityInputKind::TradeGapDetected:
    if (trade != TradeContinuity::Continuous)
      return {.failure = ListingAuxFailure::InvalidTransition};
    trade = TradeContinuity::Gapped;
    clear_trades = true;
    break;
  case ListingQualityInputKind::TradeRecoveryStarted:
    if (trade != TradeContinuity::Gapped)
      return {.failure = ListingAuxFailure::InvalidTransition};
    trade = TradeContinuity::Recovering;
    trade_evidence.reset();
    clear_trades = true;
    break;
  case ListingQualityInputKind::LogicalTimerAdvanced:
    break;
  case ListingQualityInputKind::ListingUnavailable:
    book = BookSynchronization::Unavailable;
    trade = TradeContinuity::Unavailable;
    book_evidence.reset();
    trade_evidence.reset();
    clear_trades = true;
    break;
  case ListingQualityInputKind::ListingClosed:
    book = BookSynchronization::Closed;
    trade = TradeContinuity::Closed;
    clear_trades = true;
    break;
  default:
    return {.failure = ListingAuxFailure::InvalidTransition};
  }

  const bool changed =
      book != state_->book_synchronization ||
      trade != state_->trade_continuity ||
      trade_cursor != state_->trade_cursor || clear_trades ||
      trade_continuity_cursor != state_->trade_continuity_cursor ||
      book_cursor != state_->book_cursor ||
      book_evidence != state_->last_book_evidence ||
      trade_evidence != state_->last_trade_evidence ||
      book_proof != state_->last_book_proof ||
      trade_boundary != state_->last_trade_boundary ||
      input.logical_time_nanoseconds != state_->logical_time;
  state_->book_synchronization = book;
  state_->trade_continuity = trade;
  state_->trade_cursor = trade_cursor;
  state_->trade_continuity_cursor = trade_continuity_cursor;
  state_->book_cursor = book_cursor;
  state_->last_book_evidence = book_evidence;
  state_->last_trade_evidence = trade_evidence;
  state_->last_book_proof = book_proof;
  state_->last_trade_boundary = trade_boundary;
  if (clear_trades)
    state_->trades.clear();
  state_->logical_time = input.logical_time_nanoseconds;
  state_->run_input_sequence = input.run_input_sequence;
  return {.content_changed = changed,
          .run_input_sequence = state_->run_input_sequence};
}

contracts::ListingId ListingAuxState::listing_id() const noexcept {
  return state_->config.listing_id;
}

std::span<const RecentTrade> ListingAuxState::recent_trades() const noexcept {
  return state_->trades;
}

ListingQualityState ListingAuxState::quality() const noexcept {
  const bool book_closed =
      state_->book_synchronization == BookSynchronization::Closed;
  const bool trade_closed = state_->trade_continuity == TradeContinuity::Closed;
  const auto book_freshness =
      book_closed ? FreshnessStatus::Closed
      : state_->book_synchronization == BookSynchronization::Unavailable
          ? FreshnessStatus::Unknown
          : freshness(state_->last_book_evidence, state_->logical_time,
                      state_->config.book_freshness_deadline_nanoseconds);
  const auto trade_freshness =
      trade_closed ? FreshnessStatus::Closed
      : state_->trade_continuity == TradeContinuity::Unavailable
          ? FreshnessStatus::Unknown
          : freshness(state_->last_trade_evidence, state_->logical_time,
                      state_->config.trade_freshness_deadline_nanoseconds);
  TradeWindowStatus window_status{TradeWindowStatus::Unavailable};
  switch (state_->trade_continuity) {
  case TradeContinuity::Unavailable:
    window_status = TradeWindowStatus::Unavailable;
    break;
  case TradeContinuity::Continuous:
    window_status =
        state_->trades.size() == state_->config.recent_trade_capacity
            ? TradeWindowStatus::Complete
            : TradeWindowStatus::Incomplete;
    break;
  case TradeContinuity::Gapped:
    window_status = TradeWindowStatus::Gapped;
    break;
  case TradeContinuity::Recovering:
    window_status = TradeWindowStatus::Recovering;
    break;
  case TradeContinuity::Closed:
    window_status = TradeWindowStatus::Closed;
    break;
  }
  return {
      .book_synchronization = state_->book_synchronization,
      .trade_continuity = state_->trade_continuity,
      .book_freshness = book_freshness,
      .trade_freshness = trade_freshness,
      .trade_window_status = window_status,
      .book_age_nanoseconds =
          evaluated_age(state_->last_book_evidence, state_->logical_time),
      .trade_age_nanoseconds =
          evaluated_age(state_->last_trade_evidence, state_->logical_time),
      .logical_time_nanoseconds = state_->logical_time,
      .run_input_sequence = state_->run_input_sequence,
      .freshness_policy_version = state_->config.freshness_policy_version,
      .trade_window_policy_version = state_->config.trade_window_policy_version,
      .trade_window_policy = state_->config.trade_window_policy,
      .book_cursor = state_->book_cursor,
      .trade_cursor = state_->trade_cursor,
      .trade_continuity_cursor = state_->trade_continuity_cursor,
      .last_book_proof = state_->last_book_proof,
      .last_trade_boundary = state_->last_trade_boundary,
  };
}

contracts::StreamCursor ListingAuxState::trade_cursor() const noexcept {
  return state_->trade_cursor;
}

std::size_t ListingAuxState::trade_storage_capacity() const noexcept {
  return state_->trades.capacity();
}

} // namespace chronos::core::market_state
