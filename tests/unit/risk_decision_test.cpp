#include "chronos/core/risk/risk_decision.hpp"

#include "microtest.hpp"
#include "portfolio_runtime_fixture.hpp"

#include <array>
#include <concepts>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <variant>

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

portfolio::TargetPosition target_position() {
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
      chronos::test_support::positive_portfolio_recommendation(40)};
  auto result = portfolio::PortfolioConstructionAuthority::construct(
      recommendations, snapshot, policy, cut);
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
