#include "chronos/strategies/sdk/strategy_host.hpp"

#include <algorithm>
#include <utility>

namespace chronos::strategies::sdk {
namespace {

struct InterpreterState final {
  const core::features::FeatureEvaluation *feature{};
  const core::features::ScaledRatio *ratio{};
  const StrategyParameter *parameter{};
  contracts::AmountUnits magnitude{};
  bool emits_signal{};
};

static_assert(sizeof(InterpreterState) <= kInterpreterWorkingBytes);

struct InvocationView final {
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::VersionRef strategy_definition_version;
  contracts::VersionRef strategy_implementation_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef explanation_policy_version;
  std::uint64_t maximum_operations{};
  const core::features::AcceptedFeatureEvaluationCut &feature_cut;
  std::span<const StrategyParameter> parameters;
  const LogicalCut &cut;
};

InvocationView
view(const StrategyInvocationAuthorityConfig &config,
     const core::features::AcceptedFeatureEvaluationCut &feature_cut) noexcept {
  return {
      .run_id = config.run_id,
      .listing_id = config.listing_id,
      .canonical_instrument_id = config.canonical_instrument_id,
      .strategy_definition_version = config.strategy_definition_version,
      .strategy_implementation_version = config.strategy_implementation_version,
      .arithmetic_version = config.arithmetic_version,
      .explanation_policy_version = config.explanation_policy_version,
      .maximum_operations = config.maximum_operations,
      .feature_cut = feature_cut,
      .parameters = config.parameter ? std::span<const StrategyParameter>(
                                           &*config.parameter, 1)
                                     : std::span<const StrategyParameter>{},
      .cut = config.cut,
  };
}

InvocationView view(const AcceptedStrategyInvocation &invocation) noexcept {
  return {
      .run_id = invocation.run_id(),
      .listing_id = invocation.listing_id(),
      .canonical_instrument_id = invocation.canonical_instrument_id(),
      .strategy_definition_version = invocation.strategy_definition_version(),
      .strategy_implementation_version =
          invocation.strategy_implementation_version(),
      .arithmetic_version = invocation.arithmetic_version(),
      .explanation_policy_version = invocation.explanation_policy_version(),
      .maximum_operations = invocation.maximum_operations(),
      .feature_cut = invocation.feature_cut(),
      .parameters = invocation.parameters(),
      .cut = invocation.cut(),
  };
}

std::optional<StrategyFeatureKind>
map_kind(core::features::FeatureKind kind) noexcept {
  using Source = core::features::FeatureKind;
  switch (kind) {
  case Source::OrderBookImbalance:
    return StrategyFeatureKind::OrderBookImbalance;
  case Source::Microprice:
    return StrategyFeatureKind::Microprice;
  case Source::Spread:
    return StrategyFeatureKind::Spread;
  }
  return std::nullopt;
}

const core::features::FeatureProvenance *
provenance(const core::features::FeatureEvaluation &evaluation) noexcept {
  if (evaluation.disposition ==
      core::features::FeatureDisposition::ValidObservation) {
    if (!evaluation.observation || evaluation.unavailable ||
        evaluation.observation->kind != evaluation.kind)
      return nullptr;
    return &evaluation.observation->provenance;
  }
  if (evaluation.observation || !evaluation.unavailable ||
      evaluation.unavailable->kind != evaluation.kind)
    return nullptr;
  return &evaluation.unavailable->provenance;
}

bool matches_cut(const core::features::FeatureProvenance &value,
                 const InvocationView &invocation) noexcept {
  const auto cursors = value.lineage.cursors();
  if (cursors.size() > kMaximumAcceptedLineageCursors ||
      std::find(cursors.begin(), cursors.end(),
                invocation.cut.run_timer_cursor) == cursors.end())
    return false;
  return value.lineage.run_id() == value.run_id &&
         value.lineage.run_input_sequence() == value.run_input_sequence &&
         value.run_id == invocation.run_id &&
         value.listing_id == invocation.listing_id &&
         value.canonical_instrument_id == invocation.canonical_instrument_id &&
         value.run_input_sequence == invocation.cut.run_input_sequence &&
         value.logical_time_nanoseconds ==
             invocation.cut.logical_time_nanoseconds &&
         value.configuration_epoch == invocation.cut.configuration_epoch &&
         value.effective_control_position ==
             invocation.cut.effective_control_position;
}

bool validate_feature_cut(const InvocationView &invocation) noexcept {
  const auto &accepted = invocation.feature_cut.evaluations();
  if (accepted.empty() || accepted.size() > kMaximumAcceptedFeatureEvaluations)
    return false;
  const core::features::FeatureProvenance *first_provenance{};
  for (auto current = accepted.begin(); current != accepted.end(); ++current) {
    const auto kind = map_kind(current->kind);
    const auto *source_provenance = provenance(*current);
    if (!kind || !source_provenance ||
        !matches_cut(*source_provenance, invocation) ||
        (first_provenance &&
         source_provenance->lineage != first_provenance->lineage))
      return false;
    first_provenance = source_provenance;
    if (std::count_if(accepted.begin(), accepted.end(), [&](const auto &other) {
          return map_kind(other.kind) == kind;
        }) != 1)
      return false;
  }
  return true;
}

bool validate_parameters(
    std::span<const StrategyParameter> parameters) noexcept {
  return parameters.size() <= 1;
}

bool matches_definition(const StrategyDescriptor &descriptor,
                        const InvocationView &invocation) noexcept {
  return invocation.strategy_definition_version ==
             descriptor.definition_version &&
         invocation.strategy_implementation_version ==
             descriptor.implementation_version &&
         invocation.arithmetic_version == descriptor.arithmetic_version &&
         invocation.explanation_policy_version ==
             descriptor.explanation_policy_version &&
         invocation.maximum_operations <=
             descriptor.resource_limits.maximum_operations;
}

const core::features::FeatureEvaluation *
find_feature(const InvocationView &invocation,
             StrategyFeatureKind kind) noexcept {
  const auto &accepted = invocation.feature_cut.evaluations();
  const auto found = std::find_if(
      accepted.begin(), accepted.end(),
      [&](const auto &candidate) { return map_kind(candidate.kind) == kind; });
  return found == accepted.end() ? nullptr : &*found;
}

const StrategyParameter *
find_parameter(const InvocationView &invocation,
               const StrategyParameterSchema &schema) noexcept {
  const auto found =
      std::find_if(invocation.parameters.begin(), invocation.parameters.end(),
                   [&](const auto &candidate) {
                     return candidate.parameter_id == schema.parameter_id;
                   });
  return found == invocation.parameters.end() ? nullptr : &*found;
}

void clear_factors(
    std::span<std::optional<ExplanationFactor>> factor_storage) noexcept {
  for (auto &factor : factor_storage)
    factor.reset();
}

StrategyHostResult
failure(StrategyExecutionStatus status,
        const DeterministicOperationBudget &budget,
        std::span<std::optional<ExplanationFactor>> factor_storage) noexcept {
  clear_factors(factor_storage);
  return {.status = status, .charged_operations = budget.consumed_operations()};
}

bool append_factor(std::span<std::optional<ExplanationFactor>> factor_storage,
                   std::size_t &factor_count,
                   ExplanationFactor factor) noexcept {
  if (factor_count == factor_storage.size())
    return false;
  factor.rank = static_cast<std::uint32_t>(factor_count + 1);
  factor_storage[factor_count++].emplace(std::move(factor));
  return true;
}

StrategyHostResult complete_abstention(
    const AcceptedStrategyDefinition &definition,
    StrategyAbstentionReason reason, const InterpreterState &state,
    std::size_t diagnostic_factor_index,
    const DeterministicOperationBudget &budget,
    std::span<std::optional<ExplanationFactor>> factor_storage) noexcept {
  const auto descriptor = definition.descriptor();
  const auto program = definition.program();
  const auto &factor_definition = program.factors[diagnostic_factor_index];
  const auto feature_factor =
      factor_definition.source == ExplanationSource::Feature;
  const auto scale = feature_factor && state.ratio ? state.ratio->scale
                     : !feature_factor && state.parameter
                         ? state.parameter->scale
                         : *contracts::DecimalScale::from_exponent(0);
  const auto observed = feature_factor && state.ratio ? state.ratio->units
                        : !feature_factor && state.parameter
                            ? state.parameter->units
                            : 0;
  std::size_t factor_count{};
  if (!append_factor(
          factor_storage, factor_count,
          {
              .factor_id = factor_definition.factor_id,
              .source = factor_definition.source,
              .role = ExplanationRole::ExplainsAbstention,
              .observed_units = observed,
              .observed_scale = scale,
              .signed_contribution_units = 0,
              .contribution_scale = scale,
              .causal_feature_evaluation_id =
                  feature_factor && state.feature
                      ? std::optional{state.feature->evaluation_id}
                      : std::nullopt,
              .ranking_policy_version = descriptor.explanation_policy_version,
          }))
    return failure(StrategyExecutionStatus::OutputCapacityExceeded, budget,
                   factor_storage);
  return {
      .status = StrategyExecutionStatus::Completed,
      .charged_operations = budget.consumed_operations(),
      .factor_count = factor_count,
      .terminal = AbstentionDraft{.reason = reason},
  };
}

} // namespace

std::optional<AcceptedStrategyInvocation> StrategyInvocationAuthority::accept(
    const AcceptedStrategyDefinition &definition,
    const core::features::AcceptedFeatureEvaluationCut &feature_cut)
    const noexcept {
  const auto invocation = view(config_, feature_cut);
  if (!matches_definition(definition.descriptor(), invocation) ||
      !validate_feature_cut(invocation))
    return std::nullopt;
  return AcceptedStrategyInvocation(config_, feature_cut);
}

StrategyHostResult StrategyHost::evaluate(
    const AcceptedStrategyDefinition &definition,
    const AcceptedStrategyInvocation &accepted_invocation,
    std::span<std::byte> workspace_storage,
    std::span<std::optional<ExplanationFactor>> factor_storage) noexcept {
  const auto descriptor = definition.descriptor();
  const auto program = definition.program();
  const auto invocation = view(accepted_invocation);
  DeterministicOperationBudget budget(invocation.maximum_operations);
  auto bounded_factors = factor_storage.first(
      std::min(factor_storage.size(), kThresholdProgramFactors));
  clear_factors(bounded_factors);

  if (!matches_definition(descriptor, invocation) ||
      invocation.feature_cut.evaluations().empty() ||
      invocation.feature_cut.evaluations().size() >
          kMaximumAcceptedFeatureEvaluations ||
      invocation.parameters.size() > 1)
    return failure(StrategyExecutionStatus::ContractViolation, budget,
                   bounded_factors);
  if (workspace_storage.size() < kInterpreterWorkingBytes)
    return failure(StrategyExecutionStatus::InsufficientWorkspace, budget,
                   bounded_factors);
  if (factor_storage.size() < kThresholdProgramFactors)
    return failure(StrategyExecutionStatus::OutputCapacityExceeded, budget,
                   bounded_factors);
  if (!budget.consume(kAdmissionOperations))
    return failure(StrategyExecutionStatus::DeterministicBudgetExhausted,
                   budget, bounded_factors);
  if (!validate_feature_cut(invocation) ||
      !validate_parameters(invocation.parameters))
    return failure(StrategyExecutionStatus::ContractViolation, budget,
                   bounded_factors);
  if (logical_deadline_exceeded(invocation.cut))
    return failure(StrategyExecutionStatus::LogicalDeadlineExceeded, budget,
                   bounded_factors);

  std::fill_n(workspace_storage.begin(), kInterpreterWorkingBytes, std::byte{});
  InterpreterState state;
  std::size_t factor_count{};

  for (const auto &instruction : program.instructions) {
    if (!budget.consume())
      return failure(StrategyExecutionStatus::DeterministicBudgetExhausted,
                     budget, bounded_factors);

    switch (instruction.opcode) {
    case StrategyOpcode::LoadFeature: {
      const auto &dependency =
          descriptor.required_features[instruction.operand];
      state.feature = find_feature(invocation, dependency.kind);
      if (!state.feature)
        return complete_abstention(
            definition, StrategyAbstentionReason::MissingDeclaredFeature, state,
            0, budget, bounded_factors);
      break;
    }
    case StrategyOpcode::RequireValidScaledRatio: {
      if (state.feature->disposition !=
          core::features::FeatureDisposition::ValidObservation)
        return complete_abstention(definition,
                                   StrategyAbstentionReason::NonValidFeature,
                                   state, 0, budget, bounded_factors);
      const auto &observation = *state.feature->observation;
      state.ratio =
          std::get_if<core::features::ScaledRatio>(&observation.value);
      const auto &dependency =
          descriptor.required_features[program.instructions[0].operand];
      if (!state.ratio ||
          observation.provenance.feature_definition_version !=
              dependency.definition_version ||
          state.ratio->units < -state.ratio->scale.denominator() ||
          state.ratio->units > state.ratio->scale.denominator())
        return complete_abstention(
            definition, StrategyAbstentionReason::IncompatibleFeature, state, 0,
            budget, bounded_factors);
      break;
    }
    case StrategyOpcode::LoadParameter: {
      const auto &schema = descriptor.parameter_schema[instruction.operand];
      state.parameter = find_parameter(invocation, schema);
      if (!state.parameter)
        return complete_abstention(definition,
                                   StrategyAbstentionReason::MissingParameter,
                                   state, 1, budget, bounded_factors);
      break;
    }
    case StrategyOpcode::RequirePositiveParameter: {
      const auto &schema =
          descriptor.parameter_schema[program.instructions[2].operand];
      if (state.parameter->definition_version != schema.definition_version ||
          state.parameter->scale != schema.scale ||
          state.parameter->scale != state.ratio->scale ||
          state.parameter->units <= 0 ||
          state.parameter->units > state.parameter->scale.denominator())
        return complete_abstention(definition,
                                   StrategyAbstentionReason::InvalidParameter,
                                   state, 1, budget, bounded_factors);
      break;
    }
    case StrategyOpcode::CompareAbsoluteFeatureAtLeastParameter:
      state.magnitude =
          state.ratio->units < 0 ? -state.ratio->units : state.ratio->units;
      state.emits_signal = state.magnitude >= state.parameter->units;
      break;
    case StrategyOpcode::AppendFeatureFactor: {
      const auto role =
          !state.emits_signal      ? ExplanationRole::ExplainsAbstention
          : state.ratio->units > 0 ? ExplanationRole::SupportsPositive
                                   : ExplanationRole::SupportsNegative;
      if (!append_factor(
              bounded_factors, factor_count,
              {
                  .factor_id = program.factors[instruction.operand].factor_id,
                  .source = ExplanationSource::Feature,
                  .role = role,
                  .observed_units = state.ratio->units,
                  .observed_scale = state.ratio->scale,
                  .signed_contribution_units = state.ratio->units,
                  .contribution_scale = state.ratio->scale,
                  .causal_feature_evaluation_id = state.feature->evaluation_id,
                  .ranking_policy_version =
                      descriptor.explanation_policy_version,
              }))
        return failure(StrategyExecutionStatus::OutputCapacityExceeded, budget,
                       bounded_factors);
      break;
    }
    case StrategyOpcode::AppendParameterFactor: {
      const auto margin = state.magnitude - state.parameter->units;
      const auto signed_margin =
          state.emits_signal && state.ratio->units < 0 ? -margin : margin;
      const auto role =
          !state.emits_signal      ? ExplanationRole::ExplainsAbstention
          : state.ratio->units > 0 ? ExplanationRole::SupportsPositive
                                   : ExplanationRole::SupportsNegative;
      if (!append_factor(
              bounded_factors, factor_count,
              {
                  .factor_id = program.factors[instruction.operand].factor_id,
                  .source = ExplanationSource::StrategyParameter,
                  .role = role,
                  .observed_units = state.parameter->units,
                  .observed_scale = state.parameter->scale,
                  .signed_contribution_units = signed_margin,
                  .contribution_scale = state.parameter->scale,
                  .ranking_policy_version =
                      descriptor.explanation_policy_version,
              }))
        return failure(StrategyExecutionStatus::OutputCapacityExceeded, budget,
                       bounded_factors);
      break;
    }
    case StrategyOpcode::FinishDirectionalThreshold:
      if (!state.emits_signal)
        return {
            .status = StrategyExecutionStatus::Completed,
            .charged_operations = budget.consumed_operations(),
            .factor_count = factor_count,
            .terminal =
                AbstentionDraft{
                    .reason = StrategyAbstentionReason::NoDirectionalSignal},
        };
      return {
          .status = StrategyExecutionStatus::Completed,
          .charged_operations = budget.consumed_operations(),
          .factor_count = factor_count,
          .terminal =
              SignalDraft{
                  .direction = state.ratio->units > 0
                                   ? StrategyDirection::Positive
                                   : StrategyDirection::Negative,
                  .strength = {.units = state.magnitude,
                               .scale = state.ratio->scale},
                  .horizon_nanoseconds = program.signal_horizon_nanoseconds,
              },
      };
    }
  }

  return failure(StrategyExecutionStatus::ContractViolation, budget,
                 bounded_factors);
}

} // namespace chronos::strategies::sdk
