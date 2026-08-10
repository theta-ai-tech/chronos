#include "chronos/core/risk/risk_decision.hpp"

#include "chronos/contracts/digest.hpp"
#include "microtest.hpp"
#include "portfolio_runtime_fixture.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace contracts = chronos::contracts;
namespace portfolio = chronos::core::portfolio;
namespace risk = chronos::core::risk;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

template <typename Value>
concept HasDecisionId = requires(const Value &value) {
  { value.decision_id() } -> std::same_as<contracts::RiskDecisionId>;
};

template <typename Value>
concept HasObligationId = requires(const Value &value) {
  { value.obligation_id() } -> std::same_as<contracts::RiskObligationId>;
};

template <typename Value>
concept HasAuthorizedTarget =
    requires(const Value &value) { value.authorized_target_units(); };

template <typename Value>
concept RetainsTargetSchemaVersion = requires(const Value &value) {
  { value.target_schema_version() } -> std::same_as<contracts::VersionRef>;
};

template <typename Value>
concept CompletedDecisionRetainsReplayEvidence = requires(const Value &value) {
  { value.replay_evidence_id() } -> std::same_as<contracts::IntegrityId>;
};

template <typename Value>
concept NonExecutableTerminal = requires(const Value &value) {
  { value.executable() } -> std::same_as<bool>;
};

using EvaluationResult = decltype(risk::MinimalRiskAuthority::evaluate(
    std::declval<const portfolio::TargetPosition &>(),
    std::declval<const risk::MinimalRiskPolicy &>(),
    std::declval<const risk::RiskEvaluationCut &>(),
    std::declval<const risk::TargetAdmissionEvidence &>(),
    std::declval<const risk::RiskEvaluationEvidence &>()));

static_assert(std::same_as<EvaluationResult, risk::RiskEvaluationResult>);
static_assert(
    !std::same_as<risk::RiskDecision, risk::RiskObligationUnavailable>);
static_assert(!std::same_as<risk::RiskDecision, risk::RiskAdmissionRejected>);
static_assert(!std::same_as<risk::RiskObligationUnavailable,
                            risk::RiskAdmissionRejected>);
static_assert(HasDecisionId<risk::RiskDecision>);
static_assert(!HasDecisionId<risk::RiskObligationUnavailable>);
static_assert(!HasDecisionId<risk::RiskAdmissionRejected>);
static_assert(HasObligationId<risk::RiskDecision>);
static_assert(HasObligationId<risk::RiskObligationUnavailable>);
static_assert(!HasObligationId<risk::RiskAdmissionRejected>);
static_assert(HasAuthorizedTarget<risk::RiskDecision>);
static_assert(!HasAuthorizedTarget<risk::RiskObligationUnavailable>);
static_assert(!HasAuthorizedTarget<risk::RiskAdmissionRejected>);
static_assert(RetainsTargetSchemaVersion<risk::RiskDecision>);
static_assert(RetainsTargetSchemaVersion<risk::RiskObligationUnavailable>);
static_assert(CompletedDecisionRetainsReplayEvidence<risk::RiskDecision>);
static_assert(NonExecutableTerminal<risk::RiskDecision>);
static_assert(NonExecutableTerminal<risk::RiskObligationUnavailable>);
static_assert(NonExecutableTerminal<risk::RiskAdmissionRejected>);
static_assert(std::same_as<
              risk::RiskEvaluationTerminal,
              std::variant<risk::RiskDecision, risk::RiskObligationUnavailable,
                           risk::RiskAdmissionRejected>>);

portfolio::TargetKey target_key() {
  return portfolio::TargetKey(
      id<contracts::PortfolioId>(70), id<contracts::AccountId>(71),
      id<contracts::CanonicalInstrumentId>(42), id<contracts::ListingId>(1),
      contracts::VersionRef::from(id<contracts::DefinitionId>(72), 1).value());
}

contracts::VersionRef version(std::uint8_t seed) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), 1)
      .value();
}

contracts::DecimalScale exposure_scale() {
  return contracts::DecimalScale::from_exponent(6).value();
}

contracts::DataQuality valid_quality() {
  return contracts::DataQuality::from(contracts::QualityStatus::valid, 0)
      .value();
}

contracts::DataQuality non_valid_quality(contracts::QualityStatus status) {
  return contracts::DataQuality::from(status, 1).value();
}

portfolio::TargetPosition
target_position(contracts::AmountUnits desired_exposure = 40) {
  const auto key = target_key();
  const portfolio::PortfolioStateSnapshot snapshot(
      id<contracts::PortfolioSnapshotId>(73), id<contracts::RunId>(30),
      key.portfolio_id(), key.account_id(), key.canonical_instrument_id(),
      key.listing_id(), 10, exposure_scale(), 1, 100, 2,
      portfolio::PortfolioSnapshotDisposition::FreshComplete, true);
  const portfolio::PortfolioConstructionPolicy policy(
      version(74), version(75), version(76), version(77), key,
      id<contracts::RunId>(30), {id<contracts::StrategyInstanceId>(98)},
      exposure_scale(), 4, 100, 50);
  const portfolio::PortfolioConstructionCut cut(1, 100);
  const std::array recommendations{
      chronos::test_support::positive_portfolio_recommendation(
          desired_exposure)};
  auto result = portfolio::PortfolioConstructionAuthority::construct(
      recommendations, snapshot, policy, cut);
  if (!result.completed() || !result.terminal ||
      !std::holds_alternative<portfolio::TargetPosition>(*result.terminal))
    std::abort();
  return std::get<portfolio::TargetPosition>(std::move(*result.terminal));
}

portfolio::TargetPosition minimum_target_position() {
  auto recommendation =
      chronos::test_support::positive_portfolio_recommendation(40);
  auto &action =
      const_cast<chronos::core::recommendation::ActionableRecommendation &>(
          std::get<chronos::core::recommendation::ActionableRecommendation>(
              recommendation.outcome()));
  action.indicative_exposure_units =
      std::numeric_limits<contracts::AmountUnits>::min();

  const auto key = target_key();
  const portfolio::PortfolioStateSnapshot snapshot(
      id<contracts::PortfolioSnapshotId>(73), id<contracts::RunId>(30),
      key.portfolio_id(), key.account_id(), key.canonical_instrument_id(),
      key.listing_id(), 0, exposure_scale(), 1, 100, 2,
      portfolio::PortfolioSnapshotDisposition::FreshComplete, true);
  const portfolio::PortfolioConstructionPolicy policy(
      version(74), version(75), version(76), version(77), key,
      id<contracts::RunId>(30), {id<contracts::StrategyInstanceId>(98)},
      exposure_scale(), 4, 100, 50);
  const std::array recommendations{recommendation};
  auto result = portfolio::PortfolioConstructionAuthority::construct(
      recommendations, snapshot, policy,
      portfolio::PortfolioConstructionCut(1, 100));
  if (!result.completed() || !result.terminal ||
      !std::holds_alternative<portfolio::TargetPosition>(*result.terminal))
    std::abort();
  return std::get<portfolio::TargetPosition>(std::move(*result.terminal));
}

struct EvaluationSpec final {
  contracts::AmountUnits account_current_position{10};
  contracts::AmountUnits projected_exposure_before_target{15};
  contracts::AmountUnits limit{100};
  bool trading_enabled{true};
  bool market_tradeable{true};
  bool kill_switch_enabled{false};
  bool modification_enabled{true};
};

risk::MinimalRiskPolicy risk_policy(const EvaluationSpec &spec) {
  return risk::MinimalRiskPolicy(
      id<contracts::RiskScopeId>(80), target_key(), id<contracts::RunId>(30),
      contracts::RunMode::live_paper, version(81), version(82), version(83),
      version(84), version(85), version(74), exposure_scale(),
      risk::RiskExposureDimensionSet::QuantityOnlyV1, spec.limit, 100, 100, 100,
      100, 50, spec.modification_enabled);
}

risk::RiskEvaluationCut risk_cut() { return risk::RiskEvaluationCut(5, 120); }

risk::TargetAdmissionEvidence
admission_evidence(const portfolio::TargetPosition &target) {
  const auto publication_id = id<contracts::PublicationAttemptId>(90);
  return risk::TargetAdmissionEvidence(
      risk::TargetPublicationFact(publication_id, target.target_position_id(),
                                  risk::TargetPublicationState::Published, 2,
                                  105),
      risk::TargetAcknowledgementFact(
          id<contracts::ConsumerBoundaryId>(91), publication_id,
          target.target_position_id(),
          risk::TargetAcknowledgementState::Acknowledged, 3, 110),
      risk::TargetLifecycleFact(id<contracts::EventId>(92),
                                target.target_position_id(),
                                risk::TargetLifecycleState::Active, 4, 115));
}

risk::RiskEvaluationEvidence evaluation_evidence(const EvaluationSpec &spec) {
  const auto key = target_key();
  return risk::RiskEvaluationEvidence(
      risk::RunRiskContext(
          id<contracts::IntegrityId>(93), id<contracts::RunId>(30),
          contracts::RunMode::live_paper, risk::RiskReplayClass::Faithful,
          id<contracts::IntegrityId>(94), 2, 1, 100, valid_quality()),
      risk::RiskPolicyActivation(
          id<contracts::EventId>(95), id<contracts::RunId>(30),
          id<contracts::RiskScopeId>(80), 2, version(81), version(82),
          version(83), version(84), version(85), 2, 105, valid_quality()),
      risk::AccountRiskSnapshot(id<contracts::StateViewId>(96),
                                id<contracts::RunId>(30), key.portfolio_id(),
                                key.account_id(), spec.account_current_position,
                                1'000, exposure_scale(), valid_quality(), 3,
                                110, spec.trading_enabled),
      risk::MarketRiskSnapshot(id<contracts::StateViewId>(97),
                               id<contracts::RunId>(30),
                               key.canonical_instrument_id(), key.listing_id(),
                               valid_quality(), 3, 110, spec.market_tradeable),
      risk::ProjectedExposureSnapshot(id<contracts::ProjectedExposureId>(98),
                                      id<contracts::RiskScopeId>(80), key,
                                      spec.projected_exposure_before_target,
                                      exposure_scale(), 123, valid_quality(), 3,
                                      110),
      risk::KillSwitchSnapshot(id<contracts::StateViewId>(99),
                               id<contracts::RiskScopeId>(80), valid_quality(),
                               3, 110, spec.kill_switch_enabled));
}

