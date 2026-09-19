#pragma once
#include "chronos/contracts/accounting.hpp"
#include "chronos/core/risk/risk_decision.hpp"
namespace chronos::core::risk {
struct ReservationPolicy final {
  MinimalRiskPolicy risk_policy;
  contracts::VersionRef quote_currency;
  contracts::StreamId ledger_stream_id;
  std::uint64_t ledger_epoch;
  contracts::AmountUnits maximum_absolute_projected_exposure_units;
};
enum class ReservationDisposition : std::uint8_t { Accepted, Rejected, Stale };
enum class ReservationFailure : std::uint8_t {
  None,
  InvalidPolicy,
  RiskRejected,
  ScopeMismatch,
  PolicyMismatch,
  Expired,
  InvalidCut,
  StaleSnapshot,
  OutstandingHold,
  CapExceeded,
  ArithmeticOverflow,
  RequestConflict,
  DuplicateDecision,
  UnknownReservation,
  IntentConflict,
  AlreadyTerminal,
  ConsumedCannotRelease,
  InvalidSettlement,
  SettlementConflict
};
enum class ReservationState : std::uint8_t {
  Held,
  Consumed,
  Released,
  Settled
};
class ReservationAuthority;
class Reservation final {
public:
  [[nodiscard]] contracts::ReservationId reservation_id() const noexcept {
    return id_;
  }
  [[nodiscard]] contracts::ReservationRequestId request_id() const noexcept {
    return request_;
  }
  [[nodiscard]] const RiskDecision &decision() const noexcept {
    return decision_;
  }
  [[nodiscard]] contracts::AmountUnits authorized_delta_units() const noexcept {
    return *decision_.authorized_delta_units();
  }
  [[nodiscard]] ReservationState state() const noexcept { return state_; }
  [[nodiscard]] const std::optional<contracts::ExecutableOrderIntentId> &
  intent_id() const noexcept {
    return intent_;
  }
  bool operator==(const Reservation &) const = default;

private:
  Reservation(contracts::ReservationId id,
              contracts::ReservationRequestId request, RiskDecision decision)
      : id_(id), request_(request), decision_(std::move(decision)) {}
  contracts::ReservationId id_;
  contracts::ReservationRequestId request_;
  RiskDecision decision_;
  ReservationState state_{ReservationState::Held};
  std::optional<contracts::ExecutableOrderIntentId> intent_;
  friend class ReservationAuthority;
};
class ReservationResult final {
public:
  [[nodiscard]] bool accepted() const noexcept {
    return disposition_ == ReservationDisposition::Accepted;
  }
  [[nodiscard]] ReservationDisposition disposition() const noexcept {
    return disposition_;
  }
  [[nodiscard]] ReservationFailure failure() const noexcept { return failure_; }
  [[nodiscard]] const std::optional<Reservation> &reservation() const noexcept {
    return reservation_;
  }
  bool operator==(const ReservationResult &) const = default;

private:
  ReservationResult(ReservationDisposition disposition,
                    ReservationFailure failure,
                    std::optional<Reservation> reservation = {})
      : disposition_(disposition), failure_(failure),
        reservation_(std::move(reservation)) {}
  ReservationDisposition disposition_;
  ReservationFailure failure_;
  std::optional<Reservation> reservation_;
  friend class ReservationAuthority;
};
// Single writer, one unsettled hold. Copies cannot create a second capacity
// writer.
class ReservationAuthority final {
public:
  ReservationAuthority(ReservationPolicy policy,
                       contracts::AmountUnits initial_position);
  [[nodiscard]] contracts::VersionRef quote_currency() const noexcept {
    return policy_.quote_currency;
  }
  ReservationAuthority(const ReservationAuthority &) = delete;
  ReservationAuthority &operator=(const ReservationAuthority &) = delete;
  [[nodiscard]] ProjectedExposureSnapshot
  snapshot(const RiskEvaluationCut &cut) const;
  [[nodiscard]] ReservationResult
  reserve(contracts::ReservationRequestId request, const RiskDecision &decision,
          const RiskEvaluationCut &cut);
  [[nodiscard]] ReservationResult
  consume(contracts::ReservationId reservation,
          contracts::ExecutableOrderIntentId intent,
          const RiskEvaluationCut &cut);
  [[nodiscard]] ReservationResult release(contracts::ReservationId reservation,
                                          const RiskEvaluationCut &cut);
  [[nodiscard]] ReservationResult
  reconcile(contracts::ReservationId reservation,
            const contracts::SettledExposureEvidence &evidence);
  [[nodiscard]] std::optional<Reservation>
  find(contracts::ReservationId reservation) const;

private:
  struct Request {
    contracts::ReservationRequestId id;
    RiskDecision decision;
    ReservationResult result;
  };
  struct Record {
    Reservation fact;
    std::optional<contracts::SettledExposureEvidence> settled;
  };
  [[nodiscard]] bool valid_cut(const RiskEvaluationCut &cut) const;
  [[nodiscard]] static ReservationResult reject(ReservationFailure failure);
  [[nodiscard]] static ReservationResult accept(const Reservation &reservation);
  ReservationPolicy policy_;
  bool valid_;
  contracts::AmountUnits position_{};
  contracts::AmountUnits projected_{};
  std::uint64_t sequence_{1};
  std::uint64_t ledger_sequence_{};
  std::optional<RiskEvaluationCut> last_cut_;
  std::vector<Request> requests_;
  std::vector<Record> records_;
};
} // namespace chronos::core::risk
