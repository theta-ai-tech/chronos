#pragma once

#include "chronos/contracts/accounting.hpp"
#include "chronos/contracts/paper_execution.hpp"

#include <optional>
#include <vector>

namespace chronos::accounting {

enum class LedgerUnit : std::uint8_t { BaseQuantity, QuoteCurrency };
enum class LedgerAccount : std::uint8_t {
  PositionQuantity,
  QuantityClearing,
  QuoteCash,
  TradeClearing,
  FeeExpense
};
enum class LedgerTransactionKind : std::uint8_t { Fill, Compensation };

struct LedgerPolicy final {
  contracts::RunId run_id;
  contracts::PortfolioId portfolio_id;
  contracts::AccountId account_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::ListingId listing_id;
  contracts::VersionRef quote_currency;
  contracts::RunMode run_mode;
  contracts::StreamId ledger_stream_id;
  std::uint64_t ledger_epoch;
  contracts::VersionRef posting_policy_version;
  contracts::DecimalScale quantity_scale;
  contracts::DecimalScale money_scale;
  contracts::RoundingMode notional_rounding;
  contracts::RoundingMode fee_rounding;
  bool operator==(const LedgerPolicy &) const = default;
};

struct LedgerEntry final {
  LedgerAccount account;
  LedgerUnit unit;
  contracts::AmountUnits amount_units;
  bool operator==(const LedgerEntry &) const = default;
};

struct LedgerTransaction final {
  contracts::LedgerTransactionId transaction_id;
  LedgerTransactionKind kind;
  LedgerPolicy policy;
  contracts::PaperFillFacts source_fill;
  std::vector<LedgerEntry> entries;
  contracts::AmountUnits signed_fill_quantity_units;
  contracts::AmountUnits trade_notional_units;
  contracts::AmountUnits fee_units;
  std::optional<contracts::LedgerTransactionId> original_transaction_id;
  std::optional<contracts::LedgerCorrectionId> correction_id;
  contracts::StreamCursor cursor;
  contracts::Sha256Digest checksum;
  bool operator==(const LedgerTransaction &) const = default;
};

enum class LedgerFailure : std::uint8_t {
  None,
  InvalidPolicy,
  ScopeMismatch,
  InvalidFill,
  ArithmeticOverflow,
  FillConflict,
  UnknownTransaction,
  AlreadyCompensated,
  CorrectionConflict,
  InvalidReversal
};

struct LedgerResult final {
  LedgerFailure failure{LedgerFailure::None};
  std::optional<contracts::SettledExposureEvidence> settlement;
  std::optional<contracts::LedgerTransactionId> transaction_id;
  [[nodiscard]] bool accepted() const noexcept {
    return failure == LedgerFailure::None && transaction_id.has_value();
  }
  bool operator==(const LedgerResult &) const = default;
};

class LedgerAuthority final {
public:
  explicit LedgerAuthority(LedgerPolicy policy);
  LedgerAuthority(const LedgerAuthority &) = delete;
  LedgerAuthority &operator=(const LedgerAuthority &) = delete;
  LedgerAuthority(LedgerAuthority &&) noexcept = default;
  LedgerAuthority &operator=(LedgerAuthority &&) noexcept = default;

  [[nodiscard]] LedgerResult post(const contracts::PaperFill &fill);
  [[nodiscard]] LedgerResult
  reverse(contracts::LedgerTransactionId transaction_id,
          contracts::LedgerCorrectionId correction_id);
  [[nodiscard]] const std::vector<LedgerTransaction> &
  transactions() const noexcept;
  [[nodiscard]] contracts::StreamCursor cursor() const noexcept;
  [[nodiscard]] const contracts::Sha256Digest &checksum() const noexcept;
  [[nodiscard]] contracts::AmountUnits position_units() const noexcept;

private:
  LedgerPolicy policy_;
  bool valid_policy_{};
  std::vector<LedgerTransaction> transactions_;
  contracts::StreamCursor cursor_;
  contracts::Sha256Digest checksum_{};
  contracts::AmountUnits position_units_{};
};

} // namespace chronos::accounting
