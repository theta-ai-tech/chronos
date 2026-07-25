#include "chronos/strategies/reference/imbalance_threshold_strategy.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace {
namespace contracts = chronos::contracts;
namespace features = chronos::core::features;
namespace reference = chronos::strategies::reference;
namespace sdk = chronos::strategies::sdk;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), 1)
      .value();
}

contracts::Sha256Digest digest(std::uint8_t seed) {
  contracts::Sha256Digest value;
  value.bytes.front() = seed;
  return value;
}

contracts::StateLineage lineage() {
  const std::array streams = {id<contracts::StreamId>(10)};
  const std::array cursors = {
      contracts::StreamCursor::at_sequence(streams.front(), 1, 4).value()};
  return contracts::StateLineage::from(id<contracts::RunId>(1), 5, streams,
                                       cursors)
      .value();
}

features::FeatureProvenance provenance() {
  return {
      .run_id = id<contracts::RunId>(1),
      .listing_id = id<contracts::ListingId>(2),
      .bundle_id = id<contracts::StateViewId>(3),
      .listing_view_id = id<contracts::StateViewId>(4),
      .run_input_sequence = 5,
      .causing_selection_id = id<contracts::RunInputSelectionId>(6),
      .causing_event_id = id<contracts::EventId>(7),
      .logical_time_nanoseconds = 100,
      .configuration_epoch = 2,
      .effective_control_position = 4,
      .lineage = lineage(),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(8),
      .reference_snapshot_version = version(20),
      .listing_definition_version = version(21),
      .reference_configuration_lineage_version = version(22),
      .input_view_schema_version = version(23),
      .input_view_capability_version = version(24),
      .input_bundle_schema_version = version(25),
      .input_arithmetic_version = version(26),
      .input_canonicalization_version = version(27),
      .input_identity_policy_version = version(28),
      .input_merge_policy_version = version(29),
      .input_registry_snapshot_version = version(30),
      .feature_definition_version = reference::ImbalanceThresholdStrategy::
          imbalance_feature_definition_version(),
      .implementation_version = version(31),
      .feature_arithmetic_version = version(32),
      .canonicalization_version = version(33),
      .identity_policy_version = version(34),
      .input_view_semantic_checksum = digest(35),
      .input_bundle_semantic_checksum = digest(36),
  };
}

features::FeatureEvaluation imbalance(contracts::AmountUnits units) {
  auto observation = features::FeatureObservation{
      .observation_id = id<contracts::FeatureObservationId>(40),
      .kind = features::FeatureKind::OrderBookImbalance,
      .provenance = provenance(),
      .value =
          features::ScaledRatio{
              .units = units,
              .scale = *contracts::DecimalScale::from_exponent(6),
          },
      .semantic_checksum = digest(41),
  };
  return {
      .evaluation_id = id<contracts::FeatureEvaluationId>(42),
      .kind = features::FeatureKind::OrderBookImbalance,
      .disposition = features::FeatureDisposition::ValidObservation,
      .observation = std::move(observation),
      .semantic_checksum = digest(43),
  };
}

features::FeatureEvaluation
unavailable_imbalance(features::FeatureUnavailableReason reason) {
  return {
      .evaluation_id = id<contracts::FeatureEvaluationId>(44),
      .kind = features::FeatureKind::OrderBookImbalance,
      .disposition = features::FeatureDisposition::Unavailable,
      .unavailable =
          features::FeatureUnavailable{
              .unavailable_id = id<contracts::FeatureUnavailableId>(45),
              .kind = features::FeatureKind::OrderBookImbalance,
              .provenance = provenance(),
              .reason = reason,
              .semantic_checksum = digest(46),
          },
      .semantic_checksum = digest(47),
  };
}

sdk::StrategyParameter threshold(contracts::AmountUnits units = 250000) {
  return {
      .parameter_id =
          reference::ImbalanceThresholdStrategy::threshold_parameter_id(),
      .definition_version =
          reference::ImbalanceThresholdStrategy::threshold_parameter_version(),
      .units = units,
      .scale = *contracts::DecimalScale::from_exponent(6),
  };
}

