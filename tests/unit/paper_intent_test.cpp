#include "chronos/core/execution_planning/paper_intent.hpp"
#include "microtest.hpp"
#include "paper_intent_fixture.hpp"
#include <array>
#include <limits>
#include <type_traits>
namespace {
using namespace chronos::test_support::paper;
namespace execution = chronos::core::execution_planning;
TEST_CASE("paper intent consumes genuine reservation and exact retry returns "
          "original") {
  risk::ReservationAuthority reservations(reservation_policy(), 0);
  auto approved = decision(reservations.snapshot(risk_cut()));
  auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                   approved, risk_cut());
  execution::PaperIntentAuthority authority;
  auto result = authority.create(
      id<contracts::ExecutableOrderIntentId>(160), approved, reservations,
      held.reservation()->reservation_id(), execution_evidence(), risk_cut());
  CHECK(result.accepted());
  CHECK(result.intent->facts().quantity_units == 40);
  CHECK(result.intent->facts().side == contracts::PaperSide::Buy);
  CHECK(reservations.find(held.reservation()->reservation_id())->state() ==
        risk::ReservationState::Consumed);
  CHECK(authority.create(id<contracts::ExecutableOrderIntentId>(160), approved,
                         reservations, held.reservation()->reservation_id(),
                         execution_evidence(), risk_cut()) == result);
  CHECK(!authority
             .create(id<contracts::ExecutableOrderIntentId>(161), approved,
                     reservations, held.reservation()->reservation_id(),
                     execution_evidence(), risk_cut())
             .accepted());
  auto changed = execution_evidence();
  changed.ask_price_units += 1;
  CHECK(!authority
             .create(id<contracts::ExecutableOrderIntentId>(160), approved,
                     reservations, held.reservation()->reservation_id(),
                     changed, risk_cut())
             .accepted());
}
TEST_CASE(
    "paper intent fails closed before consuming on invalid current evidence") {
  for (int scenario = 0; scenario < 15; ++scenario) {
    risk::ReservationAuthority reservations(reservation_policy(), 0);
    auto approved = decision(reservations.snapshot(risk_cut()));
    auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                     approved, risk_cut());
    execution::PaperIntentAuthority authority;
    auto current = execution_evidence();
    switch (scenario) {
    case 0:
      current.run_mode = contracts::RunMode::live_paper;
      break;
    case 1:
      current.run_id = id<contracts::RunId>(200);
      break;
    case 2:
      current.listing_id = id<contracts::ListingId>(200);
      break;
    case 3:
      current.account_id = id<contracts::AccountId>(200);
      break;
    case 4:
      current.portfolio_id = id<contracts::PortfolioId>(200);
      break;
    case 5:
      current.quality = non_valid_quality(contracts::QualityStatus::stale);
      break;
    case 6:
      current.kill_switch_permits = false;
      break;
    case 7:
      current.market_tradeable = false;
      break;
    case 8:
      current.logical_time_nanoseconds = 119;
      break;
    case 9:
      current.run_input_sequence = 4;
      break;
    case 10:
      current.logical_expiry_nanoseconds = 120;
      break;
    case 11:
      current.bid_price_units = 0;
      break;
    case 12:
      current.price_tick_units = 0;
      break;
    case 13:
      current.quantity_step_units = 3;
      break;
    case 14:
      current.quote_currency = version(200);
      break;
    }
    CHECK(!authority
               .create(id<contracts::ExecutableOrderIntentId>(160), approved,
                       reservations, held.reservation()->reservation_id(),
                       current, risk_cut())
               .accepted());
    CHECK(reservations.find(held.reservation()->reservation_id())->state() ==
          risk::ReservationState::Held);
  }
}
TEST_CASE("paper intent freezes rejected request retries") {
  risk::ReservationAuthority reservations(reservation_policy(), 0);
  auto approved = decision(reservations.snapshot(risk_cut()));
  auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                   approved, risk_cut());
  execution::PaperIntentAuthority authority;
  auto invalid = execution_evidence();
  invalid.market_tradeable = false;
  const auto request = id<contracts::ExecutableOrderIntentId>(160);

  const auto rejected = authority.create(request, approved, reservations,
                                         held.reservation()->reservation_id(),
                                         invalid, risk_cut());
  CHECK(rejected.failure == execution::PaperIntentFailure::InvalidEvidence);
  CHECK(authority.create(request, approved, reservations,
                         held.reservation()->reservation_id(), invalid,
                         risk_cut()) == rejected);

  const auto conflict = authority.create(request, approved, reservations,
                                         held.reservation()->reservation_id(),
                                         execution_evidence(), risk_cut());
  CHECK(conflict.failure == execution::PaperIntentFailure::RequestConflict);
  CHECK(reservations.find(held.reservation()->reservation_id())->state() ==
        risk::ReservationState::Held);

  CHECK(authority
            .create(id<contracts::ExecutableOrderIntentId>(161), approved,
                    reservations, held.reservation()->reservation_id(),
                    execution_evidence(), risk_cut())
            .accepted());
}
TEST_CASE("paper intent checks signed magnitude without overflowing") {
  CHECK(!execution::paper_quantity(
      std::numeric_limits<contracts::AmountUnits>::min()));
  CHECK(!execution::paper_quantity(0));
  CHECK(execution::paper_quantity(-40) == 40);
}
TEST_CASE("paper intent accepts modified quantity from upstream risk") {
  EvaluationSpec spec;
  spec.limit = 20;
  auto policy = reservation_policy();
  policy.risk_policy = risk_policy(spec);
  risk::ReservationAuthority reservations(policy,
                                          spec.account_current_position);
  TargetSnapshotSpec target_spec;
  target_spec.current_exposure = spec.account_current_position;
  auto target = target_position(target_spec, 40);
  auto evaluated = risk::MinimalRiskAuthority::evaluate(
      target, risk_policy(spec), risk_cut(), admission_evidence(target),
      evaluation_evidence(spec, reservations.snapshot(risk_cut())));
  CHECK(evaluated.terminal.has_value());
  CHECK(std::holds_alternative<risk::RiskDecision>(*evaluated.terminal));
  auto approved = std::get<risk::RiskDecision>(*evaluated.terminal);
  CHECK(approved.authorizes_target());
  auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                   approved, risk_cut());
  CHECK(held.accepted());
  execution::PaperIntentAuthority authority;
  auto result = authority.create(
      id<contracts::ExecutableOrderIntentId>(160), approved, reservations,
      held.reservation()->reservation_id(), execution_evidence(), risk_cut());
  CHECK(result.accepted());
  CHECK(result.intent->facts().quantity_units == 20);
  CHECK(result.intent->facts().side == contracts::PaperSide::Buy);
  CHECK(result.intent->facts().authorized_delta_units ==
        *approved.authorized_delta_units());
  CHECK(approved.disposition() == risk::RiskDecisionDisposition::Modified);
}

