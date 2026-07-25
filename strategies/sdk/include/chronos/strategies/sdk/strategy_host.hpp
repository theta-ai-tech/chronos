#pragma once

#include "chronos/core/features/feature_runtime.hpp"
#include "chronos/strategies/sdk/strategy.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace chronos::strategies::sdk {

struct StrategyInvocationRequest final {
  contracts::RunId run_id;
  contracts::StrategyInstanceId strategy_instance_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  const core::features::AcceptedFeatureEvaluationCut &feature_cut;
  std::span<const StrategyParameter> parameters;
  LogicalCut cut;
};

struct StrategyHostResult final {
  StrategyExecutionStatus status{StrategyExecutionStatus::ContractViolation};
  std::uint64_t charged_operations{};
  std::size_t factor_count{};
  std::optional<TerminalDraft> terminal;

  [[nodiscard]] bool completed() const noexcept {
    return status == StrategyExecutionStatus::Completed && terminal.has_value();
  }
};

class StrategyHost final {
public:
  [[nodiscard]] static StrategyHostResult
  evaluate(const Strategy &strategy, const StrategyInvocationRequest &request,
           DeterministicOperationBudget &budget,
           std::span<std::byte> workspace_storage,
           std::span<std::optional<ExplanationFactor>> factor_storage);
};

} // namespace chronos::strategies::sdk
