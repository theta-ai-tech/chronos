#include "chronos/core/features/feature_runtime.hpp"
#include "chronos/core/portfolio/portfolio_construction.hpp"
#include "chronos/core/recommendation/recommendation.hpp"
#include "chronos/runtime/strategies/strategy_evaluation.hpp"
#include "chronos/runtime/strategies/strategy_runtime.hpp"
#include "chronos/strategies/generated/chronos_reference_strategies.hpp"
#include "chronos/strategies/sdk/strategy_host.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
namespace contracts = chronos::contracts;
namespace dispatch = chronos::core::dispatch;
namespace features = chronos::core::features;
namespace portfolio = chronos::core::portfolio;
namespace recommendation = chronos::core::recommendation;
namespace market = chronos::core::market_state;
namespace strategy_runtime = chronos::runtime::strategies;
namespace generated = chronos::strategies::generated;
namespace sdk = chronos::strategies::sdk;

static_assert(
    !std::is_default_constructible_v<sdk::AcceptedStrategyInvocation>);
static_assert(!std::is_aggregate_v<sdk::AcceptedStrategyInvocation>);

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

// These fixtures begin as authority outputs but become intentionally corrupted
// after TU-only mutation. Production construction remains private.
template <typename Tag, typename Tag::type Member> struct PrivateMemberAccess {
  friend typename Tag::type private_member(Tag) { return Member; }
};

struct CorruptedRecommendationIdMember {
  using type =
      contracts::TradeRecommendationId recommendation::TradeRecommendation::*;
  friend type private_member(CorruptedRecommendationIdMember);
};

template struct PrivateMemberAccess<
    CorruptedRecommendationIdMember,
    &recommendation::TradeRecommendation::recommendation_id_>;

struct CorruptedRecommendationSignalIdMember {
  using type =
      contracts::StrategySignalId recommendation::TradeRecommendation::*;
  friend type private_member(CorruptedRecommendationSignalIdMember);
};

template struct PrivateMemberAccess<
    CorruptedRecommendationSignalIdMember,
    &recommendation::TradeRecommendation::signal_id_>;

struct CorruptedRecommendationIssueTimeMember {
  using type = std::int64_t recommendation::TradeRecommendation::*;
  friend type private_member(CorruptedRecommendationIssueTimeMember);
};

template struct PrivateMemberAccess<
    CorruptedRecommendationIssueTimeMember,
    &recommendation::TradeRecommendation::issue_logical_time_nanoseconds_>;

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number = 1) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
}

contracts::DefinitionId parsed_definition(std::string_view value) {
  return contracts::DefinitionId::parse(value).value();
}

contracts::VersionRef named_version(std::string_view value) {
  return contracts::VersionRef::from(parsed_definition(value), 1).value();
}

recommendation::RecommendationPolicy recommendation_policy(
    contracts::AmountUnits minimum_actionable_strength,
    contracts::AmountUnits maximum_indicative_exposure = 750000) {
  return {
      .policy_version = named_version("0f520000-0000-0000-0000-000000000001"),
      .schema_version = named_version("0f520000-0000-0000-0000-000000000002"),
      .authority_version =
          named_version("0f520000-0000-0000-0000-000000000003"),
      .minimum_actionable_strength = minimum_actionable_strength,
      .maximum_indicative_exposure = maximum_indicative_exposure,
      .scale = *contracts::DecimalScale::from_exponent(6),
  };
}

contracts::StreamCursor origin(std::uint8_t seed) {
  return contracts::StreamCursor::at_origin(id<contracts::StreamId>(seed), 1)
      .value();
}

contracts::StreamCursor cursor(std::uint8_t seed, std::uint64_t sequence) {
  return contracts::StreamCursor::at_sequence(id<contracts::StreamId>(seed), 1,
                                              sequence)
      .value();
}

std::vector<contracts::StreamId> required_streams() {
  return {id<contracts::StreamId>(4),  id<contracts::StreamId>(7),
          id<contracts::StreamId>(8),  id<contracts::StreamId>(10),
          id<contracts::StreamId>(11), id<contracts::StreamId>(12),
          id<contracts::StreamId>(13)};
}

contracts::StateLineage
lineage(std::uint64_t run_sequence, contracts::StreamCursor book_cursor,
        contracts::StreamCursor timer_cursor = origin(13),
        bool controlled = false) {
  const auto required = required_streams();
  const std::array cursors = {
      origin(4),   origin(7),  book_cursor,
      origin(10),  origin(11), controlled ? cursor(12, 1) : origin(12),
      timer_cursor};
  return contracts::StateLineage::from(id<contracts::RunId>(30), run_sequence,
                                       required, cursors)
      .value();
}

struct TopSpec final {
  contracts::AmountUnits bid_price{100};
  contracts::AmountUnits bid_quantity{30};
  contracts::AmountUnits ask_price{102};
  contracts::AmountUnits ask_quantity{10};
  market::L2SideCompleteness bid_completeness{
      market::L2SideCompleteness::Complete};
  market::L2SideCompleteness ask_completeness{
      market::L2SideCompleteness::Complete};
};

const std::vector<std::byte> kInitialPayload{std::byte{1}};

market::L2Book make_book(TopSpec spec = {}) {
  auto book =
      market::L2Book::create({
                                 .listing_id = id<contracts::ListingId>(1),
                                 .price_definition = version(2),
                                 .quantity_definition = version(3),
                                 .maximum_levels_per_side = 4,
                                 .maximum_changes_per_delta = 4,
                             })
          .value();
  const auto applied = book.apply_snapshot({
      .listing_id = id<contracts::ListingId>(1),
      .input_evidence =
          market::L2InputEvidence{
              .event_id = id<contracts::EventId>(23),
              .semantic_checksum = contracts::sha256(kInitialPayload),
              .kind = market::L2InputKind::Snapshot,
          },
      .bids = {{.price =
                    contracts::Price::from_units(spec.bid_price, version(2))
                        .value(),
                .quantity = contracts::Quantity::from_units(spec.bid_quantity,
                                                            version(3))
                                .value()}},
      .asks = {{.price =
                    contracts::Price::from_units(spec.ask_price, version(2))
                        .value(),
                .quantity = contracts::Quantity::from_units(spec.ask_quantity,
                                                            version(3))
                                .value()}},
      .bid_completeness = spec.bid_completeness,
      .ask_completeness = spec.ask_completeness,
  });
  if (!applied.ok())
    std::abort();
  return book;
}

market::ListingAuxConfig aux_config() {
  return {
      .listing_id = id<contracts::ListingId>(1),
      .price_definition = version(2),
      .quantity_definition = version(3),
      .trade_stream_id = id<contracts::StreamId>(4),
      .trade_stream_epoch = 1,
      .trade_continuity_stream_id = id<contracts::StreamId>(7),
      .trade_continuity_stream_epoch = 1,
      .book_stream_id = id<contracts::StreamId>(8),
      .book_stream_epoch = 1,
      .initial_run_input_sequence = 0,
      .initial_logical_time_nanoseconds = 100,
      .book_freshness_deadline_nanoseconds = 10,
      .trade_freshness_deadline_nanoseconds = 10,
      .freshness_policy_version = version(20),
      .trade_window_policy_version = version(21),
      .accepted_source_clock_domain = id<contracts::ClockDomainId>(22),
      .accepted_source_clock_class = contracts::ClockClass::source_wall,
      .required_source_time_quality = market::SourceTimeQuality::Exact,
      .correction_policy = market::TradeCorrectionPolicy::Reject,
      .trade_window_policy = market::TradeWindowPolicy::AcceptedCount,
      .recent_trade_capacity = 2,
  };
}

market::ListingAuxState make_auxiliary(const market::L2Book &book) {
  auto auxiliary = market::ListingAuxState::create(aux_config()).value();
  const auto applied = auxiliary.apply_quality_input(
      {
          .event_id = id<contracts::EventId>(23),
          .input_semantic_checksum = contracts::sha256(kInitialPayload),
          .listing_id = id<contracts::ListingId>(1),
          .kind = market::ListingQualityInputKind::BookSynchronized,
          .run_input_sequence = 1,
          .logical_time_nanoseconds = 100,
          .book_proof =
              market::BookSynchronizationProof{
                  .snapshot_event_id = id<contracts::EventId>(23),
                  .snapshot_cursor = cursor(8, 0),
                  .applied_through_cursor = cursor(8, 0),
                  .l2_transition_sequence = book.transition_sequence(),
                  .bridge_complete = true,
                  .reference_compatible = true,
              },
      },
      &book);
  if (!applied.ok())
    std::abort();
  return auxiliary;
}

market::ListingViewPublisherConfig publisher_config() {
  return {
      .run_id = id<contracts::RunId>(30),
      .listing_id = id<contracts::ListingId>(1),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .reference_snapshot_version = version(43),
      .listing_definition_version = version(44),
      .reference_configuration_lineage_version = version(45),
      .required_streams = required_streams(),
      .initial_lineage = lineage(0, origin(8)),
      .book_stream_id = id<contracts::StreamId>(8),
      .trade_stream_id = id<contracts::StreamId>(4),
      .trade_continuity_stream_id = id<contracts::StreamId>(7),
      .reference_stream_id = id<contracts::StreamId>(10),
      .market_control_stream_id = id<contracts::StreamId>(11),
      .run_control_stream_id = id<contracts::StreamId>(12),
      .run_timer_stream_id = id<contracts::StreamId>(13),
      .feature_boundary_id = id<contracts::ConsumerBoundaryId>(31),
      .dispatcher_config =
          {
              .run_id = id<contracts::RunId>(30),
              .input_stream_id = id<contracts::StreamId>(14),
              .input_stream_epoch = 1,
              .control_stream_id = id<contracts::StreamId>(15),
              .control_stream_epoch = 1,
              .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(38),
              .merge_policy_version = version(37),
              .registry_snapshot_version = version(39),
              .initial_configuration_epoch = 1,
          },
      .merge_policy_version = version(37),
      .initial_configuration_epoch = 1,
      .view_schema_version = version(32),
      .capability_version = version(33),
      .transition_policy_version = version(34),
      .arithmetic_version = version(35),
      .canonicalization_version = version(36),
      .bundle_schema_version = version(40),
      .identity_policy_version = version(41),
      .maximum_publication_transitions = 12,
      .maximum_retained_views = 4,
  };
}

market::ListingViewCutInput make_cut_input(
    std::uint64_t run_sequence, contracts::EventId event_id,
    std::string event_type, contracts::EventPosition selected_position,
    std::vector<std::byte> payload, contracts::StateLineage cut_lineage,
    std::optional<dispatch::AcceptedControlOutcome> control = std::nullopt) {
  dispatch::RunInputCandidate candidate{
      .event_id = event_id,
      .event_type = event_type,
      .event_position = contracts::EventPosition::from(
                            id<contracts::StreamId>(14), 1, run_sequence)
                            .value(),
      .semantic_payload = std::move(payload),
  };
  candidate.semantic_checksum = contracts::sha256(candidate.semantic_payload);
  dispatch::RunInputSelectionRecord selection{
      .selection_id = id<contracts::RunInputSelectionId>(1),
      .run_id = id<contracts::RunId>(30),
      .run_input_sequence = run_sequence,
      .selected_event_id = event_id,
      .selected_event_type = event_type,
      .selected_event_position = candidate.event_position,
      .pre_selection_cursors = {run_sequence == 1
                                    ? origin(14)
                                    : cursor(14, run_sequence - 1)},
      .post_selection_cursors = {cursor(14, run_sequence)},
      .control_cursor =
          control ? control->accepted_control_cursor() : origin(15),
      .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(38),
      .active_configuration_epoch =
          control ? control->reservation().new_configuration_epoch : 1,
      .merge_policy_version = version(37),
      .registry_snapshot_version = version(39),
      .input_semantic_checksum = candidate.semantic_checksum,
  };
  if (control && run_sequence == control->reservation().effective_position)
    selection.applied_controls.push_back(control->reservation());
  selection.selection_semantic_checksum =
      dispatch::derive_run_input_selection_checksum(
          publisher_config().dispatcher_config, selection, candidate);
  selection.selection_id = dispatch::derive_run_input_selection_id(
      selection.selection_semantic_checksum);
  return {
      .selection_id = selection.selection_id,
      .dispatch_selection = selection,
      .dispatch_candidate = candidate,
      .selected_event_id = event_id,
      .selected_event_type = std::move(event_type),
      .selected_event_position = selected_position,
      .input_semantic_checksum = candidate.semantic_checksum,
      .selection_semantic_checksum = selection.selection_semantic_checksum,
      .merge_policy_version = version(37),
      .configuration_epoch = selection.active_configuration_epoch,
      .effective_control_position =
          control ? std::optional(control->reservation().effective_position)
                  : std::nullopt,
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .reference_snapshot_version = version(43),
      .listing_definition_version = version(44),
      .reference_configuration_lineage_version = version(45),
      .lineage = std::move(cut_lineage),
      .accepted_control_outcome = std::move(control),
  };
}

market::ListingViewCutInput initial_cut_input(
    std::optional<dispatch::AcceptedControlOutcome> control = std::nullopt) {
  return make_cut_input(
      1, id<contracts::EventId>(23), "market.book.observation.snapshot",
      contracts::EventPosition::from(id<contracts::StreamId>(8), 1, 0).value(),
      kInitialPayload,
      lineage(1, cursor(8, 0), origin(13), control.has_value()),
      std::move(control));
}

market::ViewPublicationTransition
publication(const market::ListingViewResult &result,
            market::ViewPublicationState from, market::ViewPublicationState to,
            std::uint64_t attempt_number = 1) {
  return {
      .view_id = result.view->view_id,
      .bundle_id = result.bundle->bundle_id,
      .attempt_id = id<contracts::PublicationAttemptId>(60),
      .boundary_id = id<contracts::ConsumerBoundaryId>(31),
      .attempt_number = attempt_number,
      .from = from,
      .to = to,
  };
}

void acknowledge(market::ListingViewPublisher &publisher,
                 const market::ListingViewResult &result) {
  for (const auto &transition : {
           publication(result, market::ViewPublicationState::NotPublished,
                       market::ViewPublicationState::PublicationInProgress),
           publication(
               result, market::ViewPublicationState::PublicationInProgress,
               market::ViewPublicationState::PublishedToFeatureBoundary),
           publication(result,
                       market::ViewPublicationState::PublishedToFeatureBoundary,
                       market::ViewPublicationState::FeatureConsumerAccepted),
       }) {
    if (publisher.transition_publication(transition) !=
        market::ListingViewFailure::None)
      std::abort();
  }
}

market::ListingViewPublisher publish_initial(TopSpec spec = {}) {
  auto book = make_book(spec);
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto accepted =
      publisher.accept_cut(initial_cut_input(), book, auxiliary);
  if (!accepted.ok())
    std::abort();
  acknowledge(publisher, accepted);
  return publisher;
}

features::FeatureRuntimeConfig runtime_config() {
  return {
      .run_id = id<contracts::RunId>(30),
      .listing_id = id<contracts::ListingId>(1),
      .imbalance_definition_version = version(50),
      .microprice_definition_version = version(51),
      .spread_definition_version = version(52),
      .implementation_version = version(53),
      .required_view_schema_version = version(32),
      .required_view_capability_version = version(33),
      .required_bundle_schema_version = version(40),
      .required_input_arithmetic_version = version(35),
      .required_input_canonicalization_version = version(36),
      .required_input_identity_policy_version = version(41),
      .feature_arithmetic_version = version(54),
      .canonicalization_version = version(55),
      .identity_policy_version = version(56),
  };
}

const features::FeatureEvaluation &
evaluation(const features::FeatureRuntimeResult &result,
           features::FeatureKind kind) {
  const auto found =
      std::find_if(result.evaluations().begin(), result.evaluations().end(),
                   [&](const auto &value) { return value.kind == kind; });
  if (found == result.evaluations().end())
    std::abort();
  return *found;
}

sdk::StrategyDefinition host_definition() {
  static const std::array dependencies = {
      sdk::FeatureDependency{
          .kind = sdk::StrategyFeatureKind::OrderBookImbalance,
          .definition_version = version(50),
      },
  };
  static const std::array parameter_schema = {
      sdk::StrategyParameterSchema{
          .parameter_id = id<contracts::DefinitionId>(94),
          .definition_version = version(95),
          .scale = *contracts::DecimalScale::from_exponent(6),
      },
  };
  static const std::array instructions = {
      sdk::StrategyInstruction{.opcode = sdk::StrategyOpcode::LoadFeature},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::RequireValidScaledRatio},
      sdk::StrategyInstruction{.opcode = sdk::StrategyOpcode::LoadParameter},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::RequirePositiveParameter},
      sdk::StrategyInstruction{
          .opcode =
              sdk::StrategyOpcode::CompareAbsoluteFeatureAtLeastParameter},
      sdk::StrategyInstruction{.opcode =
                                   sdk::StrategyOpcode::AppendFeatureFactor},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::AppendParameterFactor, .operand = 1},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::FinishDirectionalThreshold},
  };
  static const std::array factors = {
      sdk::ProgramFactorDefinition{
          .factor_id = id<contracts::DefinitionId>(96),
          .source = sdk::ExplanationSource::Feature,
      },
      sdk::ProgramFactorDefinition{
          .factor_id = id<contracts::DefinitionId>(97),
          .source = sdk::ExplanationSource::StrategyParameter,
      },
  };
  return {
      .descriptor =
          {
              .definition_version = version(90),
              .implementation_version = version(91),
              .required_features = dependencies,
              .parameter_schema = parameter_schema,
              .arithmetic_version = version(93),
              .explanation_policy_version = version(92),
              .resource_limits =
                  {
                      .maximum_operations = sdk::kMaximumEvaluationOperations,
                      .maximum_features = 1,
                      .maximum_parameters = 1,
                      .maximum_explanation_factors = factors.size(),
                      .maximum_working_bytes = sdk::kInterpreterWorkingBytes,
                  },
          },
      .program =
          {
              .instructions = instructions,
              .factors = factors,
              .signal_horizon_nanoseconds = 1'000'000,
          },
  };
}

std::span<const sdk::StrategyParameter> threshold_parameters() {
  static const std::array parameters = {
      sdk::StrategyParameter{
          .parameter_id = id<contracts::DefinitionId>(94),
          .definition_version = version(95),
          .units = 250000,
          .scale = *contracts::DecimalScale::from_exponent(6),
      },
  };
  return parameters;
}

sdk::AcceptedStrategyDefinition accepted_host_definition();

strategy_runtime::StrategyRuntimeConfig host_runtime_config_base(
    std::optional<sdk::StrategyParameter> parameter,
    std::optional<std::int64_t> deadline_offset = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  const auto policy = recommendation_policy(300000);
  return {
      .strategy_instance_id = id<contracts::StrategyInstanceId>(95),
      .listing_id = id<contracts::ListingId>(1),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .definition = accepted_host_definition(),
      .parameter = parameter,
      .recommendation_policy = policy,
      .run_control_stream_id = id<contracts::StreamId>(12),
      .run_control_stream_epoch = 1,
      .run_timer_stream_id = id<contracts::StreamId>(13),
      .run_timer_stream_epoch = 1,
      .maximum_operations = maximum_operations,
      .logical_deadline_offset_nanoseconds = deadline_offset,
  };
}

strategy_runtime::StrategyRuntimeConfig host_runtime_config(
    const features::AcceptedFeatureEvaluationCut &,
    std::optional<sdk::StrategyParameter> parameter,
    std::optional<std::int64_t> deadline_offset = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  return host_runtime_config_base(parameter, deadline_offset,
                                  maximum_operations);
}