struct PolicySpec final {
  contracts::RiskScopeId risk_scope_id{id<contracts::RiskScopeId>(80)};
  portfolio::TargetKey key{target_key()};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  contracts::RunMode mode{contracts::RunMode::live_paper};
  contracts::VersionRef risk_policy_version{version(81)};
  contracts::VersionRef limit_set_version{version(82)};
  contracts::VersionRef exposure_model_version{version(83)};
  contracts::VersionRef arithmetic_version{version(84)};
  contracts::VersionRef authority_version{version(85)};
  contracts::VersionRef target_schema_version{version(74)};
  contracts::DecimalScale scale{exposure_scale()};
  risk::RiskExposureDimensionSet dimension_set{
      risk::RiskExposureDimensionSet::QuantityOnlyV1};
  contracts::AmountUnits limit{100};
  std::int64_t account_maximum_age{100};
  std::int64_t market_maximum_age{100};
  std::int64_t projected_maximum_age{100};
  std::int64_t kill_switch_maximum_age{100};
  std::int64_t decision_validity_duration{50};
  bool modification_enabled{true};
};

risk::MinimalRiskPolicy risk_policy(const PolicySpec &spec) {
  return risk::MinimalRiskPolicy(
      spec.risk_scope_id, spec.key, spec.run_id, spec.mode,
      spec.risk_policy_version, spec.limit_set_version,
      spec.exposure_model_version, spec.arithmetic_version,
      spec.authority_version, spec.target_schema_version, spec.scale,
      spec.dimension_set, spec.limit, spec.account_maximum_age,
      spec.market_maximum_age, spec.projected_maximum_age,
      spec.kill_switch_maximum_age, spec.decision_validity_duration,
      spec.modification_enabled);
}

struct AdmissionSpec final {
  bool publication_present{true};
  contracts::PublicationAttemptId publication_attempt_id{
      id<contracts::PublicationAttemptId>(90)};
  contracts::TargetPositionId publication_target_id{
      id<contracts::TargetPositionId>(1)};
  risk::TargetPublicationState publication_state{
      risk::TargetPublicationState::Published};
  std::uint64_t publication_sequence{2};
  std::int64_t publication_time{105};

  bool acknowledgement_present{true};
  contracts::ConsumerBoundaryId acknowledgement_boundary_id{
      id<contracts::ConsumerBoundaryId>(91)};
  contracts::PublicationAttemptId acknowledgement_publication_id{
      id<contracts::PublicationAttemptId>(90)};
  contracts::TargetPositionId acknowledgement_target_id{
      id<contracts::TargetPositionId>(1)};
  risk::TargetAcknowledgementState acknowledgement_state{
      risk::TargetAcknowledgementState::Acknowledged};
  std::uint64_t acknowledgement_sequence{3};
  std::int64_t acknowledgement_time{110};

  bool lifecycle_present{true};
  contracts::EventId lifecycle_event_id{id<contracts::EventId>(92)};
  contracts::TargetPositionId lifecycle_target_id{
      id<contracts::TargetPositionId>(1)};
  risk::TargetLifecycleState lifecycle_state{
      risk::TargetLifecycleState::Active};
  std::uint64_t lifecycle_sequence{4};
  std::int64_t lifecycle_time{115};
};

risk::TargetAdmissionEvidence
admission_evidence(const portfolio::TargetPosition &target,
                   const AdmissionSpec &spec) {
  auto publication_target_id = spec.publication_target_id;
  auto acknowledgement_target_id = spec.acknowledgement_target_id;
  auto lifecycle_target_id = spec.lifecycle_target_id;
  if (spec.publication_target_id == id<contracts::TargetPositionId>(1))
    publication_target_id = target.target_position_id();
  if (spec.acknowledgement_target_id == id<contracts::TargetPositionId>(1))
    acknowledgement_target_id = target.target_position_id();
  if (spec.lifecycle_target_id == id<contracts::TargetPositionId>(1))
    lifecycle_target_id = target.target_position_id();

  std::optional<risk::TargetPublicationFact> publication;
  if (spec.publication_present) {
    publication.emplace(spec.publication_attempt_id, publication_target_id,
                        spec.publication_state, spec.publication_sequence,
                        spec.publication_time);
  }
  std::optional<risk::TargetAcknowledgementFact> acknowledgement;
  if (spec.acknowledgement_present) {
    acknowledgement.emplace(
        spec.acknowledgement_boundary_id, spec.acknowledgement_publication_id,
        acknowledgement_target_id, spec.acknowledgement_state,
        spec.acknowledgement_sequence, spec.acknowledgement_time);
  }
  std::optional<risk::TargetLifecycleFact> lifecycle;
  if (spec.lifecycle_present) {
    lifecycle.emplace(spec.lifecycle_event_id, lifecycle_target_id,
                      spec.lifecycle_state, spec.lifecycle_sequence,
                      spec.lifecycle_time);
  }
  return risk::TargetAdmissionEvidence(
      std::move(publication), std::move(acknowledgement), std::move(lifecycle));
}

struct RunContextSpec final {
  contracts::IntegrityId manifest_id{id<contracts::IntegrityId>(93)};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  contracts::RunMode mode{contracts::RunMode::live_paper};
  risk::RiskReplayClass replay_class{risk::RiskReplayClass::Faithful};
  contracts::IntegrityId replay_evidence_id{id<contracts::IntegrityId>(94)};
  std::uint64_t configuration_epoch{2};
  std::uint64_t sequence{1};
  std::int64_t time{100};
  contracts::DataQuality quality{valid_quality()};
};

struct ActivationSpec final {
  contracts::EventId event_id{id<contracts::EventId>(95)};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  contracts::RiskScopeId risk_scope_id{id<contracts::RiskScopeId>(80)};
  std::uint64_t configuration_epoch{2};
  contracts::VersionRef risk_policy_version{version(81)};
  contracts::VersionRef limit_set_version{version(82)};
  contracts::VersionRef exposure_model_version{version(83)};
  contracts::VersionRef arithmetic_version{version(84)};
  contracts::VersionRef authority_version{version(85)};
  std::uint64_t sequence{2};
  std::int64_t time{105};
  contracts::DataQuality quality{valid_quality()};
};

