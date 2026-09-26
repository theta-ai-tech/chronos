#include "chronos/core/execution_planning/paper_intent.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace chronos::core::execution_planning {
std::optional<contracts::AmountUnits>
paper_quantity(contracts::AmountUnits delta) noexcept {
  if (delta == 0 || delta == std::numeric_limits<contracts::AmountUnits>::min())
    return {};
  return delta < 0 ? -delta : delta;
}
PaperIntentResult PaperIntentAuthority::create(
    contracts::ExecutableOrderIntentId request_id,
    const risk::RiskDecision &decision,
    risk::ReservationAuthority &reservations,
    contracts::ReservationId reservation_id,
    const std::optional<contracts::PaperExecutionEvidence> &evidence,
    const risk::RiskEvaluationCut &cut) {
  for (const auto &request : requests_) {
    if (request.id == request_id) {
      if (request.decision == decision && request.authority == &reservations &&
          request.reservation == reservation_id &&
          request.evidence == evidence && request.cut == cut)
        return request.result;
      return {PaperIntentFailure::RequestConflict, {}};
    }
  }
  requests_.reserve(requests_.size() + 1);
  const auto remember = [&](PaperIntentResult result) {
    requests_.push_back({request_id, decision, &reservations, reservation_id,
                         evidence, cut, result});
    return result;
  };
  if (!decision.authorizes_target() || !decision.authorized_delta_units() ||
      decision.run_mode() != contracts::RunMode::backtest)
    return remember({PaperIntentFailure::RiskRejected, {}});
  const auto reservation = reservations.find(reservation_id);
  if (!reservation || reservation->state() != risk::ReservationState::Held ||
      reservation->decision() != decision)
    return remember({PaperIntentFailure::InvalidReservation, {}});
  const auto quantity = paper_quantity(*decision.authorized_delta_units());
  if (!evidence || !quantity)
    return remember({PaperIntentFailure::InvalidEvidence, {}});
  const auto &e = *evidence;
  const auto &key = decision.target_key();
  if (e.run_mode != contracts::RunMode::backtest ||
      e.run_id != decision.run_id() || e.portfolio_id != key.portfolio_id() ||
      e.account_id != key.account_id() ||
      e.canonical_instrument_id != key.canonical_instrument_id() ||
      e.listing_id != key.listing_id() ||
      e.quote_currency != reservations.quote_currency() ||
      e.quantity_scale != decision.exposure_scale() ||
      e.market_lineage.run_id() != e.run_id ||
      e.market_lineage.run_input_sequence() != e.run_input_sequence ||
      e.run_input_sequence != cut.run_input_sequence() ||
      e.logical_time_nanoseconds != cut.logical_time_nanoseconds() ||
      cut.run_input_sequence() < decision.issue_cut().run_input_sequence() ||
      cut.logical_time_nanoseconds() <
          decision.issue_cut().logical_time_nanoseconds() ||
      cut.logical_time_nanoseconds() >= decision.logical_expiry_nanoseconds() ||
      cut.logical_time_nanoseconds() >= e.logical_expiry_nanoseconds ||
      e.quality.status() != contracts::QualityStatus::valid ||
      !e.kill_switch_permits || !e.market_tradeable || e.bid_price_units <= 0 ||
      e.ask_price_units < e.bid_price_units || e.price_tick_units <= 0 ||
      e.quantity_step_units <= 0 ||
      e.bid_price_units % e.price_tick_units != 0 ||
      e.ask_price_units % e.price_tick_units != 0 ||
      *quantity % e.quantity_step_units != 0)
    return remember({PaperIntentFailure::InvalidEvidence, {}});
  PaperIntentResult result{
      PaperIntentFailure::None,
      contracts::PaperIntent({request_id,
                              decision.target_position_id(),
                              decision.decision_id(),
                              reservation_id,
                              decision.risk_scope_id(),
                              decision.projected_exposure_id(),
                              decision.risk_sequence(),
                              decision.issue_cut().run_input_sequence(),
                              decision.issue_cut().logical_time_nanoseconds(),
                              decision.policy_activation_event_id(),
                              decision.account_state_view_id(),
                              decision.market_state_view_id(),
                              decision.kill_switch_state_view_id(),
                              decision.risk_policy_version(),
                              decision.limit_set_version(),
                              decision.exposure_model_version(),
                              decision.arithmetic_version(),
                              decision.authority_version(),
                              decision.target_schema_version(),
                              decision.run_manifest_integrity_id(),
                              decision.replay_evidence_id(),
                              e,
                              cut.run_input_sequence(),
                              cut.logical_time_nanoseconds(),
                              std::min(decision.logical_expiry_nanoseconds(),
                                       e.logical_expiry_nanoseconds),
                              *decision.authorized_delta_units(),
                              *decision.authorized_delta_units() > 0
                                  ? contracts::PaperSide::Buy
                                  : contracts::PaperSide::Sell,
                              *quantity})};
  // Allocate all copies/cache capacity before the reservation commit. Only
  // no-throw moves remain after successful consume; failed consume publishes no
  // intent.
  Request staged{request_id, decision, &reservations, reservation_id,
                 evidence,   cut,      result};
  static_assert(std::is_nothrow_move_constructible_v<Request>);
  static_assert(std::is_nothrow_move_constructible_v<PaperIntentResult>);
  const auto consumed = reservations.consume(reservation_id, request_id, cut);
  if (!consumed.accepted()) {
    staged.result = {PaperIntentFailure::ConsumeRejected, {}};
    requests_.push_back(std::move(staged));
    return requests_.back().result;
  }
  requests_.push_back(std::move(staged));
  return result;
}
} // namespace chronos::core::execution_planning
