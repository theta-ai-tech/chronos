#include "chronos/strategies/sdk/strategy.hpp"

#include <algorithm>

namespace chronos::strategies::sdk {

bool DeterministicOperationBudget::consume(std::uint64_t operations) noexcept {
  if (operations > remaining_operations())
    return false;
  consumed_operations_ += operations;
  return true;
}

bool validate_descriptor(const StrategyDescriptor &descriptor) noexcept {
  const auto &limits = descriptor.resource_limits;
  if (descriptor.required_features.empty() ||
      descriptor.required_features.size() > limits.maximum_features ||
      descriptor.parameter_schema.size() > limits.maximum_parameters ||
      limits.maximum_operations == 0 || limits.maximum_features == 0 ||
      limits.maximum_explanation_factors == 0 ||
      limits.maximum_working_bytes < kInterpreterWorkingBytes)
    return false;

  const auto duplicate_feature = std::any_of(
      descriptor.required_features.begin(), descriptor.required_features.end(),
      [&](const auto &current) {
        return std::count_if(descriptor.required_features.begin(),
                             descriptor.required_features.end(),
                             [&](const auto &candidate) {
                               return candidate.kind == current.kind;
                             }) != 1;
      });
  if (duplicate_feature)
    return false;

  return std::none_of(
      descriptor.parameter_schema.begin(), descriptor.parameter_schema.end(),
      [&](const auto &current) {
        return std::count_if(descriptor.parameter_schema.begin(),
                             descriptor.parameter_schema.end(),
                             [&](const auto &candidate) {
                               return candidate.parameter_id ==
                                      current.parameter_id;
                             }) != 1;
      });
}

bool validate_definition(const StrategyDefinition &definition) noexcept {
  if (!validate_descriptor(definition.descriptor) ||
      definition.program.instructions.size() != 8 ||
      definition.program.instructions.size() > kMaximumProgramInstructions ||
      definition.program.factors.size() != 2 ||
      definition.program.factors.size() > kMaximumProgramFactors ||
      definition.program.signal_horizon_nanoseconds <= 0 ||
      definition.descriptor.resource_limits.maximum_operations <
          definition.program.instructions.size() ||
      definition.descriptor.resource_limits.maximum_explanation_factors < 2)
    return false;

  constexpr StrategyOpcode expected[] = {
      StrategyOpcode::LoadFeature,
      StrategyOpcode::RequireValidScaledRatio,
      StrategyOpcode::LoadParameter,
      StrategyOpcode::RequirePositiveParameter,
      StrategyOpcode::CompareAbsoluteFeatureAtLeastParameter,
      StrategyOpcode::AppendFeatureFactor,
      StrategyOpcode::AppendParameterFactor,
      StrategyOpcode::FinishDirectionalThreshold,
  };
  for (std::size_t index = 0; index < definition.program.instructions.size();
       ++index) {
    if (definition.program.instructions[index].opcode != expected[index])
      return false;
  }

  const auto &instructions = definition.program.instructions;
  return instructions[0].operand <
             definition.descriptor.required_features.size() &&
         instructions[2].operand <
             definition.descriptor.parameter_schema.size() &&
         instructions[5].operand < definition.program.factors.size() &&
         instructions[6].operand < definition.program.factors.size() &&
         definition.program.factors[instructions[5].operand].source ==
             ExplanationSource::Feature &&
         definition.program.factors[instructions[6].operand].source ==
             ExplanationSource::StrategyParameter;
}

bool logical_deadline_exceeded(const LogicalCut &cut) noexcept {
  return cut.logical_deadline_nanoseconds &&
         cut.logical_time_nanoseconds > *cut.logical_deadline_nanoseconds;
}

} // namespace chronos::strategies::sdk
