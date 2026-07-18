#include "chronos/core/market_state/listing_view_publisher.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

namespace chronos::core::market_state {
namespace {

template <typename Id>
void append_id(std::vector<std::byte> &output, const Id &value) {
  for (const auto byte : value.bytes())
    output.push_back(static_cast<std::byte>(byte));
}

template <typename Integer>
void append_integer(std::vector<std::byte> &output, Integer value) {
  using Unsigned = std::make_unsigned_t<Integer>;
  const auto converted = static_cast<Unsigned>(value);
  for (std::size_t index = 0; index < sizeof(Integer); ++index) {
    const auto shift = (sizeof(Integer) - index - 1) * 8U;
    output.push_back(
        static_cast<std::byte>((converted >> shift) & Unsigned{0xFF}));
  }
}

template <typename Enum>
void append_enum(std::vector<std::byte> &output, Enum value) {
  append_integer(output, static_cast<std::underlying_type_t<Enum>>(value));
}

void append_bool(std::vector<std::byte> &output, bool value) {
  append_integer<std::uint8_t>(output, value ? 1 : 0);
}

void append_string(std::vector<std::byte> &output, std::string_view value) {
  append_integer(output, static_cast<std::uint64_t>(value.size()));
  for (const auto character : value)
    output.push_back(static_cast<std::byte>(character));
}

void append_digest(std::vector<std::byte> &output,
                   const contracts::Sha256Digest &value) {
  for (const auto byte : value.bytes)
    output.push_back(static_cast<std::byte>(byte));
}

void append_event_position(std::vector<std::byte> &output,
                           const contracts::EventPosition &value) {
  append_id(output, value.stream_id());
  append_integer(output, value.stream_epoch());
  append_integer(output, value.stream_sequence());
}

void append_optional_u64(std::vector<std::byte> &output,
                         const std::optional<std::uint64_t> &value) {
  append_bool(output, value.has_value());
  if (value)
    append_integer(output, *value);
}

void append_version(std::vector<std::byte> &output,
                    const contracts::VersionRef &value) {
  append_id(output, value.definition_id());
  append_integer(output, value.version());
}

void append_cursor(std::vector<std::byte> &output,
                   const contracts::StreamCursor &cursor) {
  append_id(output, cursor.stream_id());
  append_integer(output, cursor.stream_epoch());
  append_bool(output, cursor.last_consumed_sequence().has_value());
  if (cursor.last_consumed_sequence())
    append_integer(output, *cursor.last_consumed_sequence());
}

template <typename Id>
void append_optional_id(std::vector<std::byte> &output,
                        const std::optional<Id> &value) {
  append_bool(output, value.has_value());
  if (value)
    append_id(output, *value);
}

void append_level(std::vector<std::byte> &output, const L2Level &level) {
  append_integer(output, level.price.units());
  append_version(output, level.price.definition_ref());
  append_integer(output, level.quantity.units());
  append_version(output, level.quantity.definition_ref());
}

void append_optional_level(std::vector<std::byte> &output,
                           const std::optional<L2Level> &level) {
  append_bool(output, level.has_value());
  if (level)
    append_level(output, *level);
}

void append_time(std::vector<std::byte> &output,
                 const contracts::TimePoint &time) {
  append_integer(output, time.nanoseconds());
  append_id(output, time.clock_domain_id());
  append_enum(output, time.clock_class());
  append_integer(output, time.precision_nanoseconds());
}

void append_trade(std::vector<std::byte> &output, const RecentTrade &trade) {
  append_id(output, trade.event_id);
  append_id(output, trade.source_event_id);
  append_id(output, trade.listing_id);
  append_cursor(output, trade.cursor);
  append_time(output, trade.source_event_time);
  append_enum(output, trade.source_time_quality);
  append_enum(output, trade.fidelity);
  append_optional_id(output, trade.corrects_event_id);
  append_integer(output, trade.price.units());
  append_version(output, trade.price.definition_ref());
  append_integer(output, trade.quantity.units());
  append_version(output, trade.quantity.definition_ref());
  append_enum(output, trade.aggressor_side);
  append_integer(output, trade.run_input_sequence);
  append_integer(output, trade.logical_time_nanoseconds);
}

void append_book_proof(std::vector<std::byte> &output,
                       const BookSynchronizationProof &proof) {
  append_id(output, proof.snapshot_event_id);
  append_cursor(output, proof.snapshot_cursor);
  append_cursor(output, proof.applied_through_cursor);
  append_integer(output, proof.l2_transition_sequence);
  append_bool(output, proof.bridge_complete);
  append_bool(output, proof.reference_compatible);
}

void append_trade_proof(std::vector<std::byte> &output,
                        const TradeContinuityProof &proof) {
  append_id(output, proof.boundary_event_id);
  append_cursor(output, proof.boundary_cursor);
  append_cursor(output, proof.prior_trade_cursor);
  append_cursor(output, proof.recovered_trade_cursor);
  append_enum(output, proof.fidelity);
}

template <typename Value, typename Append>
void append_optional(std::vector<std::byte> &output,
                     const std::optional<Value> &value, Append append) {
  append_bool(output, value.has_value());
  if (value)
    append(output, *value);
}

void append_quality(std::vector<std::byte> &output,
                    const ListingQualityState &quality) {
  append_enum(output, quality.book_synchronization);
  append_enum(output, quality.trade_continuity);
  append_enum(output, quality.book_freshness);
  append_enum(output, quality.trade_freshness);
  append_enum(output, quality.trade_window_status);
  append_optional(
      output, quality.book_age_nanoseconds,
      [](auto &bytes, auto value) { append_integer(bytes, value); });
  append_optional(
      output, quality.trade_age_nanoseconds,
      [](auto &bytes, auto value) { append_integer(bytes, value); });
  append_integer(output, quality.logical_time_nanoseconds);
  append_integer(output, quality.run_input_sequence);
  append_version(output, quality.freshness_policy_version);
  append_version(output, quality.trade_window_policy_version);
  append_enum(output, quality.trade_window_policy);
  append_cursor(output, quality.book_cursor);
  append_cursor(output, quality.trade_cursor);
  append_cursor(output, quality.trade_continuity_cursor);
  append_optional(output, quality.last_book_proof, append_book_proof);
  append_optional(output, quality.last_trade_boundary, append_trade_proof);
}

std::vector<std::byte> canonical_view_bytes(
    const ListingViewPublisherConfig &config, const ListingViewCutInput &input,
    std::uint64_t l2_transition_sequence, std::span<const L2Level> bids,
    std::span<const L2Level> asks, const L2TopOfBook &top,
    std::span<const RecentTrade> trades, const ListingQualityState &quality,
    const std::optional<contracts::StateViewId> &prior_view_id) {
  std::vector<std::byte> output;
  output.reserve(1024 + (bids.size() + asks.size()) * 80 + trades.size() * 256);
  constexpr std::string_view domain = "chronos.listing-state-view.v1";
  for (const auto character : domain)
    output.push_back(static_cast<std::byte>(character));
  append_id(output, config.run_id);
  append_id(output, config.listing_id);
  append_id(output, input.selection_id);
  append_id(output, input.selected_event_id);
  append_string(output, input.selected_event_type);
  append_event_position(output, input.selected_event_position);
  append_digest(output, input.input_semantic_checksum);
  append_digest(output, input.selection_semantic_checksum);
  append_version(output, input.merge_policy_version);
  append_integer(output, input.configuration_epoch);
  append_optional_u64(output, input.effective_control_position);
  append_integer(output, input.lineage.run_input_sequence());
  append_integer(output,
                 static_cast<std::uint64_t>(input.lineage.cursors().size()));
  for (const auto &cursor : input.lineage.cursors())
    append_cursor(output, cursor);
  append_integer(output, l2_transition_sequence);
  append_integer(output, static_cast<std::uint64_t>(bids.size()));
  for (const auto &level : bids)
    append_level(output, level);
  append_integer(output, static_cast<std::uint64_t>(asks.size()));
  for (const auto &level : asks)
    append_level(output, level);
  append_optional_level(output, top.best_bid);
  append_optional_level(output, top.best_ask);
  append_bool(output, top.spread.has_value());
  if (top.spread) {
    append_integer(output, top.spread->units());
    append_version(output, top.spread->definition_ref());
  }
  append_enum(output, top.bid_completeness);
  append_enum(output, top.ask_completeness);
  append_enum(output, top.shape);
  append_integer(output, static_cast<std::uint64_t>(trades.size()));
  for (const auto &trade : trades)
    append_trade(output, trade);
  append_quality(output, quality);
  append_optional_id(output, prior_view_id);
  append_version(output, config.view_schema_version);
  append_version(output, config.capability_version);
  append_version(output, config.transition_policy_version);
  append_version(output, config.arithmetic_version);
  append_version(output, config.canonicalization_version);
  return output;
}

std::optional<contracts::StateViewId>
view_id(const contracts::Sha256Digest &checksum) {
  contracts::StateViewId::bytes_type bytes{};
  std::copy_n(checksum.bytes.begin(), bytes.size(), bytes.begin());
  return contracts::StateViewId::from_bytes(bytes);
}

const contracts::StreamCursor *
find_cursor(const contracts::StateLineage &lineage,
            contracts::StreamId stream_id) {
  const auto found = std::find_if(
      lineage.cursors().begin(), lineage.cursors().end(),
      [&](const auto &cursor) { return cursor.stream_id() == stream_id; });
  return found == lineage.cursors().end() ? nullptr : &*found;
}

bool nonzero(const contracts::Sha256Digest &value) {
  return std::any_of(value.bytes.begin(), value.bytes.end(),
                     [](std::uint8_t byte) { return byte != 0; });
}

bool complete_lineage(const ListingViewPublisherConfig &config,
                      const contracts::StateLineage &lineage) {
  if (lineage.run_id() != config.run_id ||
      lineage.cursors().size() != config.required_streams.size()) {
    return false;
  }
  return std::all_of(config.required_streams.begin(),
                     config.required_streams.end(), [&](const auto stream) {
                       return find_cursor(lineage, stream) != nullptr;
                     });
}

std::optional<contracts::StreamId>
event_stream(const ListingViewPublisherConfig &config,
             std::string_view event_type) {
  if (event_type.starts_with("market.book."))
    return config.book_stream_id;
  if (event_type.starts_with("market.trade.continuity."))
    return config.trade_continuity_stream_id;
  if (event_type.starts_with("market.trade."))
    return config.trade_stream_id;
  if (event_type.starts_with("reference."))
    return config.reference_stream_id;
  if (event_type.starts_with("market.control."))
    return config.market_control_stream_id;
  if (event_type.starts_with("run.control."))
    return config.run_control_stream_id;
  if (event_type.starts_with("run.timer."))
    return config.run_timer_stream_id;
  return std::nullopt;
}

bool cursor_follows_position(const contracts::StreamCursor &prior,
                             const contracts::EventPosition &position) {
  if (prior.stream_id() != position.stream_id() ||
      prior.stream_epoch() != position.stream_epoch()) {
    return false;
  }
  if (prior.is_origin())
    return position.stream_sequence() == 0;
  const auto sequence = prior.last_consumed_sequence();
  return sequence && *sequence != std::numeric_limits<std::uint64_t>::max() &&
         position.stream_sequence() == *sequence + 1;
}

bool valid_lineage_transition(const ListingViewPublisherConfig &config,
                              const contracts::StateLineage &prior,
                              const ListingViewCutInput &input,
                              const ListingQualityState &quality) {
  if (!complete_lineage(config, prior) ||
      !complete_lineage(config, input.lineage) ||
      prior.run_input_sequence() == std::numeric_limits<std::uint64_t>::max() ||
      input.lineage.run_input_sequence() != prior.run_input_sequence() + 1) {
    return false;
  }
  const auto selected_stream = event_stream(config, input.selected_event_type);
  if (!selected_stream ||
      input.selected_event_position.stream_id() != *selected_stream) {
    return false;
  }
  const auto expected_cursor = contracts::StreamCursor::at_sequence(
      *selected_stream, input.selected_event_position.stream_epoch(),
      input.selected_event_position.stream_sequence());
  const auto *prior_selected = find_cursor(prior, *selected_stream);
  const auto *next_selected = find_cursor(input.lineage, *selected_stream);
  if (!expected_cursor || !prior_selected || !next_selected ||
      *next_selected != *expected_cursor ||
      !cursor_follows_position(*prior_selected,
                               input.selected_event_position)) {
    return false;
  }

  for (const auto &next : input.lineage.cursors()) {
    const auto *previous = find_cursor(prior, next.stream_id());
    if (!previous)
      return false;
    if (next.stream_id() == *selected_stream)
      continue;
    if (*previous == next)
      continue;
    const bool continuity_epoch_transition =
        *selected_stream == config.trade_continuity_stream_id &&
        next.stream_id() == config.trade_stream_id &&
        quality.last_trade_boundary &&
        quality.last_trade_boundary->prior_trade_cursor == *previous &&
        quality.last_trade_boundary->recovered_trade_cursor == next;
    if (!continuity_epoch_transition)
      return false;
  }
  return true;
}

bool valid_transition(ViewPublicationState from, ViewPublicationState to) {
  if (to == ViewPublicationState::PublicationInProgress) {
    return from == ViewPublicationState::NotPublished ||
           from == ViewPublicationState::PublicationFailedRetryable;
  }
  if (from == ViewPublicationState::PublicationInProgress) {
    return to == ViewPublicationState::PublishedToFeatureBoundary ||
           to == ViewPublicationState::PublicationFailedRetryable ||
           to == ViewPublicationState::PublicationFailedTerminal;
  }
  return from == ViewPublicationState::PublishedToFeatureBoundary &&
         to == ViewPublicationState::FeatureConsumerAccepted;
}

} // namespace

struct ListingViewPublisher::State final {
  explicit State(ListingViewPublisherConfig initial_config)
      : config(std::move(initial_config)) {
    publication_history.reserve(config.maximum_publication_transitions);
  }

