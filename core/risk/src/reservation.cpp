#include "chronos/core/risk/reservation.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
namespace chronos::core::risk {
namespace {
using contracts::AmountUnits;
bool within_cap(AmountUnits value, AmountUnits cap) {
  return cap >= 0 && value >= -cap && value <= cap;
}
void append(std::vector<std::byte> &bytes, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 255U));
}
template <typename Id> void append_id(std::vector<std::byte> &bytes, Id id) {
  for (auto byte : id.bytes())
    bytes.push_back(static_cast<std::byte>(byte));
}
template <typename Id> Id identity(const std::vector<std::byte> &bytes) {
  const auto digest = contracts::sha256(bytes);
  typename Id::bytes_type id{};
  std::copy_n(digest.bytes.begin(), id.size(), id.begin());
  // Domain marker also guarantees the opaque identity is nonzero.
  id.front() |= 0x80U;
  return Id::from_bytes(id).value();
}
} // namespace
ReservationAuthority::ReservationAuthority(ReservationPolicy policy,
                                           AmountUnits initial_position)
    : policy_(std::move(policy)),
      valid_(initial_position == 0 && policy_.ledger_epoch != 0 &&
             policy_.risk_policy.supported_run_mode() ==
                 contracts::RunMode::backtest &&
             policy_.maximum_absolute_projected_exposure_units >= 0) {}
