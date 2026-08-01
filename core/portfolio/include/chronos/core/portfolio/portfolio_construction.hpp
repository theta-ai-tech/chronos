#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/core/recommendation/recommendation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace chronos::core::portfolio {

enum class PortfolioSnapshotDisposition : std::uint8_t {
  FreshComplete,
  Stale,
  Incomplete,
  Recovering,
};

enum class PortfolioNoChangeReason : std::uint8_t {
  NoActionableRecommendations,
  ContributionsCancelled,
  AlreadyAtDesiredExposure,
};

enum class PortfolioConstructionRejectionReason : std::uint8_t {
  InvalidSnapshot,
  IncompleteSnapshot,
  StaleSnapshot,
  FutureSnapshot,
  SnapshotScopeMismatch,
  RecommendationCapacityExceeded,
  DuplicateRecommendationId,
  DuplicateSignalId,
  RecommendationScopeMismatch,
  UnassignedStrategy,
  IncompatibleExposureScale,
  FutureIssuedRecommendation,
  ExpiredRecommendation,
  ArithmeticOverflow,
};

enum class PortfolioConstructionFailure : std::uint8_t {
  None,
  InvalidPolicy,
};

class TargetKey final {
public:
  TargetKey(contracts::PortfolioId portfolio_id,
            contracts::AccountId account_id,
            contracts::CanonicalInstrumentId canonical_instrument_id,
            contracts::ListingId listing_id,
            contracts::VersionRef target_policy_version) noexcept
      : portfolio_id_(portfolio_id), account_id_(account_id),
        canonical_instrument_id_(canonical_instrument_id),
        listing_id_(listing_id), target_policy_version_(target_policy_version) {
  }

  [[nodiscard]] contracts::PortfolioId portfolio_id() const noexcept {
    return portfolio_id_;
  }
  [[nodiscard]] contracts::AccountId account_id() const noexcept {
    return account_id_;
  }
  [[nodiscard]] contracts::CanonicalInstrumentId
  canonical_instrument_id() const noexcept {
    return canonical_instrument_id_;
  }
  [[nodiscard]] contracts::ListingId listing_id() const noexcept {
    return listing_id_;
  }
  [[nodiscard]] contracts::VersionRef target_policy_version() const noexcept {
    return target_policy_version_;
  }

  bool operator==(const TargetKey &) const = default;

private:
  contracts::PortfolioId portfolio_id_;
  contracts::AccountId account_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::ListingId listing_id_;
  contracts::VersionRef target_policy_version_;
};

class PortfolioStateSnapshot final {
public:
  PortfolioStateSnapshot(
      contracts::PortfolioSnapshotId snapshot_id, contracts::RunId run_id,
      contracts::PortfolioId portfolio_id, contracts::AccountId account_id,
      contracts::CanonicalInstrumentId canonical_instrument_id,
      contracts::ListingId listing_id,
      contracts::AmountUnits current_exposure_units,
      contracts::DecimalScale exposure_scale, std::uint64_t run_input_sequence,
      std::int64_t logical_time_nanoseconds, std::uint64_t configuration_epoch,
      PortfolioSnapshotDisposition disposition,
      bool paper_transition_assumption) noexcept
      : snapshot_id_(snapshot_id), run_id_(run_id), portfolio_id_(portfolio_id),
        account_id_(account_id),
        canonical_instrument_id_(canonical_instrument_id),
        listing_id_(listing_id),
        current_exposure_units_(current_exposure_units),
        exposure_scale_(exposure_scale),
        run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds),
        configuration_epoch_(configuration_epoch), disposition_(disposition),
        paper_transition_assumption_(paper_transition_assumption) {}

  [[nodiscard]] contracts::PortfolioSnapshotId snapshot_id() const noexcept {
    return snapshot_id_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::PortfolioId portfolio_id() const noexcept {
    return portfolio_id_;
  }
  [[nodiscard]] contracts::AccountId account_id() const noexcept {
    return account_id_;
  }
  [[nodiscard]] contracts::CanonicalInstrumentId
  canonical_instrument_id() const noexcept {
    return canonical_instrument_id_;
  }
  [[nodiscard]] contracts::ListingId listing_id() const noexcept {
    return listing_id_;
  }
  [[nodiscard]] contracts::AmountUnits current_exposure_units() const noexcept {
    return current_exposure_units_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }
  [[nodiscard]] std::uint64_t configuration_epoch() const noexcept {
    return configuration_epoch_;
  }
  [[nodiscard]] PortfolioSnapshotDisposition disposition() const noexcept {
    return disposition_;
  }
  [[nodiscard]] bool paper_transition_assumption() const noexcept {
    return paper_transition_assumption_;
  }

  bool operator==(const PortfolioStateSnapshot &) const = default;

private:
  contracts::PortfolioSnapshotId snapshot_id_;
  contracts::RunId run_id_;
  contracts::PortfolioId portfolio_id_;
  contracts::AccountId account_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::ListingId listing_id_;
  contracts::AmountUnits current_exposure_units_{};
  contracts::DecimalScale exposure_scale_;
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
  std::uint64_t configuration_epoch_{};
  PortfolioSnapshotDisposition disposition_{
      PortfolioSnapshotDisposition::FreshComplete};
  bool paper_transition_assumption_{};
};

