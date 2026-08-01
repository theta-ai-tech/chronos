#pragma once

#include "chronos/contracts/event_envelope.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"
#include "chronos/core/portfolio/portfolio_construction.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace chronos::core::risk {

enum class RiskReplayClass : std::uint8_t {
  Faithful,
  IndependentEquivalent,
  Synthetic,
};

enum class RiskExposureDimensionSet : std::uint8_t { QuantityOnlyV1 };

enum class TargetPublicationState : std::uint8_t { Published, NotPublished };

enum class TargetAcknowledgementState : std::uint8_t {
  Acknowledged,
  NotAcknowledged,
};

enum class TargetLifecycleState : std::uint8_t {
  Active,
  Superseded,
  Invalidated,
};

enum class RiskDecisionDisposition : std::uint8_t {
  Approved,
  Modified,
  Rejected,
};

enum class RiskEvaluationFailure : std::uint8_t { None, InvalidPolicy };

enum class RiskAdmissionRejectionReason : std::uint8_t {
  MissingTargetPublication,
  PublicationNotPublished,
  FutureTargetPublication,
  PublicationTargetMismatch,
  InvalidTargetPublicationState,
  MissingTargetAcknowledgement,
  TargetNotAcknowledged,
  FutureTargetAcknowledgement,
  AcknowledgementTargetMismatch,
  AcknowledgementPublicationMismatch,
  InvalidTargetAcknowledgementState,
  MissingTargetLifecycle,
  FutureTargetLifecycle,
  LifecycleTargetMismatch,
  TargetSuperseded,
  TargetInvalidated,
  InvalidTargetLifecycleState,
  TargetKeyMismatch,
  TargetRunMismatch,
  TargetScaleMismatch,
  TargetSchemaUnsupported,
  FutureEvaluationCut,
  TargetExpired,
  InadmissibleRunMode,
};

enum class RiskObligationUnavailableReason : std::uint8_t {
  MissingRunRiskContext,
  RunRiskContextQualityNotValid,
  RunRiskContextRunMismatch,
  RunRiskContextConfigurationEpochMismatch,
  RunRiskContextFuture,
  RunRiskContextInvalidMode,
  RunRiskContextInvalidReplayClass,
  RunRiskContextModeMismatch,
  MissingRiskPolicyActivation,
  RiskPolicyActivationQualityNotValid,
  RiskPolicyActivationRunMismatch,
  RiskPolicyActivationRiskScopeMismatch,
  RiskPolicyActivationConfigurationEpochMismatch,
  RiskPolicyActivationVersionMismatch,
  RiskPolicyActivationFuture,
  MissingAccountRiskSnapshot,
  AccountRiskSnapshotQualityNotValid,
  AccountRiskSnapshotScopeMismatch,
  AccountRiskSnapshotScaleMismatch,
  AccountRiskSnapshotFuture,
  AccountRiskSnapshotStale,
  MissingMarketRiskSnapshot,
  MarketRiskSnapshotQualityNotValid,
  MarketRiskSnapshotScopeMismatch,
  MarketRiskSnapshotFuture,
  MarketRiskSnapshotStale,
  MissingProjectedExposureSnapshot,
  ProjectedExposureSnapshotQualityNotValid,
  ProjectedExposureSnapshotScopeMismatch,
  ProjectedExposureSnapshotScaleMismatch,
  ProjectedExposureSnapshotFuture,
  ProjectedExposureSnapshotStale,
  MissingKillSwitchSnapshot,
  KillSwitchSnapshotQualityNotValid,
  KillSwitchSnapshotScopeMismatch,
  KillSwitchSnapshotFuture,
  KillSwitchSnapshotStale,
  ArithmeticUnrepresentable,
};

enum class RiskDecisionReason : std::uint8_t {
  WithinPermissiveLimit,
  KillSwitchEnabled,
  AccountTradingDisabled,
  MarketNotTradeable,
  TargetAlreadySatisfied,
  ClampedToProjectedExposureLimit,
  ProjectedExposureLimitExceeded,
  ModifiedTargetWouldNotChangeExposure,
};

[[nodiscard]] constexpr bool is_valid(RiskReplayClass value) noexcept {
  return value >= RiskReplayClass::Faithful &&
         value <= RiskReplayClass::Synthetic;
}

[[nodiscard]] constexpr bool is_valid(RiskExposureDimensionSet value) noexcept {
  return value == RiskExposureDimensionSet::QuantityOnlyV1;
}

[[nodiscard]] constexpr bool is_valid(TargetPublicationState value) noexcept {
  return value >= TargetPublicationState::Published &&
         value <= TargetPublicationState::NotPublished;
}

[[nodiscard]] constexpr bool
is_valid(TargetAcknowledgementState value) noexcept {
  return value >= TargetAcknowledgementState::Acknowledged &&
         value <= TargetAcknowledgementState::NotAcknowledged;
}

[[nodiscard]] constexpr bool is_valid(TargetLifecycleState value) noexcept {
  return value >= TargetLifecycleState::Active &&
         value <= TargetLifecycleState::Invalidated;
}

[[nodiscard]] constexpr bool is_valid(RiskDecisionDisposition value) noexcept {
  return value >= RiskDecisionDisposition::Approved &&
         value <= RiskDecisionDisposition::Rejected;
}

[[nodiscard]] constexpr bool is_valid(RiskEvaluationFailure value) noexcept {
  return value >= RiskEvaluationFailure::None &&
         value <= RiskEvaluationFailure::InvalidPolicy;
}

[[nodiscard]] constexpr bool
is_valid(RiskAdmissionRejectionReason value) noexcept {
  return value >= RiskAdmissionRejectionReason::MissingTargetPublication &&
         value <= RiskAdmissionRejectionReason::InadmissibleRunMode;
}

[[nodiscard]] constexpr bool
is_valid(RiskObligationUnavailableReason value) noexcept {
  return value >= RiskObligationUnavailableReason::MissingRunRiskContext &&
         value <= RiskObligationUnavailableReason::ArithmeticUnrepresentable;
}

[[nodiscard]] constexpr bool is_valid(RiskDecisionReason value) noexcept {
  return value >= RiskDecisionReason::WithinPermissiveLimit &&
         value <= RiskDecisionReason::ModifiedTargetWouldNotChangeExposure;
}

class MinimalRiskAuthority;

class RiskEvaluationCut final {
public:
  RiskEvaluationCut(std::uint64_t run_input_sequence,
                    std::int64_t logical_time_nanoseconds) noexcept
      : run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds) {}

  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }

  bool operator==(const RiskEvaluationCut &) const = default;

private:
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
};