struct AccountSpec final {
  contracts::StateViewId state_view_id{id<contracts::StateViewId>(96)};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  contracts::PortfolioId portfolio_id{id<contracts::PortfolioId>(70)};
  contracts::AccountId account_id{id<contracts::AccountId>(71)};
  contracts::AmountUnits current_position{10};
  contracts::AmountUnits available_capital{1'000};
  contracts::DecimalScale scale{exposure_scale()};
  contracts::DataQuality quality{valid_quality()};
  std::uint64_t sequence{3};
  std::int64_t time{110};
  bool trading_enabled{true};
};

struct MarketSpec final {
  contracts::StateViewId state_view_id{id<contracts::StateViewId>(97)};
  contracts::RunId run_id{id<contracts::RunId>(30)};
  contracts::CanonicalInstrumentId instrument_id{
      id<contracts::CanonicalInstrumentId>(42)};
  contracts::ListingId listing_id{id<contracts::ListingId>(1)};
  contracts::DataQuality quality{valid_quality()};
  std::uint64_t sequence{3};
  std::int64_t time{110};
  bool tradeable{true};
};

struct ProjectedSpec final {
  contracts::ProjectedExposureId exposure_id{
      id<contracts::ProjectedExposureId>(98)};
  contracts::RiskScopeId risk_scope_id{id<contracts::RiskScopeId>(80)};
  portfolio::TargetKey key{target_key()};
  contracts::AmountUnits before_target{15};
  contracts::DecimalScale scale{exposure_scale()};
  std::uint64_t risk_sequence{123};
  contracts::DataQuality quality{valid_quality()};
  std::uint64_t sequence{3};
  std::int64_t time{110};
};

struct KillSwitchSpec final {
  contracts::StateViewId state_view_id{id<contracts::StateViewId>(99)};
  contracts::RiskScopeId risk_scope_id{id<contracts::RiskScopeId>(80)};
  contracts::DataQuality quality{valid_quality()};
  std::uint64_t sequence{3};
  std::int64_t time{110};
  bool enabled{false};
};

struct EvidenceSpec final {
  bool run_context_present{true};
  RunContextSpec run;
  bool activation_present{true};
  ActivationSpec activation;
  bool account_present{true};
  AccountSpec account;
  bool market_present{true};
  MarketSpec market;
  bool projected_present{true};
  ProjectedSpec projected;
  bool kill_switch_present{true};
  KillSwitchSpec kill_switch;
};

risk::RiskEvaluationEvidence evaluation_evidence(const EvidenceSpec &spec) {
  std::optional<risk::RunRiskContext> run;
  if (spec.run_context_present) {
    run.emplace(spec.run.manifest_id, spec.run.run_id, spec.run.mode,
                spec.run.replay_class, spec.run.replay_evidence_id,
                spec.run.configuration_epoch, spec.run.sequence, spec.run.time,
                spec.run.quality);
  }
  std::optional<risk::RiskPolicyActivation> activation;
  if (spec.activation_present) {
    activation.emplace(
        spec.activation.event_id, spec.activation.run_id,
        spec.activation.risk_scope_id, spec.activation.configuration_epoch,
        spec.activation.risk_policy_version, spec.activation.limit_set_version,
        spec.activation.exposure_model_version,
        spec.activation.arithmetic_version, spec.activation.authority_version,
        spec.activation.sequence, spec.activation.time,
        spec.activation.quality);
  }
  std::optional<risk::AccountRiskSnapshot> account;
  if (spec.account_present) {
    account.emplace(spec.account.state_view_id, spec.account.run_id,
                    spec.account.portfolio_id, spec.account.account_id,
                    spec.account.current_position,
                    spec.account.available_capital, spec.account.scale,
                    spec.account.quality, spec.account.sequence,
                    spec.account.time, spec.account.trading_enabled);
  }
  std::optional<risk::MarketRiskSnapshot> market;
  if (spec.market_present) {
    market.emplace(spec.market.state_view_id, spec.market.run_id,
                   spec.market.instrument_id, spec.market.listing_id,
                   spec.market.quality, spec.market.sequence, spec.market.time,
                   spec.market.tradeable);
  }
  std::optional<risk::ProjectedExposureSnapshot> projected;
  if (spec.projected_present) {
    projected.emplace(spec.projected.exposure_id, spec.projected.risk_scope_id,
                      spec.projected.key, spec.projected.before_target,
                      spec.projected.scale, spec.projected.risk_sequence,
                      spec.projected.quality, spec.projected.sequence,
                      spec.projected.time);
  }
  std::optional<risk::KillSwitchSnapshot> kill_switch;
  if (spec.kill_switch_present) {
    kill_switch.emplace(spec.kill_switch.state_view_id,
                        spec.kill_switch.risk_scope_id,
                        spec.kill_switch.quality, spec.kill_switch.sequence,
                        spec.kill_switch.time, spec.kill_switch.enabled);
  }
  return risk::RiskEvaluationEvidence(
      std::move(run), std::move(activation), std::move(account),
      std::move(market), std::move(projected), std::move(kill_switch));
}

risk::RiskEvaluationResult evaluate(const portfolio::TargetPosition &target,
                                    const PolicySpec &policy_spec,
                                    const risk::RiskEvaluationCut &cut,
                                    const AdmissionSpec &admission_spec,
                                    const EvidenceSpec &evidence_spec) {
  return risk::MinimalRiskAuthority::evaluate(
      target, risk_policy(policy_spec), cut,
      admission_evidence(target, admission_spec),
      evaluation_evidence(evidence_spec));
}

risk::RiskEvaluationResult evaluate(const portfolio::TargetPosition &target,
                                    const EvaluationSpec &spec = {}) {
  return risk::MinimalRiskAuthority::evaluate(
      target, risk_policy(spec), risk_cut(), admission_evidence(target),
      evaluation_evidence(spec));
}

const risk::RiskDecision *decision(const risk::RiskEvaluationResult &result) {
  if (!result.terminal)
    return nullptr;
  return std::get_if<risk::RiskDecision>(&*result.terminal);
}

const risk::RiskObligationUnavailable *
unavailable(const risk::RiskEvaluationResult &result) {
  if (!result.terminal)
    return nullptr;
  return std::get_if<risk::RiskObligationUnavailable>(&*result.terminal);
}

const risk::RiskAdmissionRejected *
admission_rejected(const risk::RiskEvaluationResult &result) {
  if (!result.terminal)
    return nullptr;
  return std::get_if<risk::RiskAdmissionRejected>(&*result.terminal);
}

template <typename Id> bool nonzero(Id value) {
  return Id::from_bytes(value.bytes()).has_value();
}

void check_rejected(const risk::RiskDecision &value,
                    risk::RiskDecisionReason reason, int &mt_fail_count) {
  CHECK(value.disposition() == risk::RiskDecisionDisposition::Rejected);
  CHECK(value.binding_reason() == reason);
  CHECK(!value.authorizes_target());
  CHECK(!value.authorized_target_units().has_value());
  CHECK(!value.authorized_delta_units().has_value());
  CHECK(!value.authorized_projected_exposure_units().has_value());
  CHECK(!value.reduction_proof().has_value());
}

} // namespace

TEST_CASE("risk decision contract preserves distinct terminal types") {
  const auto key = target_key();
  CHECK(key.portfolio_id() == id<contracts::PortfolioId>(70));
  CHECK(key.account_id() == id<contracts::AccountId>(71));
  CHECK(!risk::RiskEvaluationResult{}.completed());
}

TEST_CASE("risk admission rejects the complete target-side matrix") {
  const auto target = target_position();
  struct AdmissionCase final {
    std::string_view name;
    risk::RiskAdmissionRejectionReason reason;
    PolicySpec policy;
    risk::RiskEvaluationCut cut{risk_cut()};
    AdmissionSpec admission;
    EvidenceSpec evidence;
  };
  std::vector<AdmissionCase> cases;
  const auto add = [&](std::string_view name,
                       risk::RiskAdmissionRejectionReason reason, auto mutate) {
    AdmissionCase value{.name = name, .reason = reason};
    mutate(value);
    cases.push_back(std::move(value));
  };

  add("missing publication",
      risk::RiskAdmissionRejectionReason::MissingTargetPublication,
      [](auto &value) { value.admission.publication_present = false; });
  add("negative publication",
      risk::RiskAdmissionRejectionReason::PublicationNotPublished,
      [](auto &value) {
        value.admission.publication_state =
            risk::TargetPublicationState::NotPublished;
      });
  add("invalid publication state",
      risk::RiskAdmissionRejectionReason::InvalidTargetPublicationState,
      [](auto &value) {
        value.admission.publication_state =
            static_cast<risk::TargetPublicationState>(255);
      });
  add("future publication sequence",
      risk::RiskAdmissionRejectionReason::FutureTargetPublication,
      [](auto &value) { value.admission.publication_sequence = 6; });
  add("future publication time",
      risk::RiskAdmissionRejectionReason::FutureTargetPublication,
      [](auto &value) { value.admission.publication_time = 121; });
  add("publication target mismatch",
      risk::RiskAdmissionRejectionReason::PublicationTargetMismatch,
      [](auto &value) {
        value.admission.publication_target_id =
            id<contracts::TargetPositionId>(2);
      });
  add("missing acknowledgement",
      risk::RiskAdmissionRejectionReason::MissingTargetAcknowledgement,
      [](auto &value) { value.admission.acknowledgement_present = false; });
  add("negative acknowledgement",
      risk::RiskAdmissionRejectionReason::TargetNotAcknowledged,
      [](auto &value) {
        value.admission.acknowledgement_state =
            risk::TargetAcknowledgementState::NotAcknowledged;
      });
  add("invalid acknowledgement state",
      risk::RiskAdmissionRejectionReason::InvalidTargetAcknowledgementState,
      [](auto &value) {
        value.admission.acknowledgement_state =
            static_cast<risk::TargetAcknowledgementState>(255);
      });
  add("future acknowledgement sequence",
      risk::RiskAdmissionRejectionReason::FutureTargetAcknowledgement,
      [](auto &value) { value.admission.acknowledgement_sequence = 6; });
  add("future acknowledgement time",
      risk::RiskAdmissionRejectionReason::FutureTargetAcknowledgement,
      [](auto &value) { value.admission.acknowledgement_time = 121; });
  add("acknowledgement target mismatch",
      risk::RiskAdmissionRejectionReason::AcknowledgementTargetMismatch,
      [](auto &value) {
        value.admission.acknowledgement_target_id =
            id<contracts::TargetPositionId>(2);
      });
  add("acknowledgement publication mismatch",
      risk::RiskAdmissionRejectionReason::AcknowledgementPublicationMismatch,
      [](auto &value) {
        value.admission.acknowledgement_publication_id =
            id<contracts::PublicationAttemptId>(89);
      });
  add("missing lifecycle",
      risk::RiskAdmissionRejectionReason::MissingTargetLifecycle,
      [](auto &value) { value.admission.lifecycle_present = false; });
  add("invalid lifecycle state",
      risk::RiskAdmissionRejectionReason::InvalidTargetLifecycleState,
      [](auto &value) {
        value.admission.lifecycle_state =
            static_cast<risk::TargetLifecycleState>(255);
      });
  add("future lifecycle sequence",
      risk::RiskAdmissionRejectionReason::FutureTargetLifecycle,
      [](auto &value) { value.admission.lifecycle_sequence = 6; });
  add("future lifecycle time",
      risk::RiskAdmissionRejectionReason::FutureTargetLifecycle,
      [](auto &value) { value.admission.lifecycle_time = 121; });
  add("lifecycle target mismatch",
      risk::RiskAdmissionRejectionReason::LifecycleTargetMismatch,
      [](auto &value) {
        value.admission.lifecycle_target_id =
            id<contracts::TargetPositionId>(2);
      });
  add("superseded target", risk::RiskAdmissionRejectionReason::TargetSuperseded,
      [](auto &value) {
        value.admission.lifecycle_state =
            risk::TargetLifecycleState::Superseded;
      });
  add("invalidated target",
      risk::RiskAdmissionRejectionReason::TargetInvalidated, [](auto &value) {
        value.admission.lifecycle_state =
            risk::TargetLifecycleState::Invalidated;
      });
  add("target key mismatch",
      risk::RiskAdmissionRejectionReason::TargetKeyMismatch, [](auto &value) {
        value.policy.key = portfolio::TargetKey(
            id<contracts::PortfolioId>(69), id<contracts::AccountId>(71),
            id<contracts::CanonicalInstrumentId>(42),
            id<contracts::ListingId>(1), version(72));
      });
  add("target run mismatch",
      risk::RiskAdmissionRejectionReason::TargetRunMismatch,
      [](auto &value) { value.policy.run_id = id<contracts::RunId>(31); });
  add("target scale mismatch",
      risk::RiskAdmissionRejectionReason::TargetScaleMismatch, [](auto &value) {
        value.policy.scale = contracts::DecimalScale::from_exponent(5).value();
      });
  add("target schema mismatch",
      risk::RiskAdmissionRejectionReason::TargetSchemaUnsupported,
      [](auto &value) { value.policy.target_schema_version = version(75); });
  add("future cut sequence",
      risk::RiskAdmissionRejectionReason::FutureEvaluationCut, [](auto &value) {
        value.cut = risk::RiskEvaluationCut(0, 120);
        value.admission.publication_sequence = 0;
        value.admission.acknowledgement_sequence = 0;
        value.admission.lifecycle_sequence = 0;
      });
  add("future cut time",
      risk::RiskAdmissionRejectionReason::FutureEvaluationCut, [](auto &value) {
        value.cut = risk::RiskEvaluationCut(5, 99);
        value.admission.publication_time = 99;
        value.admission.acknowledgement_time = 99;
        value.admission.lifecycle_time = 99;
      });
  add("expired target", risk::RiskAdmissionRejectionReason::TargetExpired,
      [](auto &value) { value.cut = risk::RiskEvaluationCut(5, 151); });
  add("capture mode", risk::RiskAdmissionRejectionReason::InadmissibleRunMode,
      [](auto &value) {
        value.policy.mode = contracts::RunMode::capture;
        value.evidence.run.mode = contracts::RunMode::capture;
      });
  add("live read only mode",
      risk::RiskAdmissionRejectionReason::InadmissibleRunMode, [](auto &value) {
        value.policy.mode = contracts::RunMode::live_read_only;
        value.evidence.run.mode = contracts::RunMode::live_read_only;
      });

  for (const auto &test : cases) {
    CHECK(!test.name.empty());
    const auto first =
        evaluate(target, test.policy, test.cut, test.admission, test.evidence);
    const auto repeated =
        evaluate(target, test.policy, test.cut, test.admission, test.evidence);
    CHECK(first.completed());
    const auto *rejected = admission_rejected(first);
    const auto *repeated_rejected = admission_rejected(repeated);
    CHECK(rejected != nullptr);
    CHECK(repeated_rejected != nullptr);
    if (!rejected || !repeated_rejected)
      continue;
    CHECK(rejected->reason() == test.reason);
    CHECK(rejected->outcome_id() == repeated_rejected->outcome_id());
    CHECK(*rejected == *repeated_rejected);
    CHECK(nonzero(rejected->outcome_id()));
    CHECK(!rejected->executable());
  }
}

