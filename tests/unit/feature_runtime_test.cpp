#include "chronos/core/features/feature_runtime.hpp"
#include "chronos/runtime/strategies/strategy_runtime.hpp"
#include "chronos/strategies/sdk/strategy_host.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

namespace {
namespace contracts = chronos::contracts;
namespace dispatch = chronos::core::dispatch;
namespace features = chronos::core::features;
namespace market = chronos::core::market_state;
namespace strategy_runtime = chronos::runtime::strategies;
namespace sdk = chronos::strategies::sdk;

static_assert(
    !std::is_default_constructible_v<sdk::AcceptedStrategyInvocation>);
static_assert(!std::is_aggregate_v<sdk::AcceptedStrategyInvocation>);

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number = 1) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
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
        contracts::StreamCursor timer_cursor = origin(13)) {
  const auto required = required_streams();
  const std::array cursors = {origin(4),  origin(7),  book_cursor, origin(10),
                              origin(11), origin(12), timer_cursor};
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
    std::vector<std::byte> payload, contracts::StateLineage cut_lineage) {
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
      .control_cursor = origin(15),
      .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(38),
      .active_configuration_epoch = 1,
      .merge_policy_version = version(37),
      .registry_snapshot_version = version(39),
      .input_semantic_checksum = candidate.semantic_checksum,
  };
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
      .configuration_epoch = 1,
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .reference_snapshot_version = version(43),
      .listing_definition_version = version(44),
      .reference_configuration_lineage_version = version(45),
      .lineage = std::move(cut_lineage),
  };
}