class PortfolioConstructionPolicy final {
public:
  PortfolioConstructionPolicy(
      contracts::VersionRef target_schema_version,
      contracts::VersionRef sizing_policy_version,
      contracts::VersionRef aggregation_policy_version,
      contracts::VersionRef authority_version, TargetKey target_key,
      contracts::RunId run_id,
      std::vector<contracts::StrategyInstanceId> assigned_strategy_ids,
      contracts::DecimalScale exposure_scale,
      std::size_t maximum_selected_recommendations,
      std::int64_t recommendation_maximum_logical_age_nanoseconds,
      std::int64_t target_validity_duration_nanoseconds)
      : target_schema_version_(target_schema_version),
        sizing_policy_version_(sizing_policy_version),
        aggregation_policy_version_(aggregation_policy_version),
        authority_version_(authority_version),
        target_key_(std::move(target_key)), run_id_(run_id),
        assigned_strategy_ids_(std::move(assigned_strategy_ids)),
        exposure_scale_(exposure_scale),
        maximum_selected_recommendations_(maximum_selected_recommendations),
        recommendation_maximum_logical_age_nanoseconds_(
            recommendation_maximum_logical_age_nanoseconds),
        target_validity_duration_nanoseconds_(
            target_validity_duration_nanoseconds) {}

  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] contracts::VersionRef sizing_policy_version() const noexcept {
    return sizing_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef
  aggregation_policy_version() const noexcept {
    return aggregation_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] const TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] std::span<const contracts::StrategyInstanceId>
  assigned_strategy_ids() const noexcept {
    return assigned_strategy_ids_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] std::size_t maximum_selected_recommendations() const noexcept {
    return maximum_selected_recommendations_;
  }
  [[nodiscard]] std::int64_t
  recommendation_maximum_logical_age_nanoseconds() const noexcept {
    return recommendation_maximum_logical_age_nanoseconds_;
  }
  [[nodiscard]] std::int64_t
  target_validity_duration_nanoseconds() const noexcept {
    return target_validity_duration_nanoseconds_;
  }

  bool operator==(const PortfolioConstructionPolicy &) const = default;

private:
  contracts::VersionRef target_schema_version_;
  contracts::VersionRef sizing_policy_version_;
  contracts::VersionRef aggregation_policy_version_;
  contracts::VersionRef authority_version_;
  TargetKey target_key_;
  contracts::RunId run_id_;
  std::vector<contracts::StrategyInstanceId> assigned_strategy_ids_;
  contracts::DecimalScale exposure_scale_;
  std::size_t maximum_selected_recommendations_{};
  std::int64_t recommendation_maximum_logical_age_nanoseconds_{};
  std::int64_t target_validity_duration_nanoseconds_{};
};

class PortfolioConstructionCut final {
public:
  PortfolioConstructionCut(std::uint64_t run_input_sequence,
                           std::int64_t logical_time_nanoseconds) noexcept
      : run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds) {}

  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }

  bool operator==(const PortfolioConstructionCut &) const = default;

private:
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
};