class MinimalRiskPolicy final {
public:
  MinimalRiskPolicy(
      contracts::RiskScopeId risk_scope_id, portfolio::TargetKey target_key,
      contracts::RunId run_id, contracts::RunMode supported_run_mode,
      contracts::VersionRef risk_policy_version,
      contracts::VersionRef limit_set_version,
      contracts::VersionRef exposure_model_version,
      contracts::VersionRef arithmetic_version,
      contracts::VersionRef authority_version,
      contracts::VersionRef target_schema_version,
      contracts::DecimalScale exposure_scale,
      RiskExposureDimensionSet exposure_dimension_set,
      contracts::AmountUnits maximum_absolute_projected_exposure_units,
      std::int64_t account_maximum_logical_age_nanoseconds,
      std::int64_t market_maximum_logical_age_nanoseconds,
      std::int64_t projected_exposure_maximum_logical_age_nanoseconds,
      std::int64_t kill_switch_maximum_logical_age_nanoseconds,
      std::int64_t decision_validity_duration_nanoseconds,
      bool equal_or_less_risky_modification_enabled)
      : risk_scope_id_(risk_scope_id), target_key_(std::move(target_key)),
        run_id_(run_id), supported_run_mode_(supported_run_mode),
        risk_policy_version_(risk_policy_version),
        limit_set_version_(limit_set_version),
        exposure_model_version_(exposure_model_version),
        arithmetic_version_(arithmetic_version),
        authority_version_(authority_version),
        target_schema_version_(target_schema_version),
        exposure_scale_(exposure_scale),
        exposure_dimension_set_(exposure_dimension_set),
        maximum_absolute_projected_exposure_units_(
            maximum_absolute_projected_exposure_units),
        account_maximum_logical_age_nanoseconds_(
            account_maximum_logical_age_nanoseconds),
        market_maximum_logical_age_nanoseconds_(
            market_maximum_logical_age_nanoseconds),
        projected_exposure_maximum_logical_age_nanoseconds_(
            projected_exposure_maximum_logical_age_nanoseconds),
        kill_switch_maximum_logical_age_nanoseconds_(
            kill_switch_maximum_logical_age_nanoseconds),
        decision_validity_duration_nanoseconds_(
            decision_validity_duration_nanoseconds),
        equal_or_less_risky_modification_enabled_(
            equal_or_less_risky_modification_enabled) {}

  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] const portfolio::TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::RunMode supported_run_mode() const noexcept {
    return supported_run_mode_;
  }
  [[nodiscard]] contracts::VersionRef risk_policy_version() const noexcept {
    return risk_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef limit_set_version() const noexcept {
    return limit_set_version_;
  }
  [[nodiscard]] contracts::VersionRef exposure_model_version() const noexcept {
    return exposure_model_version_;
  }
  [[nodiscard]] contracts::VersionRef arithmetic_version() const noexcept {
    return arithmetic_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] RiskExposureDimensionSet
  exposure_dimension_set() const noexcept {
    return exposure_dimension_set_;
  }
  [[nodiscard]] contracts::AmountUnits
  maximum_absolute_projected_exposure_units() const noexcept {
    return maximum_absolute_projected_exposure_units_;
  }
  [[nodiscard]] std::int64_t
  account_maximum_logical_age_nanoseconds() const noexcept {
    return account_maximum_logical_age_nanoseconds_;
  }
  [[nodiscard]] std::int64_t
  market_maximum_logical_age_nanoseconds() const noexcept {
    return market_maximum_logical_age_nanoseconds_;
  }
  [[nodiscard]] std::int64_t
  projected_exposure_maximum_logical_age_nanoseconds() const noexcept {
    return projected_exposure_maximum_logical_age_nanoseconds_;
  }
  [[nodiscard]] std::int64_t
  kill_switch_maximum_logical_age_nanoseconds() const noexcept {
    return kill_switch_maximum_logical_age_nanoseconds_;
  }
  [[nodiscard]] std::int64_t
  decision_validity_duration_nanoseconds() const noexcept {
    return decision_validity_duration_nanoseconds_;
  }
  [[nodiscard]] bool equal_or_less_risky_modification_enabled() const noexcept {
    return equal_or_less_risky_modification_enabled_;
  }

  bool operator==(const MinimalRiskPolicy &) const = default;

private:
  contracts::RiskScopeId risk_scope_id_;
  portfolio::TargetKey target_key_;
  contracts::RunId run_id_;
  contracts::RunMode supported_run_mode_{};
  contracts::VersionRef risk_policy_version_;
  contracts::VersionRef limit_set_version_;
  contracts::VersionRef exposure_model_version_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef authority_version_;
  contracts::VersionRef target_schema_version_;
  contracts::DecimalScale exposure_scale_;
  RiskExposureDimensionSet exposure_dimension_set_{};
  contracts::AmountUnits maximum_absolute_projected_exposure_units_{};
  std::int64_t account_maximum_logical_age_nanoseconds_{};
  std::int64_t market_maximum_logical_age_nanoseconds_{};
  std::int64_t projected_exposure_maximum_logical_age_nanoseconds_{};
  std::int64_t kill_switch_maximum_logical_age_nanoseconds_{};
  std::int64_t decision_validity_duration_nanoseconds_{};
  bool equal_or_less_risky_modification_enabled_{};
};

class TargetPublicationFact final {
public:
  TargetPublicationFact(
      contracts::PublicationAttemptId publication_attempt_id,
      contracts::TargetPositionId target_position_id,
      TargetPublicationState state, std::uint64_t effective_run_input_sequence,
      std::int64_t effective_logical_time_nanoseconds) noexcept
      : publication_attempt_id_(publication_attempt_id),
        target_position_id_(target_position_id), state_(state),
        effective_run_input_sequence_(effective_run_input_sequence),
        effective_logical_time_nanoseconds_(
            effective_logical_time_nanoseconds) {}

  [[nodiscard]] contracts::PublicationAttemptId
  publication_attempt_id() const noexcept {
    return publication_attempt_id_;
  }
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] TargetPublicationState state() const noexcept { return state_; }
  [[nodiscard]] std::uint64_t effective_run_input_sequence() const noexcept {
    return effective_run_input_sequence_;
  }
  [[nodiscard]] std::int64_t
  effective_logical_time_nanoseconds() const noexcept {
    return effective_logical_time_nanoseconds_;
  }

  bool operator==(const TargetPublicationFact &) const = default;

private:
  contracts::PublicationAttemptId publication_attempt_id_;
  contracts::TargetPositionId target_position_id_;
  TargetPublicationState state_{};
  std::uint64_t effective_run_input_sequence_{};
  std::int64_t effective_logical_time_nanoseconds_{};
};