TEST_CASE("run context and policy activation fail closed in fixed order") {
  const auto target = target_position();
  struct EvidenceCase final {
    std::string_view name;
    risk::RiskObligationUnavailableReason reason;
    PolicySpec policy;
    EvidenceSpec evidence;
  };
  std::vector<EvidenceCase> cases;
  const auto add = [&](std::string_view name,
                       risk::RiskObligationUnavailableReason reason,
                       auto mutate) {
    EvidenceCase value{.name = name, .reason = reason};
    mutate(value);
    cases.push_back(std::move(value));
  };

  add("missing run context",
      risk::RiskObligationUnavailableReason::MissingRunRiskContext,
      [](auto &value) { value.evidence.run_context_present = false; });
  for (const auto status :
       {contracts::QualityStatus::stale, contracts::QualityStatus::gapped,
        contracts::QualityStatus::recovering, contracts::QualityStatus::invalid,
        contracts::QualityStatus::unavailable}) {
    add("non-valid run context",
        risk::RiskObligationUnavailableReason::RunRiskContextQualityNotValid,
        [status](auto &value) {
          value.evidence.run.quality = non_valid_quality(status);
        });
  }
  add("run mismatch",
      risk::RiskObligationUnavailableReason::RunRiskContextRunMismatch,
      [](auto &value) {
        value.evidence.run.run_id = id<contracts::RunId>(31);
      });
  add("run epoch mismatch",
      risk::RiskObligationUnavailableReason::
          RunRiskContextConfigurationEpochMismatch,
      [](auto &value) { value.evidence.run.configuration_epoch = 3; });
  add("future run sequence",
      risk::RiskObligationUnavailableReason::RunRiskContextFuture,
      [](auto &value) { value.evidence.run.sequence = 6; });
  add("future run time",
      risk::RiskObligationUnavailableReason::RunRiskContextFuture,
      [](auto &value) { value.evidence.run.time = 121; });
  add("invalid run mode",
      risk::RiskObligationUnavailableReason::RunRiskContextInvalidMode,
      [](auto &value) {
        value.evidence.run.mode = static_cast<contracts::RunMode>(255);
      });
  add("invalid replay class",
      risk::RiskObligationUnavailableReason::RunRiskContextInvalidReplayClass,
      [](auto &value) {
        value.evidence.run.replay_class =
            static_cast<risk::RiskReplayClass>(255);
      });
  add("run mode mismatch",
      risk::RiskObligationUnavailableReason::RunRiskContextModeMismatch,
      [](auto &value) {
        value.evidence.run.mode = contracts::RunMode::replay;
      });
  add("missing activation",
      risk::RiskObligationUnavailableReason::MissingRiskPolicyActivation,
      [](auto &value) { value.evidence.activation_present = false; });
  for (const auto status :
       {contracts::QualityStatus::stale, contracts::QualityStatus::gapped,
        contracts::QualityStatus::recovering, contracts::QualityStatus::invalid,
        contracts::QualityStatus::unavailable}) {
    add("non-valid activation",
        risk::RiskObligationUnavailableReason::
            RiskPolicyActivationQualityNotValid,
        [status](auto &value) {
          value.evidence.activation.quality = non_valid_quality(status);
        });
  }
  add("activation run mismatch",
      risk::RiskObligationUnavailableReason::RiskPolicyActivationRunMismatch,
      [](auto &value) {
        value.evidence.activation.run_id = id<contracts::RunId>(31);
      });
  add("activation scope mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationRiskScopeMismatch,
      [](auto &value) {
        value.evidence.activation.risk_scope_id =
            id<contracts::RiskScopeId>(81);
      });
  add("activation epoch mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationConfigurationEpochMismatch,
      [](auto &value) { value.evidence.activation.configuration_epoch = 3; });
  add("activation policy version mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationVersionMismatch,
      [](auto &value) {
        value.evidence.activation.risk_policy_version = version(86);
      });
  add("activation limit version mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationVersionMismatch,
      [](auto &value) {
        value.evidence.activation.limit_set_version = version(87);
      });
  add("activation exposure version mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationVersionMismatch,
      [](auto &value) {
        value.evidence.activation.exposure_model_version = version(88);
      });
  add("activation arithmetic version mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationVersionMismatch,
      [](auto &value) {
        value.evidence.activation.arithmetic_version = version(89);
      });
  add("activation authority version mismatch",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationVersionMismatch,
      [](auto &value) {
        value.evidence.activation.authority_version = version(90);
      });
  add("future activation sequence",
      risk::RiskObligationUnavailableReason::RiskPolicyActivationFuture,
      [](auto &value) { value.evidence.activation.sequence = 6; });
  add("future activation time",
      risk::RiskObligationUnavailableReason::RiskPolicyActivationFuture,
      [](auto &value) { value.evidence.activation.time = 121; });
  add("run context precedes missing activation",
      risk::RiskObligationUnavailableReason::RunRiskContextQualityNotValid,
      [](auto &value) {
        value.evidence.run.quality =
            non_valid_quality(contracts::QualityStatus::stale);
        value.evidence.activation_present = false;
      });
  add("activation precedes missing account",
      risk::RiskObligationUnavailableReason::
          RiskPolicyActivationQualityNotValid,
      [](auto &value) {
        value.evidence.activation.quality =
            non_valid_quality(contracts::QualityStatus::gapped);
        value.evidence.account_present = false;
      });

  for (const auto &test : cases) {
    CHECK(!test.name.empty());
    const auto first =
        evaluate(target, test.policy, risk_cut(), {}, test.evidence);
    const auto repeated =
        evaluate(target, test.policy, risk_cut(), {}, test.evidence);
    CHECK(first.completed());
    const auto *blocked = unavailable(first);
    const auto *repeated_blocked = unavailable(repeated);
    CHECK(blocked != nullptr);
    CHECK(repeated_blocked != nullptr);
    if (!blocked || !repeated_blocked)
      continue;
    CHECK(blocked->reason() == test.reason);
    CHECK(blocked->obligation_id() == repeated_blocked->obligation_id());
    CHECK(blocked->outcome_id() == repeated_blocked->outcome_id());
    CHECK(*blocked == *repeated_blocked);
    CHECK(nonzero(blocked->obligation_id()));
    CHECK(nonzero(blocked->outcome_id()));
    CHECK(!blocked->executable());
  }

  PolicySpec read_only_policy;
  read_only_policy.mode = contracts::RunMode::live_read_only;
  EvidenceSpec stale_read_only;
  stale_read_only.run.mode = contracts::RunMode::live_read_only;
  stale_read_only.run.quality =
      non_valid_quality(contracts::QualityStatus::stale);
  const auto stale =
      evaluate(target, read_only_policy, risk_cut(), {}, stale_read_only);
  CHECK(unavailable(stale) != nullptr);
  if (const auto *blocked = unavailable(stale))
    CHECK(blocked->reason() ==
          risk::RiskObligationUnavailableReason::RunRiskContextQualityNotValid);

  stale_read_only.run.quality = valid_quality();
  const auto valid =
      evaluate(target, read_only_policy, risk_cut(), {}, stale_read_only);
  CHECK(admission_rejected(valid) != nullptr);
  if (const auto *rejected = admission_rejected(valid))
    CHECK(rejected->reason() ==
          risk::RiskAdmissionRejectionReason::InadmissibleRunMode);
}

