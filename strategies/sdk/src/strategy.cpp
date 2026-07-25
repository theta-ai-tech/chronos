#include "chronos/strategies/sdk/strategy.hpp"

#include <array>
#include <string_view>
#include <type_traits>

namespace chronos::strategies::sdk {
namespace {

struct CanonicalDefinition final {
  std::array<std::byte, 512> bytes{};
  std::size_t size{};

  void append(std::string_view value) noexcept {
    for (const auto character : value)
      bytes[size++] = static_cast<std::byte>(character);
  }

  template <typename Id> void append_id(const Id &value) noexcept {
    for (const auto byte : value.bytes())
      bytes[size++] = static_cast<std::byte>(byte);
  }

  template <typename Integer> void append_integer(Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto converted = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
      const auto shift = (sizeof(Integer) - index - 1) * 8U;
      bytes[size++] =
          static_cast<std::byte>((converted >> shift) & Unsigned{0xFF});
    }
  }

  template <typename Enum> void append_enum(Enum value) noexcept {
    append_integer(static_cast<std::underlying_type_t<Enum>>(value));
  }

  void append_version(const contracts::VersionRef &value) noexcept {
    append_id(value.definition_id());
    append_integer(value.version());
  }
};

contracts::Sha256Digest
derive_definition_digest(const StrategyDefinition &definition) noexcept {
  CanonicalDefinition canonical;
  canonical.append("chronos.strategy-definition.v1");
  const auto &descriptor = definition.descriptor;
  canonical.append_version(descriptor.definition_version);
  canonical.append_version(descriptor.implementation_version);
  canonical.append_enum(descriptor.scope);
  canonical.append_enum(descriptor.family);
  canonical.append_enum(descriptor.required_features[0].kind);
  canonical.append_version(descriptor.required_features[0].definition_version);
  canonical.append_id(descriptor.parameter_schema[0].parameter_id);
  canonical.append_version(descriptor.parameter_schema[0].definition_version);
  canonical.append_integer(descriptor.parameter_schema[0].scale.exponent());
  canonical.append_version(descriptor.arithmetic_version);
  canonical.append_version(descriptor.explanation_policy_version);
  canonical.append_integer(descriptor.resource_limits.maximum_operations);
  canonical.append_integer(
      static_cast<std::uint64_t>(descriptor.resource_limits.maximum_features));
  canonical.append_integer(static_cast<std::uint64_t>(
      descriptor.resource_limits.maximum_parameters));
  canonical.append_integer(static_cast<std::uint64_t>(
      descriptor.resource_limits.maximum_explanation_factors));
  canonical.append_integer(static_cast<std::uint64_t>(
      descriptor.resource_limits.maximum_working_bytes));
  for (const auto &instruction : definition.program.instructions) {
    canonical.append_enum(instruction.opcode);
    canonical.append_integer(instruction.operand);
  }
  for (const auto &factor : definition.program.factors) {
    canonical.append_id(factor.factor_id);
    canonical.append_enum(factor.source);
  }
  canonical.append_integer(definition.program.signal_horizon_nanoseconds);
  return contracts::sha256(
      std::span<const std::byte>(canonical.bytes).first(canonical.size));
}

} // namespace

bool DeterministicOperationBudget::consume(std::uint64_t operations) noexcept {
  if (operations > remaining_operations())
    return false;
  consumed_operations_ += operations;
  return true;
}

bool validate_descriptor(const StrategyDescriptor &descriptor) noexcept {
  const auto &limits = descriptor.resource_limits;
  return descriptor.scope == StrategyScope::SingleListing &&
         descriptor.family == StrategyFamily::OrderBookImbalance &&
         descriptor.required_features.size() == 1 &&
         descriptor.required_features[0].kind ==
             StrategyFeatureKind::OrderBookImbalance &&
         descriptor.parameter_schema.size() == 1 &&
         descriptor.parameter_schema[0].scale.denominator() == 1'000'000 &&
         limits.maximum_operations == kMaximumEvaluationOperations &&
         limits.maximum_features == 1 && limits.maximum_parameters == 1 &&
         limits.maximum_explanation_factors == kThresholdProgramFactors &&
         limits.maximum_working_bytes == kInterpreterWorkingBytes;
}

bool validate_definition(const StrategyDefinition &definition) noexcept {
  if (definition.program.instructions.size() != kThresholdProgramInstructions ||
      definition.program.factors.size() != kThresholdProgramFactors ||
      definition.program.signal_horizon_nanoseconds <= 0 ||
      definition.program.signal_horizon_nanoseconds >
          kMaximumSignalHorizonNanoseconds ||
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

  return definition.program.factors[0].factor_id !=
             definition.program.factors[1].factor_id &&
         definition.program.factors[0].source == ExplanationSource::Feature &&
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
      candidate.descriptor, derive_definition_digest(candidate),
      candidate.descriptor.required_features[0],
      candidate.descriptor.parameter_schema[0], instructions, factors,
      candidate.program.signal_horizon_nanoseconds);
}

bool logical_deadline_exceeded(const LogicalCut &cut) noexcept {
  return cut.logical_deadline_nanoseconds &&
         cut.logical_time_nanoseconds > *cut.logical_deadline_nanoseconds;
}

} // namespace chronos::strategies::sdk
