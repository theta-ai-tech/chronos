#pragma once

#include "chronos/core/features/feature_runtime.hpp"
#include "chronos/strategies/sdk/strategy.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>

namespace chronos::runtime::strategies {
class StrategyRuntime;
}

namespace chronos::strategies::sdk {

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
  [[nodiscard]] contracts::Sha256Digest definition_digest() const noexcept {
    return definition_digest_;
  }
  [[nodiscard]] contracts::EventId
  activation_control_outcome_id() const noexcept {
    return activation_control_outcome_id_;
  }
  [[nodiscard]] contracts::Sha256Digest activation_checksum() const noexcept {
    return activation_checksum_;
  }
  [[nodiscard]] contracts::StreamCursor run_control_cursor() const noexcept {
    return run_control_cursor_;
  }
  [[nodiscard]] contracts::Sha256Digest
  selection_semantic_checksum() const noexcept {
    return selection_semantic_checksum_;
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
      contracts::RunId run_id,
      contracts::StrategyInstanceId strategy_instance_id,
      contracts::ListingId listing_id,
      contracts::CanonicalInstrumentId canonical_instrument_id,
      contracts::VersionRef strategy_definition_version,
      contracts::VersionRef strategy_implementation_version,
      contracts::VersionRef arithmetic_version,
      contracts::VersionRef explanation_policy_version,
      contracts::Sha256Digest definition_digest,
      contracts::EventId activation_control_outcome_id,
      contracts::Sha256Digest activation_checksum,
      contracts::StreamCursor run_control_cursor,
      contracts::Sha256Digest selection_semantic_checksum,
      std::uint64_t maximum_operations,
      core::features::AcceptedFeatureEvaluationCut feature_cut,
      std::optional<StrategyParameter> parameter, LogicalCut cut) noexcept
      : run_id_(run_id), strategy_instance_id_(strategy_instance_id),
        listing_id_(listing_id),
        canonical_instrument_id_(canonical_instrument_id),
        strategy_definition_version_(strategy_definition_version),
        strategy_implementation_version_(strategy_implementation_version),
        arithmetic_version_(arithmetic_version),
        explanation_policy_version_(explanation_policy_version),
        definition_digest_(definition_digest),
        activation_control_outcome_id_(activation_control_outcome_id),
        activation_checksum_(activation_checksum),
        run_control_cursor_(run_control_cursor),
        selection_semantic_checksum_(selection_semantic_checksum),
        maximum_operations_(maximum_operations),
        feature_cut_(std::move(feature_cut)), parameter_(std::move(parameter)),
        cut_(cut) {}

  contracts::RunId run_id_;
  contracts::StrategyInstanceId strategy_instance_id_;
  contracts::ListingId listing_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::VersionRef strategy_definition_version_;
  contracts::VersionRef strategy_implementation_version_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef explanation_policy_version_;
  contracts::Sha256Digest definition_digest_;
  contracts::EventId activation_control_outcome_id_;
  contracts::Sha256Digest activation_checksum_;
  contracts::StreamCursor run_control_cursor_;
  contracts::Sha256Digest selection_semantic_checksum_;
  std::uint64_t maximum_operations_{};
  core::features::AcceptedFeatureEvaluationCut feature_cut_;
  std::optional<StrategyParameter> parameter_;
  LogicalCut cut_;

  friend class ::chronos::runtime::strategies::StrategyRuntime;
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
