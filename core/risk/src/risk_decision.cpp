#include "chronos/core/risk/risk_decision.hpp"

#include "chronos/contracts/digest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

namespace chronos::core::risk {
namespace {

template <typename Id>
void append_id(std::vector<std::byte> &output, const Id &value) {
  for (const auto byte : value.bytes())
    output.push_back(static_cast<std::byte>(byte));
}

template <typename Integer>
void append_integer(std::vector<std::byte> &output, Integer value) {
  using Unsigned = std::make_unsigned_t<Integer>;
  const auto converted = static_cast<Unsigned>(value);
  for (std::size_t index = 0; index < sizeof(Integer); ++index) {
    const auto shift = (sizeof(Integer) - index - 1) * 8U;
    output.push_back(
        static_cast<std::byte>((converted >> shift) & Unsigned{0xFF}));
  }
}

template <typename Enum>
void append_enum(std::vector<std::byte> &output, Enum value) {
  append_integer(output, static_cast<std::underlying_type_t<Enum>>(value));
}

void append_bool(std::vector<std::byte> &output, bool value) {
  append_integer<std::uint8_t>(output, value ? 1 : 0);
}

void append_domain(std::vector<std::byte> &output, std::string_view domain) {
  for (const auto character : domain)
    output.push_back(static_cast<std::byte>(character));
}

void append_version(std::vector<std::byte> &output,
                    contracts::VersionRef value) {
  append_id(output, value.definition_id());
  append_integer(output, value.version());
}

void append_quality(std::vector<std::byte> &output,
                    contracts::DataQuality value) {
  append_enum(output, value.status());
  append_integer(output, value.reason_code());
}

void append_scale(std::vector<std::byte> &output,
                  contracts::DecimalScale value) {
  append_integer(output, value.exponent());
}

void append_key(std::vector<std::byte> &output,
                const portfolio::TargetKey &key) {
  append_id(output, key.portfolio_id());
  append_id(output, key.account_id());
  append_id(output, key.canonical_instrument_id());
  append_id(output, key.listing_id());
  append_version(output, key.target_policy_version());
}

template <typename T, typename Appender>
void append_optional(std::vector<std::byte> &output,
                     const std::optional<T> &value, Appender append_value) {
  append_bool(output, value.has_value());
  if (value)
    append_value(output, *value);
}

template <typename Id>
void append_optional_id(std::vector<std::byte> &output,
                        const std::optional<Id> &value) {
  append_optional(output, value,
                  [](std::vector<std::byte> &bytes, const Id &identity) {
                    append_id(bytes, identity);
                  });
}

void append_cut(std::vector<std::byte> &output, const RiskEvaluationCut &cut) {
  append_integer(output, cut.run_input_sequence());
  append_integer(output, cut.logical_time_nanoseconds());
}

void append_policy(std::vector<std::byte> &output,
                   const MinimalRiskPolicy &policy) {
  append_id(output, policy.risk_scope_id());
  append_key(output, policy.target_key());
  append_id(output, policy.run_id());
  append_enum(output, policy.supported_run_mode());
  append_version(output, policy.risk_policy_version());
  append_version(output, policy.limit_set_version());
  append_version(output, policy.exposure_model_version());
  append_version(output, policy.arithmetic_version());
  append_version(output, policy.authority_version());
  append_version(output, policy.target_schema_version());
  append_scale(output, policy.exposure_scale());
  append_enum(output, policy.exposure_dimension_set());
  append_integer(output, policy.maximum_absolute_projected_exposure_units());
  append_integer(output, policy.account_maximum_logical_age_nanoseconds());
  append_integer(output, policy.market_maximum_logical_age_nanoseconds());
  append_integer(output,
                 policy.projected_exposure_maximum_logical_age_nanoseconds());
  append_integer(output, policy.kill_switch_maximum_logical_age_nanoseconds());
  append_integer(output, policy.decision_validity_duration_nanoseconds());
  append_bool(output, policy.equal_or_less_risky_modification_enabled());
}

void append_portfolio_snapshot(
    std::vector<std::byte> &output,
    const portfolio::PortfolioStateSnapshot &snapshot) {
  append_id(output, snapshot.snapshot_id());
  append_id(output, snapshot.run_id());
  append_id(output, snapshot.portfolio_id());
  append_id(output, snapshot.account_id());
  append_id(output, snapshot.canonical_instrument_id());
  append_id(output, snapshot.listing_id());
  append_integer(output, snapshot.current_exposure_units());
  append_scale(output, snapshot.exposure_scale());
  append_integer(output, snapshot.run_input_sequence());
  append_integer(output, snapshot.logical_time_nanoseconds());
  append_integer(output, snapshot.configuration_epoch());
  append_enum(output, snapshot.disposition());
  append_bool(output, snapshot.paper_transition_assumption());
}

template <typename Id>
void append_id_span(std::vector<std::byte> &output, std::span<const Id> ids) {
  append_integer(output, static_cast<std::uint64_t>(ids.size()));
  for (const auto &identity : ids)
    append_id(output, identity);
}

void append_target(std::vector<std::byte> &output,
                   const portfolio::TargetPosition &target) {
  append_id(output, target.target_position_id());
  append_id(output, target.outcome_id());
  append_key(output, target.key());
  append_id(output, target.run_id());
  append_integer(output, target.desired_exposure_units());
  append_integer(output, target.current_exposure_units());
  append_integer(output, target.explanatory_delta_units());
  append_scale(output, target.exposure_scale());
  append_portfolio_snapshot(output, target.snapshot());
  append_version(output, target.target_schema_version());
  append_version(output, target.sizing_policy_version());
  append_version(output, target.aggregation_policy_version());
  append_version(output, target.authority_version());
  append_integer(output, target.cut().run_input_sequence());
  append_integer(output, target.cut().logical_time_nanoseconds());
  append_integer(output, target.valid_until_logical_time_nanoseconds());
  append_id_span(output, target.source_recommendation_ids());
  append_id_span(output, target.source_signal_ids());
  append_id_span(output, target.excluded_recommendation_ids());
  append_id_span(output, target.excluded_signal_ids());
}

void append_publication(std::vector<std::byte> &output,
                        const TargetPublicationFact &fact) {
  append_id(output, fact.publication_attempt_id());
  append_id(output, fact.target_position_id());
  append_enum(output, fact.state());
  append_integer(output, fact.effective_run_input_sequence());
  append_integer(output, fact.effective_logical_time_nanoseconds());
}

void append_acknowledgement(std::vector<std::byte> &output,
                            const TargetAcknowledgementFact &fact) {
  append_id(output, fact.consumer_boundary_id());
  append_id(output, fact.publication_attempt_id());
  append_id(output, fact.target_position_id());
  append_enum(output, fact.state());
  append_integer(output, fact.effective_run_input_sequence());
  append_integer(output, fact.effective_logical_time_nanoseconds());
}

void append_lifecycle(std::vector<std::byte> &output,
                      const TargetLifecycleFact &fact) {
  append_id(output, fact.lifecycle_event_id());
  append_id(output, fact.target_position_id());
  append_enum(output, fact.state());
  append_integer(output, fact.effective_run_input_sequence());
  append_integer(output, fact.effective_logical_time_nanoseconds());
}

void append_admission(std::vector<std::byte> &output,
                      const TargetAdmissionEvidence &admission) {
  append_optional(output, admission.publication(), append_publication);
  append_optional(output, admission.acknowledgement(), append_acknowledgement);
  append_optional(output, admission.lifecycle(), append_lifecycle);
}

void append_run_context(std::vector<std::byte> &output,
                        const RunRiskContext &context) {
  append_id(output, context.run_manifest_integrity_id());
  append_id(output, context.run_id());
  append_enum(output, context.mode());
  append_enum(output, context.replay_class());
  append_id(output, context.replay_evidence_id());
  append_integer(output, context.configuration_epoch());
  append_integer(output, context.effective_run_input_sequence());
  append_integer(output, context.effective_logical_time_nanoseconds());
  append_quality(output, context.quality());
}

void append_activation(std::vector<std::byte> &output,
                       const RiskPolicyActivation &activation) {
  append_id(output, activation.control_outcome_event_id());
  append_id(output, activation.run_id());
  append_id(output, activation.risk_scope_id());
  append_integer(output, activation.configuration_epoch());
  append_version(output, activation.risk_policy_version());
  append_version(output, activation.limit_set_version());
  append_version(output, activation.exposure_model_version());
  append_version(output, activation.arithmetic_version());
  append_version(output, activation.authority_version());
  append_integer(output, activation.effective_run_input_sequence());
  append_integer(output, activation.effective_logical_time_nanoseconds());
  append_quality(output, activation.quality());
}

void append_account(std::vector<std::byte> &output,
                    const AccountRiskSnapshot &snapshot) {
  append_id(output, snapshot.state_view_id());
  append_id(output, snapshot.run_id());
  append_id(output, snapshot.portfolio_id());
  append_id(output, snapshot.account_id());
  append_integer(output, snapshot.current_position_units());
  append_integer(output, snapshot.available_capital_units());
  append_scale(output, snapshot.exposure_scale());
  append_quality(output, snapshot.quality());
  append_integer(output, snapshot.run_input_sequence());
  append_integer(output, snapshot.logical_time_nanoseconds());
  append_bool(output, snapshot.trading_enabled());
}

void append_market(std::vector<std::byte> &output,
                   const MarketRiskSnapshot &snapshot) {
  append_id(output, snapshot.state_view_id());
  append_id(output, snapshot.run_id());
  append_id(output, snapshot.canonical_instrument_id());
  append_id(output, snapshot.listing_id());
  append_quality(output, snapshot.quality());
  append_integer(output, snapshot.run_input_sequence());
  append_integer(output, snapshot.logical_time_nanoseconds());
  append_bool(output, snapshot.tradeable());
}

void append_projected(std::vector<std::byte> &output,
                      const ProjectedExposureSnapshot &snapshot) {
  append_id(output, snapshot.projected_exposure_id());
  append_id(output, snapshot.risk_scope_id());
  append_key(output, snapshot.target_key());
  append_integer(output, snapshot.worst_case_exposure_before_target_units());
  append_scale(output, snapshot.exposure_scale());
  append_integer(output, snapshot.risk_sequence());
  append_quality(output, snapshot.quality());
  append_integer(output, snapshot.run_input_sequence());
  append_integer(output, snapshot.logical_time_nanoseconds());
}

void append_kill_switch(std::vector<std::byte> &output,
                        const KillSwitchSnapshot &snapshot) {
  append_id(output, snapshot.state_view_id());
  append_id(output, snapshot.risk_scope_id());
  append_quality(output, snapshot.quality());
  append_integer(output, snapshot.run_input_sequence());
  append_integer(output, snapshot.logical_time_nanoseconds());
  append_bool(output, snapshot.enabled());
}

void append_evidence(std::vector<std::byte> &output,
                     const RiskEvaluationEvidence &evidence) {
  append_optional(output, evidence.run_context(), append_run_context);
  append_optional(output, evidence.policy_activation(), append_activation);
  append_optional(output, evidence.account(), append_account);
  append_optional(output, evidence.market(), append_market);
  append_optional(output, evidence.projected_exposure(), append_projected);
  append_optional(output, evidence.kill_switch(), append_kill_switch);
}

void append_context(std::vector<std::byte> &output,
                    const portfolio::TargetPosition &target,
                    const MinimalRiskPolicy &policy,
                    const RiskEvaluationCut &cut,
                    const TargetAdmissionEvidence &admission,
                    const RiskEvaluationEvidence &evidence) {
  append_target(output, target);
  append_policy(output, policy);
  append_cut(output, cut);
  append_admission(output, admission);
  append_evidence(output, evidence);
}

void append_optional_amount(
    std::vector<std::byte> &output,
    const std::optional<contracts::AmountUnits> &value) {
  append_optional(
      output, value,
      [](std::vector<std::byte> &bytes, contracts::AmountUnits amount) {
        append_integer(bytes, amount);
      });
}

void append_findings(std::vector<std::byte> &output,
                     std::span<const RiskRuleFinding> findings) {
  append_integer(output, static_cast<std::uint64_t>(findings.size()));
  for (const auto &finding : findings)
    append_enum(output, finding.reason());
}

void append_proof(std::vector<std::byte> &output,
                  const QuantityOnlyRiskReductionProof &proof) {
  append_key(output, proof.target_key());
  append_scale(output, proof.exposure_scale());
  append_enum(output, proof.exposure_dimension_set());
  append_integer(output, proof.requested_target_units());
  append_integer(output, proof.authorized_target_units());
  append_integer(output, proof.requested_delta_units());
  append_integer(output, proof.authorized_delta_units());
  append_integer(output, proof.requested_absolute_projected_exposure_units());
  append_integer(output, proof.authorized_absolute_projected_exposure_units());
  append_integer(output, proof.maximum_absolute_projected_exposure_units());
}

template <typename Id>
Id id_from_canonical(const std::vector<std::byte> &canonical) {
  return Id::from_sha256_digest(contracts::sha256(canonical));
}

contracts::RiskEvaluationOutcomeId
outcome_id_from_terminal(const std::vector<std::byte> &terminal_canonical) {
  std::vector<std::byte> canonical;
  canonical.reserve(terminal_canonical.size() + 48);
  append_domain(canonical, "chronos.risk-evaluation-outcome.v1");
  canonical.insert(canonical.end(), terminal_canonical.begin(),
                   terminal_canonical.end());
  return id_from_canonical<contracts::RiskEvaluationOutcomeId>(canonical);
}

bool checked_add(std::int64_t left, std::int64_t right, std::int64_t &result) {
  return !__builtin_add_overflow(left, right, &result);
}

bool checked_subtract(std::int64_t left, std::int64_t right,
                      std::int64_t &result) {
  return !__builtin_sub_overflow(left, right, &result);
}

std::optional<std::int64_t> checked_absolute(std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min())
    return std::nullopt;
  return value < 0 ? -value : value;
}

bool valid_policy(const MinimalRiskPolicy &policy) {
  return contracts::is_valid(policy.supported_run_mode()) &&
         is_valid(policy.exposure_dimension_set()) &&
         policy.maximum_absolute_projected_exposure_units() >= 0 &&
         policy.account_maximum_logical_age_nanoseconds() >= 0 &&
         policy.market_maximum_logical_age_nanoseconds() >= 0 &&
         policy.projected_exposure_maximum_logical_age_nanoseconds() >= 0 &&
         policy.kill_switch_maximum_logical_age_nanoseconds() >= 0 &&
         policy.decision_validity_duration_nanoseconds() > 0;
}

bool quality_is_valid(contracts::DataQuality quality) {
  return quality.status() == contracts::QualityStatus::valid;
}

bool is_future(std::uint64_t sequence, std::int64_t logical_time,
               const RiskEvaluationCut &cut) {
  return sequence > cut.run_input_sequence() ||
         logical_time > cut.logical_time_nanoseconds();
}

std::optional<bool> is_stale(std::int64_t evidence_time,
                             std::int64_t maximum_age,
                             const RiskEvaluationCut &cut) {
  std::int64_t age{};
  if (!checked_subtract(cut.logical_time_nanoseconds(), evidence_time, age))
    return std::nullopt;
  return age > maximum_age;
}

std::optional<RiskAdmissionRejectionReason> admission_rejection(
    const portfolio::TargetPosition &target, const MinimalRiskPolicy &policy,
    const RiskEvaluationCut &cut, const TargetAdmissionEvidence &admission) {
  if (!admission.publication())
    return RiskAdmissionRejectionReason::MissingTargetPublication;
  const auto &publication = *admission.publication();
  if (!is_valid(publication.state()))
    return RiskAdmissionRejectionReason::InvalidTargetPublicationState;
  if (publication.state() != TargetPublicationState::Published)
    return RiskAdmissionRejectionReason::PublicationNotPublished;
  if (is_future(publication.effective_run_input_sequence(),
                publication.effective_logical_time_nanoseconds(), cut))
    return RiskAdmissionRejectionReason::FutureTargetPublication;
  if (publication.target_position_id() != target.target_position_id())
    return RiskAdmissionRejectionReason::PublicationTargetMismatch;

  if (!admission.acknowledgement())
    return RiskAdmissionRejectionReason::MissingTargetAcknowledgement;
  const auto &acknowledgement = *admission.acknowledgement();
  if (!is_valid(acknowledgement.state()))
    return RiskAdmissionRejectionReason::InvalidTargetAcknowledgementState;
  if (acknowledgement.state() != TargetAcknowledgementState::Acknowledged)
    return RiskAdmissionRejectionReason::TargetNotAcknowledged;
  if (is_future(acknowledgement.effective_run_input_sequence(),
                acknowledgement.effective_logical_time_nanoseconds(), cut))
    return RiskAdmissionRejectionReason::FutureTargetAcknowledgement;
  if (acknowledgement.target_position_id() != target.target_position_id())
    return RiskAdmissionRejectionReason::AcknowledgementTargetMismatch;
  if (acknowledgement.publication_attempt_id() !=
      publication.publication_attempt_id())
    return RiskAdmissionRejectionReason::AcknowledgementPublicationMismatch;

  if (!admission.lifecycle())
    return RiskAdmissionRejectionReason::MissingTargetLifecycle;
  const auto &lifecycle = *admission.lifecycle();
  if (!is_valid(lifecycle.state()))
    return RiskAdmissionRejectionReason::InvalidTargetLifecycleState;
  if (is_future(lifecycle.effective_run_input_sequence(),
                lifecycle.effective_logical_time_nanoseconds(), cut))
    return RiskAdmissionRejectionReason::FutureTargetLifecycle;
  if (lifecycle.target_position_id() != target.target_position_id())
    return RiskAdmissionRejectionReason::LifecycleTargetMismatch;
  if (lifecycle.state() == TargetLifecycleState::Superseded)
    return RiskAdmissionRejectionReason::TargetSuperseded;
  if (lifecycle.state() == TargetLifecycleState::Invalidated)
    return RiskAdmissionRejectionReason::TargetInvalidated;

  if (target.key() != policy.target_key())
    return RiskAdmissionRejectionReason::TargetKeyMismatch;
  if (target.run_id() != policy.run_id())
    return RiskAdmissionRejectionReason::TargetRunMismatch;
  if (target.exposure_scale() != policy.exposure_scale())
    return RiskAdmissionRejectionReason::TargetScaleMismatch;
  if (target.target_schema_version() != policy.target_schema_version())
    return RiskAdmissionRejectionReason::TargetSchemaUnsupported;
  if (cut.run_input_sequence() < target.cut().run_input_sequence() ||
      cut.logical_time_nanoseconds() < target.cut().logical_time_nanoseconds())
    return RiskAdmissionRejectionReason::FutureEvaluationCut;
  if (cut.logical_time_nanoseconds() >
      target.valid_until_logical_time_nanoseconds())
    return RiskAdmissionRejectionReason::TargetExpired;
  return std::nullopt;
}

std::optional<RiskObligationUnavailableReason> run_context_unavailable(
    const portfolio::TargetPosition &target, const MinimalRiskPolicy &policy,
    const RiskEvaluationCut &cut, const RiskEvaluationEvidence &evidence) {
  if (!evidence.run_context())
    return RiskObligationUnavailableReason::MissingRunRiskContext;
  const auto &run = *evidence.run_context();
  if (!quality_is_valid(run.quality()))
    return RiskObligationUnavailableReason::RunRiskContextQualityNotValid;
  if (run.run_id() != policy.run_id())
    return RiskObligationUnavailableReason::RunRiskContextRunMismatch;
  if (run.configuration_epoch() != target.snapshot().configuration_epoch())
    return RiskObligationUnavailableReason::
        RunRiskContextConfigurationEpochMismatch;
  if (is_future(run.effective_run_input_sequence(),
                run.effective_logical_time_nanoseconds(), cut))
    return RiskObligationUnavailableReason::RunRiskContextFuture;
  if (!contracts::is_valid(run.mode()))
    return RiskObligationUnavailableReason::RunRiskContextInvalidMode;
  if (!is_valid(run.replay_class()))
    return RiskObligationUnavailableReason::RunRiskContextInvalidReplayClass;
  if (run.mode() != policy.supported_run_mode())
    return RiskObligationUnavailableReason::RunRiskContextModeMismatch;
  return std::nullopt;
}

std::optional<RiskObligationUnavailableReason>
remaining_evidence_unavailable(const MinimalRiskPolicy &policy,
                               const RiskEvaluationCut &cut,
                               const RiskEvaluationEvidence &evidence) {
  const auto &run = *evidence.run_context();
  if (!evidence.policy_activation())
    return RiskObligationUnavailableReason::MissingRiskPolicyActivation;
  const auto &activation = *evidence.policy_activation();
  if (!quality_is_valid(activation.quality()))
    return RiskObligationUnavailableReason::RiskPolicyActivationQualityNotValid;
  if (activation.run_id() != policy.run_id())
    return RiskObligationUnavailableReason::RiskPolicyActivationRunMismatch;
  if (activation.risk_scope_id() != policy.risk_scope_id())
    return RiskObligationUnavailableReason::
        RiskPolicyActivationRiskScopeMismatch;
  if (activation.configuration_epoch() != run.configuration_epoch())
    return RiskObligationUnavailableReason::
        RiskPolicyActivationConfigurationEpochMismatch;
  if (activation.risk_policy_version() != policy.risk_policy_version() ||
      activation.limit_set_version() != policy.limit_set_version() ||
      activation.exposure_model_version() != policy.exposure_model_version() ||
      activation.arithmetic_version() != policy.arithmetic_version() ||
      activation.authority_version() != policy.authority_version())
    return RiskObligationUnavailableReason::RiskPolicyActivationVersionMismatch;
  if (is_future(activation.effective_run_input_sequence(),
                activation.effective_logical_time_nanoseconds(), cut))
    return RiskObligationUnavailableReason::RiskPolicyActivationFuture;

  if (!evidence.account())
    return RiskObligationUnavailableReason::MissingAccountRiskSnapshot;
  const auto &account = *evidence.account();
  if (!quality_is_valid(account.quality()))
    return RiskObligationUnavailableReason::AccountRiskSnapshotQualityNotValid;
  if (account.run_id() != policy.run_id() ||
      account.portfolio_id() != policy.target_key().portfolio_id() ||
      account.account_id() != policy.target_key().account_id())
    return RiskObligationUnavailableReason::AccountRiskSnapshotScopeMismatch;
  if (account.exposure_scale() != policy.exposure_scale())
    return RiskObligationUnavailableReason::AccountRiskSnapshotScaleMismatch;
  if (is_future(account.run_input_sequence(),
                account.logical_time_nanoseconds(), cut))
    return RiskObligationUnavailableReason::AccountRiskSnapshotFuture;
  const auto account_stale =
      is_stale(account.logical_time_nanoseconds(),
               policy.account_maximum_logical_age_nanoseconds(), cut);
  if (!account_stale)
    return RiskObligationUnavailableReason::ArithmeticUnrepresentable;
  if (*account_stale)
    return RiskObligationUnavailableReason::AccountRiskSnapshotStale;

  if (!evidence.market())
    return RiskObligationUnavailableReason::MissingMarketRiskSnapshot;
  const auto &market = *evidence.market();
  if (!quality_is_valid(market.quality()))
    return RiskObligationUnavailableReason::MarketRiskSnapshotQualityNotValid;
  if (market.run_id() != policy.run_id() ||
      market.canonical_instrument_id() !=
          policy.target_key().canonical_instrument_id() ||
      market.listing_id() != policy.target_key().listing_id())
    return RiskObligationUnavailableReason::MarketRiskSnapshotScopeMismatch;
  if (is_future(market.run_input_sequence(), market.logical_time_nanoseconds(),
                cut))
    return RiskObligationUnavailableReason::MarketRiskSnapshotFuture;
  const auto market_stale =
      is_stale(market.logical_time_nanoseconds(),
               policy.market_maximum_logical_age_nanoseconds(), cut);
  if (!market_stale)
    return RiskObligationUnavailableReason::ArithmeticUnrepresentable;
  if (*market_stale)
    return RiskObligationUnavailableReason::MarketRiskSnapshotStale;

  if (!evidence.projected_exposure())
    return RiskObligationUnavailableReason::MissingProjectedExposureSnapshot;
  const auto &projected = *evidence.projected_exposure();
  if (!quality_is_valid(projected.quality()))
    return RiskObligationUnavailableReason::
        ProjectedExposureSnapshotQualityNotValid;
  if (projected.risk_scope_id() != policy.risk_scope_id() ||
      projected.target_key() != policy.target_key())
    return RiskObligationUnavailableReason::
        ProjectedExposureSnapshotScopeMismatch;
  if (projected.exposure_scale() != policy.exposure_scale())
    return RiskObligationUnavailableReason::
        ProjectedExposureSnapshotScaleMismatch;
  if (is_future(projected.run_input_sequence(),
                projected.logical_time_nanoseconds(), cut))
    return RiskObligationUnavailableReason::ProjectedExposureSnapshotFuture;
  const auto projected_stale = is_stale(
      projected.logical_time_nanoseconds(),
      policy.projected_exposure_maximum_logical_age_nanoseconds(), cut);
  if (!projected_stale)
    return RiskObligationUnavailableReason::ArithmeticUnrepresentable;
  if (*projected_stale)
    return RiskObligationUnavailableReason::ProjectedExposureSnapshotStale;

  if (!evidence.kill_switch())
    return RiskObligationUnavailableReason::MissingKillSwitchSnapshot;
  const auto &kill_switch = *evidence.kill_switch();
  if (!quality_is_valid(kill_switch.quality()))
    return RiskObligationUnavailableReason::KillSwitchSnapshotQualityNotValid;
  if (kill_switch.risk_scope_id() != policy.risk_scope_id())
    return RiskObligationUnavailableReason::KillSwitchSnapshotScopeMismatch;
  if (is_future(kill_switch.run_input_sequence(),
                kill_switch.logical_time_nanoseconds(), cut))
    return RiskObligationUnavailableReason::KillSwitchSnapshotFuture;
  const auto kill_switch_stale =
      is_stale(kill_switch.logical_time_nanoseconds(),
               policy.kill_switch_maximum_logical_age_nanoseconds(), cut);
  if (!kill_switch_stale)
    return RiskObligationUnavailableReason::ArithmeticUnrepresentable;
  if (*kill_switch_stale)
    return RiskObligationUnavailableReason::KillSwitchSnapshotStale;
  return std::nullopt;
}

contracts::RiskObligationId
obligation_id(const portfolio::TargetPosition &target,
              const MinimalRiskPolicy &policy, const RiskEvaluationCut &cut,
              const TargetAdmissionEvidence &admission) {
  std::vector<std::byte> canonical;
  canonical.reserve(2048);
  append_domain(canonical, "chronos.risk-obligation.v1");
  append_target(canonical, target);
  append_policy(canonical, policy);
  append_cut(canonical, cut);
  append_optional(canonical, admission.publication(), append_publication);
  return id_from_canonical<contracts::RiskObligationId>(canonical);
}

} // namespace

