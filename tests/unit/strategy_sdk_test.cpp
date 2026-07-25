#include "chronos/strategies/sdk/strategy_host.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
namespace contracts = chronos::contracts;
namespace sdk = chronos::strategies::sdk;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number = 1) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
}

sdk::StrategyDefinition definition() {
  static const std::array dependencies = {
      sdk::FeatureDependency{
          .kind = sdk::StrategyFeatureKind::OrderBookImbalance,
          .definition_version = version(20),
      },
  };
  static const std::array parameters = {
      sdk::StrategyParameterSchema{
          .parameter_id = id<contracts::DefinitionId>(21),
          .definition_version = version(22),
          .scale = *contracts::DecimalScale::from_exponent(6),
      },
  };
  static const std::array instructions = {
      sdk::StrategyInstruction{.opcode = sdk::StrategyOpcode::LoadFeature,
                               .operand = 0},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::RequireValidScaledRatio},
      sdk::StrategyInstruction{.opcode = sdk::StrategyOpcode::LoadParameter,
                               .operand = 0},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::RequirePositiveParameter},
      sdk::StrategyInstruction{
          .opcode =
              sdk::StrategyOpcode::CompareAbsoluteFeatureAtLeastParameter},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::AppendFeatureFactor, .operand = 0},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::AppendParameterFactor, .operand = 1},
      sdk::StrategyInstruction{
          .opcode = sdk::StrategyOpcode::FinishDirectionalThreshold},
  };
  static const std::array factors = {
      sdk::ProgramFactorDefinition{
          .factor_id = id<contracts::DefinitionId>(30),
          .source = sdk::ExplanationSource::Feature,
      },
      sdk::ProgramFactorDefinition{
          .factor_id = id<contracts::DefinitionId>(31),
          .source = sdk::ExplanationSource::StrategyParameter,
      },
  };
  return {
      .descriptor =
          {
              .definition_version = version(23),
              .implementation_version = version(24),
              .scope = sdk::StrategyScope::SingleListing,
              .family = sdk::StrategyFamily::OrderBookImbalance,
              .required_features = dependencies,
              .parameter_schema = parameters,
              .arithmetic_version = version(25),
              .explanation_policy_version = version(26),
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

} // namespace

TEST_CASE("strategy definitions pin one bounded loop-free program") {
  const auto valid = definition();
  CHECK(sdk::validate_descriptor(valid.descriptor));
  CHECK(sdk::validate_definition(valid));
  CHECK(sdk::AcceptedStrategyDefinition::accept(valid).has_value());

  auto no_fuel = valid;
  no_fuel.descriptor.resource_limits.maximum_operations = 0;
  CHECK(!sdk::validate_definition(no_fuel));

  auto no_workspace = valid;
  no_workspace.descriptor.resource_limits.maximum_working_bytes =
      sdk::kInterpreterWorkingBytes - 1;
  CHECK(!sdk::validate_definition(no_workspace));

  std::array<sdk::StrategyInstruction, 8> changed_instructions;
  std::copy(valid.program.instructions.begin(),
            valid.program.instructions.end(), changed_instructions.begin());
  changed_instructions[7].opcode = sdk::StrategyOpcode::LoadFeature;
  auto loop_or_malformed = valid;
  loop_or_malformed.program.instructions = changed_instructions;
  CHECK(!sdk::validate_definition(loop_or_malformed));

  auto invalid_factor = valid;
  std::array changed_factors = {valid.program.factors[0],
                                valid.program.factors[1]};
  changed_factors[0].source = sdk::ExplanationSource::StrategyParameter;
  invalid_factor.program.factors = changed_factors;
  CHECK(!sdk::validate_definition(invalid_factor));

  auto malformed_operand = valid;
  std::array<sdk::StrategyInstruction, sdk::kThresholdProgramInstructions>
      malformed_operand_instructions;
  std::copy(valid.program.instructions.begin(),
            valid.program.instructions.end(),
            malformed_operand_instructions.begin());
  malformed_operand_instructions[0].operand = 1;
  malformed_operand.program.instructions = malformed_operand_instructions;
  CHECK(!sdk::AcceptedStrategyDefinition::accept(malformed_operand));

  auto malformed_opcode = valid;
  std::array<sdk::StrategyInstruction, sdk::kThresholdProgramInstructions>
      malformed_opcode_instructions;
  std::copy(valid.program.instructions.begin(),
            valid.program.instructions.end(),
            malformed_opcode_instructions.begin());
  malformed_opcode_instructions[3].opcode =
      static_cast<sdk::StrategyOpcode>(255);
  malformed_opcode.program.instructions = malformed_opcode_instructions;
  CHECK(!sdk::AcceptedStrategyDefinition::accept(malformed_opcode));
}

TEST_CASE("strategy admission enforces one dependency and one parameter") {
  const auto valid = definition();
  const std::array dependencies = {valid.descriptor.required_features[0],
                                   valid.descriptor.required_features[0]};
  auto extra_dependency = valid;
  extra_dependency.descriptor.required_features = dependencies;
  extra_dependency.descriptor.resource_limits.maximum_features = 2;
  CHECK(!sdk::validate_descriptor(extra_dependency.descriptor));
  CHECK(!sdk::AcceptedStrategyDefinition::accept(extra_dependency));

  const std::array parameters = {valid.descriptor.parameter_schema[0],
                                 valid.descriptor.parameter_schema[0]};
  auto extra_parameter = valid;
  extra_parameter.descriptor.parameter_schema = parameters;
  extra_parameter.descriptor.resource_limits.maximum_parameters = 2;
  CHECK(!sdk::validate_descriptor(extra_parameter.descriptor));
  CHECK(!sdk::AcceptedStrategyDefinition::accept(extra_parameter));

  auto oversized_limit = valid;
  oversized_limit.descriptor.resource_limits.maximum_working_bytes =
      sdk::kInterpreterWorkingBytes + 1;
  CHECK(!sdk::validate_descriptor(oversized_limit.descriptor));
}

TEST_CASE("accepted strategy definitions own immutable fixed storage") {
  const auto valid = definition();
  std::array dependencies = {valid.descriptor.required_features[0]};
  std::array parameters = {valid.descriptor.parameter_schema[0]};
  std::array<sdk::StrategyInstruction, sdk::kThresholdProgramInstructions>
      instructions;
  std::copy(valid.program.instructions.begin(),
            valid.program.instructions.end(), instructions.begin());
  std::array factors = {valid.program.factors[0], valid.program.factors[1]};

  auto candidate = valid;
  candidate.descriptor.required_features = dependencies;
  candidate.descriptor.parameter_schema = parameters;
  candidate.program.instructions = instructions;
  candidate.program.factors = factors;
  const auto accepted = sdk::AcceptedStrategyDefinition::accept(candidate);
  CHECK(accepted.has_value());

  const auto accepted_dependency = accepted->descriptor().required_features[0];
  const auto accepted_parameter = accepted->descriptor().parameter_schema[0];
  const auto accepted_instruction = accepted->program().instructions[0];
  const auto accepted_factor = accepted->program().factors[0];
  dependencies[0].definition_version = version(70);
  parameters[0].definition_version = version(71);
  instructions[0].operand = 1;
  factors[0].source = sdk::ExplanationSource::DiagnosticStatus;

  CHECK(accepted->descriptor().required_features[0] == accepted_dependency);
  CHECK(accepted->descriptor().parameter_schema[0] == accepted_parameter);
  CHECK(accepted->program().instructions[0] == accepted_instruction);
  CHECK(accepted->program().factors[0] == accepted_factor);
}

static_assert(noexcept(sdk::StrategyHost::evaluate(
    std::declval<const sdk::AcceptedStrategyDefinition &>(),
    std::declval<const sdk::StrategyInvocationRequest &>(),
    std::declval<sdk::DeterministicOperationBudget &>(),
    std::declval<std::span<std::byte>>(),
    std::declval<std::span<std::optional<sdk::ExplanationFactor>>>())));

TEST_CASE("deterministic interpreter fuel is exact and overflow safe") {
  sdk::DeterministicOperationBudget budget(5);
  CHECK(budget.consume(2));
  CHECK(budget.consumed_operations() == 2);
  CHECK(budget.remaining_operations() == 3);
  CHECK(!budget.consume(4));
  CHECK(budget.consumed_operations() == 2);
  CHECK(budget.consume(3));
  CHECK(!budget.consume());

  sdk::DeterministicOperationBudget maximum(
      std::numeric_limits<std::uint64_t>::max());
  CHECK(maximum.consume(std::numeric_limits<std::uint64_t>::max()));
  CHECK(!maximum.consume());
}

TEST_CASE("logical deadlines depend only on the recorded cut") {
  const auto timer =
      contracts::StreamCursor::at_sequence(id<contracts::StreamId>(40), 1, 4);
  const auto cut = sdk::LogicalCut{
      .run_input_sequence = 9,
      .logical_time_nanoseconds = 100,
      .run_timer_cursor = *timer,
      .configuration_epoch = 2,
      .effective_control_position = 8,
      .logical_deadline_nanoseconds = 100,
  };
  CHECK(!sdk::logical_deadline_exceeded(cut));
  auto late = cut;
  late.logical_time_nanoseconds = 101;
  CHECK(sdk::logical_deadline_exceeded(late));
  auto unbounded = late;
  unbounded.logical_deadline_nanoseconds.reset();
  CHECK(!sdk::logical_deadline_exceeded(unbounded));
}
