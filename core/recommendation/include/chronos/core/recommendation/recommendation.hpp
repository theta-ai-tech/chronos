#pragma once

#include "chronos/contracts/recommendation_policy.hpp"
#include "chronos/runtime/strategies/strategy_evaluation.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace chronos::core::recommendation {

enum class RecommendationFailure : std::uint8_t {
  None,
  InvalidPolicy,
  AbstainedEvaluation,
  InvalidSignal,
};

enum class RecommendationHoldReason : std::uint8_t {
  BelowActionThreshold,
};

enum class RecommendationAcceptanceFailure : std::uint8_t {
  None,
  CapacityExceeded,
  ConflictingRecommendation,
  CardinalityMismatch,
  AcceptanceFinalized,
};

enum class RecommendationAcceptanceDisposition : std::uint8_t {
  None,
  AcceptedNew,
  DeduplicatedExisting,
};

using contracts::recommendation_policy_checksum;
using contracts::RecommendationPolicy;

struct ActionableRecommendation final {
  contracts::AmountUnits indicative_exposure_units{};

  bool operator==(const ActionableRecommendation &) const = default;
};

struct HoldRecommendation final {
  RecommendationHoldReason reason{
      RecommendationHoldReason::BelowActionThreshold};
  contracts::AmountUnits indicative_exposure_units{};

  bool operator==(const HoldRecommendation &) const = default;
};

using RecommendationOutcome =
    std::variant<ActionableRecommendation, HoldRecommendation>;

class TradeRecommendation final {
public:
  [[nodiscard]] contracts::TradeRecommendationId
  recommendation_id() const noexcept {
    return recommendation_id_;
  }
  [[nodiscard]] contracts::StrategySignalId signal_id() const noexcept {
    return signal_id_;
  }
  [[nodiscard]] contracts::StrategyEvaluationId evaluation_id() const noexcept {
    return evaluation_id_;
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
  [[nodiscard]] contracts::VersionRef policy_version() const noexcept {
    return policy_version_;
  }
  [[nodiscard]] contracts::VersionRef schema_version() const noexcept {
    return schema_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] chronos::strategies::sdk::StrategyDirection
  direction() const noexcept {
    return direction_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] std::int64_t horizon_nanoseconds() const noexcept {
    return horizon_nanoseconds_;
  }
  [[nodiscard]] std::uint64_t issue_run_input_sequence() const noexcept {
    return issue_run_input_sequence_;
  }
  [[nodiscard]] std::int64_t issue_logical_time_nanoseconds() const noexcept {
    return issue_logical_time_nanoseconds_;
  }
  [[nodiscard]] const RecommendationOutcome &outcome() const noexcept {
    return outcome_;
  }
  [[nodiscard]] bool actionable() const noexcept {
    return std::holds_alternative<ActionableRecommendation>(outcome_);
  }
  [[nodiscard]] bool hold() const noexcept {
    return std::holds_alternative<HoldRecommendation>(outcome_);
  }
  [[nodiscard]] bool downstream_target_eligible() const noexcept {
    return actionable();
  }
  [[nodiscard]] std::span<const chronos::strategies::sdk::ExplanationFactor>
  factors() const noexcept {
    return factors_;
  }

  bool operator==(const TradeRecommendation &) const = default;

private:
  TradeRecommendation(
      contracts::TradeRecommendationId recommendation_id,
      contracts::StrategySignalId signal_id,
      contracts::StrategyEvaluationId evaluation_id,
      contracts::RunId run_id,
      contracts::StrategyInstanceId strategy_instance_id,
      contracts::ListingId listing_id,
      contracts::CanonicalInstrumentId canonical_instrument_id,
      const RecommendationPolicy &policy,
      chronos::strategies::sdk::StrategyDirection direction,
      std::int64_t horizon_nanoseconds, std::uint64_t issue_run_input_sequence,
      std::int64_t issue_logical_time_nanoseconds,
      RecommendationOutcome outcome,
      std::vector<chronos::strategies::sdk::ExplanationFactor> factors) noexcept
      : recommendation_id_(recommendation_id), signal_id_(signal_id),
        evaluation_id_(evaluation_id), run_id_(run_id),
        strategy_instance_id_(strategy_instance_id), listing_id_(listing_id),
        canonical_instrument_id_(canonical_instrument_id),
        policy_version_(policy.policy_version),
        schema_version_(policy.schema_version),
        authority_version_(policy.authority_version), direction_(direction),
        exposure_scale_(policy.scale),
        horizon_nanoseconds_(horizon_nanoseconds),
        issue_run_input_sequence_(issue_run_input_sequence),
        issue_logical_time_nanoseconds_(issue_logical_time_nanoseconds),
        outcome_(std::move(outcome)), factors_(std::move(factors)) {}

  contracts::TradeRecommendationId recommendation_id_;
  contracts::StrategySignalId signal_id_;
  contracts::StrategyEvaluationId evaluation_id_;
  contracts::RunId run_id_;
  contracts::StrategyInstanceId strategy_instance_id_;
  contracts::ListingId listing_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::VersionRef policy_version_;
  contracts::VersionRef schema_version_;
  contracts::VersionRef authority_version_;
  chronos::strategies::sdk::StrategyDirection direction_;
  contracts::DecimalScale exposure_scale_;
  std::int64_t horizon_nanoseconds_{};
  std::uint64_t issue_run_input_sequence_{};
  std::int64_t issue_logical_time_nanoseconds_{};
  RecommendationOutcome outcome_;
  std::vector<chronos::strategies::sdk::ExplanationFactor> factors_;

  friend class RecommendationAuthority;
};

struct RecommendationResult final {
  RecommendationFailure failure{RecommendationFailure::None};
  std::optional<TradeRecommendation> recommendation;