class ActivationPersistence final
    : public dispatch::RunInputSelectionPersistence {
public:
  dispatch::RunInputRecoveryLoad load_recovery_state(
      const dispatch::RunInputDispatcherConfig &config) override {
    state = dispatch::RunInputRecoveryState{
        .input_cursor = origin(14),
        .control_cursor = origin(15),
        .configuration_epoch = config.initial_configuration_epoch,
    };
    return {.success = true, .state = state};
  }
  bool commit_control_reservation(
      const dispatch::ControlBoundaryReservation &reservation) override {
    state->control_cursor = cursor(15, reservation.control_sequence);
    state->pending_controls.push_back({.reservation = reservation});
    return true;
  }
  bool commit_control_visibility(
      const dispatch::ControlBoundaryReservation &reservation) override {
    state->pending_controls.front().visible =
        state->pending_controls.front().reservation == reservation;
    return state->pending_controls.front().visible;
  }
  bool commit_selection(const dispatch::RunInputSelectionRecord &selection,
                        const dispatch::RunInputCandidate &candidate) override {
    state->input_cursor = selection.post_selection_cursors.front();
    state->control_cursor = selection.control_cursor;
    state->run_input_sequence = selection.run_input_sequence;
    state->configuration_epoch = selection.active_configuration_epoch;
    state->pending_controls.clear();
    state->pending_publication = dispatch::RecoverablePublication{
        .selection = selection, .candidate = candidate};
    return true;
  }
  bool commit_publication_transition(
      const dispatch::PublicationTransition &transition) override {
    if (!state->pending_publication ||
        state->pending_publication->state != transition.from)
      return false;
    state->pending_publication->state = transition.to;
    state->pending_publication->attempt_number = transition.attempt_number;
    state->pending_publication->attempt_id = transition.attempt_id;
    return true;
  }
  std::optional<dispatch::RunInputRecoveryState> state;
};

class ActivationRegistry final : public dispatch::RunInputEligibilityRegistry {
public:
  bool is_run_input_eligible(
      std::string_view event_type,
      contracts::VersionRef registry_snapshot_version) const override {
    return event_type == "market.book.observation.snapshot" &&
           registry_snapshot_version == version(39);
  }
};

class ActivationConsumer final : public dispatch::RunInputConsumer {
public:
  contracts::ConsumerBoundaryId boundary_id() const noexcept override {
    return id<contracts::ConsumerBoundaryId>(38);
  }
  dispatch::ConsumerDisposition
  accept(const dispatch::RunInputSelectionRecord &,
         const dispatch::RunInputCandidate &,
         contracts::PublicationAttemptId) override {
    return dispatch::ConsumerDisposition::Accepted;
  }
};

dispatch::AcceptedControlOutcome accepted_activation_control(
    const strategy_runtime::StrategyRuntimeConfig &config,
    bool reserve_future_control = false) {
  ActivationPersistence persistence;
  ActivationRegistry registry;
  ActivationConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(publisher_config().dispatcher_config,
                                           persistence, registry)
          .value();
  auto payload = strategy_runtime::encode_strategy_activation_control(config);
  const dispatch::ControlBoundaryReservation reservation{
      .run_id = id<contracts::RunId>(30),
      .control_stream_id = id<contracts::StreamId>(15),
      .control_stream_epoch = 1,
      .control_outcome_id = id<contracts::EventId>(99),
      .control_sequence = 1,
      .effective_position = 1,
      .prior_configuration_epoch = 1,
      .new_configuration_epoch = 2,
      .behavior_payload = payload,
      .behavior_checksum = contracts::sha256(payload),
  };
  if (!dispatcher.reserve_control_boundary(reservation) ||
      !dispatcher.make_control_visible(reservation))
    std::abort();
  if (reserve_future_control) {
    const std::vector<std::byte> future_payload{std::byte{7}};
    const dispatch::ControlBoundaryReservation future{
        .run_id = id<contracts::RunId>(30),
        .control_stream_id = id<contracts::StreamId>(15),
        .control_stream_epoch = 1,
        .control_outcome_id = id<contracts::EventId>(100),
        .control_sequence = 2,
        .effective_position = 3,
        .prior_configuration_epoch = 2,
        .new_configuration_epoch = 3,
        .behavior_payload = future_payload,
        .behavior_checksum = contracts::sha256(future_payload),
    };
    if (!dispatcher.reserve_control_boundary(future))
      std::abort();
  }
  dispatch::RunInputCandidate candidate{
      .event_id = id<contracts::EventId>(23),
      .event_type = "market.book.observation.snapshot",
      .event_position =
          contracts::EventPosition::from(id<contracts::StreamId>(14), 1, 1)
              .value(),
      .semantic_payload = kInitialPayload,
      .semantic_checksum = contracts::sha256(kInitialPayload),
  };
  auto result = dispatcher.dispatch(std::move(candidate), consumer);
  if (!result.ok() || result.accepted_control_outcomes.size() != 1)
    std::abort();
  return result.accepted_control_outcomes.front();
}

std::optional<strategy_runtime::StrategyRuntime>
activate_runtime(strategy_runtime::StrategyRuntimeConfig config) {
  const auto control = accepted_activation_control(config);
  return strategy_runtime::StrategyRuntime::activate(std::move(config),
                                                     control);
}

sdk::AcceptedStrategyInvocation accepted_host_invocation(
    const features::AcceptedFeatureEvaluationCut &accepted,
    std::optional<std::int64_t> deadline = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  const auto runtime = activate_runtime(host_runtime_config(
      accepted, threshold_parameters()[0], deadline, maximum_operations));
  if (!runtime)
    std::abort();
  const auto invocation = runtime->admit(accepted);
  if (!invocation)
    std::abort();
  return *invocation;
}

sdk::AcceptedStrategyDefinition accepted_host_definition() {
  const auto accepted =
      sdk::AcceptedStrategyDefinition::accept(host_definition());
  if (!accepted)
    std::abort();
  return *accepted;
}

sdk::AcceptedStrategyDefinition accepted_reference_definition() {
  const auto accepted =
      generated::accepted_chronos_reference_strategies_definition();
  if (!accepted)
    std::abort();
  return *accepted;
}

features::FeatureRuntimeConfig reference_runtime_config() {
  auto config = runtime_config();
  config.imbalance_definition_version = accepted_reference_definition()
                                            .descriptor()
                                            .required_features[0]
                                            .definition_version;
  return config;
}

sdk::StrategyParameter
reference_threshold(contracts::AmountUnits units = 250000) {
  const auto schema =
      accepted_reference_definition().descriptor().parameter_schema[0];
  return {
      .parameter_id = schema.parameter_id,
      .definition_version = schema.definition_version,
      .units = units,
      .scale = schema.scale,
  };
}

strategy_runtime::StrategyRuntimeConfig reference_strategy_config(
    std::optional<sdk::StrategyParameter> parameter,
    std::optional<std::int64_t> deadline_offset = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  const auto policy = recommendation_policy(300000);
  return {
      .strategy_instance_id = id<contracts::StrategyInstanceId>(98),
      .listing_id = id<contracts::ListingId>(1),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .definition = accepted_reference_definition(),
      .parameter = parameter,
      .recommendation_policy = policy,
      .run_control_stream_id = id<contracts::StreamId>(12),
      .run_control_stream_epoch = 1,
      .run_timer_stream_id = id<contracts::StreamId>(13),
      .run_timer_stream_epoch = 1,
      .maximum_operations = maximum_operations,
      .logical_deadline_offset_nanoseconds = deadline_offset,
  };
}

features::FeatureRuntimeResult accepted_features_for_strategy(
    const strategy_runtime::StrategyRuntimeConfig &config, TopSpec spec = {},
    bool reserve_future_control = false,
    const features::FeatureRuntimeConfig *feature_config = nullptr) {
  auto book = make_book(spec);
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto control =
      accepted_activation_control(config, reserve_future_control);
  const auto accepted =
      publisher.accept_cut(initial_cut_input(control), book, auxiliary);
  if (!accepted.ok())
    std::abort();
  acknowledge(publisher, accepted);
  return features::FeatureRuntime(feature_config ? *feature_config
                                                 : runtime_config())
      .evaluate(*publisher.accepted_feature_cut());
}

struct ReferenceRun final {
  features::FeatureRuntimeResult features;
  sdk::StrategyHostResult result;
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
};

ReferenceRun run_reference(
    std::optional<sdk::StrategyParameter> parameter, TopSpec spec = {},
    std::optional<std::int64_t> deadline_offset = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations,
    std::size_t factor_capacity = 2) {
  auto config =
      reference_strategy_config(parameter, deadline_offset, maximum_operations);
  const auto feature_config = reference_runtime_config();
  ReferenceRun captured;
  captured.features =
      accepted_features_for_strategy(config, spec, false, &feature_config);
  const auto runtime = activate_runtime(config);
  const auto invocation = runtime->admit(*captured.features.accepted_cut());
  if (!invocation)
    std::abort();
  std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
  captured.result = sdk::StrategyHost::evaluate(
      config.definition, *invocation, workspace,
      std::span(captured.factors).first(factor_capacity));
  return captured;
}

struct ReferenceEvaluation final {
  features::FeatureRuntimeResult features;
  strategy_runtime::StrategyEvaluationResult result;
};

ReferenceEvaluation evaluate_reference(
    std::optional<sdk::StrategyParameter> parameter, TopSpec spec = {},
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  auto config =
      reference_strategy_config(parameter, std::nullopt, maximum_operations);
  const auto feature_config = reference_runtime_config();
  ReferenceEvaluation captured;
  captured.features =
      accepted_features_for_strategy(config, spec, false, &feature_config);
  const auto runtime = activate_runtime(config);
  const auto invocation = runtime->admit(*captured.features.accepted_cut());
  if (!invocation)
    std::abort();
  captured.result = strategy_runtime::StrategyEvaluationAuthority::evaluate(
      config.definition, *invocation);
  return captured;
}

recommendation::TradeRecommendation recommendation_for(TopSpec spec) {
  const auto evaluated = evaluate_reference(reference_threshold(), spec);
  if (!evaluated.result.completed())
    std::abort();
  const auto recommended = recommendation::RecommendationAuthority::recommend(
      *evaluated.result.evaluation, recommendation_policy(300000));
  if (!recommended.completed())
    std::abort();
  return *recommended.recommendation;
}

recommendation::TradeRecommendation host_recommendation_for(TopSpec spec) {
  const auto config = host_runtime_config_base(threshold_parameters()[0]);
  const auto evaluated_features = accepted_features_for_strategy(config, spec);
  const auto runtime = activate_runtime(config);
  if (!runtime)
    std::abort();
  const auto invocation = runtime->admit(*evaluated_features.accepted_cut());
  if (!invocation)
    std::abort();
  const auto evaluated =
      strategy_runtime::StrategyEvaluationAuthority::evaluate(config.definition,
                                                              *invocation);
  if (!evaluated.completed())
    std::abort();
  const auto recommended = recommendation::RecommendationAuthority::recommend(
      *evaluated.evaluation, config.recommendation_policy);
  if (!recommended.completed())
    std::abort();
  return *recommended.recommendation;
}

void corrupt_indicative_exposure(
    recommendation::TradeRecommendation &recommendation,
    contracts::AmountUnits units) {
  auto &outcome = const_cast<recommendation::RecommendationOutcome &>(
      recommendation.outcome());
  auto *actionable =
      std::get_if<recommendation::ActionableRecommendation>(&outcome);
  if (!actionable)
    std::abort();
  actionable->indicative_exposure_units = units;
}

void corrupt_recommendation_id(recommendation::TradeRecommendation &value,
                               contracts::TradeRecommendationId identity) {
  value.*private_member(CorruptedRecommendationIdMember{}) = identity;
}

void corrupt_signal_id(recommendation::TradeRecommendation &value,
                       contracts::StrategySignalId identity) {
  value.*private_member(CorruptedRecommendationSignalIdMember{}) = identity;
}

void corrupt_issue_logical_time(recommendation::TradeRecommendation &value,
                                std::int64_t logical_time_nanoseconds) {
  value.*private_member(CorruptedRecommendationIssueTimeMember{}) =
      logical_time_nanoseconds;
}

struct PortfolioSnapshotSpec final {
  contracts::PortfolioSnapshotId snapshot_id{
      id<contracts::PortfolioSnapshotId>(73)};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  contracts::PortfolioId portfolio_id{id<contracts::PortfolioId>(70)};
  contracts::AccountId account_id{id<contracts::AccountId>(71)};
  contracts::CanonicalInstrumentId canonical_instrument_id{
      id<contracts::CanonicalInstrumentId>(42)};
  contracts::ListingId listing_id{id<contracts::ListingId>(1)};
  contracts::DecimalScale exposure_scale{
      *contracts::DecimalScale::from_exponent(6)};
  std::uint64_t run_input_sequence{1};
  std::int64_t logical_time_nanoseconds{100};
  std::uint64_t configuration_epoch{2};
  portfolio::PortfolioSnapshotDisposition disposition{
      portfolio::PortfolioSnapshotDisposition::FreshComplete};
  bool paper_transition_assumption{true};
};

portfolio::TargetKey portfolio_target_key() {
  return portfolio::TargetKey(id<contracts::PortfolioId>(70),
                              id<contracts::AccountId>(71),
                              id<contracts::CanonicalInstrumentId>(42),
                              id<contracts::ListingId>(1), version(72));
}

portfolio::PortfolioStateSnapshot
portfolio_snapshot(contracts::AmountUnits current_exposure_units,
                   PortfolioSnapshotSpec spec = {}) {
  return portfolio::PortfolioStateSnapshot(
      spec.snapshot_id, spec.run_id, spec.portfolio_id, spec.account_id,
      spec.canonical_instrument_id, spec.listing_id, current_exposure_units,
      spec.exposure_scale, spec.run_input_sequence,
      spec.logical_time_nanoseconds, spec.configuration_epoch, spec.disposition,
      spec.paper_transition_assumption);
}

struct PortfolioPolicySpec final {
  contracts::VersionRef target_schema_version{version(74)};
  contracts::VersionRef sizing_policy_version{version(75)};
  contracts::VersionRef aggregation_policy_version{version(76)};
  contracts::VersionRef authority_version{version(77)};
  contracts::PortfolioId portfolio_id{id<contracts::PortfolioId>(70)};
  contracts::AccountId account_id{id<contracts::AccountId>(71)};
  contracts::CanonicalInstrumentId canonical_instrument_id{
      id<contracts::CanonicalInstrumentId>(42)};
  contracts::ListingId listing_id{id<contracts::ListingId>(1)};
  contracts::VersionRef target_policy_version{version(72)};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  std::vector<contracts::StrategyInstanceId> assigned_strategy_ids{
      id<contracts::StrategyInstanceId>(98)};
  contracts::DecimalScale exposure_scale{
      *contracts::DecimalScale::from_exponent(6)};
  std::size_t maximum_selected_recommendations{4};
  std::int64_t recommendation_maximum_logical_age_nanoseconds{100};
  std::int64_t target_validity_duration_nanoseconds{50};
};

portfolio::PortfolioConstructionPolicy
portfolio_policy(PortfolioPolicySpec spec);

portfolio::PortfolioConstructionPolicy
portfolio_policy(std::size_t maximum_selected_recommendations = 4) {
  PortfolioPolicySpec spec;
  spec.maximum_selected_recommendations = maximum_selected_recommendations;
  return portfolio_policy(std::move(spec));
}

portfolio::PortfolioConstructionPolicy
portfolio_policy(PortfolioPolicySpec spec) {
  return portfolio::PortfolioConstructionPolicy(
      spec.target_schema_version, spec.sizing_policy_version,
      spec.aggregation_policy_version, spec.authority_version,
      portfolio::TargetKey(spec.portfolio_id, spec.account_id,
                           spec.canonical_instrument_id, spec.listing_id,
                           spec.target_policy_version),
      spec.run_id, std::move(spec.assigned_strategy_ids), spec.exposure_scale,
      spec.maximum_selected_recommendations,
      spec.recommendation_maximum_logical_age_nanoseconds,
      spec.target_validity_duration_nanoseconds);
}

portfolio::PortfolioConstructionCut portfolio_cut() {
  return portfolio::PortfolioConstructionCut(1, 100);
}

market::ListingViewPublisher publish_quality_state(
    market::ListingQualityInputKind kind, std::string event_type,
    std::int64_t logical_time,
    const strategy_runtime::StrategyRuntimeConfig *strategy_config = nullptr,
    bool reserve_future_control = false) {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto control = strategy_config
                           ? std::optional(accepted_activation_control(
                                 *strategy_config, reserve_future_control))
                           : std::nullopt;
  const auto first =
      publisher.accept_cut(initial_cut_input(control), book, auxiliary);
  if (!first.ok())
    std::abort();
  acknowledge(publisher, first);

  const auto event_id = id<contracts::EventId>(62);
  const std::vector<std::byte> payload{std::byte{2}};
  market::ListingQualityInput quality{
      .event_id = event_id,
      .input_semantic_checksum = contracts::sha256(payload),
      .listing_id = id<contracts::ListingId>(1),
      .kind = kind,
      .run_input_sequence = 2,
      .logical_time_nanoseconds = logical_time,
  };
  contracts::EventPosition selected_position =
      contracts::EventPosition::from(id<contracts::StreamId>(13), 1, 0).value();
  auto cut_lineage =
      lineage(2, cursor(8, 0), cursor(13, 0), control.has_value());
  if (kind == market::ListingQualityInputKind::BookGapDetected) {
    quality.event_cursor = cursor(8, 1);
    selected_position =
        contracts::EventPosition::from(id<contracts::StreamId>(8), 1, 1)
            .value();
    cut_lineage = lineage(2, cursor(8, 1), origin(13), control.has_value());
  }
  if (!auxiliary.apply_quality_input(quality, &book).ok())
    std::abort();
  const auto second_input =
      make_cut_input(2, event_id, std::move(event_type), selected_position,
                     payload, std::move(cut_lineage), control);
  const auto second = publisher.accept_cut(second_input, book, auxiliary);
  if (!second.ok())
    std::abort();
  acknowledge(publisher, second);
  return publisher;
}

market::ListingViewPublisher publish_exhausted_top() {
  auto book = make_book(
      {.bid_completeness = market::L2SideCompleteness::BoundedWithProvenTop});
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first = publisher.accept_cut(initial_cut_input(), book, auxiliary);
  if (!first.ok())
    std::abort();
  acknowledge(publisher, first);

  const std::vector<std::byte> payload{std::byte{2}};
  const auto event_id = id<contracts::EventId>(42);
  if (!book
           .apply_delta({
               .listing_id = id<contracts::ListingId>(1),
               .input_evidence =
                   market::L2InputEvidence{
                       .event_id = event_id,
                       .semantic_checksum = contracts::sha256(payload),
                       .kind = market::L2InputKind::Delta,
                   },
               .bid_changes = {{
                   .price =
                       contracts::Price::from_units(100, version(2)).value(),
                   .quantity =
                       contracts::Quantity::from_units(0, version(3)).value(),
                   .operation = market::L2Operation::Delete,
               }},
           })
           .ok())
    std::abort();
  auto quality = market::ListingQualityInput{
      .event_id = event_id,
      .input_semantic_checksum = contracts::sha256(payload),
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::BookEvidenceObserved,
      .run_input_sequence = 2,
      .logical_time_nanoseconds = 101,
      .book_proof = *auxiliary.quality().last_book_proof,
  };
  quality.book_proof->applied_through_cursor = cursor(8, 1);
  quality.book_proof->l2_transition_sequence = book.transition_sequence();
  if (!auxiliary.apply_quality_input(quality, &book).ok())
    std::abort();
  const auto second_input = make_cut_input(
      2, event_id, "market.book.observation.delta",
      contracts::EventPosition::from(id<contracts::StreamId>(8), 1, 1).value(),
      payload, lineage(2, cursor(8, 1)));
  const auto second = publisher.accept_cut(second_input, book, auxiliary);
  if (!second.ok())
    std::abort();
  acknowledge(publisher, second);
  return publisher;
}

} // namespace