bool ReservationAuthority::valid_cut(const RiskEvaluationCut &cut) const {
  return cut.run_input_sequence() != 0 &&
         (!last_cut_ ||
          (cut.run_input_sequence() >= last_cut_->run_input_sequence() &&
           cut.logical_time_nanoseconds() >=
               last_cut_->logical_time_nanoseconds()));
}
ProjectedExposureSnapshot
ReservationAuthority::snapshot(const RiskEvaluationCut &cut) const {
  const auto &p = policy_.risk_policy;
  const auto &key = p.target_key();
  std::vector<std::byte> bytes;
  append_id(bytes, p.run_id());
  append_id(bytes, p.risk_scope_id());
  append_id(bytes, key.portfolio_id());
  append_id(bytes, key.account_id());
  append_id(bytes, key.canonical_instrument_id());
  append_id(bytes, key.listing_id());
  append(bytes, sequence_);
  append(bytes, static_cast<std::uint64_t>(projected_));
  append(bytes, p.exposure_scale().exponent());
  append(bytes, static_cast<std::uint64_t>(
                    policy_.maximum_absolute_projected_exposure_units));
  for (auto version : {p.risk_policy_version(), p.limit_set_version(),
                       p.exposure_model_version(), policy_.quote_currency}) {
    append_id(bytes, version.definition_id());
    append(bytes, version.version());
  }
  const bool valid = valid_ && valid_cut(cut);
  return {identity<contracts::ProjectedExposureId>(bytes),
          p.risk_scope_id(),
          key,
          projected_,
          p.exposure_scale(),
          sequence_,
          contracts::DataQuality::from(valid
                                           ? contracts::QualityStatus::valid
                                           : contracts::QualityStatus::invalid,
                                       valid ? 0 : 1)
              .value(),
          cut.run_input_sequence(),
          cut.logical_time_nanoseconds()};
}
ReservationResult ReservationAuthority::reject(ReservationFailure failure) {
  return {failure == ReservationFailure::StaleSnapshot
              ? ReservationDisposition::Stale
              : ReservationDisposition::Rejected,
          failure};
}
ReservationResult ReservationAuthority::accept(const Reservation &reservation) {
  return {ReservationDisposition::Accepted, ReservationFailure::None,
          reservation};
}
std::optional<Reservation>
ReservationAuthority::find(contracts::ReservationId reservation) const {
  for (const auto &record : records_)
    if (record.fact.id_ == reservation)
      return record.fact;
  return std::nullopt;
}
ReservationResult
ReservationAuthority::reserve(contracts::ReservationRequestId request,
                              const RiskDecision &d,
                              const RiskEvaluationCut &cut) {
  static_assert(std::is_nothrow_move_constructible_v<Request>);
  static_assert(std::is_nothrow_move_constructible_v<Record>);
  static_assert(std::is_nothrow_move_constructible_v<ReservationResult>);
  static_assert(
      std::is_nothrow_copy_assignable_v<std::optional<RiskEvaluationCut>>);
  for (const auto &old : requests_)
    if (old.id == request)
      return old.decision == d ? old.result
                               : reject(ReservationFailure::RequestConflict);
  AmountUnits accepted_projected{};
  auto evaluate = [&]() -> ReservationResult {
    if (!valid_)
      return reject(ReservationFailure::InvalidPolicy);
    for (const auto &record : records_)
      if (record.fact.decision_.decision_id() == d.decision_id())
        return reject(ReservationFailure::DuplicateDecision);
    if (!d.authorizes_target() || !d.authorized_delta_units() ||
        !d.authorized_target_units() ||
        !d.authorized_projected_exposure_units())
      return reject(ReservationFailure::RiskRejected);
    const auto &p = policy_.risk_policy;
    if (d.run_id() != p.run_id() || d.target_key() != p.target_key() ||
        d.risk_scope_id() != p.risk_scope_id() ||
        d.run_mode() != p.supported_run_mode())
      return reject(ReservationFailure::ScopeMismatch);
    if (d.risk_policy_version() != p.risk_policy_version() ||
        d.limit_set_version() != p.limit_set_version() ||
        d.exposure_model_version() != p.exposure_model_version() ||
        d.arithmetic_version() != p.arithmetic_version() ||
        d.authority_version() != p.authority_version() ||
        d.target_schema_version() != p.target_schema_version() ||
        d.exposure_scale() != p.exposure_scale())
      return reject(ReservationFailure::PolicyMismatch);
    if (!valid_cut(cut) ||
        cut.run_input_sequence() < d.issue_cut().run_input_sequence() ||
        cut.logical_time_nanoseconds() <
            d.issue_cut().logical_time_nanoseconds())
      return reject(ReservationFailure::InvalidCut);
    if (cut.logical_time_nanoseconds() >= d.logical_expiry_nanoseconds())
      return reject(ReservationFailure::Expired);
    if (d.risk_sequence() != sequence_ ||
        d.projected_exposure_id() != snapshot(cut).projected_exposure_id() ||
        d.projected_exposure_before_target_units() != projected_)
      return reject(ReservationFailure::StaleSnapshot);
    for (const auto &record : records_)
      if (record.fact.state_ == ReservationState::Held ||
          record.fact.state_ == ReservationState::Consumed)
        return reject(ReservationFailure::OutstandingHold);
    AmountUnits next{}, delta{};
    if (__builtin_add_overflow(projected_, *d.authorized_delta_units(),
                               &next) ||
        __builtin_sub_overflow(*d.authorized_target_units(),
                               d.account_current_position_units(), &delta) ||
        sequence_ == std::numeric_limits<std::uint64_t>::max())
      return reject(ReservationFailure::ArithmeticOverflow);
    if (delta == 0 || delta != *d.authorized_delta_units() ||
        next != *d.authorized_projected_exposure_units() ||
        d.account_current_position_units() != position_)
      return reject(ReservationFailure::StaleSnapshot);
    if (!within_cap(next, policy_.maximum_absolute_projected_exposure_units))
      return reject(ReservationFailure::CapExceeded);
    std::vector<std::byte> bytes;
    append_id(bytes, request);
    append_id(bytes, d.decision_id());
    append_id(bytes, p.run_id());
    Reservation fact(identity<contracts::ReservationId>(bytes), request, d);
    accepted_projected = next;
    return accept(fact);
  };
  // Every allocation-prone copy and both outer allocations complete before an
  // accepted transition changes observable authority state.
  records_.reserve(records_.size() + 1);
  requests_.reserve(requests_.size() + 1);
  auto result = evaluate();
  Request staged_request{request, d, result};
  if (result.accepted()) {
    Record staged_record{*result.reservation(), {}};
    records_.push_back(std::move(staged_record));
    requests_.push_back(std::move(staged_request));
    projected_ = accepted_projected;
    ++sequence_;
    last_cut_ = cut;
    return result;
  }
  requests_.push_back(std::move(staged_request));
  return result;
}
ReservationResult
ReservationAuthority::consume(contracts::ReservationId reservation,
                              contracts::ExecutableOrderIntentId intent,
                              const RiskEvaluationCut &cut) {
  static_assert(std::is_nothrow_move_assignable_v<Reservation>);
  static_assert(std::is_nothrow_move_constructible_v<ReservationResult>);
  static_assert(
      std::is_nothrow_copy_assignable_v<std::optional<RiskEvaluationCut>>);
  for (auto &record : records_)
    if (record.fact.id_ == reservation) {
      auto &fact = record.fact;
      if (fact.state_ == ReservationState::Consumed)
        return fact.intent_ == intent
                   ? accept(fact)
                   : reject(ReservationFailure::IntentConflict);
      if (fact.state_ != ReservationState::Held)
        return reject(ReservationFailure::AlreadyTerminal);
      if (!valid_cut(cut))
        return reject(ReservationFailure::InvalidCut);
      if (cut.logical_time_nanoseconds() >=
          fact.decision_.logical_expiry_nanoseconds())
        return reject(ReservationFailure::Expired);
      if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        return reject(ReservationFailure::ArithmeticOverflow);
      for (const auto &old : records_)
        if (old.fact.intent_ == intent)
          return reject(ReservationFailure::IntentConflict);
      Reservation staged_fact = fact;
      staged_fact.intent_ = intent;
      staged_fact.state_ = ReservationState::Consumed;
      auto result = accept(staged_fact);
      fact = std::move(staged_fact);
      ++sequence_;
      last_cut_ = cut;
      return result;
    }
  return reject(ReservationFailure::UnknownReservation);
}
ReservationResult
ReservationAuthority::release(contracts::ReservationId reservation,
                              const RiskEvaluationCut &cut) {
  static_assert(std::is_nothrow_move_assignable_v<Reservation>);
  static_assert(std::is_nothrow_move_constructible_v<ReservationResult>);
  static_assert(
      std::is_nothrow_copy_assignable_v<std::optional<RiskEvaluationCut>>);
  for (auto &record : records_)
    if (record.fact.id_ == reservation) {
      auto &fact = record.fact;
      if (fact.state_ == ReservationState::Released)
        return accept(fact);
      if (fact.state_ == ReservationState::Consumed)
        return reject(ReservationFailure::ConsumedCannotRelease);
      if (fact.state_ != ReservationState::Held)
        return reject(ReservationFailure::AlreadyTerminal);
      if (!valid_cut(cut))
        return reject(ReservationFailure::InvalidCut);
      if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        return reject(ReservationFailure::ArithmeticOverflow);
      Reservation staged_fact = fact;
      staged_fact.state_ = ReservationState::Released;
      auto result = accept(staged_fact);
      fact = std::move(staged_fact);
      projected_ = position_;
      ++sequence_;
      last_cut_ = cut;
      return result;
    }
  return reject(ReservationFailure::UnknownReservation);
}
ReservationResult
ReservationAuthority::reconcile(contracts::ReservationId reservation,
                                const contracts::SettledExposureEvidence &e) {
  static_assert(std::is_nothrow_move_assignable_v<Reservation>);
  static_assert(std::is_nothrow_move_assignable_v<
                std::optional<contracts::SettledExposureEvidence>>);
  static_assert(std::is_nothrow_move_constructible_v<ReservationResult>);
  static_assert(
      std::is_nothrow_move_assignable_v<std::optional<RiskEvaluationCut>>);
  for (auto &record : records_)
    if (record.fact.id_ == reservation) {
      auto &fact = record.fact;
      if (record.settled)
        return *record.settled == e
                   ? accept(fact)
                   : reject(ReservationFailure::SettlementConflict);
      const auto &p = policy_.risk_policy;
      const auto &key = p.target_key();
      AmountUnits expected{};
      if (fact.state_ != ReservationState::Consumed ||
          e.reservation_id != reservation || fact.intent_ != e.intent_id ||
          e.run_id != p.run_id() || e.portfolio_id != key.portfolio_id() ||
          e.account_id != key.account_id() ||
          e.canonical_instrument_id != key.canonical_instrument_id() ||
          e.listing_id != key.listing_id() ||
          e.quote_currency != policy_.quote_currency ||
          e.run_mode != p.supported_run_mode() ||
          e.exposure_scale != p.exposure_scale() ||
          e.quality.status() != contracts::QualityStatus::valid ||
          e.ledger_cursor.stream_id() != policy_.ledger_stream_id ||
          e.ledger_cursor.stream_epoch() != policy_.ledger_epoch ||
          !e.ledger_cursor.last_consumed_sequence() ||
          *e.ledger_cursor.last_consumed_sequence() <= ledger_sequence_ ||
          e.ledger_checksum == contracts::Sha256Digest{} ||
          e.posted_delta_units != fact.authorized_delta_units() ||
          !valid_cut({e.run_input_sequence, e.logical_time_nanoseconds}))
        return reject(ReservationFailure::InvalidSettlement);
      for (const auto &old : records_)
        if (old.settled && (old.settled->fill_id == e.fill_id ||
                            old.settled->transaction_id == e.transaction_id))
          return reject(ReservationFailure::SettlementConflict);
      if (__builtin_add_overflow(position_, e.posted_delta_units, &expected) ||
          sequence_ == std::numeric_limits<std::uint64_t>::max())
        return reject(ReservationFailure::ArithmeticOverflow);
      if (e.position_units != expected ||
          !within_cap(expected,
                      policy_.maximum_absolute_projected_exposure_units))
        return reject(ReservationFailure::InvalidSettlement);
      Reservation staged_fact = fact;
      staged_fact.state_ = ReservationState::Settled;
      std::optional<contracts::SettledExposureEvidence> staged_settled{e};
      auto result = accept(staged_fact);
      auto staged_cut = std::optional<RiskEvaluationCut>{
          RiskEvaluationCut(e.run_input_sequence, e.logical_time_nanoseconds)};
      record.settled = std::move(staged_settled);
      fact = std::move(staged_fact);
      position_ = expected;
      projected_ = expected;
      ledger_sequence_ = *e.ledger_cursor.last_consumed_sequence();
      ++sequence_;
      last_cut_ = std::move(staged_cut);
      return result;
    }
  return reject(ReservationFailure::UnknownReservation);
}
} // namespace chronos::core::risk