sdk::StrategyInvocation
invocation(std::span<const features::FeatureEvaluation> feature_inputs,
           std::span<const sdk::StrategyParameter> parameters,
           std::optional<std::int64_t> deadline = std::nullopt) {
  return {
      .run_id = id<contracts::RunId>(1),
      .strategy_instance_id = id<contracts::StrategyInstanceId>(50),
      .listing_id = id<contracts::ListingId>(2),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(8),
      .features = feature_inputs,
      .parameters = parameters,
      .cut =
          {
              .run_input_sequence = 5,
              .logical_time_nanoseconds = 100,
              .run_timer_cursor = contracts::StreamCursor::at_sequence(
                                      id<contracts::StreamId>(10), 1, 4)
                                      .value(),
              .configuration_epoch = 2,
              .effective_control_position = 4,
              .logical_deadline_nanoseconds = deadline,
          },
  };
}

struct CapturedResult final {
  sdk::StrategyExecutionStatus status{
      sdk::StrategyExecutionStatus::ContractViolation};
  std::optional<sdk::TerminalDraft> terminal;
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
  std::size_t factor_count{};
  std::uint64_t consumed_operations{};

  bool operator==(const CapturedResult &) const = default;
};

CapturedResult
run(const sdk::StrategyInvocation &input,
    std::uint64_t maximum_operations =
        reference::ImbalanceThresholdStrategy::kOperationsPerEvaluation,
    std::size_t factor_capacity = 2) {
  reference::ImbalanceThresholdStrategy strategy;
  sdk::DeterministicOperationBudget budget(maximum_operations);
  std::array<std::optional<sdk::ExplanationFactor>, 2> factors;
  sdk::StrategyOutput output(
      std::span(factors).first(std::min(factor_capacity, factors.size())));
  const auto status = strategy.evaluate(input, budget, output);
  return {
      .status = status,
      .terminal = output.terminal(),
      .factors = factors,
      .factor_count = output.factor_count(),
      .consumed_operations = budget.consumed_operations(),
  };
}

sdk::StrategyAbstentionReason abstention_reason(const CapturedResult &result) {
  return std::get<sdk::AbstentionDraft>(*result.terminal).reason;
}

} // namespace

TEST_CASE("reference strategy descriptor is fixed and SDK conformant") {
  const reference::ImbalanceThresholdStrategy strategy;
  const auto &descriptor = strategy.descriptor();
  CHECK(sdk::validate_descriptor(descriptor));
  CHECK(descriptor.definition_version ==
        reference::ImbalanceThresholdStrategy::definition_version());
  CHECK(descriptor.implementation_version ==
        reference::ImbalanceThresholdStrategy::implementation_version());
  CHECK(descriptor.required_features.size() == 1);
  CHECK(descriptor.parameter_schema.size() == 1);
  CHECK(descriptor.resource_limits.maximum_operations ==
        reference::ImbalanceThresholdStrategy::kOperationsPerEvaluation);
  CHECK(descriptor.resource_limits.maximum_explanation_factors == 2);
}

TEST_CASE("positive and negative threshold signals are deterministic") {
  const std::array positive_feature = {imbalance(250000)};
  const std::array negative_feature = {imbalance(-600000)};
  const std::array parameters = {threshold()};
  const auto positive = run(invocation(positive_feature, parameters));
  const auto repeated = run(invocation(positive_feature, parameters));
  const auto negative = run(invocation(negative_feature, parameters));

  CHECK(positive == repeated);
  CHECK(positive.status == sdk::StrategyExecutionStatus::Completed);
  CHECK(positive.factor_count == 2);
  const auto &positive_signal = std::get<sdk::SignalDraft>(*positive.terminal);
  CHECK(positive_signal.direction == sdk::StrategyDirection::Positive);
  CHECK(positive_signal.strength.units == 250000);
  CHECK(positive_signal.horizon_nanoseconds ==
        reference::ImbalanceThresholdStrategy::kSignalHorizonNanoseconds);
  CHECK(positive.factors[0]->causal_feature_evaluation_id ==
        positive_feature.front().evaluation_id);
  CHECK(positive.factors[0]->rank == 1);
  CHECK(positive.factors[1]->rank == 2);

  const auto &negative_signal = std::get<sdk::SignalDraft>(*negative.terminal);
  CHECK(negative_signal.direction == sdk::StrategyDirection::Negative);
  CHECK(negative_signal.strength.units == 600000);
  CHECK(negative.factors[1]->signed_contribution_units == -350000);
}

