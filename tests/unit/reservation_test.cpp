#include "chronos/core/risk/reservation.hpp"
#include "microtest.hpp"
#include "paper_risk_fixture.hpp"
#include <array>
#include <type_traits>
namespace {
using namespace chronos::test_support::paper;
risk::ReservationPolicy policy(contracts::AmountUnits capacity = 100) {
  return {risk_policy(EvaluationSpec{}), version(120),
          id<contracts::StreamId>(121), 1, capacity};
}

contracts::SettledExposureEvidence
settlement(const risk::Reservation &reservation,
           contracts::ExecutableOrderIntentId intent,
           contracts::AmountUnits position = 40,
           std::uint64_t ledger_sequence = 1) {
  const std::array payload{std::byte{0x42}};
  return {id<contracts::RunId>(30),
          target_key().portfolio_id(),
          target_key().account_id(),
          target_key().canonical_instrument_id(),
          target_key().listing_id(),
          version(120),
          contracts::RunMode::backtest,
          reservation.reservation_id(),
          intent,
          id<contracts::PaperFillId>(126),
          id<contracts::LedgerTransactionId>(127),
          *contracts::StreamCursor::at_sequence(id<contracts::StreamId>(121), 1,
                                                ledger_sequence),
          contracts::sha256(payload),
          reservation.authorized_delta_units(),
          position,
          exposure_scale(),
          valid_quality(),
          6,
          130};
}

TEST_CASE("reservation accepts exact scoped risk and deduplicates retries") {
  risk::ReservationAuthority authority(policy(), 0);
  const auto snapshot = authority.snapshot(risk_cut());
  const auto approved = decision(snapshot);
  CHECK(approved.run_id() == id<contracts::RunId>(30));
  auto accepted = authority.reserve(id<contracts::ReservationRequestId>(122),
                                    approved, risk_cut());
  CHECK(accepted.accepted());
  CHECK(approved.risk_sequence() == snapshot.risk_sequence());
  CHECK(authority.snapshot(risk_cut())
            .worst_case_exposure_before_target_units() == 40);
  CHECK(authority.reserve(id<contracts::ReservationRequestId>(122), approved,
                          risk_cut()) == accepted);
  CHECK(authority
            .reserve(id<contracts::ReservationRequestId>(123), approved,
                     risk_cut())
            .failure() == risk::ReservationFailure::DuplicateDecision);
  const auto second = decision(snapshot, 50);
  CHECK(
      authority
          .reserve(id<contracts::ReservationRequestId>(124), second, risk_cut())
          .disposition() == risk::ReservationDisposition::Stale);
  CHECK(
      authority
          .reserve(id<contracts::ReservationRequestId>(122), second, risk_cut())
          .failure() == risk::ReservationFailure::RequestConflict);
}

TEST_CASE("reservation rejects cap risk rejection expiry and scope without "
          "mutation") {
  risk::ReservationAuthority capped(policy(20), 0);
  const auto before = capped.snapshot(risk_cut());
  const auto approved = decision(before);
  CHECK(capped
            .reserve(id<contracts::ReservationRequestId>(130), approved,
                     risk_cut())
            .failure() == risk::ReservationFailure::CapExceeded);
  CHECK(capped.snapshot(risk_cut()).risk_sequence() == before.risk_sequence());
  CHECK(capped.snapshot(risk_cut()).worst_case_exposure_before_target_units() ==
        0);

  risk::ReservationAuthority rejected_authority(policy(), 0);
  EvaluationSpec rejected_spec{};
  rejected_spec.trading_enabled = false;
  const auto rejected =
      decision(rejected_authority.snapshot(risk_cut()), 40, rejected_spec);
  CHECK(rejected_authority
            .reserve(id<contracts::ReservationRequestId>(131), rejected,
                     risk_cut())
            .failure() == risk::ReservationFailure::RiskRejected);
  CHECK(rejected_authority.snapshot(risk_cut()).risk_sequence() == 1);

  risk::ReservationAuthority expired_authority(policy(), 0);
  const auto expiring = decision(expired_authority.snapshot(risk_cut()));
  CHECK(expired_authority
            .reserve(id<contracts::ReservationRequestId>(132), expiring,
                     risk::RiskEvaluationCut(6, 170))
            .failure() == risk::ReservationFailure::Expired);
  CHECK(expired_authority.snapshot(risk_cut()).risk_sequence() == 1);

  auto mismatched_policy = policy();
  mismatched_policy.risk_policy = risk::MinimalRiskPolicy(
      id<contracts::RiskScopeId>(133), target_key(), id<contracts::RunId>(30),
      contracts::RunMode::backtest, version(81), version(82), version(83),
      version(84), version(85), version(74), exposure_scale(),
      risk::RiskExposureDimensionSet::QuantityOnlyV1, 100, 100, 100, 100, 100,
      50, true);
  risk::ReservationAuthority mismatched(std::move(mismatched_policy), 0);
  CHECK(mismatched
            .reserve(id<contracts::ReservationRequestId>(134), approved,
                     risk_cut())
            .failure() == risk::ReservationFailure::ScopeMismatch);

  auto scale_policy = policy();
  scale_policy.risk_policy = risk::MinimalRiskPolicy(
      id<contracts::RiskScopeId>(80), target_key(), id<contracts::RunId>(30),
      contracts::RunMode::backtest, version(81), version(82), version(83),
      version(84), version(85), version(74),
      contracts::DecimalScale::from_exponent(5).value(),
      risk::RiskExposureDimensionSet::QuantityOnlyV1, 100, 100, 100, 100, 100,
      50, true);
  risk::ReservationAuthority wrong_scale(std::move(scale_policy), 0);
  CHECK(wrong_scale
            .reserve(id<contracts::ReservationRequestId>(136), approved,
                     risk_cut())
            .failure() == risk::ReservationFailure::PolicyMismatch);

  auto unsupported_mode = policy();
  unsupported_mode.risk_policy = risk::MinimalRiskPolicy(
      id<contracts::RiskScopeId>(80), target_key(), id<contracts::RunId>(30),
      contracts::RunMode::live_paper, version(81), version(82), version(83),
      version(84), version(85), version(74), exposure_scale(),
      risk::RiskExposureDimensionSet::QuantityOnlyV1, 100, 100, 100, 100, 100,
      50, true);
  risk::ReservationAuthority non_d0(std::move(unsupported_mode), 0);
  CHECK(non_d0
            .reserve(id<contracts::ReservationRequestId>(135), approved,
                     risk_cut())
            .failure() == risk::ReservationFailure::InvalidPolicy);
}

TEST_CASE("reservation keeps one unsettled listing hold and terminal decision "
          "cache") {
  risk::ReservationAuthority authority(policy(), 0);
  const auto first_snapshot = authority.snapshot(risk_cut());
  const auto first = decision(first_snapshot, 40);
  const auto held = authority.reserve(id<contracts::ReservationRequestId>(140),
                                      first, risk_cut());
  CHECK(held.accepted());
  EvaluationSpec opposite_spec{};
  opposite_spec.account_current_position = 40;
  const auto opposite =
      decision(authority.snapshot(risk_cut()), 20, opposite_spec);
  CHECK(authority
            .reserve(id<contracts::ReservationRequestId>(141), opposite,
                     risk_cut())
            .failure() == risk::ReservationFailure::OutstandingHold);
  CHECK(authority.release(held.reservation()->reservation_id(), risk_cut())
            .accepted());
  CHECK(
      authority
          .reserve(id<contracts::ReservationRequestId>(142), first, risk_cut())
          .failure() == risk::ReservationFailure::DuplicateDecision);
  CHECK(authority.reserve(id<contracts::ReservationRequestId>(140), first,
                          risk_cut()) == held);
}

TEST_CASE(
    "reservation consume release and full-scope settlement are idempotent") {
  risk::ReservationAuthority authority(policy(), 0);
  const auto approved = decision(authority.snapshot(risk_cut()));
  const auto accepted = authority.reserve(
      id<contracts::ReservationRequestId>(150), approved, risk_cut());
  const auto reservation_id = accepted.reservation()->reservation_id();
  const auto intent = id<contracts::ExecutableOrderIntentId>(151);
  const auto consumed = authority.consume(reservation_id, intent, risk_cut());
  CHECK(consumed.accepted());
  CHECK(authority.consume(reservation_id, intent, risk_cut()) == consumed);
  CHECK(authority
            .consume(reservation_id,
                     id<contracts::ExecutableOrderIntentId>(152), risk_cut())
            .failure() == risk::ReservationFailure::IntentConflict);
  CHECK(authority.release(reservation_id, risk_cut()).failure() ==
        risk::ReservationFailure::ConsumedCannotRelease);

  auto evidence = settlement(*consumed.reservation(), intent);
  auto bad_scope = evidence;
  bad_scope.listing_id = id<contracts::ListingId>(154);
  CHECK(authority.reconcile(reservation_id, bad_scope).failure() ==
        risk::ReservationFailure::InvalidSettlement);
  CHECK(authority.find(reservation_id)->state() ==
        risk::ReservationState::Consumed);
  const auto settled = authority.reconcile(reservation_id, evidence);
  CHECK(settled.accepted());
  CHECK(settled.reservation()->state() == risk::ReservationState::Settled);
  CHECK(authority.reconcile(reservation_id, evidence) == settled);
  auto conflicting = evidence;
  conflicting.ledger_checksum.bytes.front() ^= 1U;
  CHECK(authority.reconcile(reservation_id, conflicting).failure() ==
        risk::ReservationFailure::SettlementConflict);
  CHECK(authority.snapshot(risk::RiskEvaluationCut(6, 130))
            .worst_case_exposure_before_target_units() == 40);
}
static_assert(!std::is_copy_constructible_v<risk::ReservationAuthority>);
} // namespace
