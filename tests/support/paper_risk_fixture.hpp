#pragma once
#include "chronos/core/risk/risk_decision.hpp"
#include "portfolio_runtime_fixture.hpp"
#include <array>
#include <cstdlib>
namespace chronos::test_support::paper {
namespace contracts = chronos::contracts;
namespace portfolio = chronos::core::portfolio;
namespace risk = chronos::core::risk;
template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}
inline portfolio::TargetKey target_key() {
  return portfolio::TargetKey(
      id<contracts::PortfolioId>(70), id<contracts::AccountId>(71),
      id<contracts::CanonicalInstrumentId>(42), id<contracts::ListingId>(1),
      contracts::VersionRef::from(id<contracts::DefinitionId>(72), 1).value());
}

inline contracts::VersionRef version(std::uint8_t seed) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), 1)
      .value();
}

inline contracts::DecimalScale exposure_scale() {
  return contracts::DecimalScale::from_exponent(6).value();
}

inline contracts::DataQuality valid_quality() {
  return contracts::DataQuality::from(contracts::QualityStatus::valid, 0)
      .value();
}

inline contracts::DataQuality
non_valid_quality(contracts::QualityStatus status) {
  return contracts::DataQuality::from(status, 1).value();
}

struct TargetSnapshotSpec final {
  contracts::PortfolioSnapshotId snapshot_id{
      id<contracts::PortfolioSnapshotId>(73)};
  contracts::AmountUnits current_exposure{0};
  std::uint64_t sequence{1};
  std::int64_t time{100};
  std::uint64_t configuration_epoch{2};
  portfolio::PortfolioSnapshotDisposition disposition{
      portfolio::PortfolioSnapshotDisposition::FreshComplete};
  bool paper_transition_assumption{true};
};

inline portfolio::PortfolioConstructionResult
construct_target(const TargetSnapshotSpec &snapshot_spec,
                 contracts::AmountUnits desired_exposure = 40) {
  const auto key = target_key();
  const portfolio::PortfolioStateSnapshot snapshot(
      snapshot_spec.snapshot_id, id<contracts::RunId>(30), key.portfolio_id(),
      key.account_id(), key.canonical_instrument_id(), key.listing_id(),
      snapshot_spec.current_exposure, exposure_scale(), snapshot_spec.sequence,
      snapshot_spec.time, snapshot_spec.configuration_epoch,
      snapshot_spec.disposition, snapshot_spec.paper_transition_assumption);
  const portfolio::PortfolioConstructionPolicy policy(
      version(74), version(75), version(76), version(77), key,
      id<contracts::RunId>(30), {id<contracts::StrategyInstanceId>(98)},
      exposure_scale(), 1, 4, 100, 50);
  const portfolio::PortfolioConstructionCut cut(1, 100);
  const std::array recommendations{
      chronos::test_support::positive_portfolio_recommendation(
          desired_exposure)};
  return portfolio::PortfolioConstructionAuthority::construct(
      recommendations, snapshot, policy, cut);
}

inline portfolio::TargetPosition
target_position(const TargetSnapshotSpec &snapshot_spec,
                contracts::AmountUnits desired_exposure = 40) {
  auto result = construct_target(snapshot_spec, desired_exposure);
  if (!result.completed() || !result.terminal ||
      !std::holds_alternative<portfolio::TargetPosition>(*result.terminal))
    std::abort();
  return std::get<portfolio::TargetPosition>(std::move(*result.terminal));
}

inline portfolio::TargetPosition
target_position(contracts::AmountUnits desired_exposure = 40) {
  return target_position(TargetSnapshotSpec{}, desired_exposure);
}

struct EvaluationSpec final {
  contracts::AmountUnits account_current_position{0};
  contracts::AmountUnits projected_exposure_before_target{15};
  contracts::AmountUnits limit{100};
  bool trading_enabled{true};
  bool market_tradeable{true};
  bool kill_switch_enabled{false};
  bool modification_enabled{true};
};

inline risk::MinimalRiskPolicy risk_policy(const EvaluationSpec &spec) {
  return risk::MinimalRiskPolicy(
      id<contracts::RiskScopeId>(80), target_key(), id<contracts::RunId>(30),
      contracts::RunMode::backtest, version(81), version(82), version(83),
      version(84), version(85), version(74), exposure_scale(),
      risk::RiskExposureDimensionSet::QuantityOnlyV1, spec.limit, 100, 100, 100,
      100, 50, spec.modification_enabled);
}

inline risk::RiskEvaluationCut risk_cut() {
  return risk::RiskEvaluationCut(5, 120);
}

inline risk::TargetAdmissionEvidence
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

inline risk::RiskEvaluationEvidence
evaluation_evidence(const EvaluationSpec &spec,
                    const risk::ProjectedExposureSnapshot &projected) {
  const auto key = target_key();
  return risk::RiskEvaluationEvidence(
      risk::RunRiskContext(
          id<contracts::IntegrityId>(93), id<contracts::RunId>(30),
          contracts::RunMode::backtest, risk::RiskReplayClass::Faithful,
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
      projected,
      risk::KillSwitchSnapshot(id<contracts::StateViewId>(99),
                               id<contracts::RiskScopeId>(80), valid_quality(),
                               3, 110, spec.kill_switch_enabled));
}

inline risk::RiskDecision
decision(const risk::ProjectedExposureSnapshot &snapshot,
         contracts::AmountUnits desired = 40, EvaluationSpec spec = {}) {
  const auto target = target_position(desired);
  auto result = risk::MinimalRiskAuthority::evaluate(
      target, risk_policy(spec), risk_cut(), admission_evidence(target),
      evaluation_evidence(spec, snapshot));
  if (!result.terminal ||
      !std::holds_alternative<risk::RiskDecision>(*result.terminal))
    std::abort();
  return std::get<risk::RiskDecision>(*result.terminal);
}
} // namespace chronos::test_support::paper