  ListingViewPublisherConfig config;
  std::shared_ptr<const ListingStateView> accepted;
  std::optional<ListingViewCutInput> last_input;
  std::uint64_t configuration_epoch{config.initial_configuration_epoch};
  std::optional<std::uint64_t> effective_control_position{
      config.initial_effective_control_position};
  ViewPublicationState publication_state{ViewPublicationState::NotPublished};
  std::optional<contracts::PublicationAttemptId> active_attempt_id;
  std::uint64_t active_attempt_number{};
  std::vector<ViewPublicationTransition> publication_history;
};

ListingViewPublisher::ListingViewPublisher(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
ListingViewPublisher::ListingViewPublisher(ListingViewPublisher &&) noexcept =
    default;
ListingViewPublisher &
ListingViewPublisher::operator=(ListingViewPublisher &&) noexcept = default;
ListingViewPublisher::~ListingViewPublisher() = default;

std::optional<ListingViewPublisher>
ListingViewPublisher::create(ListingViewPublisherConfig config) {
  if (config.required_streams.empty() ||
      config.maximum_publication_transitions == 0 ||
      config.initial_configuration_epoch == 0 ||
      config.initial_effective_control_position ||
      config.initial_lineage.run_input_sequence() != 0 ||
      !complete_lineage(config, config.initial_lineage) ||
      std::any_of(config.initial_lineage.cursors().begin(),
                  config.initial_lineage.cursors().end(),
                  [](const auto &cursor) { return !cursor.is_origin(); })) {
    return std::nullopt;
  }
  auto streams = config.required_streams;
  std::sort(streams.begin(), streams.end());
  if (std::adjacent_find(streams.begin(), streams.end()) != streams.end())
    return std::nullopt;
  const std::array roles = {
      config.book_stream_id,
      config.trade_stream_id,
      config.trade_continuity_stream_id,
      config.reference_stream_id,
      config.market_control_stream_id,
      config.run_control_stream_id,
      config.run_timer_stream_id,
  };
  auto sorted_roles = roles;
  std::sort(sorted_roles.begin(), sorted_roles.end());
  if (std::adjacent_find(sorted_roles.begin(), sorted_roles.end()) !=
          sorted_roles.end() ||
      streams.size() != sorted_roles.size()) {
    return std::nullopt;
  }
  for (const auto role : roles) {
    if (!std::binary_search(streams.begin(), streams.end(), role))
      return std::nullopt;
  }
  return ListingViewPublisher(std::make_unique<State>(std::move(config)));
}

ListingViewResult
ListingViewPublisher::accept_cut(const ListingViewCutInput &input,
                                 const L2Book &book,
                                 const ListingAuxState &auxiliary) {
  if (input.lineage.run_id() != state_->config.run_id)
    return {.failure = ListingViewFailure::WrongRun};
  if (book.listing_id() != state_->config.listing_id ||
      auxiliary.listing_id() != state_->config.listing_id) {
    return {.failure = ListingViewFailure::WrongListing};
  }
  if (state_->accepted &&
      input.selection_id == state_->accepted->causing_selection_id) {
    if (state_->last_input && input == *state_->last_input)
      return {.view = state_->accepted};
    return {.failure = ListingViewFailure::ContradictorySelection};
  }
  if (state_->accepted && state_->publication_state !=
                              ViewPublicationState::FeatureConsumerAccepted) {
    return {.failure = ListingViewFailure::PriorViewPending};
  }
  if (!complete_lineage(state_->config, input.lineage)) {
    return {.failure = ListingViewFailure::IncompleteLineage};
  }
  const auto selected_stream =
      event_stream(state_->config, input.selected_event_type);
  const bool run_control =
      selected_stream &&
      *selected_stream == state_->config.run_control_stream_id;
  const bool evidence_valid =
      contracts::is_valid_event_type(input.selected_event_type) &&
      nonzero(input.input_semantic_checksum) &&
      nonzero(input.selection_semantic_checksum) &&
      input.merge_policy_version == state_->config.merge_policy_version &&
      ((!run_control &&
        input.configuration_epoch == state_->configuration_epoch &&
        input.effective_control_position ==
            state_->effective_control_position) ||
       (run_control &&
        state_->configuration_epoch !=
            std::numeric_limits<std::uint64_t>::max() &&
        input.configuration_epoch == state_->configuration_epoch + 1 &&
        input.effective_control_position ==
            input.lineage.run_input_sequence()));
  if (!evidence_valid) {
    return {.failure = ListingViewFailure::InvalidSelectionEvidence};
  }

  const auto quality_before = auxiliary.quality();
  const auto transition_before = book.transition_sequence();
  if (input.lineage.run_input_sequence() != quality_before.run_input_sequence)
    return {.failure = ListingViewFailure::RunInputMismatch};
  const auto &prior_lineage = state_->accepted ? state_->accepted->lineage
                                               : state_->config.initial_lineage;
  if (!valid_lineage_transition(state_->config, prior_lineage, input,
                                quality_before)) {
    return {.failure = ListingViewFailure::InvalidLineageTransition};
  }
  const auto *book_cursor =
      find_cursor(input.lineage, state_->config.book_stream_id);
  const auto *trade_cursor =
      find_cursor(input.lineage, state_->config.trade_stream_id);
  const auto *continuity_cursor =
      find_cursor(input.lineage, state_->config.trade_continuity_stream_id);
  if (!book_cursor || !trade_cursor || !continuity_cursor ||
      *book_cursor != quality_before.book_cursor ||
      *trade_cursor != quality_before.trade_cursor ||
      *continuity_cursor != quality_before.trade_continuity_cursor) {
    return {.failure = ListingViewFailure::CursorMismatch};
  }
  const auto &book_proof = quality_before.last_book_proof;
  if ((transition_before == 0 &&
       (book_proof || !quality_before.book_cursor.is_origin())) ||
      (transition_before != 0 &&
       (!book_proof ||
        book_proof->applied_through_cursor != quality_before.book_cursor ||
        book_proof->l2_transition_sequence != transition_before))) {
    return {.failure = ListingViewFailure::BookStateMismatch};
  }

  std::vector<L2Level> bids(book.bids().begin(), book.bids().end());
  std::vector<L2Level> asks(book.asks().begin(), book.asks().end());
  const auto top = book.top_of_book();
  std::vector<RecentTrade> trades(auxiliary.recent_trades().begin(),
                                  auxiliary.recent_trades().end());
  const auto quality_after = auxiliary.quality();
  if (quality_before != quality_after ||
      transition_before != book.transition_sequence()) {
    return {.failure = ListingViewFailure::SourceCutChanged};
  }

  const auto prior = state_->accepted ? std::optional(state_->accepted->view_id)
                                      : std::nullopt;
  const auto canonical =
      canonical_view_bytes(state_->config, input, transition_before, bids, asks,
                           top, trades, quality_before, prior);
  const auto checksum = contracts::sha256(canonical);
  const auto identity = view_id(checksum);
  if (!identity)
    return {.failure = ListingViewFailure::IdentityDerivationFailed};

  auto accepted = std::make_shared<const ListingStateView>(ListingStateView{
      .view_id = *identity,
      .run_id = state_->config.run_id,
      .listing_id = state_->config.listing_id,
      .causing_selection_id = input.selection_id,
      .causing_event_id = input.selected_event_id,
      .causing_event_type = input.selected_event_type,
      .causing_event_position = input.selected_event_position,
      .input_semantic_checksum = input.input_semantic_checksum,
      .selection_semantic_checksum = input.selection_semantic_checksum,
      .merge_policy_version = input.merge_policy_version,
      .configuration_epoch = input.configuration_epoch,
      .effective_control_position = input.effective_control_position,
      .lineage = input.lineage,
      .l2_transition_sequence = transition_before,
      .bids = std::move(bids),
      .asks = std::move(asks),
      .top = top,
      .recent_trades = std::move(trades),
      .quality = quality_before,
      .prior_view_id = prior,
      .view_schema_version = state_->config.view_schema_version,
      .capability_version = state_->config.capability_version,
      .transition_policy_version = state_->config.transition_policy_version,
      .arithmetic_version = state_->config.arithmetic_version,
      .canonicalization_version = state_->config.canonicalization_version,
      .semantic_checksum = checksum,
  });
  state_->accepted = accepted;
  state_->last_input = input;
  state_->configuration_epoch = input.configuration_epoch;
  state_->effective_control_position = input.effective_control_position;
  state_->publication_state = ViewPublicationState::NotPublished;
  state_->active_attempt_id.reset();
  state_->active_attempt_number = 0;
  state_->publication_history.clear();
  return {.view = std::move(accepted)};
}

ListingViewFailure ListingViewPublisher::transition_publication(
    const ViewPublicationTransition &transition) {
  if (!state_->accepted || transition.view_id != state_->accepted->view_id)
    return ListingViewFailure::WrongView;
  if (transition.boundary_id != state_->config.feature_boundary_id)
    return ListingViewFailure::WrongBoundary;
  if (!state_->publication_history.empty() &&
      state_->publication_history.back() == transition) {
    return ListingViewFailure::None;
  }
  if (state_->publication_history.size() ==
      state_->config.maximum_publication_transitions) {
    return ListingViewFailure::PublicationHistoryExhausted;
  }
  if (transition.from != state_->publication_state ||
      !valid_transition(transition.from, transition.to)) {
    return ListingViewFailure::InvalidPublicationTransition;
  }
  if (transition.to == ViewPublicationState::PublicationInProgress) {
    if (transition.attempt_number != state_->active_attempt_number + 1 ||
        (transition.from == ViewPublicationState::PublicationFailedRetryable &&
         state_->active_attempt_id &&
         transition.attempt_id == *state_->active_attempt_id)) {
      return ListingViewFailure::InvalidPublicationTransition;
    }
    state_->active_attempt_id = transition.attempt_id;
    state_->active_attempt_number = transition.attempt_number;
  } else if (!state_->active_attempt_id ||
             transition.attempt_id != *state_->active_attempt_id ||
             transition.attempt_number != state_->active_attempt_number) {
    return ListingViewFailure::InvalidPublicationTransition;
  }
  state_->publication_history.push_back(transition);
  state_->publication_state = transition.to;
  return ListingViewFailure::None;
}

std::shared_ptr<const ListingStateView>
ListingViewPublisher::accepted_view() const noexcept {
  return state_->accepted;
}

std::shared_ptr<const ListingStateView>
ListingViewPublisher::published_view() const noexcept {
  if (state_->publication_state ==
          ViewPublicationState::PublishedToFeatureBoundary ||
      state_->publication_state ==
          ViewPublicationState::FeatureConsumerAccepted) {
    return state_->accepted;
  }
  return {};
}

ViewPublicationState ListingViewPublisher::publication_state() const noexcept {
  return state_->publication_state;
}

std::span<const ViewPublicationTransition>
ListingViewPublisher::publication_history() const noexcept {
  return state_->publication_history;
}

std::size_t
ListingViewPublisher::publication_storage_capacity() const noexcept {
  return state_->publication_history.capacity();
}

} // namespace chronos::core::market_state