market::ListingViewCutInput initial_cut_input() {
  return make_cut_input(
      1, id<contracts::EventId>(23), "market.book.observation.snapshot",
      contracts::EventPosition::from(id<contracts::StreamId>(8), 1, 0).value(),
      kInitialPayload, lineage(1, cursor(8, 0)));
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

strategy_runtime::StrategyRuntimeConfig host_runtime_config(
    const features::AcceptedFeatureEvaluationCut &accepted,
    std::optional<sdk::StrategyParameter> parameter,
    std::optional<std::int64_t> deadline = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  const auto &feature = accepted.evaluations().front();
  const auto &provenance = feature.observation
                               ? feature.observation->provenance
                               : feature.unavailable->provenance;
  return {
      .run_id = provenance.run_id,
      .strategy_instance_id = id<contracts::StrategyInstanceId>(95),
      .listing_id = provenance.listing_id,
      .canonical_instrument_id = provenance.canonical_instrument_id,
      .definition = accepted_host_definition(),
      .parameter = parameter,
      .activation_control_outcome_id = id<contracts::EventId>(99),
      .active_configuration_epoch = provenance.configuration_epoch,
      .effective_control_position = provenance.effective_control_position,
      .run_control_stream_id = id<contracts::StreamId>(12),
      .run_control_stream_epoch = 1,
      .run_timer_stream_id = id<contracts::StreamId>(13),
      .run_timer_stream_epoch = 1,
      .maximum_operations = maximum_operations,
      .logical_deadline_nanoseconds = deadline,
  };
}

sdk::AcceptedStrategyInvocation accepted_host_invocation(
    const features::AcceptedFeatureEvaluationCut &accepted,
    std::optional<std::int64_t> deadline = std::nullopt,
    std::uint64_t maximum_operations = sdk::kMaximumEvaluationOperations) {
  const auto runtime =
      strategy_runtime::StrategyRuntime::activate(host_runtime_config(
          accepted, threshold_parameters()[0], deadline, maximum_operations));
  if (!runtime)
    std::abort();
  const auto invocation = runtime->admit(accepted);
  if (!invocation)
    std::abort();
  return *invocation;
}

sdk::AcceptedStrategyInvocation accepted_host_invocation_without_parameter(
    const features::AcceptedFeatureEvaluationCut &accepted) {
  const auto runtime = strategy_runtime::StrategyRuntime::activate(
      host_runtime_config(accepted, std::nullopt));
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

market::ListingViewPublisher
publish_quality_state(market::ListingQualityInputKind kind,
                      std::string event_type, std::int64_t logical_time) {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first = publisher.accept_cut(initial_cut_input(), book, auxiliary);
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
  auto cut_lineage = lineage(2, cursor(8, 0), cursor(13, 0));
  if (kind == market::ListingQualityInputKind::BookGapDetected) {
    quality.event_cursor = cursor(8, 1);
    selected_position =
        contracts::EventPosition::from(id<contracts::StreamId>(8), 1, 1)
            .value();
    cut_lineage = lineage(2, cursor(8, 1));
  }
  if (!auxiliary.apply_quality_input(quality, &book).ok())
    std::abort();
  const auto second_input =
      make_cut_input(2, event_id, std::move(event_type), selected_position,
                     payload, std::move(cut_lineage));
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
  auto publisher = publish_initial();
  const features::FeatureRuntime runtime(runtime_config());
  const auto result = runtime.evaluate(*publisher.accepted_feature_cut());
  CHECK(result.ok());
  CHECK(result.accepted_cut() != nullptr);

  const auto definition = accepted_host_definition();
  const auto invocation = accepted_host_invocation(*result.accepted_cut());
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
  CHECK(factors[1]->causal_configuration_epoch == 1);
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

TEST_CASE("strategy host enforces fuel deadline workspace and output bounds") {
  auto publisher = publish_initial();
  const features::FeatureRuntime runtime(runtime_config());
  const auto result = runtime.evaluate(*publisher.accepted_feature_cut());
  const auto definition = accepted_host_definition();
  std::array<std::byte, sdk::kInterpreterWorkingBytes> workspace;
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;

  const auto invocation = accepted_host_invocation(*result.accepted_cut());
  const auto low_fuel_invocation = accepted_host_invocation(
      *result.accepted_cut(), std::nullopt, sdk::kAdmissionOperations + 4);
  const auto no_fuel = sdk::StrategyHost::evaluate(
      definition, low_fuel_invocation, workspace, factors);
  CHECK(no_fuel.status ==
        sdk::StrategyExecutionStatus::DeterministicBudgetExhausted);
  CHECK(no_fuel.charged_operations == sdk::kAdmissionOperations + 4);
  CHECK(std::none_of(factors.begin(), factors.end(),
                     [](const auto &factor) { return factor.has_value(); }));

  auto wrong_run =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  wrong_run.run_id = id<contracts::RunId>(96);
  const auto wrong_run_runtime =
      strategy_runtime::StrategyRuntime::activate(wrong_run);
  CHECK(wrong_run_runtime.has_value());
  CHECK(!wrong_run_runtime->admit(*result.accepted_cut()));

  auto wrong_timer =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  wrong_timer.run_timer_stream_id = id<contracts::StreamId>(99);
  const auto wrong_timer_runtime =
      strategy_runtime::StrategyRuntime::activate(wrong_timer);
  CHECK(wrong_timer_runtime.has_value());
  CHECK(!wrong_timer_runtime->admit(*result.accepted_cut()));

  auto wrong_control =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  ++wrong_control.active_configuration_epoch;
  const auto wrong_control_runtime =
      strategy_runtime::StrategyRuntime::activate(wrong_control);
  CHECK(wrong_control_runtime.has_value());
  CHECK(!wrong_control_runtime->admit(*result.accepted_cut()));

  const auto late_invocation = accepted_host_invocation(
      *result.accepted_cut(), invocation.cut().logical_time_nanoseconds - 1);
  const auto late = sdk::StrategyHost::evaluate(definition, late_invocation,
                                                workspace, factors);
  CHECK(late.status == sdk::StrategyExecutionStatus::LogicalDeadlineExceeded);
  CHECK(late.charged_operations == sdk::kAdmissionOperations);

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
  auto owning_runtime =
      strategy_runtime::StrategyRuntime::activate(mutable_config);
  mutable_config.parameter->units = 900000;
  mutable_config.logical_deadline_nanoseconds = 0;
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
  const auto alternate_runtime =
      strategy_runtime::StrategyRuntime::activate(alternate_activation);
  CHECK(alternate_runtime.has_value());
  CHECK(alternate_runtime->activation_checksum() !=
        owning_runtime->activation_checksum());
  CHECK(
      alternate_runtime->admit(*result.accepted_cut())->activation_checksum() ==
      alternate_runtime->activation_checksum());

  auto alternate_instance =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_instance.strategy_instance_id =
      id<contracts::StrategyInstanceId>(98);
  auto alternate_fuel =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_fuel.maximum_operations = sdk::kAdmissionOperations + 4;
  auto alternate_deadline =
      host_runtime_config(*result.accepted_cut(), threshold_parameters()[0]);
  alternate_deadline.logical_deadline_nanoseconds =
      invocation.cut().logical_time_nanoseconds;
  const auto instance_runtime =
      strategy_runtime::StrategyRuntime::activate(alternate_instance);
  const auto fuel_runtime =
      strategy_runtime::StrategyRuntime::activate(alternate_fuel);
  const auto deadline_runtime =
      strategy_runtime::StrategyRuntime::activate(alternate_deadline);
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
  const auto one_fuel_invocation =
      accepted_host_invocation(*result.accepted_cut(), std::nullopt, 1);
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
  auto publisher = publish_initial();
  const features::FeatureRuntime runtime(runtime_config());
  const auto result = runtime.evaluate(*publisher.accepted_feature_cut());
  const auto invocation =
      accepted_host_invocation_without_parameter(*result.accepted_cut());
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
  CHECK(factors[0]->causal_configuration_epoch == 1);
  CHECK(factors[0]->causal_control_outcome_id == id<contracts::EventId>(99));
  CHECK(factors[0]->causal_activation_checksum ==
        invocation.activation_checksum());
}

TEST_CASE(
    "strategy host turns stale and gapped feature cuts into abstentions") {
  const features::FeatureRuntime runtime(runtime_config());
  auto stale_publisher = publish_quality_state(
      market::ListingQualityInputKind::LogicalTimerAdvanced,
      "run.timer.logical.advanced", 111);
  auto gapped_publisher =
      publish_quality_state(market::ListingQualityInputKind::BookGapDetected,
                            "market.book.quality.gap_detected", 101);
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
  const features::FeatureRuntime runtime(runtime_config());
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
    auto publisher = publish_initial(test_case.top);
    const auto result = runtime.evaluate(*publisher.accepted_feature_cut());
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
