#include "chronos/core/recommendation/recommendation.hpp"
#include "chronos/runtime/strategies/strategy_evaluation.hpp"

#include "microtest.hpp"

namespace {

template <typename T>
concept CreatesOrderDirectly = requires(T value) { value.create_order(); } ||
                               requires(T value) { value.mutate_order(); };

template <typename T>
concept CarriesExecutionAuthority =
    requires(T value) { value.order_id(); } || requires(T value) {
      value.risk_decision_id();
    } || requires(T value) { value.reservation_id(); };

static_assert(
    !CreatesOrderDirectly<chronos::runtime::strategies::StrategySignal>);
static_assert(
    !CreatesOrderDirectly<chronos::runtime::strategies::StrategyEvaluation>);
static_assert(
    !CreatesOrderDirectly<chronos::core::recommendation::TradeRecommendation>);
static_assert(
    !CarriesExecutionAuthority<chronos::runtime::strategies::StrategySignal>);
static_assert(!CarriesExecutionAuthority<
              chronos::runtime::strategies::StrategyEvaluation>);
static_assert(!CarriesExecutionAuthority<
              chronos::core::recommendation::TradeRecommendation>);
static_assert(
    !chronos::core::features::kDiagnosticFeatureObservationsActivated);
static_assert(
    !chronos::runtime::strategies::kExternalStrategyObservationsActivated);

TEST_CASE("M5 outputs expose no direct order or risk authority") {
  CHECK(!CreatesOrderDirectly<chronos::runtime::strategies::StrategySignal>);
  CHECK(
      !CreatesOrderDirectly<chronos::runtime::strategies::StrategyEvaluation>);
  CHECK(!CreatesOrderDirectly<
        chronos::core::recommendation::TradeRecommendation>);
  CHECK(!CarriesExecutionAuthority<
        chronos::runtime::strategies::StrategyEvaluation>);
  CHECK(!CarriesExecutionAuthority<
        chronos::core::recommendation::TradeRecommendation>);
}

TEST_CASE("deferred M5 observation capabilities remain unactivated") {
  CHECK(!chronos::core::features::kDiagnosticFeatureObservationsActivated);
  CHECK(!chronos::runtime::strategies::kExternalStrategyObservationsActivated);
}

} // namespace