class TargetAcknowledgementFact final {
public:
  TargetAcknowledgementFact(
      contracts::ConsumerBoundaryId consumer_boundary_id,
      contracts::PublicationAttemptId publication_attempt_id,
      contracts::TargetPositionId target_position_id,
      TargetAcknowledgementState state,
      std::uint64_t effective_run_input_sequence,
      std::int64_t effective_logical_time_nanoseconds) noexcept
      : consumer_boundary_id_(consumer_boundary_id),
        publication_attempt_id_(publication_attempt_id),
        target_position_id_(target_position_id), state_(state),
        effective_run_input_sequence_(effective_run_input_sequence),
        effective_logical_time_nanoseconds_(
            effective_logical_time_nanoseconds) {}

  [[nodiscard]] contracts::ConsumerBoundaryId
  consumer_boundary_id() const noexcept {
    return consumer_boundary_id_;
  }
  [[nodiscard]] contracts::PublicationAttemptId
  publication_attempt_id() const noexcept {
    return publication_attempt_id_;
  }
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] TargetAcknowledgementState state() const noexcept {
    return state_;
  }
  [[nodiscard]] std::uint64_t effective_run_input_sequence() const noexcept {
    return effective_run_input_sequence_;
  }
  [[nodiscard]] std::int64_t
  effective_logical_time_nanoseconds() const noexcept {
    return effective_logical_time_nanoseconds_;
  }

  bool operator==(const TargetAcknowledgementFact &) const = default;

private:
  contracts::ConsumerBoundaryId consumer_boundary_id_;
  contracts::PublicationAttemptId publication_attempt_id_;
  contracts::TargetPositionId target_position_id_;
  TargetAcknowledgementState state_{};
  std::uint64_t effective_run_input_sequence_{};
  std::int64_t effective_logical_time_nanoseconds_{};
};

class TargetLifecycleFact final {
public:
  TargetLifecycleFact(contracts::EventId lifecycle_event_id,
                      contracts::TargetPositionId target_position_id,
                      TargetLifecycleState state,
                      std::uint64_t effective_run_input_sequence,
                      std::int64_t effective_logical_time_nanoseconds) noexcept
      : lifecycle_event_id_(lifecycle_event_id),
        target_position_id_(target_position_id), state_(state),
        effective_run_input_sequence_(effective_run_input_sequence),
        effective_logical_time_nanoseconds_(
            effective_logical_time_nanoseconds) {}

  [[nodiscard]] contracts::EventId lifecycle_event_id() const noexcept {
    return lifecycle_event_id_;
  }
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] TargetLifecycleState state() const noexcept { return state_; }
  [[nodiscard]] std::uint64_t effective_run_input_sequence() const noexcept {
    return effective_run_input_sequence_;
  }
  [[nodiscard]] std::int64_t
  effective_logical_time_nanoseconds() const noexcept {
    return effective_logical_time_nanoseconds_;
  }

  bool operator==(const TargetLifecycleFact &) const = default;

private:
  contracts::EventId lifecycle_event_id_;
  contracts::TargetPositionId target_position_id_;
  TargetLifecycleState state_{};
  std::uint64_t effective_run_input_sequence_{};
  std::int64_t effective_logical_time_nanoseconds_{};
};

class TargetAdmissionEvidence final {
public:
  TargetAdmissionEvidence(
      std::optional<TargetPublicationFact> publication,
      std::optional<TargetAcknowledgementFact> acknowledgement,
      std::optional<TargetLifecycleFact> lifecycle)
      : publication_(std::move(publication)),
        acknowledgement_(std::move(acknowledgement)),
        lifecycle_(std::move(lifecycle)) {}

  [[nodiscard]] const std::optional<TargetPublicationFact> &
  publication() const noexcept {
    return publication_;
  }
  [[nodiscard]] const std::optional<TargetAcknowledgementFact> &
  acknowledgement() const noexcept {
    return acknowledgement_;
  }
  [[nodiscard]] const std::optional<TargetLifecycleFact> &
  lifecycle() const noexcept {
    return lifecycle_;
  }

  bool operator==(const TargetAdmissionEvidence &) const = default;

private:
  std::optional<TargetPublicationFact> publication_;
  std::optional<TargetAcknowledgementFact> acknowledgement_;
  std::optional<TargetLifecycleFact> lifecycle_;
};

class RunRiskContext final {
public:
  RunRiskContext(contracts::IntegrityId run_manifest_integrity_id,
                 contracts::RunId run_id, contracts::RunMode mode,
                 RiskReplayClass replay_class,
                 contracts::IntegrityId replay_evidence_id,
                 std::uint64_t configuration_epoch,
                 std::uint64_t effective_run_input_sequence,
                 std::int64_t effective_logical_time_nanoseconds,
                 contracts::DataQuality quality) noexcept
      : run_manifest_integrity_id_(run_manifest_integrity_id), run_id_(run_id),
        mode_(mode), replay_class_(replay_class),
        replay_evidence_id_(replay_evidence_id),
        configuration_epoch_(configuration_epoch),
        effective_run_input_sequence_(effective_run_input_sequence),
        effective_logical_time_nanoseconds_(effective_logical_time_nanoseconds),
        quality_(quality) {}

  [[nodiscard]] contracts::IntegrityId
  run_manifest_integrity_id() const noexcept {
    return run_manifest_integrity_id_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::RunMode mode() const noexcept { return mode_; }
  [[nodiscard]] RiskReplayClass replay_class() const noexcept {
    return replay_class_;
  }
  [[nodiscard]] contracts::IntegrityId replay_evidence_id() const noexcept {
    return replay_evidence_id_;
  }
  [[nodiscard]] std::uint64_t configuration_epoch() const noexcept {
    return configuration_epoch_;
  }
  [[nodiscard]] std::uint64_t effective_run_input_sequence() const noexcept {
    return effective_run_input_sequence_;
  }
  [[nodiscard]] std::int64_t
  effective_logical_time_nanoseconds() const noexcept {
    return effective_logical_time_nanoseconds_;
  }
  [[nodiscard]] contracts::DataQuality quality() const noexcept {
    return quality_;
  }

  bool operator==(const RunRiskContext &) const = default;

private:
  contracts::IntegrityId run_manifest_integrity_id_;
  contracts::RunId run_id_;
  contracts::RunMode mode_{};
  RiskReplayClass replay_class_{};
  contracts::IntegrityId replay_evidence_id_;
  std::uint64_t configuration_epoch_{};
  std::uint64_t effective_run_input_sequence_{};
  std::int64_t effective_logical_time_nanoseconds_{};
  contracts::DataQuality quality_;
};

class RiskPolicyActivation final {
public:
  RiskPolicyActivation(contracts::EventId control_outcome_event_id,
                       contracts::RunId run_id,
                       contracts::RiskScopeId risk_scope_id,
                       std::uint64_t configuration_epoch,
                       contracts::VersionRef risk_policy_version,
                       contracts::VersionRef limit_set_version,
                       contracts::VersionRef exposure_model_version,
                       contracts::VersionRef arithmetic_version,
                       contracts::VersionRef authority_version,
                       std::uint64_t effective_run_input_sequence,
                       std::int64_t effective_logical_time_nanoseconds,
                       contracts::DataQuality quality) noexcept
      : control_outcome_event_id_(control_outcome_event_id), run_id_(run_id),
        risk_scope_id_(risk_scope_id),
        configuration_epoch_(configuration_epoch),
        risk_policy_version_(risk_policy_version),
        limit_set_version_(limit_set_version),
        exposure_model_version_(exposure_model_version),
        arithmetic_version_(arithmetic_version),
        authority_version_(authority_version),
        effective_run_input_sequence_(effective_run_input_sequence),
        effective_logical_time_nanoseconds_(effective_logical_time_nanoseconds),
        quality_(quality) {}