class TargetPosition final {
public:
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] contracts::PortfolioConstructionOutcomeId
  outcome_id() const noexcept {
    return outcome_id_;
  }
  [[nodiscard]] const TargetKey &key() const noexcept { return key_; }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::AmountUnits desired_exposure_units() const noexcept {
    return desired_exposure_units_;
  }
  [[nodiscard]] contracts::AmountUnits current_exposure_units() const noexcept {
    return current_exposure_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  explanatory_delta_units() const noexcept {
    return explanatory_delta_units_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] const PortfolioStateSnapshot &snapshot() const noexcept {
    return snapshot_;
  }
  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] contracts::VersionRef sizing_policy_version() const noexcept {
    return sizing_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef
  aggregation_policy_version() const noexcept {
    return aggregation_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] const PortfolioConstructionCut &cut() const noexcept {
    return cut_;
  }
  [[nodiscard]] std::int64_t
  valid_until_logical_time_nanoseconds() const noexcept {
    return valid_until_logical_time_nanoseconds_;
  }
  [[nodiscard]] std::span<const contracts::TradeRecommendationId>
  source_recommendation_ids() const noexcept {
    return source_recommendation_ids_;
  }
  [[nodiscard]] std::span<const contracts::StrategySignalId>
  source_signal_ids() const noexcept {
    return source_signal_ids_;
  }
  [[nodiscard]] std::span<const contracts::TradeRecommendationId>
  excluded_recommendation_ids() const noexcept {
    return excluded_recommendation_ids_;
  }
  [[nodiscard]] std::span<const contracts::StrategySignalId>
  excluded_signal_ids() const noexcept {
    return excluded_signal_ids_;
  }
  [[nodiscard]] bool downstream_risk_eligible() const noexcept { return true; }
  [[nodiscard]] bool executable() const noexcept { return false; }

  bool operator==(const TargetPosition &) const = default;

private:
  TargetPosition(
      contracts::TargetPositionId target_position_id,
      contracts::PortfolioConstructionOutcomeId outcome_id, TargetKey key,
      contracts::RunId run_id, contracts::AmountUnits desired_exposure_units,
      contracts::AmountUnits current_exposure_units,
      contracts::AmountUnits explanatory_delta_units,
      contracts::DecimalScale exposure_scale, PortfolioStateSnapshot snapshot,
      const PortfolioConstructionPolicy &policy, PortfolioConstructionCut cut,
      std::int64_t valid_until_logical_time_nanoseconds,
      std::vector<contracts::TradeRecommendationId> source_recommendation_ids,
      std::vector<contracts::StrategySignalId> source_signal_ids,
      std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids,
      std::vector<contracts::StrategySignalId> excluded_signal_ids)
      : target_position_id_(target_position_id), outcome_id_(outcome_id),
        key_(std::move(key)), run_id_(run_id),
        desired_exposure_units_(desired_exposure_units),
        current_exposure_units_(current_exposure_units),
        explanatory_delta_units_(explanatory_delta_units),
        exposure_scale_(exposure_scale), snapshot_(std::move(snapshot)),
        target_schema_version_(policy.target_schema_version()),
        sizing_policy_version_(policy.sizing_policy_version()),
        aggregation_policy_version_(policy.aggregation_policy_version()),
        authority_version_(policy.authority_version()), cut_(std::move(cut)),
        valid_until_logical_time_nanoseconds_(
            valid_until_logical_time_nanoseconds),
        source_recommendation_ids_(std::move(source_recommendation_ids)),
        source_signal_ids_(std::move(source_signal_ids)),
        excluded_recommendation_ids_(std::move(excluded_recommendation_ids)),
        excluded_signal_ids_(std::move(excluded_signal_ids)) {}

  contracts::TargetPositionId target_position_id_;
  contracts::PortfolioConstructionOutcomeId outcome_id_;
  TargetKey key_;
  contracts::RunId run_id_;
  contracts::AmountUnits desired_exposure_units_{};
  contracts::AmountUnits current_exposure_units_{};
  contracts::AmountUnits explanatory_delta_units_{};
  contracts::DecimalScale exposure_scale_;
  PortfolioStateSnapshot snapshot_;
  contracts::VersionRef target_schema_version_;
  contracts::VersionRef sizing_policy_version_;
  contracts::VersionRef aggregation_policy_version_;
  contracts::VersionRef authority_version_;
  PortfolioConstructionCut cut_;
  std::int64_t valid_until_logical_time_nanoseconds_{};
  std::vector<contracts::TradeRecommendationId> source_recommendation_ids_;
  std::vector<contracts::StrategySignalId> source_signal_ids_;
  std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids_;
  std::vector<contracts::StrategySignalId> excluded_signal_ids_;

  friend class PortfolioConstructionAuthority;
};