TEST_CASE("top features are deterministic and carry authority provenance") {
  auto publisher = publish_initial();
  const auto cut = publisher.accepted_feature_cut();
  CHECK(cut.has_value());
  const features::FeatureRuntime runtime(runtime_config());
  const auto first = runtime.evaluate(*cut);
  const auto second = runtime.evaluate(*cut);
  CHECK(first.ok());
  CHECK(first == second);
  CHECK(first.evaluations().size() == 3);
  CHECK(first.accepted_cut() != nullptr);

  const auto &imbalance =
      evaluation(first, features::FeatureKind::OrderBookImbalance);
  const auto ratio =
      std::get<features::ScaledRatio>(imbalance.observation->value);
  CHECK(ratio.units == 500000);
  CHECK(ratio.scale.exponent() == 6);
  CHECK(std::get<contracts::Price>(
            evaluation(first, features::FeatureKind::Microprice)
                .observation->value)
            .units() == 102);
  CHECK(std::get<contracts::Price>(
            evaluation(first, features::FeatureKind::Spread).observation->value)
            .units() == 2);

  for (const auto &item : first.evaluations()) {
    CHECK(item.disposition == features::FeatureDisposition::ValidObservation);
    CHECK(item.observation->provenance.bundle_id == cut->bundle().bundle_id);
    CHECK(item.observation->provenance.listing_view_id == cut->view().view_id);
    CHECK(item.observation->provenance.lineage == cut->view().lineage);
    CHECK(item.observation->provenance.input_view_semantic_checksum ==
          cut->view().semantic_checksum);
    CHECK(item.observation->provenance.input_bundle_semantic_checksum ==
          cut->bundle().semantic_checksum);
  }
}

TEST_CASE("microprice rounds complete half-unit values ties-to-even") {
  const features::FeatureRuntime runtime(runtime_config());
  auto odd_floor = publish_initial({.bid_price = 101,
                                    .bid_quantity = 1,
                                    .ask_price = 102,
                                    .ask_quantity = 1});
  auto even_floor = publish_initial({.bid_price = 100,
                                     .bid_quantity = 1,
                                     .ask_price = 101,
                                     .ask_quantity = 1});
  const auto odd_result = runtime.evaluate(*odd_floor.accepted_feature_cut());
  const auto even_result = runtime.evaluate(*even_floor.accepted_feature_cut());
  CHECK(std::get<contracts::Price>(
            evaluation(odd_result, features::FeatureKind::Microprice)
                .observation->value)
            .units() == 102);
  CHECK(std::get<contracts::Price>(
            evaluation(even_result, features::FeatureKind::Microprice)
                .observation->value)
            .units() == 100);
}

TEST_CASE("stale and gapped authority cuts produce typed unavailability") {
  const features::FeatureRuntime runtime(runtime_config());
  auto stale = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 111);
  auto gapped =
      publish_quality_state(market::ListingQualityInputKind::BookGapDetected,
                            "market.book.quality.gap_detected", 101);
  const auto stale_result = runtime.evaluate(*stale.accepted_feature_cut());
  const auto gapped_result = runtime.evaluate(*gapped.accepted_feature_cut());
  CHECK(stale_result.ok());
  CHECK(gapped_result.ok());
  for (const auto &item : stale_result.evaluations())
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::BookStale);
  for (const auto &item : gapped_result.evaluations())
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::BookGapped);
  CHECK(stale_result != gapped_result);
}

TEST_CASE("crossed and unproven authority cuts are explicit non-valid inputs") {
  const features::FeatureRuntime runtime(runtime_config());
  auto crossed = publish_initial({.bid_price = 102, .ask_price = 101});
  auto unproven = publish_exhausted_top();
  const auto crossed_result = runtime.evaluate(*crossed.accepted_feature_cut());
  const auto unproven_result =
      runtime.evaluate(*unproven.accepted_feature_cut());
  for (const auto &item : crossed_result.evaluations())
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::UnsupportedBookShape);
  for (const auto &item : unproven_result.evaluations())
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::TopNotProven);
}

TEST_CASE("locked books are valid and imbalance preserves its sign") {
  const features::FeatureRuntime runtime(runtime_config());
  auto publisher = publish_initial({.bid_price = 100,
                                    .bid_quantity = 10,
                                    .ask_price = 100,
                                    .ask_quantity = 30});
  const auto result = runtime.evaluate(*publisher.accepted_feature_cut());
  CHECK(result.ok());
  CHECK(std::get<features::ScaledRatio>(
            evaluation(result, features::FeatureKind::OrderBookImbalance)
                .observation->value)
            .units == -500000);
  CHECK(std::get<contracts::Price>(
            evaluation(result, features::FeatureKind::Microprice)
                .observation->value)
            .units() == 100);
  CHECK(
      std::get<contracts::Price>(
          evaluation(result, features::FeatureKind::Spread).observation->value)
          .units() == 0);
}

TEST_CASE("quantity arithmetic overflow never becomes a numeric zero") {
  const features::FeatureRuntime runtime(runtime_config());
  auto publisher =
      publish_initial({.bid_quantity = std::numeric_limits<std::int64_t>::max(),
                       .ask_quantity = 1});
  const auto result = runtime.evaluate(*publisher.accepted_feature_cut());
  CHECK(result.ok());
  CHECK(evaluation(result, features::FeatureKind::OrderBookImbalance)
            .unavailable->reason ==
        features::FeatureUnavailableReason::ArithmeticOverflow);
  CHECK(evaluation(result, features::FeatureKind::Microprice)
            .unavailable->reason ==
        features::FeatureUnavailableReason::ArithmeticOverflow);
  CHECK(evaluation(result, features::FeatureKind::Spread)
            .observation.has_value());
}

TEST_CASE("runtime compatibility policy fails closed on accepted cuts") {
  auto publisher = publish_initial();
  const auto cut = publisher.accepted_feature_cut();
  auto wrong_run = runtime_config();
  wrong_run.run_id = id<contracts::RunId>(70);
  CHECK(features::FeatureRuntime(wrong_run).evaluate(*cut).failure ==
        features::FeatureRuntimeFailure::WrongRun);
  auto wrong_listing = runtime_config();
  wrong_listing.listing_id = id<contracts::ListingId>(71);
  CHECK(features::FeatureRuntime(wrong_listing).evaluate(*cut).failure ==
        features::FeatureRuntimeFailure::WrongListing);
  auto wrong_schema = runtime_config();
  wrong_schema.required_view_schema_version = version(72);
  CHECK(features::FeatureRuntime(wrong_schema).evaluate(*cut).failure ==
        features::FeatureRuntimeFailure::IncompatibleSchema);
  auto wrong_arithmetic = runtime_config();
  wrong_arithmetic.required_input_arithmetic_version = version(73);
  CHECK(features::FeatureRuntime(wrong_arithmetic).evaluate(*cut).failure ==
        features::FeatureRuntimeFailure::IncompatibleArithmetic);
  auto wrong_identity = runtime_config();
  wrong_identity.required_input_identity_policy_version = version(74);
  CHECK(features::FeatureRuntime(wrong_identity).evaluate(*cut).failure ==
        features::FeatureRuntimeFailure::IncompatibleIdentityPolicy);
}

TEST_CASE("retained accepted handles prevent future-view look-ahead") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first = publisher.accept_cut(initial_cut_input(), book, auxiliary);
  acknowledge(publisher, first);
  const auto old_cut = *publisher.accepted_feature_cut();
  const features::FeatureRuntime runtime(runtime_config());
  const auto before = runtime.evaluate(old_cut);

  const std::vector<std::byte> payload{std::byte{2}};
  const auto event_id = id<contracts::EventId>(42);
  CHECK(book
            .apply_delta({
                .listing_id = id<contracts::ListingId>(1),
                .input_evidence =
                    market::L2InputEvidence{
                        .event_id = event_id,
                        .semantic_checksum = contracts::sha256(payload),
                        .kind = market::L2InputKind::Delta,
                    },
                .bid_changes = {{
                    .price =
                        contracts::Price::from_units(100, version(2)).value(),
                    .quantity =
                        contracts::Quantity::from_units(10, version(3)).value(),
                }},
            })
            .ok());
  auto quality = market::ListingQualityInput{
      .event_id = event_id,
      .input_semantic_checksum = contracts::sha256(payload),
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::BookEvidenceObserved,
      .run_input_sequence = 2,
      .logical_time_nanoseconds = 101,
      .book_proof = *auxiliary.quality().last_book_proof,
  };
  quality.book_proof->applied_through_cursor = cursor(8, 1);
  quality.book_proof->l2_transition_sequence = book.transition_sequence();
  CHECK(auxiliary.apply_quality_input(quality, &book).ok());
  const auto second_input = make_cut_input(
      2, event_id, "market.book.observation.delta",
      contracts::EventPosition::from(id<contracts::StreamId>(8), 1, 1).value(),
      payload, lineage(2, cursor(8, 1)));
  const auto second = publisher.accept_cut(second_input, book, auxiliary);
  CHECK(second.ok());
  acknowledge(publisher, second);
  const auto future = runtime.evaluate(*publisher.accepted_feature_cut());
  CHECK(future.ok());
  CHECK(future != before);
  CHECK(runtime.evaluate(old_cut) == before);

  auto changed_config = runtime_config();
  changed_config.imbalance_definition_version = version(50, 2);
  const auto changed =
      features::FeatureRuntime(changed_config).evaluate(old_cut);
  CHECK(evaluation(changed, features::FeatureKind::OrderBookImbalance)
            .evaluation_id !=
        evaluation(before, features::FeatureKind::OrderBookImbalance)
            .evaluation_id);
  CHECK(evaluation(changed, features::FeatureKind::Microprice) ==
        evaluation(before, features::FeatureKind::Microprice));
  CHECK(evaluation(changed, features::FeatureKind::Spread) ==
        evaluation(before, features::FeatureKind::Spread));
}

TEST_CASE("strategy host admits only authority-issued immutable feature cuts") {
  const auto config = host_runtime_config_base(threshold_parameters()[0]);
  const auto result = accepted_features_for_strategy(config);
  CHECK(result.ok());
  CHECK(result.accepted_cut() != nullptr);
  CHECK(result.accepted_cut()->accepted_control_outcome().has_value());
  const auto &control_provenance =
      evaluation(result, features::FeatureKind::OrderBookImbalance)
          .observation->provenance;
  CHECK(control_provenance.active_control_outcome_id ==
        id<contracts::EventId>(99));
  CHECK(control_provenance.active_control_selection_semantic_checksum ==
        result.accepted_cut()
            ->accepted_control_outcome()
            ->selection_semantic_checksum());

  const auto definition = accepted_host_definition();
  const auto invocation = accepted_host_invocation(*result.accepted_cut());
  auto uncontrolled_publisher = publish_initial();
  const auto uncontrolled =
      features::FeatureRuntime(runtime_config())
          .evaluate(*uncontrolled_publisher.accepted_feature_cut());
  const auto activated = activate_runtime(config);
  CHECK(!activated->admit(*uncontrolled.accepted_cut()));

  const auto future_control = accepted_activation_control(config, true);
  const auto future_features = accepted_features_for_strategy(config, {}, true);
  const auto future_runtime =
      strategy_runtime::StrategyRuntime::activate(config, future_control);
  CHECK(future_runtime.has_value());
  CHECK(future_runtime->admit(*future_features.accepted_cut()).has_value());
  auto future_later_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 150, &config, true);
  const auto future_later_features =
      features::FeatureRuntime(runtime_config())
          .evaluate(*future_later_publisher.accepted_feature_cut());
  CHECK(
      future_runtime->admit(*future_later_features.accepted_cut()).has_value());
  std::array<std::byte, sdk::kInterpreterWorkingBytes + 4> workspace;
  workspace.fill(std::byte{0x7f});
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
  const auto host_result =
      sdk::StrategyHost::evaluate(definition, invocation, workspace, factors);

  CHECK(host_result.completed());
  CHECK(host_result.charged_operations == sdk::kMaximumEvaluationOperations);
  CHECK(host_result.factor_count == 2);
  CHECK(std::holds_alternative<sdk::SignalDraft>(*host_result.terminal));
  CHECK(factors[0]->causal_feature_evaluation_id ==
        evaluation(result, features::FeatureKind::OrderBookImbalance)
            .evaluation_id);
  CHECK(!factors[1]->causal_feature_evaluation_id);
  CHECK(factors[1]->causal_parameter_id == id<contracts::DefinitionId>(94));
  CHECK(factors[1]->causal_parameter_definition_version == version(95));
  CHECK(factors[1]->causal_configuration_epoch == 2);
  CHECK(factors[1]->causal_control_outcome_id == id<contracts::EventId>(99));
  CHECK(factors[1]->causal_activation_checksum ==
        invocation.activation_checksum());
  CHECK(std::all_of(workspace.begin(),
                    workspace.begin() + sdk::kInterpreterWorkingBytes,
                    [](auto byte) { return byte == std::byte{}; }));
  CHECK(std::all_of(workspace.begin() + sdk::kInterpreterWorkingBytes,
                    workspace.end(),
                    [](auto byte) { return byte == std::byte{0x7f}; }));
}

TEST_CASE("strategy activation rejects unusable recommendation policies") {
  auto malformed = host_runtime_config_base(threshold_parameters()[0]);
  malformed.recommendation_policy.minimum_actionable_strength = 0;
  CHECK(!activate_runtime(malformed));

  auto incompatible = host_runtime_config_base(threshold_parameters()[0]);
  incompatible.recommendation_policy.minimum_actionable_strength = 30000;
  incompatible.recommendation_policy.maximum_indicative_exposure = 75000;
  incompatible.recommendation_policy.scale =
      *contracts::DecimalScale::from_exponent(5);
  CHECK(contracts::valid_recommendation_policy(
      incompatible.recommendation_policy));
  CHECK(!activate_runtime(incompatible));

  const auto accepted = host_runtime_config_base(threshold_parameters()[0]);
  const auto runtime = activate_runtime(accepted);
  CHECK(runtime.has_value());
  const auto features = accepted_features_for_strategy(accepted);
  const auto invocation = runtime->admit(*features.accepted_cut());
  CHECK(invocation.has_value());
  CHECK(invocation->recommendation_policy_version() ==
        accepted.recommendation_policy.policy_version);
  CHECK(invocation->recommendation_policy_checksum() ==
        contracts::recommendation_policy_checksum(
            accepted.recommendation_policy));
}

TEST_CASE("strategy host enforces fuel deadline workspace and output bounds") {
  const auto base_config = host_runtime_config_base(threshold_parameters()[0]);
  const auto result = accepted_features_for_strategy(base_config);
  const auto definition = accepted_host_definition();
  std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;

  const auto invocation = accepted_host_invocation(*result.accepted_cut());
  auto low_fuel_config =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0],
                          std::nullopt, sdk::kAdmissionOperations + 4);
  const auto low_fuel_features =
      accepted_features_for_strategy(low_fuel_config);
  const auto low_fuel_runtime = activate_runtime(low_fuel_config);
  const auto low_fuel_invocation =
      low_fuel_runtime->admit(*low_fuel_features.accepted_cut()).value();
  const auto no_fuel = sdk::StrategyHost::evaluate(
      definition, low_fuel_invocation, workspace, factors);
  CHECK(no_fuel.status ==
        sdk::StrategyExecutionStatus::DeterministicBudgetExhausted);
  CHECK(no_fuel.charged_operations == sdk::kAdmissionOperations + 4);
  CHECK(std::none_of(factors.begin(), factors.end(),
                     [](const auto &factor) { return factor.has_value(); }));

  auto wrong_timer =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  wrong_timer.run_timer_stream_id = id<contracts::StreamId>(99);
  const auto wrong_timer_runtime = activate_runtime(wrong_timer);
  CHECK(wrong_timer_runtime.has_value());
  CHECK(!wrong_timer_runtime->admit(*result.accepted_cut()));

  auto controlled_config =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  const auto accepted_control = accepted_activation_control(controlled_config);
  controlled_config.maximum_operations = sdk::kAdmissionOperations + 4;
  const auto wrong_control_runtime =
      strategy_runtime::StrategyRuntime::activate(controlled_config,
                                                  accepted_control);
  CHECK(!wrong_control_runtime.has_value());

  auto negative_deadline =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  negative_deadline.logical_deadline_offset_nanoseconds = -1;
  const auto negative_control = accepted_activation_control(negative_deadline);
  CHECK(!strategy_runtime::StrategyRuntime::activate(negative_deadline,
                                                     negative_control));

  auto scheduled_config = host_runtime_config(*result.accepted_cut(),
                                              threshold_parameters()[0], 10);
  const auto scheduled_features =
      accepted_features_for_strategy(scheduled_config);
  const auto scheduled_runtime = activate_runtime(scheduled_config);
  const auto scheduled_invocation =
      scheduled_runtime->admit(*scheduled_features.accepted_cut()).value();
  CHECK(scheduled_invocation.cut().logical_deadline_nanoseconds ==
        invocation.cut().logical_time_nanoseconds + 10);
  auto later_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 150, &scheduled_config);
  const auto later_features =
      features::FeatureRuntime(runtime_config())
          .evaluate(*later_publisher.accepted_feature_cut());
  const auto later_invocation =
      scheduled_runtime->admit(*later_features.accepted_cut());
  CHECK(later_invocation.has_value());
  CHECK(later_invocation->cut().logical_deadline_nanoseconds == 160);
  auto overflow_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", std::numeric_limits<std::int64_t>::max(),
      &scheduled_config);
  const auto overflow_features =
      features::FeatureRuntime(runtime_config())
          .evaluate(*overflow_publisher.accepted_feature_cut());
  CHECK(!scheduled_runtime->admit(*overflow_features.accepted_cut()));

  const auto short_workspace = sdk::StrategyHost::evaluate(
      definition, invocation,
      std::span<std::byte>(workspace).first(sdk::kInterpreterWorkingBytes - 1),
      factors);
  CHECK(short_workspace.status ==
        sdk::StrategyExecutionStatus::InsufficientWorkspace);
  CHECK(short_workspace.charged_operations == 0);

  const auto no_output = sdk::StrategyHost::evaluate(
      definition, invocation, workspace,
      std::span<std::optional<sdk::ExplanationFactor>>{});
  CHECK(no_output.status ==
        sdk::StrategyExecutionStatus::OutputCapacityExceeded);
  CHECK(no_output.charged_operations == 0);

  auto mutable_config =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  auto owning_runtime = activate_runtime(mutable_config);
  mutable_config.parameter->units = 900000;
  mutable_config.logical_deadline_offset_nanoseconds = 0;
  mutable_config.strategy_instance_id = id<contracts::StrategyInstanceId>(97);
  mutable_config.maximum_operations = 0;
  const auto owned_invocation = owning_runtime->admit(*result.accepted_cut());
  CHECK(owned_invocation.has_value());
  CHECK(owned_invocation->parameters()[0].units == 250000);
  CHECK(!owned_invocation->cut().logical_deadline_nanoseconds);
  CHECK(owned_invocation->strategy_instance_id() ==
        id<contracts::StrategyInstanceId>(95));
  CHECK(owned_invocation->maximum_operations() ==
        sdk::kMaximumEvaluationOperations);

  auto alternate_candidate = host_definition();
  alternate_candidate.program.signal_horizon_nanoseconds = 2'000'000;
  const auto alternate_definition =
      sdk::AcceptedStrategyDefinition::accept(alternate_candidate);
  CHECK(alternate_definition.has_value());
  const auto definition_mismatch = sdk::StrategyHost::evaluate(
      *alternate_definition, invocation, workspace, factors);
  CHECK(definition_mismatch.status ==
        sdk::StrategyExecutionStatus::ContractViolation);
  CHECK(definition_mismatch.charged_operations == 0);

  auto alternate_activation =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_activation.parameter->units = 500000;
  const auto alternate_runtime = activate_runtime(alternate_activation);
  CHECK(alternate_runtime.has_value());
  CHECK(alternate_runtime->activation_checksum() !=
        owning_runtime->activation_checksum());
  CHECK(!alternate_runtime->admit(*result.accepted_cut()));
  auto alternate_later = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 150, &alternate_activation);
  const auto alternate_later_features =
      features::FeatureRuntime(runtime_config())
          .evaluate(*alternate_later.accepted_feature_cut());
  CHECK(!owning_runtime->admit(*alternate_later_features.accepted_cut()));

  auto alternate_instance =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_instance.strategy_instance_id =
      id<contracts::StrategyInstanceId>(98);
  auto alternate_fuel =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_fuel.maximum_operations = sdk::kAdmissionOperations + 4;
  auto alternate_deadline =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_deadline.logical_deadline_offset_nanoseconds = 10;
  const auto instance_runtime = activate_runtime(alternate_instance);
  const auto fuel_runtime = activate_runtime(alternate_fuel);
  const auto deadline_runtime = activate_runtime(alternate_deadline);
  CHECK(instance_runtime.has_value());
  CHECK(fuel_runtime.has_value());
  CHECK(deadline_runtime.has_value());
  CHECK(instance_runtime->activation_checksum() !=
        owning_runtime->activation_checksum());
  CHECK(fuel_runtime->activation_checksum() !=
        owning_runtime->activation_checksum());
  CHECK(deadline_runtime->activation_checksum() !=
        owning_runtime->activation_checksum());

  std::array<std::optional<sdk::ExplanationFactor>, 4> oversized_factors;
  CHECK(sdk::StrategyHost::evaluate(definition, invocation, workspace,
                                    oversized_factors)
            .completed());
  oversized_factors[2] = oversized_factors[0];
  oversized_factors[3] = oversized_factors[1];
  auto one_fuel_config = host_runtime_config(
      *result.accepted_cut(), threshold_parameters()[0], std::nullopt, 1);
  const auto one_fuel_features =
      accepted_features_for_strategy(one_fuel_config);
  const auto one_fuel_runtime = activate_runtime(one_fuel_config);
  const auto one_fuel_invocation =
      one_fuel_runtime->admit(*one_fuel_features.accepted_cut()).value();
  CHECK(sdk::StrategyHost::evaluate(definition, one_fuel_invocation, workspace,
                                    oversized_factors)
            .status ==
        sdk::StrategyExecutionStatus::DeterministicBudgetExhausted);
  CHECK(!oversized_factors[0]);
  CHECK(!oversized_factors[1]);
  CHECK(oversized_factors[2]);
  CHECK(oversized_factors[3]);
}