  [[nodiscard]] contracts::EventId control_outcome_event_id() const noexcept {
    return control_outcome_event_id_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] std::uint64_t configuration_epoch() const noexcept {
    return configuration_epoch_;
  }
  [[nodiscard]] contracts::VersionRef risk_policy_version() const noexcept {
    return risk_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef limit_set_version() const noexcept {
    return limit_set_version_;
  }
  [[nodiscard]] contracts::VersionRef exposure_model_version() const noexcept {
    return exposure_model_version_;
  }
  [[nodiscard]] contracts::VersionRef arithmetic_version() const noexcept {
    return arithmetic_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] std::uint64_t effective_run_input_sequence() const noexcept {
    return effective_run_input_sequence_;
  }
  [[nodiscard]] std::int64_t
  effective_logical_time_nanoseconds() const noexcept {
    return effective_logical_time_nanoseconds_;
  }
  [[nodiscard]] contracts::DataQuality quality() const noexcept {
    return quality_;
  }

  bool operator==(const RiskPolicyActivation &) const = default;

private:
  contracts::EventId control_outcome_event_id_;
  contracts::RunId run_id_;
  contracts::RiskScopeId risk_scope_id_;
  std::uint64_t configuration_epoch_{};
  contracts::VersionRef risk_policy_version_;
  contracts::VersionRef limit_set_version_;
  contracts::VersionRef exposure_model_version_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef authority_version_;
  std::uint64_t effective_run_input_sequence_{};
  std::int64_t effective_logical_time_nanoseconds_{};
  contracts::DataQuality quality_;
};

class AccountRiskSnapshot final {
public:
  AccountRiskSnapshot(
      contracts::StateViewId state_view_id, contracts::RunId run_id,
      contracts::PortfolioId portfolio_id, contracts::AccountId account_id,
      contracts::AmountUnits current_position_units,
      contracts::AmountUnits available_capital_units,
      contracts::DecimalScale exposure_scale, contracts::DataQuality quality,
      std::uint64_t run_input_sequence, std::int64_t logical_time_nanoseconds,
      bool trading_enabled) noexcept
      : state_view_id_(state_view_id), run_id_(run_id),
        portfolio_id_(portfolio_id), account_id_(account_id),
        current_position_units_(current_position_units),
        available_capital_units_(available_capital_units),
        exposure_scale_(exposure_scale), quality_(quality),
        run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds),
        trading_enabled_(trading_enabled) {}

  [[nodiscard]] contracts::StateViewId state_view_id() const noexcept {
    return state_view_id_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::PortfolioId portfolio_id() const noexcept {
    return portfolio_id_;
  }
  [[nodiscard]] contracts::AccountId account_id() const noexcept {
    return account_id_;
  }
  [[nodiscard]] contracts::AmountUnits current_position_units() const noexcept {
    return current_position_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  available_capital_units() const noexcept {
    return available_capital_units_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] contracts::DataQuality quality() const noexcept {
    return quality_;
  }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }
  [[nodiscard]] bool trading_enabled() const noexcept {
    return trading_enabled_;
  }

  bool operator==(const AccountRiskSnapshot &) const = default;

private:
  contracts::StateViewId state_view_id_;
  contracts::RunId run_id_;
  contracts::PortfolioId portfolio_id_;
  contracts::AccountId account_id_;
  contracts::AmountUnits current_position_units_{};
  contracts::AmountUnits available_capital_units_{};
  contracts::DecimalScale exposure_scale_;
  contracts::DataQuality quality_;
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
  bool trading_enabled_{};
};

class MarketRiskSnapshot final {
public:
  MarketRiskSnapshot(contracts::StateViewId state_view_id,
                     contracts::RunId run_id,
                     contracts::CanonicalInstrumentId canonical_instrument_id,
                     contracts::ListingId listing_id,
                     contracts::DataQuality quality,
                     std::uint64_t run_input_sequence,
                     std::int64_t logical_time_nanoseconds,
                     bool tradeable) noexcept
      : state_view_id_(state_view_id), run_id_(run_id),
        canonical_instrument_id_(canonical_instrument_id),
        listing_id_(listing_id), quality_(quality),
        run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds),
        tradeable_(tradeable) {}

  [[nodiscard]] contracts::StateViewId state_view_id() const noexcept {
    return state_view_id_;
  }
  [[nodiscard]] contracts::RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] contracts::CanonicalInstrumentId
  canonical_instrument_id() const noexcept {
    return canonical_instrument_id_;
  }
  [[nodiscard]] contracts::ListingId listing_id() const noexcept {
    return listing_id_;
  }
  [[nodiscard]] contracts::DataQuality quality() const noexcept {
    return quality_;
  }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }
  [[nodiscard]] bool tradeable() const noexcept { return tradeable_; }

  bool operator==(const MarketRiskSnapshot &) const = default;

private:
  contracts::StateViewId state_view_id_;
  contracts::RunId run_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::ListingId listing_id_;
  contracts::DataQuality quality_;
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
  bool tradeable_{};
};

class ProjectedExposureSnapshot final {
public:
  ProjectedExposureSnapshot(
      contracts::ProjectedExposureId projected_exposure_id,
      contracts::RiskScopeId risk_scope_id, portfolio::TargetKey target_key,
      contracts::AmountUnits worst_case_exposure_before_target_units,
      contracts::DecimalScale exposure_scale, std::uint64_t risk_sequence,
      contracts::DataQuality quality, std::uint64_t run_input_sequence,
      std::int64_t logical_time_nanoseconds) noexcept
      : projected_exposure_id_(projected_exposure_id),
        risk_scope_id_(risk_scope_id), target_key_(std::move(target_key)),
        worst_case_exposure_before_target_units_(
            worst_case_exposure_before_target_units),
        exposure_scale_(exposure_scale), risk_sequence_(risk_sequence),
        quality_(quality), run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds) {}

  [[nodiscard]] contracts::ProjectedExposureId
  projected_exposure_id() const noexcept {
    return projected_exposure_id_;
  }
  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] const portfolio::TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::AmountUnits
  worst_case_exposure_before_target_units() const noexcept {
    return worst_case_exposure_before_target_units_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] std::uint64_t risk_sequence() const noexcept {
    return risk_sequence_;
  }
  [[nodiscard]] contracts::DataQuality quality() const noexcept {
    return quality_;
  }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }

  bool operator==(const ProjectedExposureSnapshot &) const = default;

