#pragma once
#include "paper_intent_fixture.hpp"
#include <limits>
namespace chronos::test_support::paper {
inline contracts::PaperIntent broker_intent(bool sell = false,
                                            contracts::AmountUnits price = 100,
                                            std::uint8_t price_exponent = 0,
                                            std::uint8_t money_exponent = 6) {
  risk::ReservationAuthority reservations(reservation_policy(), 0);
  auto approved = decision(reservations.snapshot(risk_cut()));
  auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                   approved, risk_cut());
  if (sell) {
    const auto opening_intent = id<contracts::ExecutableOrderIntentId>(159);
    if (!reservations
             .consume(held.reservation()->reservation_id(), opening_intent,
                      risk_cut())
             .accepted())
      std::abort();
    const std::array payload{std::byte{0x42}};
    // Explicit neutral settlement port fixture; production evidence comes from
    // ledger.
    contracts::SettledExposureEvidence settled{
        id<contracts::RunId>(30),
        target_key().portfolio_id(),
        target_key().account_id(),
        target_key().canonical_instrument_id(),
        target_key().listing_id(),
        version(120),
        contracts::RunMode::backtest,
        held.reservation()->reservation_id(),
        opening_intent,
        id<contracts::PaperFillId>(126),
        id<contracts::LedgerTransactionId>(127),
        *contracts::StreamCursor::at_sequence(id<contracts::StreamId>(121), 1,
                                              1),
        contracts::sha256(payload),
        40,
        40,
        exposure_scale(),
        valid_quality(),
        5,
        120};
    if (!reservations.reconcile(settled.reservation_id, settled).accepted())
      std::abort();
    TargetSnapshotSpec target_spec;
    target_spec.current_exposure = 40;
    const auto target = target_position(target_spec, 20);
    EvaluationSpec spec;
    spec.account_current_position = 40;
    auto evaluated = risk::MinimalRiskAuthority::evaluate(
        target, risk_policy(spec), risk_cut(), admission_evidence(target),
        evaluation_evidence(spec, reservations.snapshot(risk_cut())));
    if (!evaluated.terminal ||
        !std::holds_alternative<risk::RiskDecision>(*evaluated.terminal))
      std::abort();
    approved = std::get<risk::RiskDecision>(*evaluated.terminal);
    held = reservations.reserve(id<contracts::ReservationRequestId>(123),
                                approved, risk_cut());
    if (!held.accepted())
      std::abort();
  }
  auto evidence = execution_evidence();
  evidence.bid_price_units = price;
  evidence.ask_price_units = price;
  if (price < std::numeric_limits<contracts::AmountUnits>::max())
    evidence.ask_price_units++;
  evidence.price_scale =
      *contracts::DecimalScale::from_exponent(price_exponent);
  evidence.money_scale =
      *contracts::DecimalScale::from_exponent(money_exponent);
  chronos::core::execution_planning::PaperIntentAuthority authority;
  auto result = authority.create(
      id<contracts::ExecutableOrderIntentId>(160), approved, reservations,
      held.reservation()->reservation_id(), evidence, risk_cut());
  if (!result.accepted())
    std::abort();
  return *result.intent;
}
} // namespace chronos::test_support::paper
