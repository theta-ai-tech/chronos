#include "chronos/core/risk/risk_decision.hpp"

#include "microtest.hpp"

#include <concepts>
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
      id<contracts::PortfolioId>(1), id<contracts::AccountId>(2),
      id<contracts::CanonicalInstrumentId>(3), id<contracts::ListingId>(4),
      contracts::VersionRef::from(id<contracts::DefinitionId>(5), 1).value());
}

} // namespace

TEST_CASE("risk decision contract preserves distinct terminal types") {
  const auto key = target_key();
  CHECK(key.portfolio_id() == id<contracts::PortfolioId>(1));
  CHECK(key.account_id() == id<contracts::AccountId>(2));
  CHECK(!risk::RiskEvaluationResult{}.completed());
}
