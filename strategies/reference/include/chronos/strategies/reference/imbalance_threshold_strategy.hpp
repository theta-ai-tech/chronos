#pragma once

#include "chronos/strategies/sdk/strategy.hpp"

namespace chronos::strategies::reference {

class ImbalanceThresholdStrategy final : public sdk::Strategy {
public:
  static constexpr std::uint64_t kOperationsPerEvaluation = 12;
  static constexpr std::int64_t kSignalHorizonNanoseconds = 1'000'000'000;

  [[nodiscard]] static contracts::VersionRef definition_version();
  [[nodiscard]] static contracts::VersionRef implementation_version();
  [[nodiscard]] static contracts::VersionRef
  imbalance_feature_definition_version();
  [[nodiscard]] static contracts::DefinitionId threshold_parameter_id();
  [[nodiscard]] static contracts::VersionRef threshold_parameter_version();
  [[nodiscard]] static contracts::DefinitionId imbalance_factor_id();
  [[nodiscard]] static contracts::DefinitionId threshold_factor_id();

  [[nodiscard]] const sdk::StrategyDescriptor &
  descriptor() const noexcept override;
  [[nodiscard]] sdk::StrategyExecutionStatus
  evaluate(const sdk::StrategyInvocation &invocation,
           sdk::DeterministicOperationBudget &budget,
           sdk::StrategyOutput &output) const noexcept override;
};

} // namespace chronos::strategies::reference
