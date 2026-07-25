#include "chronos/strategies/sdk/strategy.hpp"

namespace chronos::strategies::sdk {

bool DeterministicOperationBudget::consume(std::uint64_t operations) noexcept {
  if (operations > remaining_operations())
    return false;
  consumed_operations_ += operations;
  return true;
}

bool validate_descriptor(const StrategyDescriptor &descriptor) noexcept {
  const auto &limits = descriptor.resource_limits;
  return descriptor.required_features.size() == 1 &&
         descriptor.parameter_schema.size() == 1 &&
         limits.maximum_operations == kMaximumEvaluationOperations &&
         limits.maximum_features == 1 && limits.maximum_parameters == 1 &&
         limits.maximum_explanation_factors == kThresholdProgramFactors &&
         limits.maximum_working_bytes == kInterpreterWorkingBytes;
}

bool validate_definition(const StrategyDefinition &definition) noexcept {
  if (definition.program.instructions.size() != kThresholdProgramInstructions ||
      definition.program.factors.size() != kThresholdProgramFactors ||
      definition.program.signal_horizon_nanoseconds <= 0 ||
      !validate_descriptor(definition.descriptor))
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
  constexpr std::uint16_t expected_operands[] = {0, 0, 0, 0, 0, 0, 1, 0};
  for (std::size_t index = 0; index < definition.program.instructions.size();
       ++index) {
    if (definition.program.instructions[index].opcode != expected[index] ||
        definition.program.instructions[index].operand !=
            expected_operands[index])
      return false;
  }

  return definition.program.factors[0].source == ExplanationSource::Feature &&
         definition.program.factors[1].source ==
             ExplanationSource::StrategyParameter;
}

std::optional<AcceptedStrategyDefinition> AcceptedStrategyDefinition::accept(
    const StrategyDefinition &candidate) noexcept {
  if (!validate_definition(candidate))
    return std::nullopt;
  const std::array<StrategyInstruction, kThresholdProgramInstructions>
      instructions = {
          candidate.program.instructions[0], candidate.program.instructions[1],
          candidate.program.instructions[2], candidate.program.instructions[3],
          candidate.program.instructions[4], candidate.program.instructions[5],
          candidate.program.instructions[6], candidate.program.instructions[7],
      };
  const std::array<ProgramFactorDefinition, kThresholdProgramFactors> factors =
      {candidate.program.factors[0], candidate.program.factors[1]};
  return AcceptedStrategyDefinition(
      candidate.descriptor, candidate.descriptor.required_features[0],
      candidate.descriptor.parameter_schema[0], instructions, factors,
      candidate.program.signal_horizon_nanoseconds);
}

bool logical_deadline_exceeded(const LogicalCut &cut) noexcept {
  return cut.logical_deadline_nanoseconds &&
         cut.logical_time_nanoseconds > *cut.logical_deadline_nanoseconds;
}

} // namespace chronos::strategies::sdk