RiskEvaluationResult MinimalRiskAuthority::evaluate(
    const portfolio::TargetPosition &target, const MinimalRiskPolicy &policy,
    const RiskEvaluationCut &cut, const TargetAdmissionEvidence &admission,
    const RiskEvaluationEvidence &evidence) {
  if (!valid_policy(policy))
    return {.failure = RiskEvaluationFailure::InvalidPolicy};

  const auto admission_terminal = [&](RiskAdmissionRejectionReason reason) {
    std::vector<std::byte> canonical;
    canonical.reserve(4096);
    append_domain(canonical, "chronos.risk-admission-rejected.v1");
    append_enum(canonical, reason);
    append_context(canonical, target, policy, cut, admission, evidence);
    const auto outcome = outcome_id_from_terminal(canonical);
    RiskEvaluationResult result;
    result.terminal.emplace(RiskAdmissionRejected(
        outcome, target.target_position_id(), target.key(),
        policy.risk_scope_id(), cut, reason,
        admission.publication()
            ? std::optional(admission.publication()->publication_attempt_id())
            : std::nullopt,
        admission.acknowledgement()
            ? std::optional(admission.acknowledgement()->consumer_boundary_id())
            : std::nullopt,
        admission.acknowledgement()
            ? std::optional(
                  admission.acknowledgement()->publication_attempt_id())
            : std::nullopt,
        admission.lifecycle()
            ? std::optional(admission.lifecycle()->lifecycle_event_id())
            : std::nullopt));
    return result;
  };

  if (const auto reason = admission_rejection(target, policy, cut, admission))
    return admission_terminal(*reason);

  const auto obligation = obligation_id(target, policy, cut, admission);
  const auto unavailable_terminal = [&](RiskObligationUnavailableReason
                                            reason) {
    std::vector<std::byte> canonical;
    canonical.reserve(4096);
    append_domain(canonical, "chronos.risk-obligation-unavailable.v1");
    append_id(canonical, obligation);
    append_enum(canonical, reason);
    append_context(canonical, target, policy, cut, admission, evidence);
    const auto outcome = outcome_id_from_terminal(canonical);
    RiskEvaluationResult result;
    result.terminal.emplace(RiskObligationUnavailable(
        obligation, outcome, target.target_position_id(), target.key(), policy,
        cut, reason,
        evidence.run_context()
            ? std::optional(evidence.run_context()->run_manifest_integrity_id())
            : std::nullopt,
        evidence.run_context()
            ? std::optional(evidence.run_context()->replay_evidence_id())
            : std::nullopt,
        evidence.policy_activation()
            ? std::optional(
                  evidence.policy_activation()->control_outcome_event_id())
            : std::nullopt,
        evidence.account() ? std::optional(evidence.account()->state_view_id())
                           : std::nullopt,
        evidence.market() ? std::optional(evidence.market()->state_view_id())
                          : std::nullopt,
        evidence.projected_exposure()
            ? std::optional(
                  evidence.projected_exposure()->projected_exposure_id())
            : std::nullopt,
        evidence.kill_switch()
            ? std::optional(evidence.kill_switch()->state_view_id())
            : std::nullopt,
        admission.publication()
            ? std::optional(admission.publication()->publication_attempt_id())
            : std::nullopt,
        admission.acknowledgement()
            ? std::optional(admission.acknowledgement()->consumer_boundary_id())
            : std::nullopt,
        admission.lifecycle()
            ? std::optional(admission.lifecycle()->lifecycle_event_id())
            : std::nullopt));
    return result;
  };

  if (const auto reason =
          run_context_unavailable(target, policy, cut, evidence))
    return unavailable_terminal(*reason);

  const auto &run = *evidence.run_context();
  if (run.mode() == contracts::RunMode::capture ||
      run.mode() == contracts::RunMode::live_read_only)
    return admission_terminal(
        RiskAdmissionRejectionReason::InadmissibleRunMode);

  if (const auto reason = remaining_evidence_unavailable(policy, cut, evidence))
    return unavailable_terminal(*reason);

  const auto &account = *evidence.account();
  const auto &market = *evidence.market();
  const auto &projected = *evidence.projected_exposure();
  const auto &kill_switch = *evidence.kill_switch();

  contracts::AmountUnits requested_delta{};
  contracts::AmountUnits requested_projected{};
  std::int64_t expiry{};
  if (!checked_subtract(target.desired_exposure_units(),
                        account.current_position_units(), requested_delta) ||
      !checked_add(projected.worst_case_exposure_before_target_units(),
                   requested_delta, requested_projected) ||
      !checked_add(cut.logical_time_nanoseconds(),
                   policy.decision_validity_duration_nanoseconds(), expiry))
    return unavailable_terminal(
        RiskObligationUnavailableReason::ArithmeticUnrepresentable);
  expiry = std::min(expiry, target.valid_until_logical_time_nanoseconds());

  const auto requested_absolute = checked_absolute(requested_projected);
  const auto requested_delta_absolute = checked_absolute(requested_delta);
  if (!requested_absolute || !requested_delta_absolute)
    return unavailable_terminal(
        RiskObligationUnavailableReason::ArithmeticUnrepresentable);

  std::vector<RiskRuleFinding> findings;
  if (kill_switch.enabled())
    findings.emplace_back(RiskDecisionReason::KillSwitchEnabled);
  if (!account.trading_enabled())
    findings.emplace_back(RiskDecisionReason::AccountTradingDisabled);
  if (!market.tradeable())
    findings.emplace_back(RiskDecisionReason::MarketNotTradeable);
  if (requested_delta == 0)
    findings.emplace_back(RiskDecisionReason::TargetAlreadySatisfied);

  RiskDecisionDisposition disposition{};
  RiskDecisionReason binding_reason{};
  std::optional<contracts::AmountUnits> authorized_target;
  std::optional<contracts::AmountUnits> authorized_delta;
  std::optional<contracts::AmountUnits> authorized_projected;
  std::optional<QuantityOnlyRiskReductionProof> proof;

  const bool exceeds_limit =
      *requested_absolute > policy.maximum_absolute_projected_exposure_units();
  if (!findings.empty()) {
    if (exceeds_limit)
      findings.emplace_back(RiskDecisionReason::ProjectedExposureLimitExceeded);
    disposition = RiskDecisionDisposition::Rejected;
    binding_reason = findings.front().reason();
  } else if (!exceeds_limit) {
    disposition = RiskDecisionDisposition::Approved;
    binding_reason = RiskDecisionReason::WithinPermissiveLimit;
    authorized_target = target.desired_exposure_units();
    authorized_delta = requested_delta;
    authorized_projected = requested_projected;
  } else if (!policy.equal_or_less_risky_modification_enabled()) {
    disposition = RiskDecisionDisposition::Rejected;
    binding_reason = RiskDecisionReason::ProjectedExposureLimitExceeded;
    findings.emplace_back(binding_reason);
  } else {
    const auto limit = policy.maximum_absolute_projected_exposure_units();
    const auto solved_projected = requested_projected < 0 ? -limit : limit;
    contracts::AmountUnits solved_delta{};
    contracts::AmountUnits solved_target{};
    const bool solved =
        checked_subtract(solved_projected,
                         projected.worst_case_exposure_before_target_units(),
                         solved_delta) &&
        checked_add(account.current_position_units(), solved_delta,
                    solved_target);
    const auto solved_projected_absolute = checked_absolute(solved_projected);
    const auto solved_delta_absolute = checked_absolute(solved_delta);
    if (!solved || !solved_projected_absolute || !solved_delta_absolute)
      return unavailable_terminal(
          RiskObligationUnavailableReason::ArithmeticUnrepresentable);

    const bool same_direction = solved_delta == 0 ||
                                (requested_delta > 0 && solved_delta > 0) ||
                                (requested_delta < 0 && solved_delta < 0);
    const bool reduction_proven =
        *solved_projected_absolute < *requested_absolute &&
        *solved_projected_absolute <= limit &&
        *solved_delta_absolute <= *requested_delta_absolute && same_direction &&
        solved_target != account.current_position_units();
    if (!reduction_proven) {
      disposition = RiskDecisionDisposition::Rejected;
      binding_reason = RiskDecisionReason::ProjectedExposureLimitExceeded;
      findings.emplace_back(binding_reason);
      if (solved_delta == 0 ||
          solved_target == account.current_position_units())
        findings.emplace_back(
            RiskDecisionReason::ModifiedTargetWouldNotChangeExposure);
    } else {
      disposition = RiskDecisionDisposition::Modified;
      binding_reason = RiskDecisionReason::ClampedToProjectedExposureLimit;
      authorized_target = solved_target;
      authorized_delta = solved_delta;
      authorized_projected = solved_projected;
      proof.emplace(target.key(), policy.exposure_scale(),
                    policy.exposure_dimension_set(),
                    target.desired_exposure_units(), solved_target,
                    requested_delta, solved_delta, *requested_absolute,
                    *solved_projected_absolute, limit);
    }
  }

  std::vector<std::byte> decision_canonical;
  decision_canonical.reserve(4096);
  append_domain(decision_canonical, "chronos.risk-decision.v1");
  append_id(decision_canonical, obligation);
  append_enum(decision_canonical, disposition);
  append_integer(decision_canonical, account.current_position_units());
  append_integer(decision_canonical, target.desired_exposure_units());
  append_integer(decision_canonical, requested_delta);
  append_integer(decision_canonical,
                 projected.worst_case_exposure_before_target_units());
  append_integer(decision_canonical, requested_projected);
  append_optional_amount(decision_canonical, authorized_target);
  append_optional_amount(decision_canonical, authorized_delta);
  append_optional_amount(decision_canonical, authorized_projected);
  append_integer(decision_canonical, expiry);
  append_enum(decision_canonical, binding_reason);
  append_findings(decision_canonical, findings);
  append_optional(decision_canonical, proof, append_proof);
  append_context(decision_canonical, target, policy, cut, admission, evidence);
  const auto decision_identity =
      id_from_canonical<contracts::RiskDecisionId>(decision_canonical);

  std::vector<std::byte> decision_terminal_canonical;
  decision_terminal_canonical.reserve(decision_canonical.size() + 16);
  append_id(decision_terminal_canonical, decision_identity);
  decision_terminal_canonical.insert(decision_terminal_canonical.end(),
                                     decision_canonical.begin(),
                                     decision_canonical.end());
  const auto outcome_identity =
      outcome_id_from_terminal(decision_terminal_canonical);

  RiskEvaluationResult result;
  result.terminal.emplace(RiskDecision(
      decision_identity, obligation, outcome_identity, disposition,
      target.target_position_id(), target.key(),
      account.current_position_units(), target.desired_exposure_units(),
      requested_delta, projected.worst_case_exposure_before_target_units(),
      requested_projected, authorized_target, authorized_delta,
      authorized_projected, policy, run.run_manifest_integrity_id(),
      run.replay_evidence_id(),
      evidence.policy_activation()->control_outcome_event_id(),
      account.state_view_id(), market.state_view_id(),
      projected.projected_exposure_id(), kill_switch.state_view_id(),
      projected.risk_sequence(), cut, expiry, run.mode(), binding_reason,
      std::move(findings), std::move(proof)));
  return result;
}

} // namespace chronos::core::risk
