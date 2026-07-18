#include "chronos/strategies/sdk/strategy.hpp"

#include "microtest.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace {
namespace contracts = chronos::contracts;
namespace features = chronos::core::features;
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

sdk::ExplanationFactor factor(std::uint32_t rank) {
  return {
      .factor_id = id<contracts::DefinitionId>(10),
      .rank = rank,
      .source = sdk::ExplanationSource::Feature,
      .role = sdk::ExplanationRole::SupportsPositive,
      .observed_units = 500000,
      .observed_scale = *contracts::DecimalScale::from_exponent(6),
      .signed_contribution_units = 500000,
      .contribution_scale = *contracts::DecimalScale::from_exponent(6),
      .causal_feature_evaluation_id = id<contracts::FeatureEvaluationId>(11),
      .ranking_policy_version = version(12),
  };
}

sdk::SignalDraft signal() {
  return {
      .direction = sdk::StrategyDirection::Positive,
      .strength = {.units = 500000,
                   .scale = *contracts::DecimalScale::from_exponent(6)},
      .horizon_nanoseconds = 1'000'000,
  };
}

sdk::StrategyDescriptor descriptor() {
  static const std::array dependencies = {
      sdk::FeatureDependency{
          .kind = features::FeatureKind::OrderBookImbalance,
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
  return {
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
              .maximum_operations = 16,
              .maximum_features = 1,
              .maximum_parameters = 1,
              .maximum_explanation_factors = 2,
              .maximum_working_bytes = 256,
          },
  };
}

static_assert(std::is_same_v<decltype(sdk::StrategyInvocation::features),
                             std::span<const features::FeatureEvaluation>>);
static_assert(std::is_same_v<decltype(sdk::StrategyInvocation::parameters),
                             std::span<const sdk::StrategyParameter>>);

} // namespace

TEST_CASE("strategy descriptors pin dependencies and bounded resources") {
  auto valid = descriptor();
  CHECK(sdk::validate_descriptor(valid));

  auto no_fuel = valid;
  no_fuel.resource_limits.maximum_operations = 0;
  CHECK(!sdk::validate_descriptor(no_fuel));

  auto too_many_features = valid;
  too_many_features.resource_limits.maximum_features = 0;
  CHECK(!sdk::validate_descriptor(too_many_features));

  const std::array duplicate_dependencies = {valid.required_features.front(),
                                             valid.required_features.front()};
  auto duplicate = valid;
  duplicate.required_features = duplicate_dependencies;
  duplicate.resource_limits.maximum_features = 2;
  CHECK(!sdk::validate_descriptor(duplicate));
}

TEST_CASE(
    "deterministic operation fuel is exact and non-consuming on failure") {
  sdk::DeterministicOperationBudget budget(5);
  CHECK(budget.consume(2));
  CHECK(budget.consumed_operations() == 2);
  CHECK(budget.remaining_operations() == 3);
  CHECK(!budget.exhausted());
  CHECK(!budget.consume(4));
  CHECK(budget.consumed_operations() == 2);
  CHECK(budget.consume(3));
  CHECK(budget.exhausted());
  CHECK(!budget.consume());

  sdk::DeterministicOperationBudget maximum(
      std::numeric_limits<std::uint64_t>::max());
  CHECK(maximum.consume(std::numeric_limits<std::uint64_t>::max()));
  CHECK(!maximum.consume());
}

TEST_CASE("logical deadlines depend only on the recorded cut") {
  const auto timer =
      contracts::StreamCursor::at_sequence(id<contracts::StreamId>(30), 1, 4);
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

TEST_CASE("bounded output enforces ranked factors before one terminal result") {
  std::array<std::optional<sdk::ExplanationFactor>, 2> storage;
  sdk::StrategyOutput output(storage);
  CHECK(!output.append_factor(factor(2)));
  CHECK(output.factor_count() == 0);
  CHECK(output.append_factor(factor(1)));
  CHECK(output.append_factor(factor(2)));
  CHECK(!output.append_factor(factor(3)));
  CHECK(output.factor_count() == 2);
  CHECK(output.factor(0).rank == 1);
  CHECK(output.factor(1).rank == 2);
  CHECK(output.emit_signal(signal()));
  CHECK(std::holds_alternative<sdk::SignalDraft>(*output.terminal()));
  CHECK(!output.emit_signal(signal()));
  CHECK(!output.abstain(
      {.reason = sdk::StrategyAbstentionReason::NonValidFeature}));
  CHECK(!output.append_factor(factor(3)));
}

TEST_CASE("abstention is terminal and cannot be encoded as a signal") {
  std::array<std::optional<sdk::ExplanationFactor>, 1> storage;
  sdk::StrategyOutput output(storage);
  CHECK(output.append_factor({
      .factor_id = id<contracts::DefinitionId>(40),
      .rank = 1,
      .source = sdk::ExplanationSource::DiagnosticStatus,
      .role = sdk::ExplanationRole::ExplainsAbstention,
      .observed_units = 1,
      .observed_scale = *contracts::DecimalScale::from_exponent(0),
      .signed_contribution_units = 0,
      .contribution_scale = *contracts::DecimalScale::from_exponent(0),
      .ranking_policy_version = version(41),
  }));
  CHECK(output.abstain(
      {.reason = sdk::StrategyAbstentionReason::NonValidFeature}));
  CHECK(std::holds_alternative<sdk::AbstentionDraft>(*output.terminal()));
  CHECK(!output.emit_signal(signal()));

  std::array<std::optional<sdk::ExplanationFactor>, 1> other_storage;
  sdk::StrategyOutput invalid_signal(other_storage);
  auto no_horizon = signal();
  no_horizon.horizon_nanoseconds = 0;
  CHECK(!invalid_signal.emit_signal(no_horizon));
  CHECK(!invalid_signal.terminal());
}