private:
  contracts::ProjectedExposureId projected_exposure_id_;
  contracts::RiskScopeId risk_scope_id_;
  portfolio::TargetKey target_key_;
  contracts::AmountUnits worst_case_exposure_before_target_units_{};
  contracts::DecimalScale exposure_scale_;
  std::uint64_t risk_sequence_{};
  contracts::DataQuality quality_;
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
};

class KillSwitchSnapshot final {
public:
  KillSwitchSnapshot(contracts::StateViewId state_view_id,
                     contracts::RiskScopeId risk_scope_id,
                     contracts::DataQuality quality,
                     std::uint64_t run_input_sequence,
                     std::int64_t logical_time_nanoseconds,
                     bool enabled) noexcept
      : state_view_id_(state_view_id), risk_scope_id_(risk_scope_id),
        quality_(quality), run_input_sequence_(run_input_sequence),
        logical_time_nanoseconds_(logical_time_nanoseconds), enabled_(enabled) {
  }

  [[nodiscard]] contracts::StateViewId state_view_id() const noexcept {
    return state_view_id_;
  }
  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] contracts::DataQuality quality() const noexcept {
    return quality_;
  }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::int64_t logical_time_nanoseconds() const noexcept {
    return logical_time_nanoseconds_;
  }
  [[nodiscard]] bool enabled() const noexcept { return enabled_; }

  bool operator==(const KillSwitchSnapshot &) const = default;

private:
  contracts::StateViewId state_view_id_;
  contracts::RiskScopeId risk_scope_id_;
  contracts::DataQuality quality_;
  std::uint64_t run_input_sequence_{};
  std::int64_t logical_time_nanoseconds_{};
  bool enabled_{};
};

class RiskEvaluationEvidence final {
public:
  RiskEvaluationEvidence(
      std::optional<RunRiskContext> run_context,
      std::optional<RiskPolicyActivation> policy_activation,
      std::optional<AccountRiskSnapshot> account,
      std::optional<MarketRiskSnapshot> market,
      std::optional<ProjectedExposureSnapshot> projected_exposure,
      std::optional<KillSwitchSnapshot> kill_switch)
      : run_context_(std::move(run_context)),
        policy_activation_(std::move(policy_activation)),
        account_(std::move(account)), market_(std::move(market)),
        projected_exposure_(std::move(projected_exposure)),
        kill_switch_(std::move(kill_switch)) {}

  [[nodiscard]] const std::optional<RunRiskContext> &
  run_context() const noexcept {
    return run_context_;
  }
  [[nodiscard]] const std::optional<RiskPolicyActivation> &
  policy_activation() const noexcept {
    return policy_activation_;
  }
  [[nodiscard]] const std::optional<AccountRiskSnapshot> &
  account() const noexcept {
    return account_;
  }
  [[nodiscard]] const std::optional<MarketRiskSnapshot> &
  market() const noexcept {
    return market_;
  }
  [[nodiscard]] const std::optional<ProjectedExposureSnapshot> &
  projected_exposure() const noexcept {
    return projected_exposure_;
  }
  [[nodiscard]] const std::optional<KillSwitchSnapshot> &
  kill_switch() const noexcept {
    return kill_switch_;
  }

  bool operator==(const RiskEvaluationEvidence &) const = default;

private:
  std::optional<RunRiskContext> run_context_;
  std::optional<RiskPolicyActivation> policy_activation_;
  std::optional<AccountRiskSnapshot> account_;
  std::optional<MarketRiskSnapshot> market_;
  std::optional<ProjectedExposureSnapshot> projected_exposure_;
  std::optional<KillSwitchSnapshot> kill_switch_;
};

class RiskRuleFinding final {
public:
  explicit RiskRuleFinding(RiskDecisionReason reason) noexcept
      : reason_(reason) {}

  [[nodiscard]] RiskDecisionReason reason() const noexcept { return reason_; }

  bool operator==(const RiskRuleFinding &) const = default;

private:
  RiskDecisionReason reason_{};
};

class QuantityOnlyRiskReductionProof final {
public:
  QuantityOnlyRiskReductionProof(
      portfolio::TargetKey target_key, contracts::DecimalScale exposure_scale,
      RiskExposureDimensionSet exposure_dimension_set,
      contracts::AmountUnits requested_target_units,
      contracts::AmountUnits authorized_target_units,
      contracts::AmountUnits requested_delta_units,
      contracts::AmountUnits authorized_delta_units,
      contracts::AmountUnits requested_absolute_projected_exposure_units,
      contracts::AmountUnits authorized_absolute_projected_exposure_units,
      contracts::AmountUnits maximum_absolute_projected_exposure_units)
      : target_key_(std::move(target_key)), exposure_scale_(exposure_scale),
        exposure_dimension_set_(exposure_dimension_set),
        requested_target_units_(requested_target_units),
        authorized_target_units_(authorized_target_units),
        requested_delta_units_(requested_delta_units),
        authorized_delta_units_(authorized_delta_units),
        requested_absolute_projected_exposure_units_(
            requested_absolute_projected_exposure_units),
        authorized_absolute_projected_exposure_units_(
            authorized_absolute_projected_exposure_units),
        maximum_absolute_projected_exposure_units_(
            maximum_absolute_projected_exposure_units) {}

