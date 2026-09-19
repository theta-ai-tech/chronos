#pragma once
#include "chronos/contracts/event_envelope.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/state_lineage.hpp"
namespace chronos::core::execution_planning {
class PaperIntentAuthority;
}
namespace chronos::contracts {
enum class PaperSide : std::uint8_t { Buy, Sell };
// D0 evidence is a current synchronous cut, not a cached risk market snapshot.
struct PaperExecutionEvidence final {
  RunId run_id;
  PortfolioId portfolio_id;
  AccountId account_id;
  CanonicalInstrumentId canonical_instrument_id;
  ListingId listing_id;
  VersionRef quote_currency;
  RunMode run_mode;
  StateViewId market_state_view_id;
  StateLineage market_lineage;
  std::uint64_t run_input_sequence;
  std::int64_t logical_time_nanoseconds;
  std::int64_t logical_expiry_nanoseconds;
  DataQuality quality;
  bool kill_switch_permits;
  bool market_tradeable;
  VersionRef execution_policy_version;
  VersionRef broker_model_version;
  DecimalScale price_scale;
  DecimalScale quantity_scale;
  DecimalScale money_scale;
  AmountUnits bid_price_units;
  AmountUnits ask_price_units;
  AmountUnits price_tick_units;
  AmountUnits quantity_step_units;
  bool operator==(const PaperExecutionEvidence &) const = default;
};
struct PaperIntentFacts final {
  ExecutableOrderIntentId intent_id;
  TargetPositionId target_position_id;
  RiskDecisionId risk_decision_id;
  ReservationId reservation_id;
  RiskScopeId risk_scope_id;
  ProjectedExposureId projected_exposure_id;
  std::uint64_t risk_sequence;
  std::uint64_t risk_issue_sequence;
  std::int64_t risk_issue_time_nanoseconds;
  EventId risk_policy_activation_event_id;
  StateViewId risk_account_state_view_id;
  StateViewId risk_market_state_view_id;
  StateViewId risk_kill_switch_state_view_id;
  VersionRef risk_policy_version;
  VersionRef limit_set_version;
  VersionRef exposure_model_version;
  VersionRef arithmetic_version;
  VersionRef risk_authority_version;
  VersionRef target_schema_version;
  IntegrityId run_manifest_integrity_id;
  IntegrityId replay_evidence_id;
  PaperExecutionEvidence execution;
  std::uint64_t issue_sequence;
  std::int64_t issue_time_nanoseconds;
  std::int64_t logical_expiry_nanoseconds;
  AmountUnits authorized_delta_units;
  PaperSide side;
  AmountUnits quantity_units;
  bool operator==(const PaperIntentFacts &) const = default;
};
// Only execution authority may mint this fact; DTO copies cannot forge it.
class PaperIntent final {
public:
  [[nodiscard]] const PaperIntentFacts &facts() const noexcept {
    return facts_;
  }
  bool operator==(const PaperIntent &) const = default;

private:
  explicit PaperIntent(PaperIntentFacts facts) : facts_(std::move(facts)) {}
  PaperIntentFacts facts_;
  friend class chronos::core::execution_planning::PaperIntentAuthority;
};
} // namespace chronos::contracts