TEST_CASE("strategy host emits typed abstentions without native callbacks") {
  auto config = host_runtime_config_base(std::nullopt);
  const auto result = accepted_features_for_strategy(config);
  const auto runtime = activate_runtime(config);
  const auto invocation = runtime->admit(*result.accepted_cut()).value();
  std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
  const auto missing = sdk::StrategyHost::evaluate(
      accepted_host_definition(), invocation, workspace, factors);
  CHECK(missing.completed());
  CHECK(missing.charged_operations == sdk::kAdmissionOperations + 3);
  CHECK(std::get<sdk::AbstentionDraft>(*missing.terminal).reason ==
        sdk::StrategyAbstentionReason::MissingParameter);
  CHECK(missing.factor_count == 1);
  CHECK(!factors[0]->causal_feature_evaluation_id);
  CHECK(factors[0]->causal_parameter_id == id<contracts::DefinitionId>(94));
  CHECK(factors[0]->causal_parameter_definition_version == version(95));
  CHECK(factors[0]->causal_configuration_epoch == 2);
  CHECK(factors[0]->causal_control_outcome_id == id<contracts::EventId>(99));
  CHECK(factors[0]->causal_activation_checksum ==
        invocation.activation_checksum());
}

TEST_CASE(
    "strategy host turns stale and gapped feature cuts into abstentions") {
  const features::FeatureRuntime runtime(runtime_config());
  const auto config = host_runtime_config_base(threshold_parameters()[0]);
  auto stale_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 111, &config);
  auto gapped_publisher =
      publish_quality_state(market::ListingQualityInputKind::BookGapDetected,
                            "market.book.quality.gap_detected", 101, &config);
  const std::array results = {
      runtime.evaluate(*stale_publisher.accepted_feature_cut()),
      runtime.evaluate(*gapped_publisher.accepted_feature_cut()),
  };
  const auto definition = accepted_host_definition();

  for (const auto &result : results) {
    CHECK(result.ok());
    const auto invocation = accepted_host_invocation(*result.accepted_cut());
    std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
    std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
    const auto host_result =
        sdk::StrategyHost::evaluate(definition, invocation, workspace, factors);
    CHECK(host_result.completed());
    CHECK(std::get<sdk::AbstentionDraft>(*host_result.terminal).reason ==
          sdk::StrategyAbstentionReason::NonValidFeature);
    CHECK(host_result.factor_count == 1);
    CHECK(factors[0]->causal_feature_evaluation_id ==
          evaluation(result, features::FeatureKind::OrderBookImbalance)
              .evaluation_id);
    CHECK(host_result.charged_operations == sdk::kAdmissionOperations + 2);
  }
}

TEST_CASE("strategy host preserves exact threshold and ratio boundaries") {
  const auto definition = accepted_host_definition();
  struct Case final {
    TopSpec top;
    sdk::StrategyDirection direction;
    contracts::AmountUnits expected_strength;
  };
  const std::array cases = {
      Case{.top = {.bid_quantity = 5, .ask_quantity = 3},
           .direction = sdk::StrategyDirection::Positive,
           .expected_strength = 250000},
      Case{.top = {.bid_quantity = 3, .ask_quantity = 5},
           .direction = sdk::StrategyDirection::Negative,
           .expected_strength = 250000},
      Case{.top = {.bid_quantity = 1'000'000'000'000, .ask_quantity = 1},
           .direction = sdk::StrategyDirection::Positive,
           .expected_strength = 1'000'000},
      Case{.top = {.bid_quantity = 1, .ask_quantity = 1'000'000'000'000},
           .direction = sdk::StrategyDirection::Negative,
           .expected_strength = 1'000'000},
  };

  for (const auto &test_case : cases) {
    const auto config = host_runtime_config_base(threshold_parameters()[0]);
    const auto result = accepted_features_for_strategy(config, test_case.top);
    CHECK(result.ok());
    const auto invocation = accepted_host_invocation(*result.accepted_cut());
    std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
    std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
    const auto host_result =
        sdk::StrategyHost::evaluate(definition, invocation, workspace, factors);
    CHECK(host_result.completed());
    const auto &signal = std::get<sdk::SignalDraft>(*host_result.terminal);
    CHECK(signal.direction == test_case.direction);
    CHECK(signal.strength.units == test_case.expected_strength);
    CHECK(signal.strength.scale.denominator() == 1'000'000);
  }
}

TEST_CASE("reference imbalance strategy is an accepted data-only definition") {
  const auto definition = accepted_reference_definition();
  const auto descriptor = definition.descriptor();
  const auto program = definition.program();
  CHECK(descriptor.definition_version ==
        named_version("0f510000-0000-0000-0000-000000000001"));
  CHECK(descriptor.implementation_version ==
        named_version("0f510000-0000-0000-0000-000000000002"));
  CHECK(descriptor.required_features[0].definition_version ==
        named_version("0f510000-0000-0000-0000-000000000003"));
  CHECK(descriptor.parameter_schema[0].parameter_id ==
        parsed_definition("0f510000-0000-0000-0000-000000000004"));
  CHECK(program.factors[0].factor_id ==
        parsed_definition("0f510000-0000-0000-0000-000000000006"));
  CHECK(program.factors[1].factor_id ==
        parsed_definition("0f510000-0000-0000-0000-000000000007"));
  CHECK(program.signal_horizon_nanoseconds == 1'000'000'000);
}

TEST_CASE("reference imbalance signals are exact signed and repeatable") {
  const auto positive = run_reference(reference_threshold(),
                                      {.bid_quantity = 5, .ask_quantity = 3});
  const auto repeated = run_reference(reference_threshold(),
                                      {.bid_quantity = 5, .ask_quantity = 3});
  const auto negative = run_reference(reference_threshold(),
                                      {.bid_quantity = 3, .ask_quantity = 5});

  CHECK(positive.result.completed());
  CHECK(positive.result.terminal == repeated.result.terminal);
  CHECK(positive.factors == repeated.factors);
  const auto &positive_signal =
      std::get<sdk::SignalDraft>(*positive.result.terminal);
  CHECK(positive_signal.direction == sdk::StrategyDirection::Positive);
  CHECK(positive_signal.strength.units == 250000);
  CHECK(positive.factors[0]->causal_feature_evaluation_id ==
        evaluation(positive.features, features::FeatureKind::OrderBookImbalance)
            .evaluation_id);
  const auto &negative_signal =
      std::get<sdk::SignalDraft>(*negative.result.terminal);
  CHECK(negative_signal.direction == sdk::StrategyDirection::Negative);
  CHECK(negative_signal.strength.units == 250000);
  CHECK(negative.factors[1]->signed_contribution_units == 0);
}

TEST_CASE("reference imbalance stays abstained below its threshold") {
  const auto result = run_reference(reference_threshold(),
                                    {.bid_quantity = 3, .ask_quantity = 2});
  CHECK(result.result.completed());
  CHECK(std::get<sdk::AbstentionDraft>(*result.result.terminal).reason ==
        sdk::StrategyAbstentionReason::NoDirectionalSignal);
  CHECK(result.result.factor_count == 2);
  CHECK(result.factors[0]->role == sdk::ExplanationRole::ExplainsAbstention);
  CHECK(result.factors[1]->signed_contribution_units == -50000);
}

TEST_CASE("reference imbalance abstains for stale and gapped authority cuts") {
  const features::FeatureRuntime runtime(reference_runtime_config());
  const auto config = reference_strategy_config(reference_threshold());
  auto stale_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 111, &config);
  auto gapped_publisher =
      publish_quality_state(market::ListingQualityInputKind::BookGapDetected,
                            "market.book.quality.gap_detected", 101, &config);
  const std::array feature_results = {
      runtime.evaluate(*stale_publisher.accepted_feature_cut()),
      runtime.evaluate(*gapped_publisher.accepted_feature_cut()),
  };
  const auto strategy = activate_runtime(config);
  for (const auto &feature_result : feature_results) {
    const auto invocation = strategy->admit(*feature_result.accepted_cut());
    CHECK(invocation.has_value());
    std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
    std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
    const auto result = sdk::StrategyHost::evaluate(
        config.definition, *invocation, workspace, factors);
    CHECK(result.completed());
    CHECK(std::get<sdk::AbstentionDraft>(*result.terminal).reason ==
          sdk::StrategyAbstentionReason::NonValidFeature);
    CHECK(factors[0]->causal_feature_evaluation_id ==
          evaluation(feature_result, features::FeatureKind::OrderBookImbalance)
              .evaluation_id);
  }
}

TEST_CASE("reference imbalance respects parameter fuel deadline and output") {
  const auto missing = run_reference(std::nullopt);
  CHECK(std::get<sdk::AbstentionDraft>(*missing.result.terminal).reason ==
        sdk::StrategyAbstentionReason::MissingParameter);
  const auto invalid = run_reference(reference_threshold(0));
  CHECK(std::get<sdk::AbstentionDraft>(*invalid.result.terminal).reason ==
        sdk::StrategyAbstentionReason::InvalidParameter);

  const auto bounded_deadline = run_reference(reference_threshold(), {}, 0);
  CHECK(bounded_deadline.result.completed());
  CHECK(bounded_deadline.result.charged_operations ==
        sdk::kMaximumEvaluationOperations);

  const auto low_fuel = run_reference(reference_threshold(), {}, std::nullopt,
                                      sdk::kAdmissionOperations + 4);
  CHECK(low_fuel.result.status ==
        sdk::StrategyExecutionStatus::DeterministicBudgetExhausted);
  CHECK(low_fuel.result.charged_operations == sdk::kAdmissionOperations + 4);

  const auto no_output = run_reference(reference_threshold(), {}, std::nullopt,
                                       sdk::kMaximumEvaluationOperations, 1);
  CHECK(no_output.result.status ==
        sdk::StrategyExecutionStatus::OutputCapacityExceeded);
  CHECK(!no_output.result.terminal);
}

TEST_CASE("strategy evaluation emits one deterministic nonzero signal") {
  const auto first = evaluate_reference(reference_threshold(),
                                        {.bid_quantity = 5, .ask_quantity = 3});
  const auto repeated = evaluate_reference(
      reference_threshold(), {.bid_quantity = 5, .ask_quantity = 3});

  CHECK(first.result.completed());
  CHECK(repeated.result.completed());
  CHECK(first.result.evaluation == repeated.result.evaluation);
  const auto &evaluation = *first.result.evaluation;
  CHECK(first.result.accepted_evaluation_key == evaluation.evaluation_key());
  CHECK(first.result.accepted_evaluation_id == evaluation.evaluation_id());
  CHECK(evaluation.signal_emitted());
  CHECK(!evaluation.abstained());
  const auto &signal =
      std::get<strategy_runtime::StrategySignal>(evaluation.terminal());
  CHECK(signal.evaluation_id() == evaluation.evaluation_id());
  CHECK(signal.draft().direction == sdk::StrategyDirection::Positive);
  CHECK(signal.draft().strength.units == 250000);
  CHECK(signal.draft().horizon_nanoseconds == 1'000'000'000);
  CHECK(evaluation.feature_evaluation_ids().size() == 3);
  CHECK(evaluation.factors().size() == 2);
  CHECK(evaluation.factors()[0].causal_feature_evaluation_id ==
        evaluation.feature_evaluation_ids()[0]);
}

TEST_CASE("strategy evaluation records abstention without a signal") {
  const auto result = evaluate_reference(
      reference_threshold(), {.bid_quantity = 1, .ask_quantity = 1});
  CHECK(result.result.completed());
  const auto &evaluation = *result.result.evaluation;
  CHECK(evaluation.abstained());
  CHECK(!evaluation.signal_emitted());
  CHECK(std::get<strategy_runtime::StrategyAbstention>(evaluation.terminal())
            .reason == sdk::StrategyAbstentionReason::NoDirectionalSignal);
}

TEST_CASE("strategy evaluation abstains on stale and gapped feature cuts") {
  const features::FeatureRuntime runtime(reference_runtime_config());
  const auto config = reference_strategy_config(reference_threshold());
  auto stale_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 111, &config);
  auto gapped_publisher =
      publish_quality_state(market::ListingQualityInputKind::BookGapDetected,
                            "market.book.quality.gap_detected", 101, &config);
  const std::array feature_results = {
      runtime.evaluate(*stale_publisher.accepted_feature_cut()),
      runtime.evaluate(*gapped_publisher.accepted_feature_cut()),
  };
  const auto strategy = activate_runtime(config);
  for (const auto &feature_result : feature_results) {
    const auto invocation = strategy->admit(*feature_result.accepted_cut());
    CHECK(invocation.has_value());
    const auto result = strategy_runtime::StrategyEvaluationAuthority::evaluate(
        config.definition, *invocation);
    CHECK(result.completed());
    CHECK(result.evaluation->abstained());
    CHECK(!result.evaluation->signal_emitted());
    CHECK(std::get<strategy_runtime::StrategyAbstention>(
              result.evaluation->terminal())
              .reason == sdk::StrategyAbstentionReason::NonValidFeature);
  }
}

TEST_CASE("strategy evaluation does not invent outcomes for host failures") {
  const auto interrupted = evaluate_reference(reference_threshold(), {},
                                              sdk::kAdmissionOperations + 4);
  const auto repeated = evaluate_reference(reference_threshold(), {},
                                           sdk::kAdmissionOperations + 4);
  CHECK(!interrupted.result.completed());
  CHECK(interrupted.result.failure ==
        strategy_runtime::StrategyEvaluationFailure::OperationalInterruption);
  CHECK(interrupted.result.execution_status ==
        sdk::StrategyExecutionStatus::DeterministicBudgetExhausted);
  CHECK(interrupted.result.accepted_evaluation_key.has_value());
  CHECK(interrupted.result.accepted_evaluation_id.has_value());
  CHECK(interrupted.result.accepted_evaluation_key ==
        repeated.result.accepted_evaluation_key);
  CHECK(interrupted.result.accepted_evaluation_id ==
        repeated.result.accepted_evaluation_id);
  CHECK(!interrupted.result.evaluation);

  auto config = reference_strategy_config(reference_threshold());
  const auto feature_config = reference_runtime_config();
  const auto features =
      accepted_features_for_strategy(config, {}, false, &feature_config);
  const auto runtime = activate_runtime(config);
  const auto invocation = runtime->admit(*features.accepted_cut());
  CHECK(invocation.has_value());
  const auto rejected = strategy_runtime::StrategyEvaluationAuthority::evaluate(
      accepted_host_definition(), *invocation);
  CHECK(rejected.failure ==
        strategy_runtime::StrategyEvaluationFailure::ContractViolation);
  CHECK(rejected.accepted_evaluation_key.has_value());
  CHECK(rejected.accepted_evaluation_id.has_value());
  CHECK(!rejected.evaluation);
}

TEST_CASE(
    "recommendation authority emits one deterministic actionable result") {
  const auto evaluated = evaluate_reference(
      reference_threshold(), {.bid_quantity = 3, .ask_quantity = 1});
  CHECK(evaluated.result.completed());
  const auto policy = recommendation_policy(300000);
  const auto first = recommendation::RecommendationAuthority::recommend(
      *evaluated.result.evaluation, policy);
  const auto repeated = recommendation::RecommendationAuthority::recommend(
      *evaluated.result.evaluation, policy);

  CHECK(first.completed());
  CHECK(repeated.completed());
  CHECK(first.recommendation == repeated.recommendation);
  const auto &value = *first.recommendation;
  const auto &signal = std::get<strategy_runtime::StrategySignal>(
      evaluated.result.evaluation->terminal());
  CHECK(value.signal_id() == signal.signal_id());
  CHECK(value.evaluation_id() == evaluated.result.evaluation->evaluation_id());
  CHECK(value.run_id() == evaluated.result.evaluation->run_id());
  CHECK(value.strategy_instance_id() ==
        evaluated.result.evaluation->strategy_instance_id());
  CHECK(value.listing_id() == evaluated.result.evaluation->listing_id());
  CHECK(value.canonical_instrument_id() ==
        evaluated.result.evaluation->canonical_instrument_id());
  CHECK(value.actionable());
  CHECK(!value.hold());
  CHECK(value.downstream_target_eligible());
  CHECK(std::get<recommendation::ActionableRecommendation>(value.outcome())
            .indicative_exposure_units == 500000);
  CHECK(value.direction() == sdk::StrategyDirection::Positive);
  CHECK(std::equal(value.factors().begin(), value.factors().end(),
                   evaluated.result.evaluation->factors().begin(),
                   evaluated.result.evaluation->factors().end()));
}