TEST_CASE("inside-threshold input abstains instead of emitting zero signal") {
  const std::array feature_inputs = {imbalance(249999)};
  const std::array parameters = {threshold()};
  const auto result = run(invocation(feature_inputs, parameters));
  CHECK(result.status == sdk::StrategyExecutionStatus::Completed);
  CHECK(abstention_reason(result) ==
        sdk::StrategyAbstentionReason::NoDirectionalSignal);
  CHECK(!std::holds_alternative<sdk::SignalDraft>(*result.terminal));
  CHECK(result.factor_count == 2);
  CHECK(result.factors[0]->role == sdk::ExplanationRole::ExplainsAbstention);
  CHECK(result.factors[1]->signed_contribution_units == -1);
}

TEST_CASE("missing unavailable and incompatible features abstain explicitly") {
  const std::array parameters = {threshold()};
  const std::array<features::FeatureEvaluation, 0> missing;
  CHECK(abstention_reason(run(invocation(missing, parameters))) ==
        sdk::StrategyAbstentionReason::MissingDeclaredFeature);

  const std::array stale = {
      unavailable_imbalance(features::FeatureUnavailableReason::BookStale)};
  CHECK(abstention_reason(run(invocation(stale, parameters))) ==
        sdk::StrategyAbstentionReason::NonValidFeature);

  auto wrong_definition = imbalance(500000);
  wrong_definition.observation->provenance.feature_definition_version =
      version(70);
  const std::array incompatible = {wrong_definition};
  CHECK(abstention_reason(run(invocation(incompatible, parameters))) ==
        sdk::StrategyAbstentionReason::IncompatibleFeature);

  auto future = imbalance(500000);
  future.observation->provenance.run_input_sequence = 6;
  const std::array future_inputs = {future};
  CHECK(abstention_reason(run(invocation(future_inputs, parameters))) ==
        sdk::StrategyAbstentionReason::IncompatibleFeature);
}

TEST_CASE(
    "parameter deadline budget and output bounds fail deterministically") {
  const std::array feature_inputs = {imbalance(500000)};
  const std::array<sdk::StrategyParameter, 0> missing_parameters;
  CHECK(
      abstention_reason(run(invocation(feature_inputs, missing_parameters))) ==
      sdk::StrategyAbstentionReason::MissingParameter);

  const std::array invalid_parameters = {threshold(0)};
  CHECK(
      abstention_reason(run(invocation(feature_inputs, invalid_parameters))) ==
      sdk::StrategyAbstentionReason::InvalidParameter);

  const std::array parameters = {threshold()};
  CHECK(abstention_reason(run(invocation(feature_inputs, parameters, 99))) ==
        sdk::StrategyAbstentionReason::LogicalDeadlineExceeded);

  const auto no_fuel =
      run(invocation(feature_inputs, parameters),
          reference::ImbalanceThresholdStrategy::kOperationsPerEvaluation - 1);
  CHECK(no_fuel.status ==
        sdk::StrategyExecutionStatus::DeterministicBudgetExhausted);
  CHECK(!no_fuel.terminal);
  CHECK(no_fuel.factor_count == 0);
  CHECK(no_fuel.consumed_operations == 0);

  const auto too_small =
      run(invocation(feature_inputs, parameters),
          reference::ImbalanceThresholdStrategy::kOperationsPerEvaluation, 1);
  CHECK(too_small.status ==
        sdk::StrategyExecutionStatus::OutputCapacityExceeded);
  CHECK(!too_small.terminal);
}