  [[nodiscard]] bool completed() const noexcept {
    return failure == RecommendationFailure::None && recommendation.has_value();
  }
};

class RecommendationAuthority final {
public:
  [[nodiscard]] static RecommendationResult
  recommend(const runtime::strategies::StrategyEvaluation &evaluation,
            const RecommendationPolicy &policy);
};

struct RecommendationAcceptanceResult final {
  RecommendationAcceptanceFailure failure{
      RecommendationAcceptanceFailure::None};
  RecommendationAcceptanceDisposition disposition{
      RecommendationAcceptanceDisposition::None};
  std::optional<TradeRecommendation> recommendation;

  [[nodiscard]] bool accepted() const noexcept {
    return failure == RecommendationAcceptanceFailure::None &&
           recommendation.has_value();
  }
};

class RecommendationAcceptanceAuthority final {
public:
  static constexpr std::size_t kMaximumAcceptedRecommendations = 4096;

  [[nodiscard]] static std::optional<RecommendationAcceptanceAuthority>
  create(std::size_t maximum_recommendations);

  [[nodiscard]] RecommendationAcceptanceResult
  accept(const TradeRecommendation &candidate);
  [[nodiscard]] bool
  finalize(std::span<const contracts::StrategySignalId> emitted_signal_ids);

  [[nodiscard]] std::span<const TradeRecommendation>
  accepted_recommendations() const noexcept {
    return accepted_;
  }
  [[nodiscard]] bool cardinality_proven() const noexcept {
    return finalized_ &&
           terminal_failure_ == RecommendationAcceptanceFailure::None;
  }
  [[nodiscard]] RecommendationAcceptanceFailure
  terminal_failure() const noexcept {
    return terminal_failure_;
  }

private:
  explicit RecommendationAcceptanceAuthority(
      std::size_t maximum_recommendations)
      : maximum_recommendations_(maximum_recommendations) {
    accepted_.reserve(maximum_recommendations);
  }

  std::size_t maximum_recommendations_{};
  std::vector<TradeRecommendation> accepted_;
  RecommendationAcceptanceFailure terminal_failure_{
      RecommendationAcceptanceFailure::None};
  std::vector<contracts::StrategySignalId> emitted_signal_ids_;
  bool finalized_{false};
};

} // namespace chronos::core::recommendation