TEST_CASE("paper intent rejects missing released mismatched expired and "
          "rejected upstream facts") {
  for (int scenario = 0; scenario < 6; ++scenario) {
    risk::ReservationAuthority reservations(reservation_policy(), 0);
    const auto snapshot = reservations.snapshot(risk_cut());
    auto approved = decision(snapshot);
    auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                     approved, risk_cut());
    auto reservation_id = held.reservation()->reservation_id();
    execution::PaperIntentAuthority authority;
    std::optional<contracts::PaperExecutionEvidence> current =
        execution_evidence();
    auto cut = risk_cut();
    if (scenario == 0)
      reservation_id = id<contracts::ReservationId>(200);
    if (scenario == 1)
      CHECK(reservations.release(reservation_id, risk_cut()).accepted());
    if (scenario == 2)
      approved = decision(snapshot, 50);
    if (scenario == 3) {
      cut = risk::RiskEvaluationCut(6, 200);
      current->run_input_sequence = 6;
      current->logical_time_nanoseconds = 200;
      current->logical_expiry_nanoseconds = 250;
      const std::array streams{id<contracts::StreamId>(150)};
      const std::array cursors{
          *contracts::StreamCursor::at_sequence(streams[0], 1, 6)};
      current->market_lineage = *contracts::StateLineage::from(
          id<contracts::RunId>(30), 6, streams, cursors);
    }
    if (scenario == 4)
      current.reset();
    if (scenario == 5) {
      EvaluationSpec spec;
      spec.kill_switch_enabled = true;
      approved = decision(snapshot, 40, spec);
    }
    CHECK(!authority
               .create(id<contracts::ExecutableOrderIntentId>(160), approved,
                       reservations, reservation_id, current, cut)
               .accepted());
    CHECK(reservations.find(held.reservation()->reservation_id())->state() ==
          (scenario == 1 ? risk::ReservationState::Released
                         : risk::ReservationState::Held));
  }
}
TEST_CASE("paper intent rejects every nonpaper mode and does not consume") {
  for (auto mode :
       {contracts::RunMode::capture, contracts::RunMode::replay,
        contracts::RunMode::live_read_only, contracts::RunMode::live_paper,
        static_cast<contracts::RunMode>(255)}) {
    risk::ReservationAuthority reservations(reservation_policy(), 0);
    auto approved = decision(reservations.snapshot(risk_cut()));
    auto held = reservations.reserve(id<contracts::ReservationRequestId>(122),
                                     approved, risk_cut());
    auto current = execution_evidence();
    current.run_mode = mode;
    execution::PaperIntentAuthority authority;
    CHECK(!authority
               .create(id<contracts::ExecutableOrderIntentId>(160), approved,
                       reservations, held.reservation()->reservation_id(),
                       current, risk_cut())
               .accepted());
    CHECK(reservations.find(held.reservation()->reservation_id())->state() ==
          risk::ReservationState::Held);
  }
}
static_assert(!std::is_default_constructible_v<contracts::PaperIntent>);
static_assert(!std::is_aggregate_v<contracts::PaperIntent>);
} // namespace