TEST_CASE("recommendation hold is explicit zero and stops before target") {
  const auto evaluated = evaluate_reference(
      reference_threshold(), {.bid_quantity = 5, .ask_quantity = 3});
  CHECK(evaluated.result.completed());
  const auto result = recommendation::RecommendationAuthority::recommend(
      *evaluated.result.evaluation, recommendation_policy(300000));

  CHECK(result.completed());
  const auto &value = *result.recommendation;
  CHECK(value.hold());
  CHECK(!value.actionable());
  CHECK(!value.downstream_target_eligible());
  const auto &hold =
      std::get<recommendation::HoldRecommendation>(value.outcome());
  CHECK(hold.reason ==
        recommendation::RecommendationHoldReason::BelowActionThreshold);
  CHECK(hold.indicative_exposure_units == 0);
  CHECK(std::equal(value.factors().begin(), value.factors().end(),
                   evaluated.result.evaluation->factors().begin(),
                   evaluated.result.evaluation->factors().end()));
}

TEST_CASE("recommendations reject abstentions and invalid policies") {
  const auto abstained = evaluate_reference(
      reference_threshold(), {.bid_quantity = 1, .ask_quantity = 1});
  CHECK(abstained.result.completed());
  const auto no_signal = recommendation::RecommendationAuthority::recommend(
      *abstained.result.evaluation, recommendation_policy(300000));
  CHECK(no_signal.failure ==
        recommendation::RecommendationFailure::AbstainedEvaluation);
  CHECK(!no_signal.recommendation);

  const auto signaled = evaluate_reference(
      reference_threshold(), {.bid_quantity = 3, .ask_quantity = 1});
  auto invalid = recommendation_policy(0);
  const auto rejected = recommendation::RecommendationAuthority::recommend(
      *signaled.result.evaluation, invalid);
  CHECK(rejected.failure ==
        recommendation::RecommendationFailure::InvalidPolicy);
  CHECK(!rejected.recommendation);

  invalid = recommendation_policy(800000, 750000);
  CHECK(recommendation::RecommendationAuthority::recommend(
            *signaled.result.evaluation, invalid)
            .failure == recommendation::RecommendationFailure::InvalidPolicy);

  const auto alternate = recommendation_policy(400000);
  CHECK(recommendation::RecommendationAuthority::recommend(
            *signaled.result.evaluation, alternate)
            .failure == recommendation::RecommendationFailure::InvalidPolicy);
}

TEST_CASE("M5 invariants preserve terminal and recommendation cardinality") {
  const auto policy = recommendation_policy(300000);
  auto acceptance =
      recommendation::RecommendationAcceptanceAuthority::create(64).value();
  const auto expected_strategy =
      reference_strategy_config(reference_threshold());
  std::size_t signal_count{};
  std::size_t abstention_count{};
  std::size_t actionable_count{};
  std::size_t hold_count{};
  std::vector<contracts::StrategySignalId> emitted_signal_ids;
  for (contracts::AmountUnits bid = 1; bid <= 8; ++bid) {
    for (contracts::AmountUnits ask = 1; ask <= 8; ++ask) {
      const auto evaluated = evaluate_reference(
          reference_threshold(), {.bid_quantity = bid, .ask_quantity = ask});
      CHECK(evaluated.result.completed());
      const auto &value = *evaluated.result.evaluation;
      CHECK(value.signal_emitted() != value.abstained());
      CHECK(value.run_id() == runtime_config().run_id);
      CHECK(value.strategy_instance_id() ==
            expected_strategy.strategy_instance_id);
      CHECK(value.listing_id() == expected_strategy.listing_id);
      CHECK(value.canonical_instrument_id() ==
            expected_strategy.canonical_instrument_id);
      CHECK(value.definition_digest() ==
            expected_strategy.definition.definition_digest());
      CHECK(value.feature_evaluation_ids().size() ==
            evaluated.features.evaluations().size());
      for (std::size_t index = 0;
           index < evaluated.features.evaluations().size(); ++index)
        CHECK(value.feature_evaluation_ids()[index] ==
              evaluated.features.evaluations()[index].evaluation_id);

      const auto recommended =
          recommendation::RecommendationAuthority::recommend(value, policy);
      if (value.signal_emitted()) {
        ++signal_count;
        const auto &signal =
            std::get<strategy_runtime::StrategySignal>(value.terminal());
        emitted_signal_ids.push_back(signal.signal_id());
        CHECK(signal.evaluation_id() == value.evaluation_id());
        CHECK(signal.draft().strength.units > 0);
        CHECK(signal.draft().horizon_nanoseconds > 0);
        CHECK(value.factors().size() == 2);
        for (std::size_t index = 0; index < value.factors().size(); ++index)
          CHECK(value.factors()[index].rank ==
                static_cast<std::uint32_t>(index + 1));
        CHECK(value.factors()[0].causal_feature_evaluation_id ==
              evaluation(evaluated.features,
                         features::FeatureKind::OrderBookImbalance)
                  .evaluation_id);
        CHECK(value.factors()[1].causal_parameter_id ==
              reference_threshold().parameter_id);
        CHECK(value.factors()[1].causal_parameter_definition_version ==
              reference_threshold().definition_version);
        CHECK(value.factors()[0].causal_configuration_epoch.has_value());
        CHECK(value.factors()[0].causal_control_outcome_id.has_value());
        CHECK(value.factors()[0].causal_activation_checksum ==
              value.activation_checksum());

        CHECK(recommended.completed());
        CHECK(recommended.recommendation->signal_id() == signal.signal_id());
        CHECK(recommended.recommendation->evaluation_id() ==
              value.evaluation_id());
        CHECK(recommended.recommendation->horizon_nanoseconds() ==
              signal.draft().horizon_nanoseconds);
        CHECK(recommended.recommendation->issue_run_input_sequence() ==
              value.run_input_sequence());
        CHECK(recommended.recommendation->issue_logical_time_nanoseconds() ==
              value.logical_time_nanoseconds());
        CHECK(std::equal(recommended.recommendation->factors().begin(),
                         recommended.recommendation->factors().end(),
                         value.factors().begin(), value.factors().end()));
        CHECK(recommended.recommendation->actionable() !=
              recommended.recommendation->hold());
        if (recommended.recommendation->actionable()) {
          ++actionable_count;
          CHECK(std::get<recommendation::ActionableRecommendation>(
                    recommended.recommendation->outcome())
                    .indicative_exposure_units > 0);
          CHECK(recommended.recommendation->downstream_target_eligible());
        } else {
          ++hold_count;
          const auto &hold = std::get<recommendation::HoldRecommendation>(
              recommended.recommendation->outcome());
          CHECK(hold.indicative_exposure_units == 0);
          CHECK(hold.reason ==
                recommendation::RecommendationHoldReason::BelowActionThreshold);
          CHECK(!recommended.recommendation->downstream_target_eligible());
        }
        const auto accepted = acceptance.accept(*recommended.recommendation);
        const auto retried = acceptance.accept(*recommended.recommendation);
        CHECK(accepted.accepted());
        CHECK(accepted.disposition ==
              recommendation::RecommendationAcceptanceDisposition::AcceptedNew);
        CHECK(retried.accepted());
        CHECK(retried.disposition ==
              recommendation::RecommendationAcceptanceDisposition::
                  DeduplicatedExisting);
        CHECK(accepted.recommendation->recommendation_id() ==
              retried.recommendation->recommendation_id());
        CHECK(acceptance.accepted_recommendations().size() == signal_count);
      } else {
        ++abstention_count;
        CHECK(recommended.failure ==
              recommendation::RecommendationFailure::AbstainedEvaluation);
        CHECK(!recommended.recommendation);
      }
    }
  }
  CHECK(signal_count > 0);
  CHECK(abstention_count > 0);
  CHECK(actionable_count > 0);
  CHECK(hold_count > 0);
  CHECK(acceptance.accepted_recommendations().size() == signal_count);
  CHECK(acceptance.finalize(emitted_signal_ids));
  CHECK(acceptance.cardinality_proven());
  CHECK(acceptance.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::None);
}

TEST_CASE(
    "recommendation acceptance deduplicates retries and bounds capacity") {
  const auto policy = recommendation_policy(300000);
  const auto first_evaluation = evaluate_reference(
      reference_threshold(), {.bid_quantity = 3, .ask_quantity = 1});
  const auto second_evaluation = evaluate_reference(
      reference_threshold(), {.bid_quantity = 4, .ask_quantity = 1});
  const auto first = recommendation::RecommendationAuthority::recommend(
      *first_evaluation.result.evaluation, policy);
  const auto second = recommendation::RecommendationAuthority::recommend(
      *second_evaluation.result.evaluation, policy);
  CHECK(first.completed());
  CHECK(second.completed());
  const std::array emitted_signals{first.recommendation->signal_id(),
                                   second.recommendation->signal_id()};
  const std::array first_signal{first.recommendation->signal_id()};

  CHECK(!recommendation::RecommendationAcceptanceAuthority::create(0));
  CHECK(!recommendation::RecommendationAcceptanceAuthority::create(
      recommendation::RecommendationAcceptanceAuthority::
          kMaximumAcceptedRecommendations +
      1));
  auto acceptance =
      recommendation::RecommendationAcceptanceAuthority::create(1).value();
  CHECK(!acceptance.cardinality_proven());
  const auto accepted = acceptance.accept(*first.recommendation);
  const auto retried = acceptance.accept(*first.recommendation);
  const auto exhausted = acceptance.accept(*second.recommendation);
  const auto after_exhaustion = acceptance.accept(*first.recommendation);
  CHECK(accepted.accepted());
  CHECK(retried.accepted());
  CHECK(retried.disposition ==
        recommendation::RecommendationAcceptanceDisposition::
            DeduplicatedExisting);
  CHECK(exhausted.failure ==
        recommendation::RecommendationAcceptanceFailure::CapacityExceeded);
  CHECK(exhausted.disposition ==
        recommendation::RecommendationAcceptanceDisposition::None);
  CHECK(!exhausted.recommendation);
  CHECK(!acceptance.finalize(emitted_signals));
  CHECK(!acceptance.cardinality_proven());
  CHECK(acceptance.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::CapacityExceeded);
  CHECK(!acceptance.finalize(emitted_signals));
  CHECK(acceptance.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::CapacityExceeded);
  CHECK(after_exhaustion.failure ==
        recommendation::RecommendationAcceptanceFailure::CapacityExceeded);
  CHECK(!after_exhaustion.recommendation);
  CHECK(acceptance.accepted_recommendations().size() == 1);

  auto omitted =
      recommendation::RecommendationAcceptanceAuthority::create(2).value();
  CHECK(omitted.accept(*first.recommendation).accepted());
  CHECK(!omitted.finalize(emitted_signals));
  CHECK(!omitted.cardinality_proven());
  CHECK(omitted.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::CardinalityMismatch);

  auto wrong_identity =
      recommendation::RecommendationAcceptanceAuthority::create(2).value();
  CHECK(wrong_identity.accept(*first.recommendation).accepted());
  const std::array wrong_emitted_identity{second.recommendation->signal_id()};
  CHECK(!wrong_identity.finalize(wrong_emitted_identity));
  CHECK(wrong_identity.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::CardinalityMismatch);

  auto complete =
      recommendation::RecommendationAcceptanceAuthority::create(2).value();
  CHECK(complete.accept(*first.recommendation).accepted());
  CHECK(complete.finalize(first_signal));
  CHECK(complete.cardinality_proven());
  const auto after_finalize = complete.accept(*second.recommendation);
  CHECK(after_finalize.failure ==
        recommendation::RecommendationAcceptanceFailure::AcceptanceFinalized);
  CHECK(complete.cardinality_proven());

  auto reordered =
      recommendation::RecommendationAcceptanceAuthority::create(2).value();
  CHECK(reordered.accept(*first.recommendation).accepted());
  CHECK(reordered.accept(*second.recommendation).accepted());
  CHECK(reordered.finalize(emitted_signals));
  const std::array reversed_signals{second.recommendation->signal_id(),
                                    first.recommendation->signal_id()};
  CHECK(reordered.finalize(reversed_signals));

  const std::array contradictory_signals{first.recommendation->signal_id(),
                                         id<contracts::StrategySignalId>(99)};
  CHECK(!reordered.finalize(contradictory_signals));
  CHECK(!reordered.cardinality_proven());
  CHECK(reordered.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::CardinalityMismatch);

  auto duplicate_repeat =
      recommendation::RecommendationAcceptanceAuthority::create(2).value();
  CHECK(duplicate_repeat.accept(*first.recommendation).accepted());
  CHECK(duplicate_repeat.accept(*second.recommendation).accepted());
  CHECK(duplicate_repeat.finalize(emitted_signals));
  const std::array duplicate_signals{first.recommendation->signal_id(),
                                     first.recommendation->signal_id()};
  CHECK(!duplicate_repeat.finalize(duplicate_signals));
  CHECK(!duplicate_repeat.cardinality_proven());
  CHECK(duplicate_repeat.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::CardinalityMismatch);

  auto conflicting =
      recommendation::RecommendationAcceptanceAuthority::create(2).value();
  CHECK(conflicting.accept(*first.recommendation).accepted());
  auto accepted_values = conflicting.accepted_recommendations();
  CHECK(!accepted_values.front().factors().empty());
  auto &stored_factor = const_cast<sdk::ExplanationFactor &>(
      accepted_values.front().factors().front());
  ++stored_factor.rank;
  const auto conflict = conflicting.accept(*first.recommendation);
  CHECK(conflict.failure == recommendation::RecommendationAcceptanceFailure::
                                ConflictingRecommendation);
  CHECK(!conflicting.finalize(first_signal));
  CHECK(!conflicting.cardinality_proven());
  CHECK(conflicting.terminal_failure() ==
        recommendation::RecommendationAcceptanceFailure::
            ConflictingRecommendation);
  const auto after_conflict = conflicting.accept(*second.recommendation);
  CHECK(after_conflict.failure ==
        recommendation::RecommendationAcceptanceFailure::
            ConflictingRecommendation);
}

TEST_CASE("M5 invalid and non-consumable features terminate at abstention") {
  auto config = reference_strategy_config(reference_threshold());
  auto incompatible_feature_config = reference_runtime_config();
  incompatible_feature_config.imbalance_definition_version = version(201);
  const auto incompatible = accepted_features_for_strategy(
      config, {}, false, &incompatible_feature_config);
  const auto runtime = activate_runtime(config);
  const auto invocation = runtime->admit(*incompatible.accepted_cut());
  CHECK(invocation.has_value());
  const auto evaluated =
      strategy_runtime::StrategyEvaluationAuthority::evaluate(config.definition,
                                                              *invocation);
  CHECK(evaluated.completed());
  CHECK(evaluated.evaluation->abstained());
  CHECK(std::get<strategy_runtime::StrategyAbstention>(
            evaluated.evaluation->terminal())
            .reason == sdk::StrategyAbstentionReason::IncompatibleFeature);
  const auto recommended = recommendation::RecommendationAuthority::recommend(
      *evaluated.evaluation, recommendation_policy(300000));
  CHECK(recommended.failure ==
        recommendation::RecommendationFailure::AbstainedEvaluation);
  CHECK(!recommended.recommendation);

  const features::FeatureRuntime feature_runtime(reference_runtime_config());
  auto stale_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 111, &config);
  auto gapped_publisher =
      publish_quality_state(market::ListingQualityInputKind::BookGapDetected,
                            "market.book.quality.gap_detected", 101, &config);
  const std::array non_consumable = {
      feature_runtime.evaluate(*stale_publisher.accepted_feature_cut()),
      feature_runtime.evaluate(*gapped_publisher.accepted_feature_cut()),
  };
  for (const auto &features : non_consumable) {
    const auto admitted = runtime->admit(*features.accepted_cut());
    CHECK(admitted.has_value());
    const auto result = strategy_runtime::StrategyEvaluationAuthority::evaluate(
        config.definition, *admitted);
    CHECK(result.completed());
    CHECK(result.evaluation->abstained());
    CHECK(!recommendation::RecommendationAuthority::recommend(
               *result.evaluation, recommendation_policy(300000))
               .recommendation);
  }
}

TEST_CASE("portfolio invalid policies fail before obligation admission") {
  std::vector<PortfolioPolicySpec> invalid_specs;

  auto zero_capacity = PortfolioPolicySpec{};
  zero_capacity.maximum_selected_recommendations = 0;
  invalid_specs.push_back(std::move(zero_capacity));

  auto excessive_capacity = PortfolioPolicySpec{};
  excessive_capacity.maximum_selected_recommendations =
      portfolio::PortfolioConstructionAuthority::kMaximumRecommendations + 1;
  invalid_specs.push_back(std::move(excessive_capacity));

  auto empty_assignments = PortfolioPolicySpec{};
  empty_assignments.assigned_strategy_ids.clear();
  invalid_specs.push_back(std::move(empty_assignments));

  auto duplicate_assignments = PortfolioPolicySpec{};
  duplicate_assignments.assigned_strategy_ids.push_back(
      duplicate_assignments.assigned_strategy_ids.front());
  invalid_specs.push_back(std::move(duplicate_assignments));

  auto excessive_assignments = PortfolioPolicySpec{};
  excessive_assignments.assigned_strategy_ids.clear();
  for (std::size_t index = 0;
       index <=
       portfolio::PortfolioConstructionAuthority::kMaximumAssignedStrategies;
       ++index) {
    excessive_assignments.assigned_strategy_ids.push_back(
        id<contracts::StrategyInstanceId>(
            static_cast<std::uint8_t>(index + 1)));
  }
  invalid_specs.push_back(std::move(excessive_assignments));

  auto negative_maximum_age = PortfolioPolicySpec{};
  negative_maximum_age.recommendation_maximum_logical_age_nanoseconds = -1;
  invalid_specs.push_back(std::move(negative_maximum_age));

  auto zero_validity = PortfolioPolicySpec{};
  zero_validity.target_validity_duration_nanoseconds = 0;
  invalid_specs.push_back(std::move(zero_validity));

  auto negative_validity = PortfolioPolicySpec{};
  negative_validity.target_validity_duration_nanoseconds = -1;
  invalid_specs.push_back(std::move(negative_validity));

  const std::span<const recommendation::TradeRecommendation> no_recommendations;
  for (auto &spec : invalid_specs) {
    const auto result = portfolio::PortfolioConstructionAuthority::construct(
        no_recommendations, portfolio_snapshot(0),
        portfolio_policy(std::move(spec)), portfolio_cut());
    CHECK(result.failure ==
          portfolio::PortfolioConstructionFailure::InvalidPolicy);
    CHECK(!result.terminal.has_value());
    CHECK(!result.completed());
  }
}

TEST_CASE("portfolio construction creates a positive absolute target") {
  const auto recommendation =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{recommendation};
  const auto snapshot = portfolio_snapshot(100000);
  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, snapshot, portfolio_policy(), portfolio_cut());

  CHECK(result.completed());
  CHECK(result.failure == portfolio::PortfolioConstructionFailure::None);
  CHECK(result.terminal.has_value());
  CHECK(std::holds_alternative<portfolio::TargetPosition>(*result.terminal));
  const auto &target = std::get<portfolio::TargetPosition>(*result.terminal);
  CHECK(target.key() == portfolio_target_key());
  CHECK(target.run_id() == id<contracts::RunId>(30));
  CHECK(target.desired_exposure_units() == 500000);
  CHECK(target.current_exposure_units() == 100000);
  CHECK(target.explanatory_delta_units() == 400000);
  CHECK(target.exposure_scale() == *contracts::DecimalScale::from_exponent(6));
  CHECK(target.snapshot() == snapshot);
  CHECK(target.target_schema_version() == version(74));
  CHECK(target.sizing_policy_version() == version(75));
  CHECK(target.aggregation_policy_version() == version(76));
  CHECK(target.authority_version() == version(77));
  CHECK(target.cut() == portfolio_cut());
  CHECK(target.valid_until_logical_time_nanoseconds() == 150);
  CHECK(target.source_recommendation_ids().size() == 1);
  CHECK(target.source_recommendation_ids()[0] ==
        recommendation.recommendation_id());
  CHECK(target.source_signal_ids().size() == 1);
  CHECK(target.source_signal_ids()[0] == recommendation.signal_id());
  CHECK(target.excluded_recommendation_ids().empty());
  CHECK(target.downstream_risk_eligible());
  CHECK(!target.executable());
}

