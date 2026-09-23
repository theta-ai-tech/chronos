#pragma once
#include "chronos/accounting/ledger.hpp"
#include "chronos/contracts/valuation.hpp"
#include <span>
namespace chronos::accounting {
struct ValuationCut { std::uint64_t run_input_sequence; std::int64_t logical_time_nanoseconds; };
struct ValuationPolicy { LedgerPolicy scope; contracts::VersionRef valuation_policy_version; contracts::VersionRef mark_policy_version; std::int64_t maximum_mark_age_nanoseconds; };
struct PositionLot { contracts::AmountUnits signed_quantity_units; contracts::AmountUnits basis_units; };
struct PositionProjection {
  contracts::AmountUnits position_units{}, remaining_basis_units{}, realized_gross_units{}, fees_units{};
  std::uint64_t closing_events{}, profitable_gross_closing_events{};
  std::vector<PositionLot> lots;
};
enum class ValuationFailure : std::uint8_t { None, InvalidHistory, ScopeMismatch, MissingMark, InvalidMark, ArithmeticOverflow };
struct PositionResult {
  ValuationFailure failure{ValuationFailure::None};
  std::optional<PositionProjection> position;
  [[nodiscard]] bool available() const noexcept { return position.has_value() && failure == ValuationFailure::None; }
};
struct ValuationResult {
  ValuationFailure failure{ValuationFailure::None};
  std::optional<PositionProjection> position;
  std::optional<contracts::AmountUnits> unrealized_gross_units, total_net_units;
  [[nodiscard]] bool available() const noexcept { return total_net_units.has_value() && failure == ValuationFailure::None; }
};
[[nodiscard]] PositionResult derive_position(std::span<const LedgerTransaction> transactions, const LedgerPolicy &scope);
[[nodiscard]] ValuationResult value_position(std::span<const LedgerTransaction> transactions, const std::optional<contracts::PositionMark> &mark, const ValuationPolicy &policy, ValuationCut cut);
}