  [[nodiscard]] const portfolio::TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::DecimalScale exposure_scale() const noexcept {
    return exposure_scale_;
  }
  [[nodiscard]] RiskExposureDimensionSet
  exposure_dimension_set() const noexcept {
    return exposure_dimension_set_;
  }
  [[nodiscard]] contracts::AmountUnits requested_target_units() const noexcept {
    return requested_target_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  authorized_target_units() const noexcept {
    return authorized_target_units_;
  }
  [[nodiscard]] contracts::AmountUnits requested_delta_units() const noexcept {
    return requested_delta_units_;
  }
  [[nodiscard]] contracts::AmountUnits authorized_delta_units() const noexcept {
    return authorized_delta_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  requested_absolute_projected_exposure_units() const noexcept {
    return requested_absolute_projected_exposure_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  authorized_absolute_projected_exposure_units() const noexcept {
    return authorized_absolute_projected_exposure_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  maximum_absolute_projected_exposure_units() const noexcept {
    return maximum_absolute_projected_exposure_units_;
  }

  bool operator==(const QuantityOnlyRiskReductionProof &) const = default;

private:
  portfolio::TargetKey target_key_;
  contracts::DecimalScale exposure_scale_;
  RiskExposureDimensionSet exposure_dimension_set_{};
  contracts::AmountUnits requested_target_units_{};
  contracts::AmountUnits authorized_target_units_{};
  contracts::AmountUnits requested_delta_units_{};
  contracts::AmountUnits authorized_delta_units_{};
  contracts::AmountUnits requested_absolute_projected_exposure_units_{};
  contracts::AmountUnits authorized_absolute_projected_exposure_units_{};
  contracts::AmountUnits maximum_absolute_projected_exposure_units_{};
};

class RiskDecision final {
public:
  [[nodiscard]] contracts::RiskDecisionId decision_id() const noexcept {
    return decision_id_;
  }
  [[nodiscard]] contracts::RiskObligationId obligation_id() const noexcept {
    return obligation_id_;
  }
  [[nodiscard]] contracts::RiskEvaluationOutcomeId outcome_id() const noexcept {
    return outcome_id_;
  }
  [[nodiscard]] RiskDecisionDisposition disposition() const noexcept {
    return disposition_;
  }
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] const portfolio::TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::AmountUnits
  account_current_position_units() const noexcept {
    return account_current_position_units_;
  }
  [[nodiscard]] contracts::AmountUnits requested_target_units() const noexcept {
    return requested_target_units_;
  }
  [[nodiscard]] contracts::AmountUnits requested_delta_units() const noexcept {
    return requested_delta_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  projected_exposure_before_target_units() const noexcept {
    return projected_exposure_before_target_units_;
  }
  [[nodiscard]] contracts::AmountUnits
  requested_projected_exposure_units() const noexcept {
    return requested_projected_exposure_units_;
  }
  [[nodiscard]] std::optional<contracts::AmountUnits>
  authorized_target_units() const noexcept {
    return authorized_target_units_;
  }
  [[nodiscard]] std::optional<contracts::AmountUnits>
  authorized_delta_units() const noexcept {
    return authorized_delta_units_;
  }
  [[nodiscard]] std::optional<contracts::AmountUnits>
  authorized_projected_exposure_units() const noexcept {
    return authorized_projected_exposure_units_;
  }
  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] contracts::VersionRef risk_policy_version() const noexcept {
    return risk_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef limit_set_version() const noexcept {
    return limit_set_version_;
  }
  [[nodiscard]] contracts::VersionRef exposure_model_version() const noexcept {
    return exposure_model_version_;
  }
  [[nodiscard]] contracts::VersionRef arithmetic_version() const noexcept {
    return arithmetic_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] contracts::IntegrityId
  run_manifest_integrity_id() const noexcept {
    return run_manifest_integrity_id_;
  }
  [[nodiscard]] contracts::EventId policy_activation_event_id() const noexcept {
    return policy_activation_event_id_;
  }
  [[nodiscard]] contracts::StateViewId account_state_view_id() const noexcept {
    return account_state_view_id_;
  }
  [[nodiscard]] contracts::StateViewId market_state_view_id() const noexcept {
    return market_state_view_id_;
  }
  [[nodiscard]] contracts::ProjectedExposureId
  projected_exposure_id() const noexcept {
    return projected_exposure_id_;
  }
  [[nodiscard]] contracts::StateViewId
  kill_switch_state_view_id() const noexcept {
    return kill_switch_state_view_id_;
  }
  [[nodiscard]] std::uint64_t risk_sequence() const noexcept {
    return risk_sequence_;
  }
  [[nodiscard]] const RiskEvaluationCut &issue_cut() const noexcept {
    return issue_cut_;
  }
  [[nodiscard]] std::int64_t logical_expiry_nanoseconds() const noexcept {
    return logical_expiry_nanoseconds_;
  }
  [[nodiscard]] contracts::RunMode run_mode() const noexcept {
    return run_mode_;
  }
  [[nodiscard]] RiskDecisionReason binding_reason() const noexcept {
    return binding_reason_;
  }
  [[nodiscard]] std::span<const RiskRuleFinding>
  rule_findings() const noexcept {
    return rule_findings_;
  }
  [[nodiscard]] const std::optional<QuantityOnlyRiskReductionProof> &
  reduction_proof() const noexcept {
    return reduction_proof_;
  }
  [[nodiscard]] bool authorizes_target() const noexcept {
    return disposition_ == RiskDecisionDisposition::Approved ||
           disposition_ == RiskDecisionDisposition::Modified;
  }
  [[nodiscard]] bool executable() const noexcept { return false; }

  bool operator==(const RiskDecision &) const = default;

private:
  RiskDecision(
      contracts::RiskDecisionId decision_id,
      contracts::RiskObligationId obligation_id,
      contracts::RiskEvaluationOutcomeId outcome_id,
      RiskDecisionDisposition disposition,
      contracts::TargetPositionId target_position_id,
      portfolio::TargetKey target_key,
      contracts::AmountUnits account_current_position_units,
      contracts::AmountUnits requested_target_units,
      contracts::AmountUnits requested_delta_units,
      contracts::AmountUnits projected_exposure_before_target_units,
      contracts::AmountUnits requested_projected_exposure_units,
      std::optional<contracts::AmountUnits> authorized_target_units,
      std::optional<contracts::AmountUnits> authorized_delta_units,
      std::optional<contracts::AmountUnits> authorized_projected_exposure_units,
      const MinimalRiskPolicy &policy,
      contracts::IntegrityId run_manifest_integrity_id,
      contracts::EventId policy_activation_event_id,
      contracts::StateViewId account_state_view_id,
      contracts::StateViewId market_state_view_id,
      contracts::ProjectedExposureId projected_exposure_id,
      contracts::StateViewId kill_switch_state_view_id,
      std::uint64_t risk_sequence, RiskEvaluationCut issue_cut,
      std::int64_t logical_expiry_nanoseconds, contracts::RunMode run_mode,
      RiskDecisionReason binding_reason,
      std::vector<RiskRuleFinding> rule_findings,
      std::optional<QuantityOnlyRiskReductionProof> reduction_proof)
      : decision_id_(decision_id), obligation_id_(obligation_id),
        outcome_id_(outcome_id), disposition_(disposition),
        target_position_id_(target_position_id),
        target_key_(std::move(target_key)),
        account_current_position_units_(account_current_position_units),
        requested_target_units_(requested_target_units),
        requested_delta_units_(requested_delta_units),
        projected_exposure_before_target_units_(
            projected_exposure_before_target_units),
        requested_projected_exposure_units_(requested_projected_exposure_units),
        authorized_target_units_(authorized_target_units),
        authorized_delta_units_(authorized_delta_units),
        authorized_projected_exposure_units_(
            authorized_projected_exposure_units),
        risk_scope_id_(policy.risk_scope_id()),
        risk_policy_version_(policy.risk_policy_version()),
        limit_set_version_(policy.limit_set_version()),
        exposure_model_version_(policy.exposure_model_version()),
        arithmetic_version_(policy.arithmetic_version()),
        authority_version_(policy.authority_version()),
        target_schema_version_(policy.target_schema_version()),
        run_manifest_integrity_id_(run_manifest_integrity_id),
        policy_activation_event_id_(policy_activation_event_id),
        account_state_view_id_(account_state_view_id),
        market_state_view_id_(market_state_view_id),
        projected_exposure_id_(projected_exposure_id),
        kill_switch_state_view_id_(kill_switch_state_view_id),
        risk_sequence_(risk_sequence), issue_cut_(std::move(issue_cut)),
        logical_expiry_nanoseconds_(logical_expiry_nanoseconds),
        run_mode_(run_mode), binding_reason_(binding_reason),
        rule_findings_(std::move(rule_findings)),
        reduction_proof_(std::move(reduction_proof)) {}

  contracts::RiskDecisionId decision_id_;
  contracts::RiskObligationId obligation_id_;
  contracts::RiskEvaluationOutcomeId outcome_id_;
  RiskDecisionDisposition disposition_{};
  contracts::TargetPositionId target_position_id_;
  portfolio::TargetKey target_key_;
  contracts::AmountUnits account_current_position_units_{};
  contracts::AmountUnits requested_target_units_{};
  contracts::AmountUnits requested_delta_units_{};
  contracts::AmountUnits projected_exposure_before_target_units_{};
  contracts::AmountUnits requested_projected_exposure_units_{};
  std::optional<contracts::AmountUnits> authorized_target_units_;
  std::optional<contracts::AmountUnits> authorized_delta_units_;
  std::optional<contracts::AmountUnits> authorized_projected_exposure_units_;
  contracts::RiskScopeId risk_scope_id_;
  contracts::VersionRef risk_policy_version_;
  contracts::VersionRef limit_set_version_;
  contracts::VersionRef exposure_model_version_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef authority_version_;
  contracts::VersionRef target_schema_version_;
  contracts::IntegrityId run_manifest_integrity_id_;
  contracts::EventId policy_activation_event_id_;
  contracts::StateViewId account_state_view_id_;
  contracts::StateViewId market_state_view_id_;
  contracts::ProjectedExposureId projected_exposure_id_;
  contracts::StateViewId kill_switch_state_view_id_;
  std::uint64_t risk_sequence_{};
  RiskEvaluationCut issue_cut_;
  std::int64_t logical_expiry_nanoseconds_{};
  contracts::RunMode run_mode_{};
  RiskDecisionReason binding_reason_{};
  std::vector<RiskRuleFinding> rule_findings_;
  std::optional<QuantityOnlyRiskReductionProof> reduction_proof_;

  friend class MinimalRiskAuthority;
};

class RiskObligationUnavailable final {
public:
  [[nodiscard]] contracts::RiskObligationId obligation_id() const noexcept {
    return obligation_id_;
  }
  [[nodiscard]] contracts::RiskEvaluationOutcomeId outcome_id() const noexcept {
    return outcome_id_;
  }
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] const portfolio::TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] contracts::VersionRef risk_policy_version() const noexcept {
    return risk_policy_version_;
  }
  [[nodiscard]] contracts::VersionRef limit_set_version() const noexcept {
    return limit_set_version_;
  }
  [[nodiscard]] contracts::VersionRef exposure_model_version() const noexcept {
    return exposure_model_version_;
  }
  [[nodiscard]] contracts::VersionRef arithmetic_version() const noexcept {
    return arithmetic_version_;
  }
  [[nodiscard]] contracts::VersionRef authority_version() const noexcept {
    return authority_version_;
  }
  [[nodiscard]] contracts::VersionRef target_schema_version() const noexcept {
    return target_schema_version_;
  }
  [[nodiscard]] const RiskEvaluationCut &cut() const noexcept { return cut_; }
  [[nodiscard]] RiskObligationUnavailableReason reason() const noexcept {
    return reason_;
  }
  [[nodiscard]] const std::optional<contracts::IntegrityId> &
  run_manifest_integrity_id() const noexcept {
    return run_manifest_integrity_id_;
  }
  [[nodiscard]] const std::optional<contracts::IntegrityId> &
  replay_evidence_id() const noexcept {
    return replay_evidence_id_;
  }
  [[nodiscard]] const std::optional<contracts::EventId> &
  policy_activation_event_id() const noexcept {
    return policy_activation_event_id_;
  }
  [[nodiscard]] const std::optional<contracts::StateViewId> &
  account_state_view_id() const noexcept {
    return account_state_view_id_;
  }
  [[nodiscard]] const std::optional<contracts::StateViewId> &
  market_state_view_id() const noexcept {
    return market_state_view_id_;
  }
  [[nodiscard]] const std::optional<contracts::ProjectedExposureId> &
  projected_exposure_id() const noexcept {
    return projected_exposure_id_;
  }
  [[nodiscard]] const std::optional<contracts::StateViewId> &
  kill_switch_state_view_id() const noexcept {
    return kill_switch_state_view_id_;
  }
  [[nodiscard]] const std::optional<contracts::PublicationAttemptId> &
  publication_attempt_id() const noexcept {
    return publication_attempt_id_;
  }
  [[nodiscard]] const std::optional<contracts::ConsumerBoundaryId> &
  acknowledgement_boundary_id() const noexcept {
    return acknowledgement_boundary_id_;
  }
  [[nodiscard]] const std::optional<contracts::EventId> &
  lifecycle_event_id() const noexcept {
    return lifecycle_event_id_;
  }
  [[nodiscard]] bool executable() const noexcept { return false; }

  bool operator==(const RiskObligationUnavailable &) const = default;

private:
  RiskObligationUnavailable(
      contracts::RiskObligationId obligation_id,
      contracts::RiskEvaluationOutcomeId outcome_id,
      contracts::TargetPositionId target_position_id,
      portfolio::TargetKey target_key, const MinimalRiskPolicy &policy,
      RiskEvaluationCut cut, RiskObligationUnavailableReason reason,
      std::optional<contracts::IntegrityId> run_manifest_integrity_id,
      std::optional<contracts::IntegrityId> replay_evidence_id,
      std::optional<contracts::EventId> policy_activation_event_id,
      std::optional<contracts::StateViewId> account_state_view_id,
      std::optional<contracts::StateViewId> market_state_view_id,
      std::optional<contracts::ProjectedExposureId> projected_exposure_id,
      std::optional<contracts::StateViewId> kill_switch_state_view_id,
      std::optional<contracts::PublicationAttemptId> publication_attempt_id,
      std::optional<contracts::ConsumerBoundaryId> acknowledgement_boundary_id,
      std::optional<contracts::EventId> lifecycle_event_id)
      : obligation_id_(obligation_id), outcome_id_(outcome_id),
        target_position_id_(target_position_id),
        target_key_(std::move(target_key)),
        risk_scope_id_(policy.risk_scope_id()),
        risk_policy_version_(policy.risk_policy_version()),
        limit_set_version_(policy.limit_set_version()),
        exposure_model_version_(policy.exposure_model_version()),
        arithmetic_version_(policy.arithmetic_version()),
        authority_version_(policy.authority_version()),
        target_schema_version_(policy.target_schema_version()),
        cut_(std::move(cut)), reason_(reason),
        run_manifest_integrity_id_(run_manifest_integrity_id),
        replay_evidence_id_(replay_evidence_id),
        policy_activation_event_id_(policy_activation_event_id),
        account_state_view_id_(account_state_view_id),
        market_state_view_id_(market_state_view_id),
        projected_exposure_id_(projected_exposure_id),
        kill_switch_state_view_id_(kill_switch_state_view_id),
        publication_attempt_id_(publication_attempt_id),
        acknowledgement_boundary_id_(acknowledgement_boundary_id),
        lifecycle_event_id_(lifecycle_event_id) {}

  contracts::RiskObligationId obligation_id_;
  contracts::RiskEvaluationOutcomeId outcome_id_;
  contracts::TargetPositionId target_position_id_;
  portfolio::TargetKey target_key_;
  contracts::RiskScopeId risk_scope_id_;
  contracts::VersionRef risk_policy_version_;
  contracts::VersionRef limit_set_version_;
  contracts::VersionRef exposure_model_version_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef authority_version_;
  contracts::VersionRef target_schema_version_;
  RiskEvaluationCut cut_;
  RiskObligationUnavailableReason reason_{};
  std::optional<contracts::IntegrityId> run_manifest_integrity_id_;
  std::optional<contracts::IntegrityId> replay_evidence_id_;
  std::optional<contracts::EventId> policy_activation_event_id_;
  std::optional<contracts::StateViewId> account_state_view_id_;
  std::optional<contracts::StateViewId> market_state_view_id_;
  std::optional<contracts::ProjectedExposureId> projected_exposure_id_;
  std::optional<contracts::StateViewId> kill_switch_state_view_id_;
  std::optional<contracts::PublicationAttemptId> publication_attempt_id_;
  std::optional<contracts::ConsumerBoundaryId> acknowledgement_boundary_id_;
  std::optional<contracts::EventId> lifecycle_event_id_;

  friend class MinimalRiskAuthority;
};

class RiskAdmissionRejected final {
public:
  [[nodiscard]] contracts::RiskEvaluationOutcomeId outcome_id() const noexcept {
    return outcome_id_;
  }
  [[nodiscard]] contracts::TargetPositionId
  target_position_id() const noexcept {
    return target_position_id_;
  }
  [[nodiscard]] const portfolio::TargetKey &target_key() const noexcept {
    return target_key_;
  }
  [[nodiscard]] contracts::RiskScopeId risk_scope_id() const noexcept {
    return risk_scope_id_;
  }
  [[nodiscard]] const RiskEvaluationCut &cut() const noexcept { return cut_; }
  [[nodiscard]] RiskAdmissionRejectionReason reason() const noexcept {
    return reason_;
  }
  [[nodiscard]] const std::optional<contracts::PublicationAttemptId> &
  publication_attempt_id() const noexcept {
    return publication_attempt_id_;
  }
  [[nodiscard]] const std::optional<contracts::ConsumerBoundaryId> &
  acknowledgement_boundary_id() const noexcept {
    return acknowledgement_boundary_id_;
  }
  [[nodiscard]] const std::optional<contracts::PublicationAttemptId> &
  acknowledgement_publication_attempt_id() const noexcept {
    return acknowledgement_publication_attempt_id_;
  }
  [[nodiscard]] const std::optional<contracts::EventId> &
  lifecycle_event_id() const noexcept {
    return lifecycle_event_id_;
  }
  [[nodiscard]] bool executable() const noexcept { return false; }

  bool operator==(const RiskAdmissionRejected &) const = default;

private:
  RiskAdmissionRejected(
      contracts::RiskEvaluationOutcomeId outcome_id,
      contracts::TargetPositionId target_position_id,
      portfolio::TargetKey target_key, contracts::RiskScopeId risk_scope_id,
      RiskEvaluationCut cut, RiskAdmissionRejectionReason reason,
      std::optional<contracts::PublicationAttemptId> publication_attempt_id,
      std::optional<contracts::ConsumerBoundaryId> acknowledgement_boundary_id,
      std::optional<contracts::PublicationAttemptId>
          acknowledgement_publication_attempt_id,
      std::optional<contracts::EventId> lifecycle_event_id)
      : outcome_id_(outcome_id), target_position_id_(target_position_id),
        target_key_(std::move(target_key)), risk_scope_id_(risk_scope_id),
        cut_(std::move(cut)), reason_(reason),
        publication_attempt_id_(publication_attempt_id),
        acknowledgement_boundary_id_(acknowledgement_boundary_id),
        acknowledgement_publication_attempt_id_(
            acknowledgement_publication_attempt_id),
        lifecycle_event_id_(lifecycle_event_id) {}

  contracts::RiskEvaluationOutcomeId outcome_id_;
  contracts::TargetPositionId target_position_id_;
  portfolio::TargetKey target_key_;
  contracts::RiskScopeId risk_scope_id_;
  RiskEvaluationCut cut_;
  RiskAdmissionRejectionReason reason_{};
  std::optional<contracts::PublicationAttemptId> publication_attempt_id_;
  std::optional<contracts::ConsumerBoundaryId> acknowledgement_boundary_id_;
  std::optional<contracts::PublicationAttemptId>
      acknowledgement_publication_attempt_id_;
  std::optional<contracts::EventId> lifecycle_event_id_;

  friend class MinimalRiskAuthority;
};

using RiskEvaluationTerminal =
    std::variant<RiskDecision, RiskObligationUnavailable,
                 RiskAdmissionRejected>;

struct RiskEvaluationResult final {
  RiskEvaluationFailure failure{RiskEvaluationFailure::None};
  std::optional<RiskEvaluationTerminal> terminal;

  [[nodiscard]] bool completed() const noexcept {
    return failure == RiskEvaluationFailure::None && terminal.has_value();
  }
};

class MinimalRiskAuthority final {
public:
  [[nodiscard]] static RiskEvaluationResult
  evaluate(const portfolio::TargetPosition &target,
           const MinimalRiskPolicy &policy, const RiskEvaluationCut &cut,
           const TargetAdmissionEvidence &admission,
           const RiskEvaluationEvidence &evidence);
};

} // namespace chronos::core::risk