TEST_CASE("portfolio construction creates a negative absolute target") {
  const auto recommendation =
      recommendation_for({.bid_quantity = 1, .ask_quantity = 3});
  const std::array selected{recommendation};
  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(100000), portfolio_policy(),
      portfolio_cut());

  CHECK(result.completed());
  CHECK(result.terminal.has_value());
  CHECK(std::holds_alternative<portfolio::TargetPosition>(*result.terminal));
  const auto &target = std::get<portfolio::TargetPosition>(*result.terminal);
  CHECK(target.desired_exposure_units() == -500000);
  CHECK(target.current_exposure_units() == 100000);
  CHECK(target.explanatory_delta_units() == -600000);
  CHECK(target.source_recommendation_ids()[0] ==
        recommendation.recommendation_id());
  CHECK(target.source_signal_ids()[0] == recommendation.signal_id());
  CHECK(target.downstream_risk_eligible());
  CHECK(!target.executable());
}

TEST_CASE("portfolio construction reports an already-held desired exposure") {
  const auto recommendation =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{recommendation};
  const auto snapshot = portfolio_snapshot(500000);
  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, snapshot, portfolio_policy(), portfolio_cut());

  CHECK(result.completed());
  CHECK(result.terminal.has_value());
  CHECK(std::holds_alternative<portfolio::PortfolioNoChange>(*result.terminal));
  const auto &no_change =
      std::get<portfolio::PortfolioNoChange>(*result.terminal);
  CHECK(no_change.reason() ==
        portfolio::PortfolioNoChangeReason::AlreadyAtDesiredExposure);
  CHECK(no_change.key() == portfolio_target_key());
  CHECK(no_change.run_id() == id<contracts::RunId>(30));
  CHECK(no_change.snapshot() == snapshot);
  CHECK(no_change.desired_exposure_units() == 500000);
  CHECK(no_change.current_exposure_units() == 500000);
  CHECK(no_change.source_recommendation_ids().size() == 1);
  CHECK(no_change.source_recommendation_ids()[0] ==
        recommendation.recommendation_id());
  CHECK(no_change.source_signal_ids()[0] == recommendation.signal_id());
  CHECK(no_change.cut() == portfolio_cut());
  CHECK(!no_change.downstream_risk_eligible());
  CHECK(!no_change.executable());
}

TEST_CASE("portfolio reinforcing recommendations sum absolute exposure") {
  auto first = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  auto second = recommendation_for({.bid_quantity = 4, .ask_quantity = 1});
  corrupt_indicative_exposure(first, 200000);
  corrupt_indicative_exposure(second, 300000);
  const std::array selected{first, second};

  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(100000), portfolio_policy(),
      portfolio_cut());
  const auto *target =
      result.terminal
          ? std::get_if<portfolio::TargetPosition>(&*result.terminal)
          : nullptr;
  CHECK(target != nullptr);
  if (!target)
    return;
  CHECK(target->desired_exposure_units() == 500000);
  CHECK(target->current_exposure_units() == 100000);
  CHECK(target->explanatory_delta_units() == 400000);
  CHECK(target->source_recommendation_ids().size() == 2);
}

TEST_CASE("portfolio opposing recommendations net absolute exposure") {
  auto positive = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  auto negative = recommendation_for({.bid_quantity = 1, .ask_quantity = 4});
  corrupt_indicative_exposure(positive, 700000);
  corrupt_indicative_exposure(negative, 200000);
  const std::array selected{positive, negative};

  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(-100000), portfolio_policy(),
      portfolio_cut());
  const auto *target =
      result.terminal
          ? std::get_if<portfolio::TargetPosition>(&*result.terminal)
          : nullptr;
  CHECK(target != nullptr);
  if (!target)
    return;
  CHECK(target->desired_exposure_units() == 500000);
  CHECK(target->current_exposure_units() == -100000);
  CHECK(target->explanatory_delta_units() == 600000);
}

TEST_CASE("portfolio complete cancellation preserves current exposure") {
  auto positive = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  auto negative = recommendation_for({.bid_quantity = 1, .ask_quantity = 4});
  corrupt_indicative_exposure(positive, 400000);
  corrupt_indicative_exposure(negative, 400000);
  const std::array selected{positive, negative};

  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(123456), portfolio_policy(),
      portfolio_cut());
  const auto *no_change =
      result.terminal
          ? std::get_if<portfolio::PortfolioNoChange>(&*result.terminal)
          : nullptr;
  CHECK(no_change != nullptr);
  if (!no_change)
    return;
  CHECK(no_change->reason() ==
        portfolio::PortfolioNoChangeReason::ContributionsCancelled);
  CHECK(no_change->desired_exposure_units() == 123456);
  CHECK(no_change->current_exposure_units() == 123456);
  CHECK(!no_change->downstream_risk_eligible());
}

TEST_CASE("portfolio empty and hold-only selections explicitly do not change") {
  const std::array<recommendation::TradeRecommendation, 0> empty{};
  const auto hold = recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  const std::array hold_only{hold};
  const auto snapshot = portfolio_snapshot(-321000);
  const auto policy = portfolio_policy();

  const auto empty_result =
      portfolio::PortfolioConstructionAuthority::construct(
          empty, snapshot, policy, portfolio_cut());
  const auto hold_result = portfolio::PortfolioConstructionAuthority::construct(
      hold_only, snapshot, policy, portfolio_cut());
  const auto *empty_no_change =
      empty_result.terminal
          ? std::get_if<portfolio::PortfolioNoChange>(&*empty_result.terminal)
          : nullptr;
  const auto *hold_no_change =
      hold_result.terminal
          ? std::get_if<portfolio::PortfolioNoChange>(&*hold_result.terminal)
          : nullptr;
  CHECK(empty_no_change != nullptr);
  CHECK(hold_no_change != nullptr);
  if (!empty_no_change || !hold_no_change)
    return;
  CHECK(empty_no_change->reason() ==
        portfolio::PortfolioNoChangeReason::NoActionableRecommendations);
  CHECK(hold_no_change->reason() == empty_no_change->reason());
  CHECK(empty_no_change->desired_exposure_units() == -321000);
  CHECK(hold_no_change->desired_exposure_units() == -321000);
  CHECK(empty_no_change->source_recommendation_ids().empty());
  CHECK(empty_no_change->excluded_recommendation_ids().empty());
  CHECK(hold_no_change->source_recommendation_ids().empty());
  CHECK(hold_no_change->excluded_recommendation_ids().size() == 1);
  CHECK(hold_no_change->excluded_recommendation_ids()[0] ==
        hold.recommendation_id());
}

TEST_CASE("portfolio reordered input preserves target values and identities") {
  auto positive = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  auto negative = recommendation_for({.bid_quantity = 1, .ask_quantity = 4});
  auto reinforcing = recommendation_for({.bid_quantity = 5, .ask_quantity = 1});
  corrupt_indicative_exposure(positive, 600000);
  corrupt_indicative_exposure(negative, 200000);
  corrupt_indicative_exposure(reinforcing, 100000);
  const std::array first_order{positive, negative, reinforcing};
  const std::array second_order{reinforcing, negative, positive};

  const auto first = portfolio::PortfolioConstructionAuthority::construct(
      first_order, portfolio_snapshot(100000), portfolio_policy(),
      portfolio_cut());
  const auto second = portfolio::PortfolioConstructionAuthority::construct(
      second_order, portfolio_snapshot(100000), portfolio_policy(),
      portfolio_cut());
  const auto *first_target =
      first.terminal ? std::get_if<portfolio::TargetPosition>(&*first.terminal)
                     : nullptr;
  const auto *second_target =
      second.terminal
          ? std::get_if<portfolio::TargetPosition>(&*second.terminal)
          : nullptr;
  CHECK(first_target != nullptr);
  CHECK(second_target != nullptr);
  if (!first_target || !second_target)
    return;
  CHECK(*first_target == *second_target);
  CHECK(first_target->target_position_id() ==
        second_target->target_position_id());
  CHECK(first_target->outcome_id() == second_target->outcome_id());
  CHECK(first_target->desired_exposure_units() == 500000);
  CHECK(std::is_sorted(first_target->source_recommendation_ids().begin(),
                       first_target->source_recommendation_ids().end()));
}

TEST_CASE("portfolio duplicate identities fail closed") {
  const auto first = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  auto same_signal = recommendation_for({.bid_quantity = 4, .ask_quantity = 1});
  corrupt_signal_id(same_signal, first.signal_id());
  const std::array duplicate_recommendation{first, first};
  const std::array duplicate_signal{first, same_signal};

  const auto recommendation_result =
      portfolio::PortfolioConstructionAuthority::construct(
          duplicate_recommendation, portfolio_snapshot(0), portfolio_policy(),
          portfolio_cut());
  const auto signal_result =
      portfolio::PortfolioConstructionAuthority::construct(
          duplicate_signal, portfolio_snapshot(0), portfolio_policy(),
          portfolio_cut());
  const auto *recommendation_rejected =
      recommendation_result.terminal
          ? std::get_if<portfolio::PortfolioConstructionRejected>(
                &*recommendation_result.terminal)
          : nullptr;
  const auto *signal_rejected =
      signal_result.terminal
          ? std::get_if<portfolio::PortfolioConstructionRejected>(
                &*signal_result.terminal)
          : nullptr;
  CHECK(recommendation_rejected != nullptr);
  CHECK(signal_rejected != nullptr);
  if (!recommendation_rejected || !signal_rejected)
    return;
  CHECK(recommendation_rejected->reason() ==
        portfolio::PortfolioConstructionRejectionReason::
            DuplicateRecommendationId);
  CHECK(signal_rejected->reason() ==
        portfolio::PortfolioConstructionRejectionReason::DuplicateSignalId);
  CHECK(!recommendation_rejected->downstream_risk_eligible());
  CHECK(!signal_rejected->downstream_risk_eligible());
}

TEST_CASE("portfolio recommendation compatibility matrix fails closed") {
  const auto recommendation =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{recommendation};
  const auto scale_five = *contracts::DecimalScale::from_exponent(5);

  const auto check_rejection =
      [&](const portfolio::PortfolioStateSnapshot &snapshot,
          const portfolio::PortfolioConstructionPolicy &policy,
          const portfolio::PortfolioConstructionCut &cut, auto expected) {
        const auto result =
            portfolio::PortfolioConstructionAuthority::construct(
                selected, snapshot, policy, cut);
        const auto *rejected =
            result.terminal
                ? std::get_if<portfolio::PortfolioConstructionRejected>(
                      &*result.terminal)
                : nullptr;
        CHECK(rejected != nullptr);
        if (rejected)
          CHECK(rejected->reason() == expected);
      };

  check_rejection(portfolio_snapshot(0, {.run_id = id<contracts::RunId>(31)}),
                  portfolio_policy({.run_id = id<contracts::RunId>(31)}),
                  portfolio_cut(),
                  portfolio::PortfolioConstructionRejectionReason::
                      RecommendationScopeMismatch);
  check_rejection(
      portfolio_snapshot(0, {.listing_id = id<contracts::ListingId>(2)}),
      portfolio_policy({.listing_id = id<contracts::ListingId>(2)}),
      portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::
          RecommendationScopeMismatch);
  check_rejection(
      portfolio_snapshot(0, {.canonical_instrument_id =
                                 id<contracts::CanonicalInstrumentId>(43)}),
      portfolio_policy({.canonical_instrument_id =
                            id<contracts::CanonicalInstrumentId>(43)}),
      portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::
          RecommendationScopeMismatch);
  check_rejection(
      portfolio_snapshot(0),
      portfolio_policy(
          {.assigned_strategy_ids = {id<contracts::StrategyInstanceId>(95)}}),
      portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::UnassignedStrategy);
  check_rejection(portfolio_snapshot(0, {.exposure_scale = scale_five}),
                  portfolio_policy({.exposure_scale = scale_five}),
                  portfolio_cut(),
                  portfolio::PortfolioConstructionRejectionReason::
                      IncompatibleExposureScale);
  check_rejection(portfolio_snapshot(0, {.run_input_sequence = 0}),
                  portfolio_policy(),
                  portfolio::PortfolioConstructionCut(0, 100),
                  portfolio::PortfolioConstructionRejectionReason::
                      FutureIssuedRecommendation);
  check_rejection(portfolio_snapshot(0, {.logical_time_nanoseconds = 99}),
                  portfolio_policy(),
                  portfolio::PortfolioConstructionCut(1, 99),
                  portfolio::PortfolioConstructionRejectionReason::
                      FutureIssuedRecommendation);
  check_rejection(
      portfolio_snapshot(0), portfolio_policy(),
      portfolio::PortfolioConstructionCut(1, 201),
      portfolio::PortfolioConstructionRejectionReason::ExpiredRecommendation);
}

TEST_CASE("portfolio every non-fresh snapshot disposition fails closed") {
  const auto recommendation =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{recommendation};
  struct SnapshotCase final {
    portfolio::PortfolioSnapshotDisposition disposition;
    portfolio::PortfolioConstructionRejectionReason expected;
  };
  const std::array cases = {
      SnapshotCase{
          portfolio::PortfolioSnapshotDisposition::Stale,
          portfolio::PortfolioConstructionRejectionReason::StaleSnapshot},
      SnapshotCase{
          portfolio::PortfolioSnapshotDisposition::Incomplete,
          portfolio::PortfolioConstructionRejectionReason::IncompleteSnapshot},
      SnapshotCase{
          portfolio::PortfolioSnapshotDisposition::Recovering,
          portfolio::PortfolioConstructionRejectionReason::InvalidSnapshot},
  };

  for (const auto &test_case : cases) {
    const auto result = portfolio::PortfolioConstructionAuthority::construct(
        selected, portfolio_snapshot(0, {.disposition = test_case.disposition}),
        portfolio_policy(), portfolio_cut());
    const auto *rejected =
        result.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                              &*result.terminal)
                        : nullptr;
    CHECK(rejected != nullptr);
    if (rejected)
      CHECK(rejected->reason() == test_case.expected);
  }
}

TEST_CASE(
    "portfolio invalid future and scope-mismatched snapshots fail closed") {
  const auto recommendation =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{recommendation};
  const std::array invalid_snapshots = {
      portfolio_snapshot(0, {.configuration_epoch = 0}),
      portfolio_snapshot(0, {.paper_transition_assumption = false}),
  };
  for (const auto &snapshot : invalid_snapshots) {
    const auto result = portfolio::PortfolioConstructionAuthority::construct(
        selected, snapshot, portfolio_policy(), portfolio_cut());
    const auto &rejected =
        std::get<portfolio::PortfolioConstructionRejected>(*result.terminal);
    CHECK(rejected.reason() ==
          portfolio::PortfolioConstructionRejectionReason::InvalidSnapshot);
  }

  const auto future_sequence =
      portfolio::PortfolioConstructionAuthority::construct(
          selected, portfolio_snapshot(0, {.run_input_sequence = 2}),
          portfolio_policy(), portfolio_cut());
  const auto future_time = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(0, {.logical_time_nanoseconds = 101}),
      portfolio_policy(), portfolio_cut());
  const auto wrong_account =
      portfolio::PortfolioConstructionAuthority::construct(
          selected,
          portfolio_snapshot(0, {.account_id = id<contracts::AccountId>(72)}),
          portfolio_policy(), portfolio_cut());
  CHECK(std::get<portfolio::PortfolioConstructionRejected>(
            *future_sequence.terminal)
            .reason() ==
        portfolio::PortfolioConstructionRejectionReason::FutureSnapshot);
  CHECK(
      std::get<portfolio::PortfolioConstructionRejected>(*future_time.terminal)
          .reason() ==
      portfolio::PortfolioConstructionRejectionReason::FutureSnapshot);
  CHECK(std::get<portfolio::PortfolioConstructionRejected>(
            *wrong_account.terminal)
            .reason() ==
        portfolio::PortfolioConstructionRejectionReason::SnapshotScopeMismatch);
}

TEST_CASE("portfolio arithmetic boundaries reject without wrapped values") {
  auto first = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  auto second = recommendation_for({.bid_quantity = 4, .ask_quantity = 1});
  corrupt_indicative_exposure(
      first, std::numeric_limits<contracts::AmountUnits>::max());
  corrupt_indicative_exposure(second, 1);
  const std::array aggregate_overflow{first, second};
  const auto aggregate = portfolio::PortfolioConstructionAuthority::construct(
      aggregate_overflow, portfolio_snapshot(0), portfolio_policy(),
      portfolio_cut());

  const std::array one{first};
  const auto delta = portfolio::PortfolioConstructionAuthority::construct(
      one,
      portfolio_snapshot(std::numeric_limits<contracts::AmountUnits>::min()),
      portfolio_policy(), portfolio_cut());

  auto minimum_issued = first;
  corrupt_issue_logical_time(minimum_issued,
                             std::numeric_limits<std::int64_t>::min());
  const std::array minimum_issued_selected{minimum_issued};
  const auto logical_age = portfolio::PortfolioConstructionAuthority::construct(
      minimum_issued_selected, portfolio_snapshot(0),
      portfolio_policy({
          .recommendation_maximum_logical_age_nanoseconds =
              std::numeric_limits<std::int64_t>::max(),
      }),
      portfolio::PortfolioConstructionCut(
          1, std::numeric_limits<std::int64_t>::max()));

  for (const auto *result : {&aggregate, &delta, &logical_age}) {
    const auto *rejected =
        result->terminal
            ? std::get_if<portfolio::PortfolioConstructionRejected>(
                  &*result->terminal)
            : nullptr;
    CHECK(rejected != nullptr);
    if (rejected)
      CHECK(
          rejected->reason() ==
          portfolio::PortfolioConstructionRejectionReason::ArithmeticOverflow);
  }
}

TEST_CASE("portfolio target-validity overflow follows only valid precursors") {
  auto corrupted = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  corrupt_indicative_exposure(
      corrupted, std::numeric_limits<contracts::AmountUnits>::max());
  constexpr auto cut_time = std::numeric_limits<std::int64_t>::max() - 10;
  constexpr std::int64_t logical_age = 1'000'000'000;
  constexpr std::int64_t validity_duration = 50;
  constexpr auto issue_time = cut_time - logical_age;
  corrupt_issue_logical_time(corrupted, issue_time);

  std::int64_t expires_at{};
  CHECK(corrupted.issue_run_input_sequence() == 1);
  CHECK(corrupted.issue_logical_time_nanoseconds() == issue_time);
  CHECK(corrupted.issue_logical_time_nanoseconds() <= cut_time);
  CHECK(corrupted.horizon_nanoseconds() == logical_age);
  CHECK(!__builtin_add_overflow(issue_time, corrupted.horizon_nanoseconds(),
                                &expires_at));
  CHECK(expires_at == cut_time);
  std::int64_t derived_age{};
  CHECK(!__builtin_sub_overflow(cut_time, issue_time, &derived_age));
  CHECK(derived_age == logical_age);

  const auto policy = portfolio_policy({
      .recommendation_maximum_logical_age_nanoseconds = logical_age,
      .target_validity_duration_nanoseconds = validity_duration,
  });
  CHECK(derived_age <= policy.recommendation_maximum_logical_age_nanoseconds());
  CHECK(cut_time <= expires_at);

  const auto desired =
      std::get<recommendation::ActionableRecommendation>(corrupted.outcome())
          .indicative_exposure_units;
  contracts::AmountUnits delta{};
  CHECK(!__builtin_sub_overflow(desired, contracts::AmountUnits{0}, &delta));
  CHECK(delta == std::numeric_limits<contracts::AmountUnits>::max());
  CHECK(delta != 0);
  std::int64_t invalid_valid_until{};
  CHECK(__builtin_add_overflow(cut_time, validity_duration,
                               &invalid_valid_until));

  const std::array selected{corrupted};
  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(0), policy,
      portfolio::PortfolioConstructionCut(1, cut_time));
  const auto *rejected =
      result.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                            &*result.terminal)
                      : nullptr;
  CHECK(rejected != nullptr);
  if (!rejected)
    return;
  CHECK(rejected->reason() ==
        portfolio::PortfolioConstructionRejectionReason::ArithmeticOverflow);
}