TEST_CASE("account snapshot unavailable matrix is exhaustive") {
  const auto target = target_position();
  struct SnapshotCase final {
    std::string_view name;
    risk::RiskObligationUnavailableReason reason;
    EvidenceSpec evidence;
  };
  std::vector<SnapshotCase> cases;
  const auto add = [&](std::string_view name,
                       risk::RiskObligationUnavailableReason reason,
                       auto mutate) {
    SnapshotCase value{.name = name, .reason = reason};
    mutate(value.evidence);
    cases.push_back(std::move(value));
  };
  add("missing",
      risk::RiskObligationUnavailableReason::MissingAccountRiskSnapshot,
      [](auto &evidence) { evidence.account_present = false; });
  for (const auto status :
       {contracts::QualityStatus::stale, contracts::QualityStatus::gapped,
        contracts::QualityStatus::recovering, contracts::QualityStatus::invalid,
        contracts::QualityStatus::unavailable}) {
    add("non-valid quality",
        risk::RiskObligationUnavailableReason::
            AccountRiskSnapshotQualityNotValid,
        [status](auto &evidence) {
          evidence.account.quality = non_valid_quality(status);
        });
  }
  add("wrong run",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.account.run_id = id<contracts::RunId>(31);
      });
  add("wrong portfolio",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.account.portfolio_id = id<contracts::PortfolioId>(69);
      });
  add("wrong account",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.account.account_id = id<contracts::AccountId>(72);
      });
  add("wrong scale",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotScaleMismatch,
      [](auto &evidence) {
        evidence.account.scale =
            contracts::DecimalScale::from_exponent(5).value();
      });
  add("future sequence",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotFuture,
      [](auto &evidence) { evidence.account.sequence = 6; });
  add("future time",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotFuture,
      [](auto &evidence) { evidence.account.time = 121; });
  add("one nanosecond stale",
      risk::RiskObligationUnavailableReason::AccountRiskSnapshotStale,
      [](auto &evidence) { evidence.account.time = 19; });

  for (const auto &test : cases) {
    CHECK(!test.name.empty());
    const auto result = evaluate(target, {}, risk_cut(), {}, test.evidence);
    const auto *blocked = unavailable(result);
    CHECK(blocked != nullptr);
    if (blocked)
      CHECK(blocked->reason() == test.reason);
  }

  EvidenceSpec exact_age;
  exact_age.account.time = 20;
  CHECK(decision(evaluate(target, {}, risk_cut(), {}, exact_age)) != nullptr);
}

TEST_CASE("market snapshot unavailable matrix is exhaustive") {
  const auto target = target_position();
  struct SnapshotCase final {
    std::string_view name;
    risk::RiskObligationUnavailableReason reason;
    EvidenceSpec evidence;
  };
  std::vector<SnapshotCase> cases;
  const auto add = [&](std::string_view name,
                       risk::RiskObligationUnavailableReason reason,
                       auto mutate) {
    SnapshotCase value{.name = name, .reason = reason};
    mutate(value.evidence);
    cases.push_back(std::move(value));
  };
  add("missing",
      risk::RiskObligationUnavailableReason::MissingMarketRiskSnapshot,
      [](auto &evidence) { evidence.market_present = false; });
  for (const auto status :
       {contracts::QualityStatus::stale, contracts::QualityStatus::gapped,
        contracts::QualityStatus::recovering, contracts::QualityStatus::invalid,
        contracts::QualityStatus::unavailable}) {
    add("non-valid quality",
        risk::RiskObligationUnavailableReason::
            MarketRiskSnapshotQualityNotValid,
        [status](auto &evidence) {
          evidence.market.quality = non_valid_quality(status);
        });
  }
  add("wrong run",
      risk::RiskObligationUnavailableReason::MarketRiskSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.market.run_id = id<contracts::RunId>(31);
      });
  add("wrong instrument",
      risk::RiskObligationUnavailableReason::MarketRiskSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.market.instrument_id =
            id<contracts::CanonicalInstrumentId>(43);
      });
  add("wrong listing",
      risk::RiskObligationUnavailableReason::MarketRiskSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.market.listing_id = id<contracts::ListingId>(2);
      });
  add("future sequence",
      risk::RiskObligationUnavailableReason::MarketRiskSnapshotFuture,
      [](auto &evidence) { evidence.market.sequence = 6; });
  add("future time",
      risk::RiskObligationUnavailableReason::MarketRiskSnapshotFuture,
      [](auto &evidence) { evidence.market.time = 121; });
  add("one nanosecond stale",
      risk::RiskObligationUnavailableReason::MarketRiskSnapshotStale,
      [](auto &evidence) { evidence.market.time = 19; });

  for (const auto &test : cases) {
    CHECK(!test.name.empty());
    const auto result = evaluate(target, {}, risk_cut(), {}, test.evidence);
    const auto *blocked = unavailable(result);
    CHECK(blocked != nullptr);
    if (blocked)
      CHECK(blocked->reason() == test.reason);
  }

  EvidenceSpec exact_age;
  exact_age.market.time = 20;
  CHECK(decision(evaluate(target, {}, risk_cut(), {}, exact_age)) != nullptr);
}

TEST_CASE("projected exposure snapshot unavailable matrix is exhaustive") {
  const auto target = target_position();
  struct SnapshotCase final {
    std::string_view name;
    risk::RiskObligationUnavailableReason reason;
    EvidenceSpec evidence;
  };
  std::vector<SnapshotCase> cases;
  const auto add = [&](std::string_view name,
                       risk::RiskObligationUnavailableReason reason,
                       auto mutate) {
    SnapshotCase value{.name = name, .reason = reason};
    mutate(value.evidence);
    cases.push_back(std::move(value));
  };
  add("missing",
      risk::RiskObligationUnavailableReason::MissingProjectedExposureSnapshot,
      [](auto &evidence) { evidence.projected_present = false; });
  for (const auto status :
       {contracts::QualityStatus::stale, contracts::QualityStatus::gapped,
        contracts::QualityStatus::recovering, contracts::QualityStatus::invalid,
        contracts::QualityStatus::unavailable}) {
    add("non-valid quality",
        risk::RiskObligationUnavailableReason::
            ProjectedExposureSnapshotQualityNotValid,
        [status](auto &evidence) {
          evidence.projected.quality = non_valid_quality(status);
        });
  }
  add("wrong risk scope",
      risk::RiskObligationUnavailableReason::
          ProjectedExposureSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.projected.risk_scope_id = id<contracts::RiskScopeId>(81);
      });
  add("wrong target key",
      risk::RiskObligationUnavailableReason::
          ProjectedExposureSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.projected.key = portfolio::TargetKey(
            id<contracts::PortfolioId>(69), id<contracts::AccountId>(71),
            id<contracts::CanonicalInstrumentId>(42),
            id<contracts::ListingId>(1), version(72));
      });
  add("wrong scale",
      risk::RiskObligationUnavailableReason::
          ProjectedExposureSnapshotScaleMismatch,
      [](auto &evidence) {
        evidence.projected.scale =
            contracts::DecimalScale::from_exponent(5).value();
      });
  add("future sequence",
      risk::RiskObligationUnavailableReason::ProjectedExposureSnapshotFuture,
      [](auto &evidence) { evidence.projected.sequence = 6; });
  add("future time",
      risk::RiskObligationUnavailableReason::ProjectedExposureSnapshotFuture,
      [](auto &evidence) { evidence.projected.time = 121; });
  add("one nanosecond stale",
      risk::RiskObligationUnavailableReason::ProjectedExposureSnapshotStale,
      [](auto &evidence) { evidence.projected.time = 19; });

  for (const auto &test : cases) {
    CHECK(!test.name.empty());
    const auto result = evaluate(target, {}, risk_cut(), {}, test.evidence);
    const auto *blocked = unavailable(result);
    CHECK(blocked != nullptr);
    if (blocked)
      CHECK(blocked->reason() == test.reason);
  }

  EvidenceSpec exact_age;
  exact_age.projected.time = 20;
  CHECK(decision(evaluate(target, {}, risk_cut(), {}, exact_age)) != nullptr);
}

TEST_CASE("kill switch snapshot unavailable matrix is exhaustive") {
  const auto target = target_position();
  struct SnapshotCase final {
    std::string_view name;
    risk::RiskObligationUnavailableReason reason;
    EvidenceSpec evidence;
  };
  std::vector<SnapshotCase> cases;
  const auto add = [&](std::string_view name,
                       risk::RiskObligationUnavailableReason reason,
                       auto mutate) {
    SnapshotCase value{.name = name, .reason = reason};
    mutate(value.evidence);
    cases.push_back(std::move(value));
  };
  add("missing",
      risk::RiskObligationUnavailableReason::MissingKillSwitchSnapshot,
      [](auto &evidence) { evidence.kill_switch_present = false; });
  for (const auto status :
       {contracts::QualityStatus::stale, contracts::QualityStatus::gapped,
        contracts::QualityStatus::recovering, contracts::QualityStatus::invalid,
        contracts::QualityStatus::unavailable}) {
    add("non-valid quality",
        risk::RiskObligationUnavailableReason::
            KillSwitchSnapshotQualityNotValid,
        [status](auto &evidence) {
          evidence.kill_switch.quality = non_valid_quality(status);
        });
  }
  add("wrong risk scope",
      risk::RiskObligationUnavailableReason::KillSwitchSnapshotScopeMismatch,
      [](auto &evidence) {
        evidence.kill_switch.risk_scope_id = id<contracts::RiskScopeId>(81);
      });
  add("future sequence",
      risk::RiskObligationUnavailableReason::KillSwitchSnapshotFuture,
      [](auto &evidence) { evidence.kill_switch.sequence = 6; });
  add("future time",
      risk::RiskObligationUnavailableReason::KillSwitchSnapshotFuture,
      [](auto &evidence) { evidence.kill_switch.time = 121; });
  add("one nanosecond stale",
      risk::RiskObligationUnavailableReason::KillSwitchSnapshotStale,
      [](auto &evidence) { evidence.kill_switch.time = 19; });

  for (const auto &test : cases) {
    CHECK(!test.name.empty());
    const auto result = evaluate(target, {}, risk_cut(), {}, test.evidence);
    const auto *blocked = unavailable(result);
    CHECK(blocked != nullptr);
    if (blocked)
      CHECK(blocked->reason() == test.reason);
  }

  EvidenceSpec exact_age;
  exact_age.kill_switch.time = 20;
  CHECK(decision(evaluate(target, {}, risk_cut(), {}, exact_age)) != nullptr);
}

