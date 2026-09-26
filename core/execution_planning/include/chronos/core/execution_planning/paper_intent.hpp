#pragma once
#include "chronos/contracts/paper_execution.hpp"
#include "chronos/core/risk/reservation.hpp"
namespace chronos::core::execution_planning {
enum class PaperIntentFailure : std::uint8_t {
  None,
  RequestConflict,
  RiskRejected,
  InvalidReservation,
  InvalidEvidence,
  ConsumeRejected
};
struct PaperIntentResult final {
  PaperIntentFailure failure;
  std::optional<contracts::PaperIntent> intent;
  [[nodiscard]] bool accepted() const noexcept { return intent.has_value(); }
  bool operator==(const PaperIntentResult &) const = default;
};
[[nodiscard]] std::optional<contracts::AmountUnits>
paper_quantity(contracts::AmountUnits delta) noexcept;
// Single writer with immutable retry outcomes. No routing or accounting.
class PaperIntentAuthority final {
public:
  PaperIntentAuthority() = default;
  PaperIntentAuthority(const PaperIntentAuthority &) = delete;
  PaperIntentAuthority &operator=(const PaperIntentAuthority &) = delete;
  [[nodiscard]] PaperIntentResult
  create(contracts::ExecutableOrderIntentId request_id,
         const risk::RiskDecision &decision,
         risk::ReservationAuthority &reservations,
         contracts::ReservationId reservation_id,
         const std::optional<contracts::PaperExecutionEvidence> &evidence,
         const risk::RiskEvaluationCut &cut);

private:
  struct Request {
    contracts::ExecutableOrderIntentId id;
    risk::RiskDecision decision;
    const risk::ReservationAuthority *authority;
    contracts::ReservationId reservation;
    std::optional<contracts::PaperExecutionEvidence> evidence;
    risk::RiskEvaluationCut cut;
    PaperIntentResult result;
  };
  std::vector<Request> requests_;
};
} // namespace chronos::core::execution_planning