TEST_CASE(
    "portfolio mixed hold and actionable input constructs with exclusions") {
  const auto hold = recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  const auto actionable =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{hold, actionable};

  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(0), portfolio_policy(), portfolio_cut());
  const auto *target =
      result.terminal
          ? std::get_if<portfolio::TargetPosition>(&*result.terminal)
          : nullptr;
  CHECK(target != nullptr);
  if (!target)
    return;
  CHECK(target->source_recommendation_ids().size() == 1);
  CHECK(target->source_recommendation_ids()[0] ==
        actionable.recommendation_id());
  CHECK(target->excluded_recommendation_ids().size() == 1);
  CHECK(target->excluded_recommendation_ids()[0] == hold.recommendation_id());
}

TEST_CASE("portfolio identities bind every semantic input category") {
  auto actionable = recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  corrupt_indicative_exposure(actionable, 500000);
  const std::array selected{actionable};

  const auto target_identity =
      [&](const auto &recommendations,
          const portfolio::PortfolioStateSnapshot &snapshot,
          const portfolio::PortfolioConstructionPolicy &policy,
          const portfolio::PortfolioConstructionCut &cut) {
        const auto result =
            portfolio::PortfolioConstructionAuthority::construct(
                recommendations, snapshot, policy, cut);
        const auto *target =
            result.terminal
                ? std::get_if<portfolio::TargetPosition>(&*result.terminal)
                : nullptr;
        CHECK(target != nullptr);
        if (!target)
          throw std::logic_error("expected constructed target");
        return std::pair{target->target_position_id(), target->outcome_id()};
      };
  const auto baseline = target_identity(selected, portfolio_snapshot(0),
                                        portfolio_policy(), portfolio_cut());
  const auto check_target_change = [&](const auto &identity) {
    CHECK(identity.first != baseline.first);
    CHECK(identity.second != baseline.second);
  };

  check_target_change(
      target_identity(selected, portfolio_snapshot(0),
                      portfolio_policy({.target_schema_version = version(78)}),
                      portfolio_cut()));
  check_target_change(
      target_identity(selected, portfolio_snapshot(0),
                      portfolio_policy({.sizing_policy_version = version(79)}),
                      portfolio_cut()));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0),
      portfolio_policy({.aggregation_policy_version = version(80)}),
      portfolio_cut()));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0),
      portfolio_policy({.authority_version = version(81)}), portfolio_cut()));
  check_target_change(target_identity(
      selected,
      portfolio_snapshot(0, {.portfolio_id = id<contracts::PortfolioId>(72)}),
      portfolio_policy({.portfolio_id = id<contracts::PortfolioId>(72)}),
      portfolio_cut()));
  check_target_change(target_identity(
      selected,
      portfolio_snapshot(0, {.account_id = id<contracts::AccountId>(72)}),
      portfolio_policy({.account_id = id<contracts::AccountId>(72)}),
      portfolio_cut()));
  check_target_change(
      target_identity(selected, portfolio_snapshot(0),
                      portfolio_policy({.target_policy_version = version(82)}),
                      portfolio_cut()));
  check_target_change(target_identity(
      selected,
      portfolio_snapshot(
          0, {.snapshot_id = id<contracts::PortfolioSnapshotId>(83)}),
      portfolio_policy(), portfolio_cut()));
  check_target_change(target_identity(selected, portfolio_snapshot(1),
                                      portfolio_policy(), portfolio_cut()));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0, {.run_input_sequence = 0}),
      portfolio_policy(), portfolio_cut()));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0, {.logical_time_nanoseconds = 99}),
      portfolio_policy(), portfolio_cut()));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0, {.configuration_epoch = 3}),
      portfolio_policy(), portfolio_cut()));
  check_target_change(target_identity(selected, portfolio_snapshot(0),
                                      portfolio_policy(5), portfolio_cut()));
  check_target_change(
      target_identity(selected, portfolio_snapshot(0),
                      portfolio_policy({
                          .recommendation_maximum_logical_age_nanoseconds = 101,
                      }),
                      portfolio_cut()));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0),
      portfolio_policy({.target_validity_duration_nanoseconds = 51}),
      portfolio_cut()));
  check_target_change(
      target_identity(selected, portfolio_snapshot(0), portfolio_policy(),
                      portfolio::PortfolioConstructionCut(2, 100)));
  check_target_change(
      target_identity(selected, portfolio_snapshot(0), portfolio_policy(),
                      portfolio::PortfolioConstructionCut(1, 101)));
  check_target_change(target_identity(
      selected, portfolio_snapshot(0),
      portfolio_policy(
          {.assigned_strategy_ids = {id<contracts::StrategyInstanceId>(98),
                                     id<contracts::StrategyInstanceId>(95)}}),
      portfolio_cut()));

  auto different_source =
      recommendation_for({.bid_quantity = 4, .ask_quantity = 1});
  corrupt_indicative_exposure(different_source, 500000);
  const std::array different_selected{different_source};
  check_target_change(target_identity(different_selected, portfolio_snapshot(0),
                                      portfolio_policy(), portfolio_cut()));

  auto different_desired = actionable;
  corrupt_indicative_exposure(different_desired, 600000);
  const std::array different_desired_selected{different_desired};
  check_target_change(target_identity(different_desired_selected,
                                      portfolio_snapshot(0), portfolio_policy(),
                                      portfolio_cut()));

  const auto first_hold =
      recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  const auto second_hold =
      recommendation_for({.bid_quantity = 10, .ask_quantity = 6});
  const std::array first_excluded{actionable, first_hold};
  const std::array second_excluded{actionable, second_hold};
  CHECK(target_identity(first_excluded, portfolio_snapshot(0),
                        portfolio_policy(), portfolio_cut()) !=
        target_identity(second_excluded, portfolio_snapshot(0),
                        portfolio_policy(), portfolio_cut()));

  const auto ordered_assignments = portfolio_policy({
      .assigned_strategy_ids = {id<contracts::StrategyInstanceId>(95),
                                id<contracts::StrategyInstanceId>(98)},
  });
  const auto reversed_assignments = portfolio_policy({
      .assigned_strategy_ids = {id<contracts::StrategyInstanceId>(98),
                                id<contracts::StrategyInstanceId>(95)},
  });
  CHECK(target_identity(selected, portfolio_snapshot(0), ordered_assignments,
                        portfolio_cut()) ==
        target_identity(selected, portfolio_snapshot(0), reversed_assignments,
                        portfolio_cut()));
}

TEST_CASE("portfolio no-change identity binds every reachable semantic input") {
  const auto identity_for =
      [&](const auto &recommendations,
          const portfolio::PortfolioStateSnapshot &snapshot,
          const portfolio::PortfolioConstructionPolicy &policy,
          const portfolio::PortfolioConstructionCut &cut,
          portfolio::PortfolioNoChangeReason expected_reason) {
        const auto result =
            portfolio::PortfolioConstructionAuthority::construct(
                recommendations, snapshot, policy, cut);
        const auto *no_change =
            result.terminal
                ? std::get_if<portfolio::PortfolioNoChange>(&*result.terminal)
                : nullptr;
        CHECK(no_change != nullptr);
        if (!no_change)
          throw std::logic_error("expected no-change outcome");
        CHECK(no_change->reason() == expected_reason);
        return no_change->outcome_id();
      };
  constexpr auto no_action =
      portfolio::PortfolioNoChangeReason::NoActionableRecommendations;
  const std::array<recommendation::TradeRecommendation, 0> empty{};
  const auto baseline_snapshot = portfolio_snapshot(0);
  CHECK(baseline_snapshot.disposition() ==
        portfolio::PortfolioSnapshotDisposition::FreshComplete);
  CHECK(baseline_snapshot.paper_transition_assumption());
  const auto baseline = identity_for(
      empty, baseline_snapshot, portfolio_policy(), portfolio_cut(), no_action);
  const auto check_change = [&](auto identity) { CHECK(identity != baseline); };

  const auto positive =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const auto negative =
      recommendation_for({.bid_quantity = 1, .ask_quantity = 3});
  const std::array cancelled{positive, negative};
  check_change(identity_for(
      cancelled, portfolio_snapshot(0), portfolio_policy(), portfolio_cut(),
      portfolio::PortfolioNoChangeReason::ContributionsCancelled));
  const std::array already_desired{positive};
  const auto already_desired_identity = identity_for(
      already_desired, portfolio_snapshot(500000), portfolio_policy(),
      portfolio_cut(),
      portfolio::PortfolioNoChangeReason::AlreadyAtDesiredExposure);
  check_change(already_desired_identity);

  check_change(identity_for(
      empty, portfolio_snapshot(0, {.run_id = id<contracts::RunId>(31)}),
      portfolio_policy({.run_id = id<contracts::RunId>(31)}), portfolio_cut(),
      no_action));
  check_change(identity_for(
      empty,
      portfolio_snapshot(0, {.portfolio_id = id<contracts::PortfolioId>(72)}),
      portfolio_policy({.portfolio_id = id<contracts::PortfolioId>(72)}),
      portfolio_cut(), no_action));
  check_change(identity_for(
      empty,
      portfolio_snapshot(0, {.account_id = id<contracts::AccountId>(72)}),
      portfolio_policy({.account_id = id<contracts::AccountId>(72)}),
      portfolio_cut(), no_action));
  check_change(identity_for(
      empty,
      portfolio_snapshot(0, {.canonical_instrument_id =
                                 id<contracts::CanonicalInstrumentId>(43)}),
      portfolio_policy({.canonical_instrument_id =
                            id<contracts::CanonicalInstrumentId>(43)}),
      portfolio_cut(), no_action));
  check_change(identity_for(
      empty, portfolio_snapshot(0, {.listing_id = id<contracts::ListingId>(2)}),
      portfolio_policy({.listing_id = id<contracts::ListingId>(2)}),
      portfolio_cut(), no_action));
  check_change(
      identity_for(empty, portfolio_snapshot(0),
                   portfolio_policy({.target_policy_version = version(82)}),
                   portfolio_cut(), no_action));

  check_change(identity_for(empty, portfolio_snapshot(0), portfolio_policy(),
                            portfolio::PortfolioConstructionCut(2, 100),
                            no_action));
  check_change(identity_for(empty, portfolio_snapshot(0), portfolio_policy(),
                            portfolio::PortfolioConstructionCut(1, 101),
                            no_action));

  check_change(identity_for(
      empty,
      portfolio_snapshot(
          0, {.snapshot_id = id<contracts::PortfolioSnapshotId>(83)}),
      portfolio_policy(), portfolio_cut(), no_action));
  check_change(identity_for(empty, portfolio_snapshot(1), portfolio_policy(),
                            portfolio_cut(), no_action));
  const auto scale_five = *contracts::DecimalScale::from_exponent(5);
  check_change(
      identity_for(empty, portfolio_snapshot(0, {.exposure_scale = scale_five}),
                   portfolio_policy({.exposure_scale = scale_five}),
                   portfolio_cut(), no_action));
  check_change(identity_for(empty,
                            portfolio_snapshot(0, {.run_input_sequence = 0}),
                            portfolio_policy(), portfolio_cut(), no_action));
  check_change(identity_for(
      empty, portfolio_snapshot(0, {.logical_time_nanoseconds = 99}),
      portfolio_policy(), portfolio_cut(), no_action));
  check_change(identity_for(empty,
                            portfolio_snapshot(0, {.configuration_epoch = 3}),
                            portfolio_policy(), portfolio_cut(), no_action));

  check_change(
      identity_for(empty, portfolio_snapshot(0),
                   portfolio_policy({.target_schema_version = version(78)}),
                   portfolio_cut(), no_action));
  check_change(
      identity_for(empty, portfolio_snapshot(0),
                   portfolio_policy({.sizing_policy_version = version(79)}),
                   portfolio_cut(), no_action));
  check_change(identity_for(
      empty, portfolio_snapshot(0),
      portfolio_policy({.aggregation_policy_version = version(80)}),
      portfolio_cut(), no_action));
  check_change(
      identity_for(empty, portfolio_snapshot(0),
                   portfolio_policy({.authority_version = version(81)}),
                   portfolio_cut(), no_action));
  check_change(identity_for(empty, portfolio_snapshot(0), portfolio_policy(5),
                            portfolio_cut(), no_action));
  check_change(
      identity_for(empty, portfolio_snapshot(0),
                   portfolio_policy({
                       .recommendation_maximum_logical_age_nanoseconds = 101,
                   }),
                   portfolio_cut(), no_action));
  check_change(identity_for(
      empty, portfolio_snapshot(0),
      portfolio_policy({.target_validity_duration_nanoseconds = 51}),
      portfolio_cut(), no_action));

  const auto assignments = portfolio_policy({
      .assigned_strategy_ids = {id<contracts::StrategyInstanceId>(98),
                                id<contracts::StrategyInstanceId>(95)},
  });
  const auto reversed_assignments = portfolio_policy({
      .assigned_strategy_ids = {id<contracts::StrategyInstanceId>(95),
                                id<contracts::StrategyInstanceId>(98)},
  });
  check_change(identity_for(empty, portfolio_snapshot(0), assignments,
                            portfolio_cut(), no_action));
  CHECK(identity_for(empty, portfolio_snapshot(0), assignments, portfolio_cut(),
                     no_action) == identity_for(empty, portfolio_snapshot(0),
                                                reversed_assignments,
                                                portfolio_cut(), no_action));

  const auto hold = recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  const auto second_hold =
      recommendation_for({.bid_quantity = 10, .ask_quantity = 6});
  const std::array hold_only{hold};
  const std::array two_holds{hold, second_hold};
  const auto hold_identity =
      identity_for(hold_only, portfolio_snapshot(0), portfolio_policy(),
                   portfolio_cut(), no_action);
  check_change(hold_identity);
  CHECK(identity_for(two_holds, portfolio_snapshot(0), portfolio_policy(),
                     portfolio_cut(), no_action) != hold_identity);

  auto corrupted_hold_recommendation_id = hold;
  corrupt_recommendation_id(corrupted_hold_recommendation_id,
                            id<contracts::TradeRecommendationId>(201));
  CHECK(corrupted_hold_recommendation_id.recommendation_id() !=
        hold.recommendation_id());
  CHECK(corrupted_hold_recommendation_id.signal_id() == hold.signal_id());
  const std::array corrupted_hold_recommendation_ids{
      corrupted_hold_recommendation_id};
  CHECK(identity_for(corrupted_hold_recommendation_ids, portfolio_snapshot(0),
                     portfolio_policy(), portfolio_cut(),
                     no_action) != hold_identity);
  auto corrupted_hold_signal_id = hold;
  corrupt_signal_id(corrupted_hold_signal_id,
                    id<contracts::StrategySignalId>(202));
  CHECK(corrupted_hold_signal_id.recommendation_id() ==
        hold.recommendation_id());
  CHECK(corrupted_hold_signal_id.signal_id() != hold.signal_id());
  const std::array corrupted_hold_signal_ids{corrupted_hold_signal_id};
  CHECK(identity_for(corrupted_hold_signal_ids, portfolio_snapshot(0),
                     portfolio_policy(), portfolio_cut(),
                     no_action) != hold_identity);

  auto corrupted_source_recommendation_id = positive;
  corrupt_recommendation_id(corrupted_source_recommendation_id,
                            id<contracts::TradeRecommendationId>(203));
  CHECK(corrupted_source_recommendation_id.recommendation_id() !=
        positive.recommendation_id());
  CHECK(corrupted_source_recommendation_id.signal_id() == positive.signal_id());
  const std::array corrupted_source_recommendation_ids{
      corrupted_source_recommendation_id};
  CHECK(identity_for(
            corrupted_source_recommendation_ids, portfolio_snapshot(500000),
            portfolio_policy(), portfolio_cut(),
            portfolio::PortfolioNoChangeReason::AlreadyAtDesiredExposure) !=
        already_desired_identity);
  auto corrupted_source_signal_id = positive;
  corrupt_signal_id(corrupted_source_signal_id,
                    id<contracts::StrategySignalId>(204));
  CHECK(corrupted_source_signal_id.recommendation_id() ==
        positive.recommendation_id());
  CHECK(corrupted_source_signal_id.signal_id() != positive.signal_id());
  const std::array corrupted_source_signal_ids{corrupted_source_signal_id};
  CHECK(identity_for(
            corrupted_source_signal_ids, portfolio_snapshot(500000),
            portfolio_policy(), portfolio_cut(),
            portfolio::PortfolioNoChangeReason::AlreadyAtDesiredExposure) !=
        already_desired_identity);
}