class PortfolioNoChange final {
public:
  [[nodiscard]] contracts::PortfolioConstructionOutcomeId
  outcome_id() const noexcept {
    return outcome_id_;
  }
  [[nodiscard]] PortfolioNoChangeReason reason() const noexcept {
    return reason_;
  }
  [[nodiscard]] const TargetKey &key() const noexcept { return key_; }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::AmountUnits desired_exposure_units() const noexcept {
    return desired_exposure_units_;
  }
  [[nodiscard]] contracts::AmountUnits current_exposure_units() const noexcept {
    return current_exposure_units_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] const PortfolioStateSnapshot &snapshot() const noexcept {
    return snapshot_;
  }
  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] contracts::VersionRef sizing_policy_version() const noexcept {
    return sizing_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef
  aggregation_policy_version() const noexcept {
    return aggregation_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] const PortfolioConstructionCut &cut() const noexcept {
    return cut_;
  }
  [[nodiscard]] std::span<const contracts::TradeRecommendationId>
  source_recommendation_ids() const noexcept {
    return source_recommendation_ids_;
  }
  [[nodiscard]] std::span<const contracts::StrategySignalId>
  source_signal_ids() const noexcept {
    return source_signal_ids_;
  }
  [[nodiscard]] std::span<const contracts::TradeRecommendationId>
  excluded_recommendation_ids() const noexcept {
    return excluded_recommendation_ids_;
  }
  [[nodiscard]] std::span<const contracts::StrategySignalId>
  excluded_signal_ids() const noexcept {
    return excluded_signal_ids_;
  }
  [[nodiscard]] bool downstream_risk_eligible() const noexcept { return false; }
  [[nodiscard]] bool executable() const noexcept { return false; }

  bool operator==(const PortfolioNoChange &) const = default;

private:
  PortfolioNoChange(
      contracts::PortfolioConstructionOutcomeId outcome_id,
      PortfolioNoChangeReason reason, TargetKey key, contracts::RunId run_id,
      contracts::AmountUnits desired_exposure_units,
      contracts::AmountUnits current_exposure_units,
      contracts::DecimalScale exposure_scale, PortfolioStateSnapshot snapshot,
      const PortfolioConstructionPolicy &policy, PortfolioConstructionCut cut,
      std::vector<contracts::TradeRecommendationId> source_recommendation_ids,
      std::vector<contracts::StrategySignalId> source_signal_ids,
      std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids,
      std::vector<contracts::StrategySignalId> excluded_signal_ids)
      : outcome_id_(outcome_id), reason_(reason), key_(std::move(key)),
        run_id_(run_id), desired_exposure_units_(desired_exposure_units),
        current_exposure_units_(current_exposure_units),
        exposure_scale_(exposure_scale), snapshot_(std::move(snapshot)),
        target_schema_version_(policy.target_schema_version()),
        sizing_policy_version_(policy.sizing_policy_version()),
        aggregation_policy_version_(policy.aggregation_policy_version()),
        authority_version_(policy.authority_version()), cut_(std::move(cut)),
        source_recommendation_ids_(std::move(source_recommendation_ids)),
        source_signal_ids_(std::move(source_signal_ids)),
        excluded_recommendation_ids_(std::move(excluded_recommendation_ids)),
        excluded_signal_ids_(std::move(excluded_signal_ids)) {}

  contracts::PortfolioConstructionOutcomeId outcome_id_;
  PortfolioNoChangeReason reason_{
      PortfolioNoChangeReason::NoActionableRecommendations};
  TargetKey key_;
  contracts::RunId run_id_;
  contracts::AmountUnits desired_exposure_units_{};
  contracts::AmountUnits current_exposure_units_{};
  contracts::DecimalScale exposure_scale_;
  PortfolioStateSnapshot snapshot_;
  contracts::VersionRef target_schema_version_;
  contracts::VersionRef sizing_policy_version_;
  contracts::VersionRef aggregation_policy_version_;
  contracts::VersionRef authority_version_;
  PortfolioConstructionCut cut_;
  std::vector<contracts::TradeRecommendationId> source_recommendation_ids_;
  std::vector<contracts::StrategySignalId> source_signal_ids_;
  std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids_;
  std::vector<contracts::StrategySignalId> excluded_signal_ids_;

  friend class PortfolioConstructionAuthority;
};

