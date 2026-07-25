#include "chronos/strategies/conformance/threshold_pack.hpp"

#include <array>

namespace chronos::strategies::conformance {
namespace {

template <typename Id> Id id(std::uint8_t seed) noexcept {
  typename Id::bytes_type bytes{};
  bytes.front() = seed;
  return *Id::from_bytes(bytes);
}

contracts::VersionRef version(std::uint8_t seed) noexcept {
  return *contracts::VersionRef::from(id<contracts::DefinitionId>(seed), 1);
}

} // namespace

std::optional<sdk::AcceptedStrategyDefinition>
accepted_threshold_definition() noexcept {
  static const std::array dependencies = {
      sdk::FeatureDependency{
          .kind = sdk::StrategyFeatureKind::OrderBookImbalance,
          .definition_version = version(1),
      },
  };
  static const std::array parameters = {
      sdk::StrategyParameterSchema{
          .parameter_id = id<contracts::DefinitionId>(2),
          .definition_version = version(3),
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
          .factor_id = id<contracts::DefinitionId>(4),
          .source = sdk::ExplanationSource::Feature,
      },
      sdk::ProgramFactorDefinition{
          .factor_id = id<contracts::DefinitionId>(5),
          .source = sdk::ExplanationSource::StrategyParameter,
      },
  };
  return sdk::AcceptedStrategyDefinition::accept({
      .descriptor =
          {
              .definition_version = version(6),
              .implementation_version = version(7),
              .required_features = dependencies,
              .parameter_schema = parameters,
              .arithmetic_version = version(8),
              .explanation_policy_version = version(9),
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
  });
}

} // namespace chronos::strategies::conformance