TEST_CASE("portfolio rejection identity binds every semantic input") {
  struct RejectionIdentity final {
    contracts::PortfolioConstructionOutcomeId outcome_id;
    contracts::Sha256Digest full_input_digest;
    std::size_t omitted_count;
  };
  const auto identity_for =
      [&](const auto &recommendations,
          const portfolio::PortfolioStateSnapshot &snapshot,
          const portfolio::PortfolioConstructionPolicy &policy,
          const portfolio::PortfolioConstructionCut &cut,
          portfolio::PortfolioConstructionRejectionReason expected_reason) {
        const auto result =
            portfolio::PortfolioConstructionAuthority::construct(
                recommendations, snapshot, policy, cut);
        const auto *rejected =
            result.terminal
                ? std::get_if<portfolio::PortfolioConstructionRejected>(
                      &*result.terminal)
                : nullptr;
        CHECK(rejected != nullptr);
        if (!rejected)
          throw std::logic_error("expected construction rejection");
        CHECK(rejected->reason() == expected_reason);
        return RejectionIdentity{rejected->outcome_id(),
                                 rejected->full_input_evidence_digest(),
                                 rejected->omitted_evidence_count()};
      };

  const auto actionable =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array duplicates{actionable, actionable};
  constexpr auto duplicate_reason = portfolio::
      PortfolioConstructionRejectionReason::DuplicateRecommendationId;
  const auto baseline =
      identity_for(duplicates, portfolio_snapshot(0), portfolio_policy(),
                   portfolio_cut(), duplicate_reason);
  const auto check_change = [&](const RejectionIdentity &identity) {
    CHECK(identity.outcome_id != baseline.outcome_id);
  };

  check_change(identity_for(
      duplicates, portfolio_snapshot(0, {.run_id = id<contracts::RunId>(31)}),
      portfolio_policy({.run_id = id<contracts::RunId>(31)}), portfolio_cut(),
      duplicate_reason));
  check_change(identity_for(
      duplicates,
      portfolio_snapshot(0, {.portfolio_id = id<contracts::PortfolioId>(72)}),
      portfolio_policy({.portfolio_id = id<contracts::PortfolioId>(72)}),
      portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates,
      portfolio_snapshot(0, {.account_id = id<contracts::AccountId>(72)}),
      portfolio_policy({.account_id = id<contracts::AccountId>(72)}),
      portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates,
      portfolio_snapshot(0, {.canonical_instrument_id =
                                 id<contracts::CanonicalInstrumentId>(43)}),
      portfolio_policy({.canonical_instrument_id =
                            id<contracts::CanonicalInstrumentId>(43)}),
      portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates,
      portfolio_snapshot(0, {.listing_id = id<contracts::ListingId>(2)}),
      portfolio_policy({.listing_id = id<contracts::ListingId>(2)}),
      portfolio_cut(), duplicate_reason));
  check_change(
      identity_for(duplicates, portfolio_snapshot(0),
                   portfolio_policy({.target_policy_version = version(82)}),
                   portfolio_cut(), duplicate_reason));

  check_change(identity_for(
      duplicates, portfolio_snapshot(0), portfolio_policy(),
      portfolio::PortfolioConstructionCut(2, 100), duplicate_reason));
  check_change(identity_for(
      duplicates, portfolio_snapshot(0), portfolio_policy(),
      portfolio::PortfolioConstructionCut(1, 101), duplicate_reason));

  check_change(identity_for(
      duplicates,
      portfolio_snapshot(
          0, {.snapshot_id = id<contracts::PortfolioSnapshotId>(83)}),
      portfolio_policy(), portfolio_cut(), duplicate_reason));
  check_change(identity_for(duplicates, portfolio_snapshot(1),
                            portfolio_policy(), portfolio_cut(),
                            duplicate_reason));
  const auto scale_five = *contracts::DecimalScale::from_exponent(5);
  check_change(identity_for(
      duplicates, portfolio_snapshot(0, {.exposure_scale = scale_five}),
      portfolio_policy({.exposure_scale = scale_five}), portfolio_cut(),
      duplicate_reason));
  check_change(
      identity_for(duplicates, portfolio_snapshot(0, {.run_input_sequence = 0}),
                   portfolio_policy(), portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates, portfolio_snapshot(0, {.logical_time_nanoseconds = 99}),
      portfolio_policy(), portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates, portfolio_snapshot(0, {.configuration_epoch = 3}),
      portfolio_policy(), portfolio_cut(), duplicate_reason));

  check_change(identity_for(
      duplicates,
      portfolio_snapshot(
          0, {.disposition = portfolio::PortfolioSnapshotDisposition::Stale}),
      portfolio_policy(), portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::StaleSnapshot));
  check_change(identity_for(
      duplicates,
      portfolio_snapshot(
          0,
          {.disposition = portfolio::PortfolioSnapshotDisposition::Incomplete}),
      portfolio_policy(), portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::IncompleteSnapshot));
  check_change(identity_for(
      duplicates,
      portfolio_snapshot(
          0,
          {.disposition = portfolio::PortfolioSnapshotDisposition::Recovering}),
      portfolio_policy(), portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::InvalidSnapshot));
  check_change(identity_for(
      duplicates, portfolio_snapshot(0, {.paper_transition_assumption = false}),
      portfolio_policy(), portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::InvalidSnapshot));

  check_change(
      identity_for(duplicates, portfolio_snapshot(0),
                   portfolio_policy({.target_schema_version = version(78)}),
                   portfolio_cut(), duplicate_reason));
  check_change(
      identity_for(duplicates, portfolio_snapshot(0),
                   portfolio_policy({.sizing_policy_version = version(79)}),
                   portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates, portfolio_snapshot(0),
      portfolio_policy({.aggregation_policy_version = version(80)}),
      portfolio_cut(), duplicate_reason));
  check_change(
      identity_for(duplicates, portfolio_snapshot(0),
                   portfolio_policy({.authority_version = version(81)}),
                   portfolio_cut(), duplicate_reason));
  check_change(identity_for(duplicates, portfolio_snapshot(0),
                            portfolio_policy(3), portfolio_cut(),
                            duplicate_reason));
  check_change(
      identity_for(duplicates, portfolio_snapshot(0),
                   portfolio_policy({
                       .recommendation_maximum_logical_age_nanoseconds = 101,
                   }),
                   portfolio_cut(), duplicate_reason));
  check_change(identity_for(
      duplicates, portfolio_snapshot(0),
      portfolio_policy({.target_validity_duration_nanoseconds = 51}),
      portfolio_cut(), duplicate_reason));

  const auto assignments = portfolio_policy({
      .assigned_strategy_ids = {id<contracts::StrategyInstanceId>(98),
                                id<contracts::StrategyInstanceId>(95)},
  });
  const auto reversed_assignments = portfolio_policy({
      .assigned_strategy_ids = {id<contracts::StrategyInstanceId>(95),
                                id<contracts::StrategyInstanceId>(98)},
  });
  check_change(identity_for(duplicates, portfolio_snapshot(0), assignments,
                            portfolio_cut(), duplicate_reason));
  CHECK(identity_for(duplicates, portfolio_snapshot(0), assignments,
                     portfolio_cut(), duplicate_reason)
            .outcome_id == identity_for(duplicates, portfolio_snapshot(0),
                                        reversed_assignments, portfolio_cut(),
                                        duplicate_reason)
                               .outcome_id);

  const std::array triple_duplicates{actionable, actionable, actionable};
  const auto triple =
      identity_for(triple_duplicates, portfolio_snapshot(0), portfolio_policy(),
                   portfolio_cut(), duplicate_reason);
  check_change(triple);
  CHECK(triple.full_input_digest != baseline.full_input_digest);

  auto corrupted_recommendation_id = actionable;
  corrupt_recommendation_id(corrupted_recommendation_id,
                            id<contracts::TradeRecommendationId>(205));
  CHECK(corrupted_recommendation_id.recommendation_id() !=
        actionable.recommendation_id());
  CHECK(corrupted_recommendation_id.signal_id() == actionable.signal_id());
  const std::array corrupted_recommendation_ids{corrupted_recommendation_id,
                                                corrupted_recommendation_id};
  const auto changed_recommendation_id =
      identity_for(corrupted_recommendation_ids, portfolio_snapshot(0),
                   portfolio_policy(), portfolio_cut(), duplicate_reason);
  check_change(changed_recommendation_id);
  CHECK(changed_recommendation_id.full_input_digest !=
        baseline.full_input_digest);

  auto corrupted_signal_id = actionable;
  corrupt_signal_id(corrupted_signal_id, id<contracts::StrategySignalId>(206));
  CHECK(corrupted_signal_id.recommendation_id() ==
        actionable.recommendation_id());
  CHECK(corrupted_signal_id.signal_id() != actionable.signal_id());
  const std::array corrupted_signal_ids{corrupted_signal_id,
                                        corrupted_signal_id};
  const auto changed_signal_id =
      identity_for(corrupted_signal_ids, portfolio_snapshot(0),
                   portfolio_policy(), portfolio_cut(), duplicate_reason);
  check_change(changed_signal_id);
  CHECK(changed_signal_id.full_input_digest != baseline.full_input_digest);

  const auto hold = recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  const std::array duplicate_holds{hold, hold};
  const auto excluded =
      identity_for(duplicate_holds, portfolio_snapshot(0), portfolio_policy(),
                   portfolio_cut(), duplicate_reason);
  check_change(excluded);
  CHECK(excluded.full_input_digest != baseline.full_input_digest);

  const auto unassigned =
      host_recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array unassigned_selected{unassigned};
  check_change(identity_for(
      unassigned_selected, portfolio_snapshot(0), portfolio_policy(),
      portfolio_cut(),
      portfolio::PortfolioConstructionRejectionReason::UnassignedStrategy));

  std::vector<recommendation::TradeRecommendation> recommendations;
  recommendations.reserve(66);
  for (contracts::AmountUnits offset = 0; offset < 66; ++offset) {
    recommendations.push_back(
        recommendation_for({.bid_quantity = 3 + offset, .ask_quantity = 1}));
  }
  const std::vector first_capacity(recommendations.begin(),
                                   recommendations.begin() + 65);
  std::vector second_capacity(recommendations.begin(),
                              recommendations.begin() + 64);
  second_capacity.push_back(recommendations[65]);
  constexpr auto capacity_reason = portfolio::
      PortfolioConstructionRejectionReason::RecommendationCapacityExceeded;
  const auto first_capacity_identity =
      identity_for(first_capacity, portfolio_snapshot(0), portfolio_policy(64),
                   portfolio_cut(), capacity_reason);
  const auto second_capacity_identity =
      identity_for(second_capacity, portfolio_snapshot(0), portfolio_policy(64),
                   portfolio_cut(), capacity_reason);
  const auto larger_capacity_identity =
      identity_for(recommendations, portfolio_snapshot(0), portfolio_policy(64),
                   portfolio_cut(), capacity_reason);
  CHECK(first_capacity_identity.omitted_count == 1);
  CHECK(second_capacity_identity.omitted_count == 1);
  CHECK(larger_capacity_identity.omitted_count == 2);
  CHECK(first_capacity_identity.full_input_digest !=
        second_capacity_identity.full_input_digest);
  CHECK(first_capacity_identity.full_input_digest !=
        larger_capacity_identity.full_input_digest);
  CHECK(first_capacity_identity.outcome_id !=
        second_capacity_identity.outcome_id);
  CHECK(first_capacity_identity.outcome_id !=
        larger_capacity_identity.outcome_id);
}

TEST_CASE(
    "portfolio cancellation is invariant to grouped and interleaved input") {
  constexpr auto large_contribution =
      std::numeric_limits<contracts::AmountUnits>::max() / 16;
  std::vector<recommendation::TradeRecommendation> positive;
  std::vector<recommendation::TradeRecommendation> negative;
  positive.reserve(32);
  negative.reserve(32);
  for (contracts::AmountUnits offset = 0; offset < 32; ++offset) {
    positive.push_back(
        recommendation_for({.bid_quantity = 3 + offset, .ask_quantity = 1}));
    negative.push_back(
        recommendation_for({.bid_quantity = 1, .ask_quantity = 3 + offset}));
    corrupt_indicative_exposure(positive.back(), large_contribution);
    corrupt_indicative_exposure(negative.back(), large_contribution);
  }

  std::vector<recommendation::TradeRecommendation> grouped;
  std::vector<recommendation::TradeRecommendation> interleaved;
  grouped.reserve(64);
  interleaved.reserve(64);
  grouped.insert(grouped.end(), positive.begin(), positive.end());
  grouped.insert(grouped.end(), negative.begin(), negative.end());
  for (std::size_t index = 0; index < positive.size(); ++index) {
    interleaved.push_back(positive[index]);
    interleaved.push_back(negative[index]);
  }
  auto reversed = grouped;
  std::reverse(reversed.begin(), reversed.end());

  const auto snapshot = portfolio_snapshot(123);
  const auto policy = portfolio_policy(64);
  const auto grouped_result =
      portfolio::PortfolioConstructionAuthority::construct(
          grouped, snapshot, policy, portfolio_cut());
  const auto interleaved_result =
      portfolio::PortfolioConstructionAuthority::construct(
          interleaved, snapshot, policy, portfolio_cut());
  const auto reversed_result =
      portfolio::PortfolioConstructionAuthority::construct(
          reversed, snapshot, policy, portfolio_cut());

  const auto *grouped_no_change =
      grouped_result.terminal
          ? std::get_if<portfolio::PortfolioNoChange>(&*grouped_result.terminal)
          : nullptr;
  const auto *interleaved_no_change =
      interleaved_result.terminal ? std::get_if<portfolio::PortfolioNoChange>(
                                        &*interleaved_result.terminal)
                                  : nullptr;
  const auto *reversed_no_change =
      reversed_result.terminal ? std::get_if<portfolio::PortfolioNoChange>(
                                     &*reversed_result.terminal)
                               : nullptr;
  CHECK(grouped_no_change != nullptr);
  CHECK(interleaved_no_change != nullptr);
  CHECK(reversed_no_change != nullptr);
  if (!grouped_no_change || !interleaved_no_change || !reversed_no_change)
    return;
  CHECK(grouped_no_change->reason() ==
        portfolio::PortfolioNoChangeReason::ContributionsCancelled);
  CHECK(interleaved_no_change->reason() == grouped_no_change->reason());
  CHECK(reversed_no_change->reason() == grouped_no_change->reason());
  CHECK(interleaved_no_change->outcome_id() == grouped_no_change->outcome_id());
  CHECK(reversed_no_change->outcome_id() == grouped_no_change->outcome_id());
  CHECK(grouped_no_change->source_recommendation_ids().size() == 64);
}

TEST_CASE("portfolio rejection precedence is recommendation-order invariant") {
  const auto expired =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const auto unassigned =
      host_recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array first_order{expired, unassigned};
  const std::array second_order{unassigned, expired};
  const portfolio::PortfolioConstructionCut late_cut(1, 2'000'000'000);

  const auto first = portfolio::PortfolioConstructionAuthority::construct(
      first_order, portfolio_snapshot(0), portfolio_policy(), late_cut);
  const auto second = portfolio::PortfolioConstructionAuthority::construct(
      second_order, portfolio_snapshot(0), portfolio_policy(), late_cut);
  const auto *first_rejected =
      first.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                           &*first.terminal)
                     : nullptr;
  const auto *second_rejected =
      second.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                            &*second.terminal)
                      : nullptr;
  CHECK(first_rejected != nullptr);
  CHECK(second_rejected != nullptr);
  if (!first_rejected || !second_rejected)
    return;
  CHECK(first_rejected->reason() == second_rejected->reason());
  CHECK(first_rejected->outcome_id() == second_rejected->outcome_id());
  CHECK(first_rejected->full_input_evidence_digest() ==
        second_rejected->full_input_evidence_digest());
  CHECK(first_rejected->omitted_evidence_count() == 0);
  CHECK(second_rejected->omitted_evidence_count() == 0);
}

TEST_CASE("portfolio capacity rejection retains bounded canonical evidence") {
  std::vector<recommendation::TradeRecommendation> recommendations;
  recommendations.reserve(66);
  for (contracts::AmountUnits offset = 0; offset < 66; ++offset) {
    recommendations.push_back(
        recommendation_for({.bid_quantity = 3 + offset, .ask_quantity = 1}));
  }
  std::vector<recommendation::TradeRecommendation> first(
      recommendations.begin(), recommendations.begin() + 65);
  std::vector<recommendation::TradeRecommendation> second(
      recommendations.begin(), recommendations.begin() + 64);
  second.push_back(recommendations[65]);

  const auto first_result =
      portfolio::PortfolioConstructionAuthority::construct(
          first, portfolio_snapshot(0), portfolio_policy(64), portfolio_cut());
  const auto second_result =
      portfolio::PortfolioConstructionAuthority::construct(
          second, portfolio_snapshot(0), portfolio_policy(64), portfolio_cut());
  const auto *first_rejected =
      first_result.terminal
          ? std::get_if<portfolio::PortfolioConstructionRejected>(
                &*first_result.terminal)
          : nullptr;
  const auto *second_rejected =
      second_result.terminal
          ? std::get_if<portfolio::PortfolioConstructionRejected>(
                &*second_result.terminal)
          : nullptr;
  CHECK(first_rejected != nullptr);
  CHECK(second_rejected != nullptr);
  if (!first_rejected || !second_rejected)
    return;
  CHECK(first_rejected->reason() ==
        portfolio::PortfolioConstructionRejectionReason::
            RecommendationCapacityExceeded);
  CHECK(second_rejected->reason() == first_rejected->reason());
  CHECK(first_rejected->source_recommendation_ids().size() == 64);
  CHECK(first_rejected->source_signal_ids().size() == 64);
  CHECK(first_rejected->excluded_recommendation_ids().empty());
  CHECK(second_rejected->source_recommendation_ids().size() == 64);
  CHECK(second_rejected->source_signal_ids().size() == 64);
  CHECK(second_rejected->excluded_recommendation_ids().empty());
  CHECK(first_rejected->omitted_evidence_count() == 1);
  CHECK(second_rejected->omitted_evidence_count() == 1);
  CHECK(std::is_sorted(first_rejected->source_recommendation_ids().begin(),
                       first_rejected->source_recommendation_ids().end()));
  CHECK(std::is_sorted(second_rejected->source_recommendation_ids().begin(),
                       second_rejected->source_recommendation_ids().end()));
  CHECK(first_rejected->full_input_evidence_digest() !=
        second_rejected->full_input_evidence_digest());
  CHECK(first_rejected->outcome_id() != second_rejected->outcome_id());
}

TEST_CASE("portfolio capacity rejection bounds materially larger input") {
  const auto actionable =
      recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const auto hold = recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  std::vector<recommendation::TradeRecommendation> selected;
  selected.reserve(4096);
  for (std::size_t index = 0; index < 4096; ++index)
    selected.push_back(index % 2 == 0 ? actionable : hold);
  auto reversed = selected;
  std::reverse(reversed.begin(), reversed.end());

  const auto first = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(0), portfolio_policy(64), portfolio_cut());
  const auto second = portfolio::PortfolioConstructionAuthority::construct(
      reversed, portfolio_snapshot(0), portfolio_policy(64), portfolio_cut());
  const auto *first_rejected =
      first.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                           &*first.terminal)
                     : nullptr;
  const auto *second_rejected =
      second.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                            &*second.terminal)
                      : nullptr;
  CHECK(first_rejected != nullptr);
  CHECK(second_rejected != nullptr);
  if (!first_rejected || !second_rejected)
    return;
  CHECK(first_rejected->reason() ==
        portfolio::PortfolioConstructionRejectionReason::
            RecommendationCapacityExceeded);
  CHECK(second_rejected->reason() == first_rejected->reason());
  CHECK(first_rejected->source_recommendation_ids().size() +
            first_rejected->excluded_recommendation_ids().size() ==
        portfolio::PortfolioConstructionAuthority::kMaximumRecommendations);
  CHECK(first_rejected->source_signal_ids().size() +
            first_rejected->excluded_signal_ids().size() ==
        portfolio::PortfolioConstructionAuthority::kMaximumRecommendations);
  CHECK(first_rejected->omitted_evidence_count() == 4096 - 64);
  CHECK(second_rejected->omitted_evidence_count() == 4096 - 64);
  CHECK(first_rejected->full_input_evidence_digest() ==
        second_rejected->full_input_evidence_digest());
  CHECK(first_rejected->outcome_id() == second_rejected->outcome_id());
}

TEST_CASE("portfolio early rejection retains holds as excluded evidence") {
  const auto hold = recommendation_for({.bid_quantity = 5, .ask_quantity = 3});
  const auto unassigned_actionable =
      host_recommendation_for({.bid_quantity = 3, .ask_quantity = 1});
  const std::array selected{hold, unassigned_actionable};

  const auto result = portfolio::PortfolioConstructionAuthority::construct(
      selected, portfolio_snapshot(0), portfolio_policy(), portfolio_cut());
  const auto *rejected =
      result.terminal ? std::get_if<portfolio::PortfolioConstructionRejected>(
                            &*result.terminal)
                      : nullptr;
  CHECK(rejected != nullptr);
  if (!rejected)
    return;
  CHECK(rejected->reason() ==
        portfolio::PortfolioConstructionRejectionReason::UnassignedStrategy);
  CHECK(rejected->omitted_evidence_count() == 0);
  CHECK(rejected->source_recommendation_ids().size() == 1);
  CHECK(rejected->source_signal_ids().size() == 1);
  CHECK(rejected->excluded_recommendation_ids().size() == 1);
  CHECK(rejected->excluded_signal_ids().size() == 1);
  if (rejected->source_recommendation_ids().size() != 1 ||
      rejected->source_signal_ids().size() != 1 ||
      rejected->excluded_recommendation_ids().size() != 1 ||
      rejected->excluded_signal_ids().size() != 1)
    return;
  CHECK(rejected->source_recommendation_ids()[0] ==
        unassigned_actionable.recommendation_id());
  CHECK(rejected->source_signal_ids()[0] == unassigned_actionable.signal_id());
  CHECK(rejected->excluded_recommendation_ids()[0] == hold.recommendation_id());
  CHECK(rejected->excluded_signal_ids()[0] == hold.signal_id());
}