TEST_CASE("snapshot family precedence is fixed when multiple inputs are bad") {
  const auto target = target_position();
  struct PrecedenceCase final {
    EvidenceSpec evidence;
    risk::RiskObligationUnavailableReason reason;
  };
  std::array<PrecedenceCase, 4> cases{};
  cases[0].evidence.account_present = false;
  cases[0].evidence.market_present = false;
  cases[0].evidence.projected_present = false;
  cases[0].evidence.kill_switch_present = false;
  cases[0].reason =
      risk::RiskObligationUnavailableReason::MissingAccountRiskSnapshot;
  cases[1].evidence.market_present = false;
  cases[1].evidence.projected_present = false;
  cases[1].evidence.kill_switch_present = false;
  cases[1].reason =
      risk::RiskObligationUnavailableReason::MissingMarketRiskSnapshot;
  cases[2].evidence.projected_present = false;
  cases[2].evidence.kill_switch_present = false;
  cases[2].reason =
      risk::RiskObligationUnavailableReason::MissingProjectedExposureSnapshot;
  cases[3].evidence.kill_switch_present = false;
  cases[3].reason =
      risk::RiskObligationUnavailableReason::MissingKillSwitchSnapshot;

  for (const auto &test : cases) {
    const auto result = evaluate(target, {}, risk_cut(), {}, test.evidence);
    const auto *blocked = unavailable(result);
    CHECK(blocked != nullptr);
    if (blocked)
      CHECK(blocked->reason() == test.reason);
  }
}

TEST_CASE("invalid risk policies fail before domain admission") {
  std::vector<PolicySpec> cases;
  const auto add = [&](auto mutate) {
    PolicySpec policy;
    mutate(policy);
    cases.push_back(std::move(policy));
  };
  add([](auto &policy) { policy.mode = static_cast<contracts::RunMode>(255); });
  add([](auto &policy) {
    policy.dimension_set = static_cast<risk::RiskExposureDimensionSet>(255);
  });
  add([](auto &policy) { policy.limit = -1; });
  add([](auto &policy) { policy.account_maximum_age = -1; });
  add([](auto &policy) { policy.market_maximum_age = -1; });
  add([](auto &policy) { policy.projected_maximum_age = -1; });
  add([](auto &policy) { policy.kill_switch_maximum_age = -1; });
  add([](auto &policy) { policy.decision_validity_duration = 0; });
  add([](auto &policy) { policy.decision_validity_duration = -1; });

  const auto target = target_position();
  for (const auto &policy : cases) {
    const auto result = evaluate(target, policy, risk_cut(), {}, {});
    CHECK(result.failure == risk::RiskEvaluationFailure::InvalidPolicy);
    CHECK(!result.terminal.has_value());
    CHECK(!result.completed());
  }
}

TEST_CASE("risk arithmetic inability is unavailable without authorization") {
  const auto target = target_position();
  const auto assert_arithmetic_unavailable =
      [&](const portfolio::TargetPosition &candidate, const PolicySpec &policy,
          const EvidenceSpec &evidence) {
        const auto first =
            evaluate(candidate, policy, risk_cut(), {}, evidence);
        const auto repeated =
            evaluate(candidate, policy, risk_cut(), {}, evidence);
        const auto *blocked = unavailable(first);
        const auto *repeated_blocked = unavailable(repeated);
        CHECK(blocked != nullptr);
        CHECK(repeated_blocked != nullptr);
        if (!blocked || !repeated_blocked)
          return;
        CHECK(blocked->reason() ==
              risk::RiskObligationUnavailableReason::ArithmeticUnrepresentable);
        CHECK(blocked->obligation_id() == repeated_blocked->obligation_id());
        CHECK(blocked->outcome_id() == repeated_blocked->outcome_id());
        CHECK(!blocked->executable());
      };

  EvidenceSpec subtract_overflow;
  subtract_overflow.account.current_position =
      std::numeric_limits<contracts::AmountUnits>::min();
  assert_arithmetic_unavailable(target, {}, subtract_overflow);

  EvidenceSpec add_overflow;
  add_overflow.projected.before_target =
      std::numeric_limits<contracts::AmountUnits>::max();
  assert_arithmetic_unavailable(target, {}, add_overflow);

  EvidenceSpec projected_minimum;
  projected_minimum.projected.before_target =
      std::numeric_limits<contracts::AmountUnits>::min();
  assert_arithmetic_unavailable(target, {}, projected_minimum);

  const auto minimum_target = minimum_target_position();
  CHECK(minimum_target.desired_exposure_units() ==
        std::numeric_limits<contracts::AmountUnits>::min());
  EvidenceSpec target_minimum;
  target_minimum.account.current_position = 0;
  target_minimum.projected.before_target = 1;
  assert_arithmetic_unavailable(minimum_target, {}, target_minimum);

  PolicySpec expiry_overflow;
  expiry_overflow.decision_validity_duration =
      std::numeric_limits<std::int64_t>::max();
  assert_arithmetic_unavailable(target, expiry_overflow, {});

  std::array<EvidenceSpec, 4> age_overflow{};
  age_overflow[0].account.time = std::numeric_limits<std::int64_t>::min();
  age_overflow[1].market.time = std::numeric_limits<std::int64_t>::min();
  age_overflow[2].projected.time = std::numeric_limits<std::int64_t>::min();
  age_overflow[3].kill_switch.time = std::numeric_limits<std::int64_t>::min();
  for (const auto &evidence : age_overflow)
    assert_arithmetic_unavailable(target, {}, evidence);
}

TEST_CASE("projected exposure accepts exact positive and negative limits") {
  const auto target = target_position();

  EvidenceSpec positive;
  positive.projected.before_target = 70;
  const auto positive_result = evaluate(target, {}, risk_cut(), {}, positive);
  const auto *positive_decision = decision(positive_result);
  CHECK(positive_decision != nullptr);
  if (positive_decision) {
    CHECK(positive_decision->requested_projected_exposure_units() == 100);
    CHECK(positive_decision->disposition() ==
          risk::RiskDecisionDisposition::Approved);
    CHECK(positive_decision->binding_reason() ==
          risk::RiskDecisionReason::WithinPermissiveLimit);
    CHECK(positive_decision->authorizes_target());
  }

  EvidenceSpec negative;
  negative.account.current_position = 100;
  negative.projected.before_target = -40;
  const auto negative_result = evaluate(target, {}, risk_cut(), {}, negative);
  const auto *negative_decision = decision(negative_result);
  CHECK(negative_decision != nullptr);
  if (negative_decision) {
    CHECK(negative_decision->requested_delta_units() == -60);
    CHECK(negative_decision->requested_projected_exposure_units() == -100);
    CHECK(negative_decision->disposition() ==
          risk::RiskDecisionDisposition::Approved);
    CHECK(negative_decision->binding_reason() ==
          risk::RiskDecisionReason::WithinPermissiveLimit);
    CHECK(negative_decision->authorizes_target());
  }
}

