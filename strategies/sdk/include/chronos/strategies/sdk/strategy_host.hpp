#pragma once

#include "chronos/core/features/feature_runtime.hpp"
#include "chronos/strategies/sdk/strategy.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>

namespace chronos::strategies::sdk {

struct StrategyInvocationAuthorityConfig final {
  contracts::RunId run_id;
  contracts::StrategyInstanceId strategy_instance_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::VersionRef strategy_definition_version;
  contracts::VersionRef strategy_implementation_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef explanation_policy_version;
  std::uint64_t maximum_operations{};
  std::optional<StrategyParameter> parameter;
  LogicalCut cut;
};

class AcceptedStrategyInvocation final {
public:
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::StrategyInstanceId
  strategy_instance_id() const noexcept {
    return strategy_instance_id_;
  }
  [[nodiscard]] contracts::ListingId listing_id() const noexcept {
    return listing_id_;
  }
  [[nodiscard]] contracts::CanonicalInstrumentId
  canonical_instrument_id() const noexcept {
    return canonical_instrument_id_;
  }
  [[nodiscard]] contracts::VersionRef
  strategy_definition_version() const noexcept {
    return strategy_definition_version_;
  }
  [[nodiscard]] contracts::VersionRef
  strategy_implementation_version() const noexcept {
    return strategy_implementation_version_;
  }
  [[nodiscard]] contracts::VersionRef arithmetic_version() const noexcept {
    return arithmetic_version_;
  }
  [[nodiscard]] contracts::VersionRef
  explanation_policy_version() const noexcept {
    return explanation_policy_version_;
  }
  [[nodiscard]] std::uint64_t maximum_operations() const noexcept {
    return maximum_operations_;
  }
  [[nodiscard]] const core::features::AcceptedFeatureEvaluationCut &
  feature_cut() const noexcept {
    return feature_cut_;
  }
  [[nodiscard]] std::span<const StrategyParameter> parameters() const noexcept {
    return parameter_ ? std::span<const StrategyParameter>(&*parameter_, 1)
                      : std::span<const StrategyParameter>{};
  }
  [[nodiscard]] const LogicalCut &cut() const noexcept { return cut_; }

private:
  AcceptedStrategyInvocation(
      const StrategyInvocationAuthorityConfig &config,
      const core::features::AcceptedFeatureEvaluationCut &feature_cut) noexcept
      : run_id_(config.run_id),
        strategy_instance_id_(config.strategy_instance_id),
        listing_id_(config.listing_id),
        canonical_instrument_id_(config.canonical_instrument_id),
        strategy_definition_version_(config.strategy_definition_version),
        strategy_implementation_version_(
            config.strategy_implementation_version),
        arithmetic_version_(config.arithmetic_version),
        explanation_policy_version_(config.explanation_policy_version),
        maximum_operations_(config.maximum_operations),
        feature_cut_(feature_cut), parameter_(config.parameter),
        cut_(config.cut) {}

  contracts::RunId run_id_;
  contracts::StrategyInstanceId strategy_instance_id_;
  contracts::ListingId listing_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::VersionRef strategy_definition_version_;
  contracts::VersionRef strategy_implementation_version_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef explanation_policy_version_;
  std::uint64_t maximum_operations_{};
  core::features::AcceptedFeatureEvaluationCut feature_cut_;
  std::optional<StrategyParameter> parameter_;
  LogicalCut cut_;

  friend class StrategyInvocationAuthority;
};

class StrategyInvocationAuthority final {
public:
  [[nodiscard]] static StrategyInvocationAuthority
  from(StrategyInvocationAuthorityConfig config) noexcept {
    return StrategyInvocationAuthority(std::move(config));
  }

  [[nodiscard]] std::optional<AcceptedStrategyInvocation>
  accept(const AcceptedStrategyDefinition &definition,
         const core::features::AcceptedFeatureEvaluationCut &feature_cut)
      const noexcept;

private:
  explicit StrategyInvocationAuthority(
      StrategyInvocationAuthorityConfig config) noexcept
      : config_(std::move(config)) {}

  StrategyInvocationAuthorityConfig config_;
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
  evaluate(const AcceptedStrategyDefinition &definition,
           const AcceptedStrategyInvocation &invocation,
           std::span<std::byte> workspace_storage,
           std::span<std::optional<ExplanationFactor>> factor_storage) noexcept;
};

} // namespace chronos::strategies::sdk