class PortfolioConstructionRejected final {
public:
  [[nodiscard]] contracts::PortfolioConstructionOutcomeId
  outcome_id() const noexcept {
    return outcome_id_;
  }
  [[nodiscard]] PortfolioConstructionRejectionReason reason() const noexcept {
    return reason_;
  }
  [[nodiscard]] const TargetKey &key() const noexcept { return key_; }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] const PortfolioStateSnapshot &snapshot() const noexcept {
    return snapshot_;
  }
  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] contracts::VersionRef sizing_policy_version() const noexcept {
    return sizing_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef
  aggregation_policy_version() const noexcept {
    return aggregation_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] const PortfolioConstructionCut &cut() const noexcept {
    return cut_;
  }
  [[nodiscard]] std::span<const contracts::TradeRecommendationId>
  source_recommendation_ids() const noexcept {
    return source_recommendation_ids_;
  }
  [[nodiscard]] std::span<const contracts::StrategySignalId>
  source_signal_ids() const noexcept {
    return source_signal_ids_;
  }
  [[nodiscard]] std::span<const contracts::TradeRecommendationId>
  excluded_recommendation_ids() const noexcept {
    return excluded_recommendation_ids_;
  }
  [[nodiscard]] std::span<const contracts::StrategySignalId>
  excluded_signal_ids() const noexcept {
    return excluded_signal_ids_;
  }
  [[nodiscard]] const contracts::Sha256Digest &
  full_input_evidence_digest() const noexcept {
    return full_input_evidence_digest_;
  }
  [[nodiscard]] std::size_t omitted_evidence_count() const noexcept {
    return omitted_evidence_count_;
  }
  [[nodiscard]] bool downstream_risk_eligible() const noexcept { return false; }
  [[nodiscard]] bool executable() const noexcept { return false; }

  bool operator==(const PortfolioConstructionRejected &) const = default;

private:
  PortfolioConstructionRejected(
      contracts::PortfolioConstructionOutcomeId outcome_id,
      PortfolioConstructionRejectionReason reason, TargetKey key,
      contracts::RunId run_id, PortfolioStateSnapshot snapshot,
      const PortfolioConstructionPolicy &policy, PortfolioConstructionCut cut,
      std::vector<contracts::TradeRecommendationId> source_recommendation_ids,
      std::vector<contracts::StrategySignalId> source_signal_ids,
      std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids,
      std::vector<contracts::StrategySignalId> excluded_signal_ids,
      contracts::Sha256Digest full_input_evidence_digest,
      std::size_t omitted_evidence_count)
      : outcome_id_(outcome_id), reason_(reason), key_(std::move(key)),
        run_id_(run_id), snapshot_(std::move(snapshot)),
        target_schema_version_(policy.target_schema_version()),
        sizing_policy_version_(policy.sizing_policy_version()),
        aggregation_policy_version_(policy.aggregation_policy_version()),
        authority_version_(policy.authority_version()), cut_(std::move(cut)),
        source_recommendation_ids_(std::move(source_recommendation_ids)),
        source_signal_ids_(std::move(source_signal_ids)),
        excluded_recommendation_ids_(std::move(excluded_recommendation_ids)),
        excluded_signal_ids_(std::move(excluded_signal_ids)),
        full_input_evidence_digest_(full_input_evidence_digest),
        omitted_evidence_count_(omitted_evidence_count) {}

  contracts::PortfolioConstructionOutcomeId outcome_id_;
  PortfolioConstructionRejectionReason reason_{
      PortfolioConstructionRejectionReason::InvalidSnapshot};
  TargetKey key_;
  contracts::RunId run_id_;
  PortfolioStateSnapshot snapshot_;
  contracts::VersionRef target_schema_version_;
  contracts::VersionRef sizing_policy_version_;
  contracts::VersionRef aggregation_policy_version_;
  contracts::VersionRef authority_version_;
  PortfolioConstructionCut cut_;
  std::vector<contracts::TradeRecommendationId> source_recommendation_ids_;
  std::vector<contracts::StrategySignalId> source_signal_ids_;
  std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids_;
  std::vector<contracts::StrategySignalId> excluded_signal_ids_;
  contracts::Sha256Digest full_input_evidence_digest_;
  std::size_t omitted_evidence_count_{};

  friend class PortfolioConstructionAuthority;
};

using PortfolioConstructionTerminal =
    std::variant<TargetPosition, PortfolioNoChange,
                 PortfolioConstructionRejected>;

struct PortfolioConstructionResult final {
  PortfolioConstructionFailure failure{PortfolioConstructionFailure::None};
  std::optional<PortfolioConstructionTerminal> terminal;

  [[nodiscard]] bool completed() const noexcept {
    return failure == PortfolioConstructionFailure::None &&
           terminal.has_value();
  }
};

class PortfolioConstructionAuthority final {
public:
  static constexpr std::size_t kMaximumRecommendations = 64;
  static constexpr std::size_t kMaximumAssignedStrategies = 64;

  [[nodiscard]] static PortfolioConstructionResult construct(
      std::span<const recommendation::TradeRecommendation> recommendations,
      const PortfolioStateSnapshot &snapshot,
      const PortfolioConstructionPolicy &policy,
      const PortfolioConstructionCut &cut);
};

} // namespace chronos::core::portfolio
