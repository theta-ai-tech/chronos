#include "chronos/strategies/reference/imbalance_threshold_strategy.hpp"

#include <array>

namespace chronos::strategies::reference {
namespace {

contracts::DefinitionId definition(std::string_view value) {
  return contracts::DefinitionId::parse(value).value();
}

contracts::VersionRef version(std::string_view value) {
  return contracts::VersionRef::from(definition(value), 1).value();
}

const auto kScale = *contracts::DecimalScale::from_exponent(
    core::features::FeatureRuntime::kImbalanceScaleExponent);

sdk::StrategyExecutionStatus abstain(sdk::StrategyOutput &output,
                                     sdk::StrategyAbstentionReason reason) {
  return output.abstain({.reason = reason})
             ? sdk::StrategyExecutionStatus::Completed
             : sdk::StrategyExecutionStatus::ContractViolation;
}

bool append_factor(sdk::StrategyOutput &output,
                   contracts::DefinitionId factor_id, std::uint32_t rank,
                   sdk::ExplanationRole role,
                   contracts::AmountUnits observed_units,
                   contracts::AmountUnits contribution_units,
                   std::optional<contracts::FeatureEvaluationId> cause) {
  return output.append_factor({
      .factor_id = factor_id,
      .rank = rank,
      .source = cause ? sdk::ExplanationSource::Feature
                      : sdk::ExplanationSource::StrategyParameter,
      .role = role,
      .observed_units = observed_units,
      .observed_scale = kScale,
      .signed_contribution_units = contribution_units,
      .contribution_scale = kScale,
      .causal_feature_evaluation_id = cause,
      .ranking_policy_version = version("0f510000-0000-0000-0000-000000000009"),
  });
}

} // namespace

contracts::VersionRef ImbalanceThresholdStrategy::definition_version() {
  return version("0f510000-0000-0000-0000-000000000001");
}

contracts::VersionRef ImbalanceThresholdStrategy::implementation_version() {
  return version("0f510000-0000-0000-0000-000000000002");
}

contracts::VersionRef
ImbalanceThresholdStrategy::imbalance_feature_definition_version() {
  return version("0f510000-0000-0000-0000-000000000003");
}

contracts::DefinitionId ImbalanceThresholdStrategy::threshold_parameter_id() {
  return definition("0f510000-0000-0000-0000-000000000004");
}

contracts::VersionRef
ImbalanceThresholdStrategy::threshold_parameter_version() {
  return version("0f510000-0000-0000-0000-000000000005");
}

contracts::DefinitionId ImbalanceThresholdStrategy::imbalance_factor_id() {
  return definition("0f510000-0000-0000-0000-000000000006");
}

contracts::DefinitionId ImbalanceThresholdStrategy::threshold_factor_id() {
  return definition("0f510000-0000-0000-0000-000000000007");
}

const sdk::StrategyDescriptor &
ImbalanceThresholdStrategy::descriptor() const noexcept {
  static const std::array dependencies = {
      sdk::FeatureDependency{
          .kind = core::features::FeatureKind::OrderBookImbalance,
          .definition_version = imbalance_feature_definition_version(),
      },
  };
  static const std::array parameters = {
      sdk::StrategyParameterSchema{
          .parameter_id = threshold_parameter_id(),
          .definition_version = threshold_parameter_version(),
          .scale = kScale,
      },
  };
  static const sdk::StrategyDescriptor descriptor{
      .definition_version = definition_version(),
      .implementation_version = implementation_version(),
      .scope = sdk::StrategyScope::SingleListing,
      .family = sdk::StrategyFamily::OrderBookImbalance,
      .required_features = dependencies,
      .parameter_schema = parameters,
      .arithmetic_version = version("0f510000-0000-0000-0000-000000000008"),
      .explanation_policy_version =
          version("0f510000-0000-0000-0000-000000000009"),
      .resource_limits =
          {
              .maximum_operations = kOperationsPerEvaluation,
              .maximum_features = 1,
              .maximum_parameters = 1,
              .maximum_explanation_factors = 2,
              .maximum_working_bytes = 256,
          },
  };
  return descriptor;
}

sdk::StrategyExecutionStatus ImbalanceThresholdStrategy::evaluate(
    const sdk::StrategyInvocation &invocation,
    sdk::DeterministicOperationBudget &budget,
    sdk::StrategyOutput &output) const noexcept {
  if (!budget.consume(kOperationsPerEvaluation))
    return sdk::StrategyExecutionStatus::DeterministicBudgetExhausted;
  if (sdk::logical_deadline_exceeded(invocation.cut))
    return abstain(output,
                   sdk::StrategyAbstentionReason::LogicalDeadlineExceeded);
  if (invocation.features.empty())
    return abstain(output,
                   sdk::StrategyAbstentionReason::MissingDeclaredFeature);
  if (invocation.features.size() != 1)
    return abstain(output, sdk::StrategyAbstentionReason::IncompatibleFeature);

  const auto &evaluation = invocation.features.front();
  if (evaluation.kind != core::features::FeatureKind::OrderBookImbalance)
    return abstain(output, sdk::StrategyAbstentionReason::IncompatibleFeature);
  if (evaluation.disposition !=
          core::features::FeatureDisposition::ValidObservation ||
      !evaluation.observation || evaluation.unavailable) {
    return abstain(output, sdk::StrategyAbstentionReason::NonValidFeature);
  }

  const auto &observation = *evaluation.observation;
  const auto &provenance = observation.provenance;
  const auto *imbalance =
      std::get_if<core::features::ScaledRatio>(&observation.value);
  if (observation.kind != core::features::FeatureKind::OrderBookImbalance ||
      !imbalance || imbalance->scale != kScale ||
      provenance.feature_definition_version !=
          imbalance_feature_definition_version() ||
      provenance.run_id != invocation.run_id ||
      provenance.listing_id != invocation.listing_id ||
      provenance.canonical_instrument_id !=
          invocation.canonical_instrument_id ||
      provenance.run_input_sequence != invocation.cut.run_input_sequence ||
      provenance.logical_time_nanoseconds !=
          invocation.cut.logical_time_nanoseconds ||
      provenance.configuration_epoch != invocation.cut.configuration_epoch ||
      provenance.effective_control_position !=
          invocation.cut.effective_control_position ||
      imbalance->units < -kScale.denominator() ||
      imbalance->units > kScale.denominator()) {
    return abstain(output, sdk::StrategyAbstentionReason::IncompatibleFeature);
  }

  if (invocation.parameters.empty())
    return abstain(output, sdk::StrategyAbstentionReason::MissingParameter);
  if (invocation.parameters.size() != 1)
    return abstain(output, sdk::StrategyAbstentionReason::InvalidParameter);
  const auto &threshold = invocation.parameters.front();
  if (threshold.parameter_id != threshold_parameter_id() ||
      threshold.definition_version != threshold_parameter_version() ||
      threshold.scale != kScale || threshold.units <= 0 ||
      threshold.units > kScale.denominator()) {
    return abstain(output, sdk::StrategyAbstentionReason::InvalidParameter);
  }

  const auto units = imbalance->units;
  const auto magnitude = units < 0 ? -units : units;
  const bool emits_signal = magnitude >= threshold.units;
  const auto role = !emits_signal ? sdk::ExplanationRole::ExplainsAbstention
                    : units > 0   ? sdk::ExplanationRole::SupportsPositive
                                  : sdk::ExplanationRole::SupportsNegative;
  const auto threshold_margin = magnitude - threshold.units;
  const auto signed_threshold_margin =
      emits_signal && units < 0 ? -threshold_margin : threshold_margin;
  if (!append_factor(output, imbalance_factor_id(), 1, role, units, units,
                     evaluation.evaluation_id) ||
      !append_factor(output, threshold_factor_id(), 2, role, threshold.units,
                     signed_threshold_margin, std::nullopt)) {
    return sdk::StrategyExecutionStatus::OutputCapacityExceeded;
  }

  if (!emits_signal)
    return abstain(output, sdk::StrategyAbstentionReason::NoDirectionalSignal);

  const auto direction = units > 0 ? sdk::StrategyDirection::Positive
                                   : sdk::StrategyDirection::Negative;
  return output.emit_signal({
             .direction = direction,
             .strength = {.units = magnitude, .scale = kScale},
             .horizon_nanoseconds = kSignalHorizonNanoseconds,
         })
             ? sdk::StrategyExecutionStatus::Completed
             : sdk::StrategyExecutionStatus::ContractViolation;
}

} // namespace chronos::strategies::reference
