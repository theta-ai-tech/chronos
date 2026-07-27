#pragma once

#include "chronos/runtime/strategies/strategy_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace chronos::runtime::strategies {

inline constexpr bool kExternalStrategyObservationsActivated = false;

enum class StrategyEvaluationFailure : std::uint8_t {
  None,
  ContractViolation,
  OperationalInterruption,
  InvalidTerminal,
};

class StrategySignal final {
public:
  [[nodiscard]] contracts::StrategySignalId signal_id() const noexcept {
    return signal_id_;
  }
  [[nodiscard]] contracts::StrategyEvaluationId evaluation_id() const noexcept {
    return evaluation_id_;
  }
  [[nodiscard]] const chronos::strategies::sdk::SignalDraft &
  draft() const noexcept {
    return draft_;
  }

  bool operator==(const StrategySignal &) const = default;

private:
  StrategySignal(contracts::StrategySignalId signal_id,
                 contracts::StrategyEvaluationId evaluation_id,
                 chronos::strategies::sdk::SignalDraft draft) noexcept
      : signal_id_(signal_id), evaluation_id_(evaluation_id),
        draft_(std::move(draft)) {}

  contracts::StrategySignalId signal_id_;
  contracts::StrategyEvaluationId evaluation_id_;
  chronos::strategies::sdk::SignalDraft draft_;

  friend class StrategyEvaluationAuthority;
};

struct StrategyAbstention final {
  chronos::strategies::sdk::StrategyAbstentionReason reason{
      chronos::strategies::sdk::StrategyAbstentionReason::
          MissingDeclaredFeature};

  bool operator==(const StrategyAbstention &) const = default;
};

using StrategyEvaluationTerminal =
    std::variant<StrategySignal, StrategyAbstention>;

class StrategyEvaluation final {
public:
  [[nodiscard]] contracts::StrategyEvaluationId evaluation_id() const noexcept {
    return evaluation_id_;
  }
  [[nodiscard]] contracts::Sha256Digest evaluation_key() const noexcept {
    return evaluation_key_;
  }
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
  [[nodiscard]] contracts::Sha256Digest definition_digest() const noexcept {
    return definition_digest_;
  }
  [[nodiscard]] contracts::Sha256Digest activation_checksum() const noexcept {
    return activation_checksum_;
  }
  [[nodiscard]] contracts::VersionRef
  recommendation_policy_version() const noexcept {
    return recommendation_policy_version_;
  }
  [[nodiscard]] contracts::Sha256Digest
  recommendation_policy_checksum() const noexcept {
    return recommendation_policy_checksum_;
  }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }
  [[nodiscard]] std::span<const contracts::FeatureEvaluationId>
  feature_evaluation_ids() const noexcept {
    return feature_evaluation_ids_;
  }
  [[nodiscard]] const StrategyEvaluationTerminal &terminal() const noexcept {
    return terminal_;
  }
  [[nodiscard]] bool signal_emitted() const noexcept {
    return std::holds_alternative<StrategySignal>(terminal_);
  }
  [[nodiscard]] bool abstained() const noexcept {
    return std::holds_alternative<StrategyAbstention>(terminal_);
  }
  [[nodiscard]] std::span<const chronos::strategies::sdk::ExplanationFactor>
  factors() const noexcept {
    return factors_;
  }

  bool operator==(const StrategyEvaluation &) const = default;

private:
  StrategyEvaluation(
      contracts::StrategyEvaluationId evaluation_id,
      contracts::Sha256Digest evaluation_key, contracts::RunId run_id,
      contracts::StrategyInstanceId strategy_instance_id,
      contracts::ListingId listing_id,
      contracts::CanonicalInstrumentId canonical_instrument_id,
      contracts::Sha256Digest definition_digest,
      contracts::Sha256Digest activation_checksum,
      contracts::VersionRef recommendation_policy_version,
      contracts::Sha256Digest recommendation_policy_checksum,
      std::uint64_t run_input_sequence, std::int64_t logical_time_nanoseconds,
      std::vector<contracts::FeatureEvaluationId> feature_evaluation_ids,
      StrategyEvaluationTerminal terminal,
      std::vector<chronos::strategies::sdk::ExplanationFactor> factors) noexcept
      : evaluation_id_(evaluation_id), evaluation_key_(evaluation_key),
        run_id_(run_id), strategy_instance_id_(strategy_instance_id),
        listing_id_(listing_id),
        canonical_instrument_id_(canonical_instrument_id),
        definition_digest_(definition_digest),
        activation_checksum_(activation_checksum),
        recommendation_policy_version_(recommendation_policy_version),
        recommendation_policy_checksum_(recommendation_policy_checksum),
        run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds),
        feature_evaluation_ids_(std::move(feature_evaluation_ids)),
        terminal_(std::move(terminal)), factors_(std::move(factors)) {}

  contracts::StrategyEvaluationId evaluation_id_;
  contracts::Sha256Digest evaluation_key_;
  contracts::RunId run_id_;
  contracts::StrategyInstanceId strategy_instance_id_;
  contracts::ListingId listing_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::Sha256Digest definition_digest_;
  contracts::Sha256Digest activation_checksum_;
  contracts::VersionRef recommendation_policy_version_;
  contracts::Sha256Digest recommendation_policy_checksum_;
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
  std::vector<contracts::FeatureEvaluationId> feature_evaluation_ids_;
  StrategyEvaluationTerminal terminal_;
  std::vector<chronos::strategies::sdk::ExplanationFactor> factors_;

  friend class StrategyEvaluationAuthority;
};

struct StrategyEvaluationResult final {
  StrategyEvaluationFailure failure{StrategyEvaluationFailure::None};
  chronos::strategies::sdk::StrategyExecutionStatus execution_status{
      chronos::strategies::sdk::StrategyExecutionStatus::ContractViolation};
  std::uint64_t charged_operations{};
  std::optional<contracts::Sha256Digest> accepted_evaluation_key;
  std::optional<contracts::StrategyEvaluationId> accepted_evaluation_id;
  std::optional<StrategyEvaluation> evaluation;

  [[nodiscard]] bool completed() const noexcept {
    return failure == StrategyEvaluationFailure::None && evaluation.has_value();
  }
};

class StrategyEvaluationAuthority final {
public:
  [[nodiscard]] static StrategyEvaluationResult evaluate(
      const chronos::strategies::sdk::AcceptedStrategyDefinition &definition,
      const chronos::strategies::sdk::AcceptedStrategyInvocation &invocation);
};

} // namespace chronos::runtime::strategies