TEST_CASE("risk identities are deterministic and bind semantic ownership") {
  const auto target = target_position();
  const auto baseline_result = evaluate(target, {}, risk_cut(), {}, {});
  const auto *baseline = decision(baseline_result);
  CHECK(baseline != nullptr);
  if (!baseline)
    return;

  struct IdentityScenario final {
    std::string_view name;
    bool obligation_changes;
    PolicySpec policy;
    risk::RiskEvaluationCut cut{risk_cut()};
    AdmissionSpec admission;
    EvidenceSpec evidence;
  };
  std::vector<IdentityScenario> scenarios;
  const auto add = [&](std::string_view name, bool obligation_changes,
                       auto mutate) {
    IdentityScenario value{.name = name,
                           .obligation_changes = obligation_changes};
    mutate(value);
    scenarios.push_back(std::move(value));
  };

  add("risk scope", true, [](auto &value) {
    value.policy.risk_scope_id = id<contracts::RiskScopeId>(81);
    value.evidence.activation.risk_scope_id = id<contracts::RiskScopeId>(81);
    value.evidence.projected.risk_scope_id = id<contracts::RiskScopeId>(81);
    value.evidence.kill_switch.risk_scope_id = id<contracts::RiskScopeId>(81);
  });
  add("run mode", true, [](auto &value) {
    value.policy.mode = contracts::RunMode::replay;
    value.evidence.run.mode = contracts::RunMode::replay;
  });
  add("risk policy version", true, [](auto &value) {
    value.policy.risk_policy_version = version(86);
    value.evidence.activation.risk_policy_version = version(86);
  });
  add("limit set version", true, [](auto &value) {
    value.policy.limit_set_version = version(87);
    value.evidence.activation.limit_set_version = version(87);
  });
  add("exposure model version", true, [](auto &value) {
    value.policy.exposure_model_version = version(88);
    value.evidence.activation.exposure_model_version = version(88);
  });
  add("arithmetic version", true, [](auto &value) {
    value.policy.arithmetic_version = version(89);
    value.evidence.activation.arithmetic_version = version(89);
  });
  add("authority version", true, [](auto &value) {
    value.policy.authority_version = version(90);
    value.evidence.activation.authority_version = version(90);
  });
  add("limit", true, [](auto &value) { value.policy.limit = 101; });
  add("account maximum age", true,
      [](auto &value) { value.policy.account_maximum_age = 101; });
  add("market maximum age", true,
      [](auto &value) { value.policy.market_maximum_age = 101; });
  add("projected maximum age", true,
      [](auto &value) { value.policy.projected_maximum_age = 101; });
  add("kill maximum age", true,
      [](auto &value) { value.policy.kill_switch_maximum_age = 101; });
  add("decision duration", true,
      [](auto &value) { value.policy.decision_validity_duration = 40; });
  add("modification policy", true,
      [](auto &value) { value.policy.modification_enabled = false; });
  add("cut sequence", true,
      [](auto &value) { value.cut = risk::RiskEvaluationCut(6, 120); });
  add("cut time", true,
      [](auto &value) { value.cut = risk::RiskEvaluationCut(5, 121); });
  add("publication identity", true, [](auto &value) {
    value.admission.publication_attempt_id =
        id<contracts::PublicationAttemptId>(89);
    value.admission.acknowledgement_publication_id =
        id<contracts::PublicationAttemptId>(89);
  });
  add("publication sequence", true,
      [](auto &value) { value.admission.publication_sequence = 1; });
  add("publication time", true,
      [](auto &value) { value.admission.publication_time = 104; });
  add("acknowledgement boundary", false, [](auto &value) {
    value.admission.acknowledgement_boundary_id =
        id<contracts::ConsumerBoundaryId>(92);
  });
  add("acknowledgement sequence", false,
      [](auto &value) { value.admission.acknowledgement_sequence = 4; });
  add("acknowledgement time", false,
      [](auto &value) { value.admission.acknowledgement_time = 111; });
  add("lifecycle event", false, [](auto &value) {
    value.admission.lifecycle_event_id = id<contracts::EventId>(93);
  });
  add("lifecycle sequence", false,
      [](auto &value) { value.admission.lifecycle_sequence = 5; });
  add("lifecycle time", false,
      [](auto &value) { value.admission.lifecycle_time = 116; });
  add("manifest identity", false, [](auto &value) {
    value.evidence.run.manifest_id = id<contracts::IntegrityId>(92);
  });
  add("replay class", false, [](auto &value) {
    value.evidence.run.replay_class = risk::RiskReplayClass::Synthetic;
  });
  add("replay evidence identity", false, [](auto &value) {
    value.evidence.run.replay_evidence_id = id<contracts::IntegrityId>(95);
  });
  add("run context sequence", false,
      [](auto &value) { value.evidence.run.sequence = 2; });
  add("run context time", false,
      [](auto &value) { value.evidence.run.time = 101; });
  add("activation identity", false, [](auto &value) {
    value.evidence.activation.event_id = id<contracts::EventId>(96);
  });
  add("activation sequence", false,
      [](auto &value) { value.evidence.activation.sequence = 3; });
  add("activation time", false,
      [](auto &value) { value.evidence.activation.time = 106; });
  add("account identity", false, [](auto &value) {
    value.evidence.account.state_view_id = id<contracts::StateViewId>(95);
  });
  add("account position", false,
      [](auto &value) { value.evidence.account.current_position = 11; });
  add("available capital", false,
      [](auto &value) { value.evidence.account.available_capital = 1'001; });
  add("account sequence", false,
      [](auto &value) { value.evidence.account.sequence = 4; });
  add("account time", false,
      [](auto &value) { value.evidence.account.time = 111; });
  add("account trading state", false,
      [](auto &value) { value.evidence.account.trading_enabled = false; });
  add("market identity", false, [](auto &value) {
    value.evidence.market.state_view_id = id<contracts::StateViewId>(98);
  });
  add("market sequence", false,
      [](auto &value) { value.evidence.market.sequence = 4; });
  add("market time", false,
      [](auto &value) { value.evidence.market.time = 111; });
  add("market tradeable state", false,
      [](auto &value) { value.evidence.market.tradeable = false; });
  add("projected identity", false, [](auto &value) {
    value.evidence.projected.exposure_id =
        id<contracts::ProjectedExposureId>(99);
  });
  add("projected amount", false,
      [](auto &value) { value.evidence.projected.before_target = 16; });
  add("risk sequence", false,
      [](auto &value) { value.evidence.projected.risk_sequence = 124; });
  add("projected sequence", false,
      [](auto &value) { value.evidence.projected.sequence = 4; });
  add("projected time", false,
      [](auto &value) { value.evidence.projected.time = 111; });
  add("kill switch identity", false, [](auto &value) {
    value.evidence.kill_switch.state_view_id = id<contracts::StateViewId>(100);
  });
  add("kill switch sequence", false,
      [](auto &value) { value.evidence.kill_switch.sequence = 4; });
  add("kill switch time", false,
      [](auto &value) { value.evidence.kill_switch.time = 111; });
  add("kill switch state", false,
      [](auto &value) { value.evidence.kill_switch.enabled = true; });

  for (const auto &test : scenarios) {
    CHECK(!test.name.empty());
    const auto result =
        evaluate(target, test.policy, test.cut, test.admission, test.evidence);
    const auto *changed = decision(result);
    CHECK(changed != nullptr);
    if (!changed)
      continue;
    if (test.obligation_changes)
      CHECK(changed->obligation_id() != baseline->obligation_id());
    else
      CHECK(changed->obligation_id() == baseline->obligation_id());
    CHECK(changed->decision_id() != baseline->decision_id());
    CHECK(changed->outcome_id() != baseline->outcome_id());
  }

  const auto changed_target = target_position(41);
  const auto changed_target_result =
      evaluate(changed_target, {}, risk_cut(), {}, {});
  const auto *changed_target_decision = decision(changed_target_result);
  CHECK(changed_target_decision != nullptr);
  if (changed_target_decision) {
    CHECK(changed_target_decision->obligation_id() !=
          baseline->obligation_id());
    CHECK(changed_target_decision->decision_id() != baseline->decision_id());
    CHECK(changed_target_decision->outcome_id() != baseline->outcome_id());
  }
}

TEST_CASE("risk identity ignores object address and evaluation call order") {
  const auto first_target = target_position();
  const auto second_target = target_position();
  CHECK(&first_target != &second_target);
  CHECK(first_target == second_target);

  const auto first_result = evaluate(first_target, {}, risk_cut(), {}, {});
  EvidenceSpec intervening_evidence;
  intervening_evidence.kill_switch.enabled = true;
  const auto intervening =
      evaluate(first_target, {}, risk_cut(), {}, intervening_evidence);
  const auto second_result = evaluate(second_target, {}, risk_cut(), {}, {});
  const auto *first = decision(first_result);
  const auto *middle = decision(intervening);
  const auto *second = decision(second_result);
  CHECK(first != nullptr);
  CHECK(middle != nullptr);
  CHECK(second != nullptr);
  if (!first || !middle || !second)
    return;
  CHECK(*first == *second);
  CHECK(first->obligation_id() == second->obligation_id());
  CHECK(first->decision_id() == second->decision_id());
  CHECK(first->outcome_id() == second->outcome_id());
  CHECK(middle->decision_id() != first->decision_id());
  CHECK(middle->outcome_id() != first->outcome_id());
}

TEST_CASE("all-zero digest projection remains a valid opaque risk identity") {
  const contracts::Sha256Digest all_zero{};
  const auto scope = contracts::RiskScopeId::from_sha256_digest(all_zero);
  const auto obligation =
      contracts::RiskObligationId::from_sha256_digest(all_zero);
  const auto decision_id =
      contracts::RiskDecisionId::from_sha256_digest(all_zero);
  const auto outcome =
      contracts::RiskEvaluationOutcomeId::from_sha256_digest(all_zero);
  const auto projected =
      contracts::ProjectedExposureId::from_sha256_digest(all_zero);
  CHECK(nonzero(scope));
  CHECK(nonzero(obligation));
  CHECK(nonzero(decision_id));
  CHECK(nonzero(outcome));
  CHECK(nonzero(projected));
  CHECK(scope.bytes().back() == 1);
  CHECK(obligation.bytes().back() == 1);
  CHECK(decision_id.bytes().back() == 1);
  CHECK(outcome.bytes().back() == 1);
  CHECK(projected.bytes().back() == 1);
}

TEST_CASE("portfolio runtime fixture keeps payload and identity consistent") {
  const auto first =
      chronos::test_support::positive_portfolio_recommendation(40);
  const auto repeated =
      chronos::test_support::positive_portfolio_recommendation(40);
  const auto different =
      chronos::test_support::positive_portfolio_recommendation(41);
  const auto &first_action =
      std::get<chronos::core::recommendation::ActionableRecommendation>(
          first.outcome());
  const auto &different_action =
      std::get<chronos::core::recommendation::ActionableRecommendation>(
          different.outcome());

  CHECK(first_action.indicative_exposure_units == 40);
  CHECK(different_action.indicative_exposure_units == 41);
  CHECK(first.recommendation_id() == repeated.recommendation_id());
  CHECK(first.recommendation_id() != different.recommendation_id());
}

TEST_CASE("risk authority approves a complete in-limit target") {
  const auto target = target_position();
  CHECK(target.current_exposure_units() == 10);
  CHECK(target.desired_exposure_units() == 40);

  const auto result = evaluate(target);
  CHECK(result.completed());
  const auto *approved = decision(result);
  CHECK(approved != nullptr);
  if (!approved)
    return;

  CHECK(approved->disposition() == risk::RiskDecisionDisposition::Approved);
  CHECK(approved->target_position_id() == target.target_position_id());
  CHECK(approved->target_key() == target.key());
  CHECK(approved->account_current_position_units() == 10);
  CHECK(approved->requested_target_units() == 40);
  CHECK(approved->requested_delta_units() == 30);
  CHECK(approved->projected_exposure_before_target_units() == 15);
  CHECK(approved->requested_projected_exposure_units() == 45);
  CHECK(approved->authorized_target_units() == 40);
  CHECK(approved->authorized_delta_units() == 30);
  CHECK(approved->authorized_projected_exposure_units() == 45);
  CHECK(approved->authorizes_target());
  CHECK(approved->risk_sequence() == 123);
  CHECK(approved->logical_expiry_nanoseconds() == 150);
  CHECK(approved->issue_cut() == risk_cut());
  CHECK(approved->run_mode() == contracts::RunMode::live_paper);
  CHECK(approved->binding_reason() ==
        risk::RiskDecisionReason::WithinPermissiveLimit);
  CHECK(approved->rule_findings().empty());
  CHECK(!approved->reduction_proof().has_value());

  CHECK(approved->risk_scope_id() == id<contracts::RiskScopeId>(80));
  CHECK(approved->risk_policy_version() == version(81));
  CHECK(approved->limit_set_version() == version(82));
  CHECK(approved->exposure_model_version() == version(83));
  CHECK(approved->arithmetic_version() == version(84));
  CHECK(approved->authority_version() == version(85));
  CHECK(approved->target_schema_version() == version(74));
  CHECK(approved->run_manifest_integrity_id() ==
        id<contracts::IntegrityId>(93));
  CHECK(approved->replay_evidence_id() == id<contracts::IntegrityId>(94));
  CHECK(approved->policy_activation_event_id() == id<contracts::EventId>(95));
  CHECK(approved->account_state_view_id() == id<contracts::StateViewId>(96));
  CHECK(approved->market_state_view_id() == id<contracts::StateViewId>(97));
  CHECK(approved->projected_exposure_id() ==
        id<contracts::ProjectedExposureId>(98));
  CHECK(approved->kill_switch_state_view_id() ==
        id<contracts::StateViewId>(99));
}

TEST_CASE("risk authority rejects each known semantic safety state") {
  const auto target = target_position();
  const std::array cases{
      std::pair{EvaluationSpec{.kill_switch_enabled = true},
                risk::RiskDecisionReason::KillSwitchEnabled},
      std::pair{EvaluationSpec{.trading_enabled = false},
                risk::RiskDecisionReason::AccountTradingDisabled},
      std::pair{EvaluationSpec{.market_tradeable = false},
                risk::RiskDecisionReason::MarketNotTradeable},
      std::pair{EvaluationSpec{.account_current_position = 40},
                risk::RiskDecisionReason::TargetAlreadySatisfied},
  };

  for (const auto &[spec, reason] : cases) {
    const auto result = evaluate(target, spec);
    CHECK(result.completed());
    const auto *rejected = decision(result);
    CHECK(rejected != nullptr);
    if (!rejected)
      continue;
    check_rejected(*rejected, reason, mt_fail_count);
    CHECK(rejected->rule_findings().size() == 1);
    if (rejected->rule_findings().size() == 1)
      CHECK(rejected->rule_findings()[0].reason() == reason);
  }
}

TEST_CASE("risk authority retains all simultaneous findings in policy order") {
  const auto target = target_position();
  const EvaluationSpec spec{.account_current_position = 40,
                            .projected_exposure_before_target = 130,
                            .trading_enabled = false,
                            .market_tradeable = false,
                            .kill_switch_enabled = true};
  const auto result = evaluate(target, spec);
  const auto *rejected = decision(result);
  CHECK(rejected != nullptr);
  if (!rejected)
    return;

  check_rejected(*rejected, risk::RiskDecisionReason::KillSwitchEnabled,
                 mt_fail_count);
  const std::array expected{
      risk::RiskDecisionReason::KillSwitchEnabled,
      risk::RiskDecisionReason::AccountTradingDisabled,
      risk::RiskDecisionReason::MarketNotTradeable,
      risk::RiskDecisionReason::TargetAlreadySatisfied,
      risk::RiskDecisionReason::ProjectedExposureLimitExceeded,
  };
  CHECK(rejected->rule_findings().size() == expected.size());
  if (rejected->rule_findings().size() != expected.size())
    return;
  for (std::size_t index = 0; index < expected.size(); ++index)
    CHECK(rejected->rule_findings()[index].reason() == expected[index]);
}

TEST_CASE("risk authority retains exposure limit before clamp no movement") {
  const auto target = target_position();
  const EvaluationSpec spec{.projected_exposure_before_target = 100};
  const auto result = evaluate(target, spec);
  const auto *rejected = decision(result);
  CHECK(rejected != nullptr);
  if (!rejected)
    return;

  check_rejected(*rejected,
                 risk::RiskDecisionReason::ProjectedExposureLimitExceeded,
                 mt_fail_count);
  CHECK(rejected->requested_delta_units() == 30);
  CHECK(rejected->requested_projected_exposure_units() == 130);
  const std::array expected{
      risk::RiskDecisionReason::ProjectedExposureLimitExceeded,
      risk::RiskDecisionReason::ModifiedTargetWouldNotChangeExposure,
  };
  CHECK(rejected->rule_findings().size() == expected.size());
  if (rejected->rule_findings().size() != expected.size())
    return;
  for (std::size_t index = 0; index < expected.size(); ++index)
    CHECK(rejected->rule_findings()[index].reason() == expected[index]);
}

TEST_CASE("risk authority keeps exposure reason when clamp grows movement") {
  const auto target = target_position();
  const EvaluationSpec spec{.projected_exposure_before_target = -150};
  const auto result = evaluate(target, spec);
  const auto *rejected = decision(result);
  CHECK(rejected != nullptr);
  if (!rejected)
    return;

  check_rejected(*rejected,
                 risk::RiskDecisionReason::ProjectedExposureLimitExceeded,
                 mt_fail_count);
  CHECK(rejected->requested_delta_units() == 30);
  CHECK(rejected->requested_projected_exposure_units() == -120);
  CHECK(rejected->rule_findings().size() == 1);
  if (rejected->rule_findings().size() == 1)
    CHECK(rejected->rule_findings()[0].reason() ==
          risk::RiskDecisionReason::ProjectedExposureLimitExceeded);
}

TEST_CASE("risk authority proof-checks positive exposure modification") {
  const auto target = target_position();
  const EvaluationSpec spec{.projected_exposure_before_target = 90};
  const auto result = evaluate(target, spec);
  const auto *modified = decision(result);
  CHECK(modified != nullptr);
  if (!modified)
    return;

  CHECK(modified->disposition() == risk::RiskDecisionDisposition::Modified);
  CHECK(modified->requested_delta_units() == 30);
  CHECK(modified->requested_projected_exposure_units() == 120);
  CHECK(modified->authorized_projected_exposure_units() == 100);
  CHECK(modified->authorized_delta_units() == 10);
  CHECK(modified->authorized_target_units() == 20);
  CHECK(modified->authorizes_target());
  CHECK(modified->binding_reason() ==
        risk::RiskDecisionReason::ClampedToProjectedExposureLimit);
  CHECK(modified->reduction_proof().has_value());
  if (!modified->reduction_proof())
    return;
  const auto &proof = *modified->reduction_proof();
  CHECK(proof.target_key() == target.key());
  CHECK(proof.exposure_scale() == exposure_scale());
  CHECK(proof.exposure_dimension_set() ==
        risk::RiskExposureDimensionSet::QuantityOnlyV1);
  CHECK(proof.requested_target_units() == 40);
  CHECK(proof.authorized_target_units() == 20);
  CHECK(proof.requested_delta_units() == 30);
  CHECK(proof.authorized_delta_units() == 10);
  CHECK(proof.requested_absolute_projected_exposure_units() == 120);
  CHECK(proof.authorized_absolute_projected_exposure_units() == 100);
  CHECK(proof.maximum_absolute_projected_exposure_units() == 100);
}

TEST_CASE("risk authority proof-checks negative exposure modification") {
  const auto target = target_position();
  const EvaluationSpec spec{.account_current_position = 100,
                            .projected_exposure_before_target = -70};
  const auto result = evaluate(target, spec);
  const auto *modified = decision(result);
  CHECK(modified != nullptr);
  if (!modified)
    return;

  CHECK(modified->disposition() == risk::RiskDecisionDisposition::Modified);
  CHECK(modified->requested_delta_units() == -60);
  CHECK(modified->requested_projected_exposure_units() == -130);
  CHECK(modified->authorized_projected_exposure_units() == -100);
  CHECK(modified->authorized_delta_units() == -30);
  CHECK(modified->authorized_target_units() == 70);
  CHECK(modified->authorizes_target());
  CHECK(modified->reduction_proof().has_value());
  if (!modified->reduction_proof())
    return;
  const auto &proof = *modified->reduction_proof();
  CHECK(proof.requested_absolute_projected_exposure_units() == 130);
  CHECK(proof.authorized_absolute_projected_exposure_units() == 100);
  CHECK(proof.requested_delta_units() == -60);
  CHECK(proof.authorized_delta_units() == -30);
}

TEST_CASE(
    "risk authority rejects over-limit target when modification disabled") {
  const auto target = target_position();
  const EvaluationSpec spec{.projected_exposure_before_target = 90,
                            .modification_enabled = false};
  const auto result = evaluate(target, spec);
  const auto *rejected = decision(result);
  CHECK(rejected != nullptr);
  if (!rejected)
    return;

  check_rejected(*rejected,
                 risk::RiskDecisionReason::ProjectedExposureLimitExceeded,
                 mt_fail_count);
  CHECK(rejected->rule_findings().size() == 1);
  if (rejected->rule_findings().size() == 1)
    CHECK(rejected->rule_findings()[0].reason() ==
          risk::RiskDecisionReason::ProjectedExposureLimitExceeded);
}
