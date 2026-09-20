#include "chronos/accounting/ledger.hpp"

#include <limits>
#include <span>
#include <type_traits>

namespace chronos::accounting {
namespace {
using namespace contracts;

template <typename Integer>
void append_integer(std::vector<std::byte> &output, Integer value) {
  auto bits = static_cast<std::uint64_t>(value);
  for (std::size_t index = 0; index < sizeof(bits); ++index) {
    output.push_back(static_cast<std::byte>(bits & UINT64_C(0xff)));
    bits >>= 8U;
  }
}

template <typename Id>
void append_id(std::vector<std::byte> &output, const Id &value) {
  for (const auto byte : value.bytes())
    output.push_back(static_cast<std::byte>(byte));
}

void append_version(std::vector<std::byte> &output, const VersionRef &value) {
  append_id(output, value.definition_id());
  append_integer(output, value.version());
}

void append_cursor(std::vector<std::byte> &output, const StreamCursor &value) {
  append_id(output, value.stream_id());
  append_integer(output, value.stream_epoch());
  append_integer(output, value.last_consumed_sequence().has_value());
  if (value.last_consumed_sequence())
    append_integer(output, *value.last_consumed_sequence());
}

void append_execution(std::vector<std::byte> &out,
                      const PaperExecutionEvidence &e) {
  append_id(out, e.run_id);
  append_id(out, e.portfolio_id);
  append_id(out, e.account_id);
  append_id(out, e.canonical_instrument_id);
  append_id(out, e.listing_id);
  append_version(out, e.quote_currency);
  append_integer(out, e.run_mode);
  append_id(out, e.market_state_view_id);
  append_id(out, e.market_lineage.run_id());
  append_integer(out, e.market_lineage.run_input_sequence());
  append_integer(out, e.market_lineage.cursors().size());
  for (const auto &cursor : e.market_lineage.cursors())
    append_cursor(out, cursor);
  append_integer(out, e.run_input_sequence);
  append_integer(out, e.logical_time_nanoseconds);
  append_integer(out, e.logical_expiry_nanoseconds);
  append_integer(out, e.quality.status());
  append_integer(out, e.quality.reason_code());
  append_integer(out, e.kill_switch_permits);
  append_integer(out, e.market_tradeable);
  append_version(out, e.execution_policy_version);
  append_version(out, e.broker_model_version);
  append_integer(out, e.price_scale.exponent());
  append_integer(out, e.quantity_scale.exponent());
  append_integer(out, e.money_scale.exponent());
  append_integer(out, e.bid_price_units);
  append_integer(out, e.ask_price_units);
  append_integer(out, e.price_tick_units);
  append_integer(out, e.quantity_step_units);
}

void append_fill(std::vector<std::byte> &out, const PaperFillFacts &f) {
  append_id(out, f.order_id);
  append_id(out, f.fill_id);
  const auto &i = f.intent;
  append_id(out, i.intent_id);
  append_id(out, i.target_position_id);
  append_id(out, i.risk_decision_id);
  append_id(out, i.reservation_id);
  append_id(out, i.risk_scope_id);
  append_id(out, i.projected_exposure_id);
  append_integer(out, i.risk_sequence);
  append_integer(out, i.risk_issue_sequence);
  append_integer(out, i.risk_issue_time_nanoseconds);
  append_id(out, i.risk_policy_activation_event_id);
  append_id(out, i.risk_account_state_view_id);
  append_id(out, i.risk_market_state_view_id);
  append_id(out, i.risk_kill_switch_state_view_id);
  append_version(out, i.risk_policy_version);
  append_version(out, i.limit_set_version);
  append_version(out, i.exposure_model_version);
  append_version(out, i.arithmetic_version);
  append_version(out, i.risk_authority_version);
  append_version(out, i.target_schema_version);
  append_id(out, i.run_manifest_integrity_id);
  append_id(out, i.replay_evidence_id);
  append_execution(out, i.execution);
  append_integer(out, i.issue_sequence);
  append_integer(out, i.issue_time_nanoseconds);
  append_integer(out, i.logical_expiry_nanoseconds);
  append_integer(out, i.authorized_delta_units);
  append_integer(out, i.side);
  append_integer(out, i.quantity_units);
  append_version(out, f.model_policy.model_version);
  append_integer(out, f.model_policy.slippage_ticks);
  append_integer(out, f.model_policy.fee_basis_points);
  append_integer(out, f.model_policy.acknowledgement_latency_nanoseconds);
  append_integer(out, f.model_policy.fill_latency_nanoseconds);
  append_integer(out, f.price_units);
  append_integer(out, f.notional_units);
  append_integer(out, f.fee_units);
  append_integer(out, f.acknowledgement_time_nanoseconds);
  append_integer(out, f.fill_time_nanoseconds);
}

void append_policy(std::vector<std::byte> &out, const LedgerPolicy &p) {
  append_id(out, p.run_id);
  append_id(out, p.portfolio_id);
  append_id(out, p.account_id);
  append_id(out, p.canonical_instrument_id);
  append_id(out, p.listing_id);
  append_version(out, p.quote_currency);
  append_integer(out, p.run_mode);
  append_id(out, p.ledger_stream_id);
  append_integer(out, p.ledger_epoch);
  append_version(out, p.posting_policy_version);
  append_integer(out, p.quantity_scale.exponent());
  append_integer(out, p.money_scale.exponent());
  append_integer(out, p.notional_rounding);
  append_integer(out, p.fee_rounding);
}

std::vector<std::byte> transaction_semantics(
    const LedgerPolicy &policy, const PaperFillFacts &fill,
    LedgerTransactionKind kind, std::span<const LedgerEntry> entries,
    AmountUnits quantity, AmountUnits notional, AmountUnits fee,
    std::optional<LedgerTransactionId> original,
    std::optional<LedgerCorrectionId> correction) {
  std::vector<std::byte> output;
  output.reserve(1024);
  append_policy(output, policy);
  append_fill(output, fill);
  append_integer(output, kind);
  append_integer(output, entries.size());
  for (const auto &entry : entries) {
    append_integer(output, entry.account);
    append_integer(output, entry.unit);
    append_integer(output, entry.amount_units);
  }
  append_integer(output, quantity);
  append_integer(output, notional);
  append_integer(output, fee);
  append_integer(output, original.has_value());
  if (original)
    append_id(output, *original);
  append_integer(output, correction.has_value());
  if (correction)
    append_id(output, *correction);
  return output;
}

Sha256Digest chained_checksum(const Sha256Digest &prior,
                              std::span<const std::byte> semantics,
                              const StreamCursor &cursor) {
  std::vector<std::byte> canonical;
  canonical.reserve(prior.bytes.size() + semantics.size() + 40);
  for (const auto byte : prior.bytes)
    canonical.push_back(static_cast<std::byte>(byte));
  canonical.insert(canonical.end(), semantics.begin(), semantics.end());
  append_cursor(canonical, cursor);
  return sha256(canonical);
}

bool policy_valid(const LedgerPolicy &policy) {
  return policy.run_mode == RunMode::backtest && policy.ledger_epoch != 0 &&
         policy.notional_rounding == RoundingMode::toward_positive &&
         policy.fee_rounding == RoundingMode::toward_positive;
}

LedgerFailure validate_fill(const LedgerPolicy &policy,
                            const PaperFillFacts &f) {
  const auto &e = f.intent.execution;
  if (e.run_id != policy.run_id || e.portfolio_id != policy.portfolio_id ||
      e.account_id != policy.account_id ||
      e.canonical_instrument_id != policy.canonical_instrument_id ||
      e.listing_id != policy.listing_id ||
      e.quote_currency != policy.quote_currency ||
      e.run_mode != policy.run_mode)
    return LedgerFailure::ScopeMismatch;
  if (e.quantity_scale != policy.quantity_scale ||
      e.money_scale != policy.money_scale)
    return LedgerFailure::ScopeMismatch;
  if (f.order_id.bytes() != f.intent.intent_id.bytes() ||
      f.fill_id.bytes() != f.intent.intent_id.bytes() ||
      f.model_policy.model_version != e.broker_model_version ||
      f.price_units <= 0 || f.notional_units <= 0 || f.fee_units < 0 ||
      f.intent.quantity_units <= 0 ||
      f.acknowledgement_time_nanoseconds > f.fill_time_nanoseconds ||
      f.fill_time_nanoseconds >= f.intent.logical_expiry_nanoseconds ||
      f.intent.authorized_delta_units ==
          std::numeric_limits<AmountUnits>::min())
    return LedgerFailure::InvalidFill;
  const auto signed_quantity = f.intent.side == PaperSide::Buy
                                   ? f.intent.quantity_units
                                   : -f.intent.quantity_units;
  if (signed_quantity != f.intent.authorized_delta_units)
    return LedgerFailure::InvalidFill;

  __int128 scaled =
      static_cast<__int128>(f.price_units) * f.intent.quantity_units;
  const int shift = static_cast<int>(e.money_scale.exponent()) -
                    e.price_scale.exponent() - e.quantity_scale.exponent();
  if (shift > 0) {
    for (int index = 0; index < shift; ++index)
      if (__builtin_mul_overflow(scaled, static_cast<__int128>(10), &scaled))
        return LedgerFailure::ArithmeticOverflow;
  } else {
    __int128 divisor = 1;
    for (int index = 0; index < -shift; ++index)
      divisor *= 10;
    scaled = scaled / divisor + (scaled % divisor != 0 ? 1 : 0);
  }
  if (scaled > std::numeric_limits<AmountUnits>::max() ||
      static_cast<AmountUnits>(scaled) != f.notional_units)
    return LedgerFailure::ArithmeticOverflow;
  const auto fee =
      checked_multiply_divide(f.notional_units, f.model_policy.fee_basis_points,
                              10000, RoundingMode::toward_positive);
  if (!fee || *fee != f.fee_units)
    return LedgerFailure::ArithmeticOverflow;
  return LedgerFailure::None;
}

bool balanced(std::span<const LedgerEntry> entries) {
  __int128 quantity{};
  __int128 quote{};
  for (const auto &entry : entries) {
    auto &total = entry.unit == LedgerUnit::BaseQuantity ? quantity : quote;
    if (__builtin_add_overflow(total, static_cast<__int128>(entry.amount_units),
                               &total))
      return false;
  }
  return quantity == 0 && quote == 0;
}
} // namespace

LedgerAuthority::LedgerAuthority(LedgerPolicy policy)
    : policy_(std::move(policy)), valid_policy_(policy_valid(policy_)),
      cursor_(*StreamCursor::at_origin(
          policy_.ledger_stream_id,
          policy_.ledger_epoch == 0 ? 1 : policy_.ledger_epoch)) {}

LedgerResult LedgerAuthority::post(const PaperFill &fill) {
  const auto &facts = fill.facts();
  for (const auto &transaction : transactions_) {
    if (transaction.kind == LedgerTransactionKind::Fill &&
        transaction.source_fill.fill_id == facts.fill_id) {
      if (transaction.source_fill != facts)
        return {LedgerFailure::FillConflict, {}, {}};
      AmountUnits replay_position{};
      for (const auto &candidate : transactions_) {
        if (__builtin_add_overflow(replay_position,
                                   candidate.signed_fill_quantity_units,
                                   &replay_position))
          return {LedgerFailure::ArithmeticOverflow, {}, {}};
        if (candidate.transaction_id == transaction.transaction_id)
          break;
      }
      SettledExposureEvidence settlement{
          policy_.run_id,
          policy_.portfolio_id,
          policy_.account_id,
          policy_.canonical_instrument_id,
          policy_.listing_id,
          policy_.quote_currency,
          policy_.run_mode,
          facts.intent.reservation_id,
          facts.intent.intent_id,
          facts.fill_id,
          transaction.transaction_id,
          transaction.cursor,
          transaction.checksum,
          transaction.signed_fill_quantity_units,
          replay_position,
          policy_.quantity_scale,
          facts.intent.execution.quality,
          facts.intent.execution.run_input_sequence,
          facts.fill_time_nanoseconds};
      return {LedgerFailure::None, settlement, transaction.transaction_id};
    }
  }
  if (!valid_policy_)
    return {LedgerFailure::InvalidPolicy, {}, {}};
  const auto validation = validate_fill(policy_, facts);
  if (validation != LedgerFailure::None)
    return {validation, {}, {}};

  const AmountUnits signed_quantity = facts.intent.side == PaperSide::Buy
                                          ? facts.intent.quantity_units
                                          : -facts.intent.quantity_units;
  const AmountUnits signed_notional = facts.intent.side == PaperSide::Buy
                                          ? -facts.notional_units
                                          : facts.notional_units;
  AmountUnits cash{};
  AmountUnits next_position{};
  if (__builtin_sub_overflow(signed_notional, facts.fee_units, &cash) ||
      __builtin_add_overflow(position_units_, signed_quantity, &next_position))
    return {LedgerFailure::ArithmeticOverflow, {}, {}};
  std::vector<LedgerEntry> entries{
      {LedgerAccount::PositionQuantity, LedgerUnit::BaseQuantity,
       signed_quantity},
      {LedgerAccount::QuantityClearing, LedgerUnit::BaseQuantity,
       -signed_quantity},
      {LedgerAccount::QuoteCash, LedgerUnit::QuoteCurrency, cash},
      {LedgerAccount::TradeClearing, LedgerUnit::QuoteCurrency,
       -signed_notional},
      {LedgerAccount::FeeExpense, LedgerUnit::QuoteCurrency, facts.fee_units}};
  if (!balanced(entries))
    return {LedgerFailure::ArithmeticOverflow, {}, {}};

  const auto semantics = transaction_semantics(
      policy_, facts, LedgerTransactionKind::Fill, entries, signed_quantity,
      facts.notional_units, facts.fee_units, {}, {});
  const auto transaction_id =
      LedgerTransactionId::from_sha256_digest(sha256(semantics));
  if (transactions_.size() == std::numeric_limits<std::uint64_t>::max())
    return {LedgerFailure::ArithmeticOverflow, {}, {}};
  const auto next_cursor = *StreamCursor::at_sequence(
      policy_.ledger_stream_id, policy_.ledger_epoch,
      static_cast<std::uint64_t>(transactions_.size() + 1));
  const auto next_checksum =
      chained_checksum(checksum_, semantics, next_cursor);
  LedgerTransaction staged{transaction_id,
                           LedgerTransactionKind::Fill,
                           policy_,
                           facts,
                           std::move(entries),
                           signed_quantity,
                           facts.notional_units,
                           facts.fee_units,
                           {},
                           {},
                           next_cursor,
                           next_checksum};
  static_assert(std::is_nothrow_move_constructible_v<LedgerTransaction>);
  transactions_.reserve(transactions_.size() + 1);
  transactions_.push_back(std::move(staged));
  cursor_ = next_cursor;
  checksum_ = next_checksum;
  position_units_ = next_position;
  const auto &committed = transactions_.back();
  SettledExposureEvidence settlement{policy_.run_id,
                                     policy_.portfolio_id,
                                     policy_.account_id,
                                     policy_.canonical_instrument_id,
                                     policy_.listing_id,
                                     policy_.quote_currency,
                                     policy_.run_mode,
                                     facts.intent.reservation_id,
                                     facts.intent.intent_id,
                                     facts.fill_id,
                                     transaction_id,
                                     cursor_,
                                     checksum_,
                                     signed_quantity,
                                     position_units_,
                                     policy_.quantity_scale,
                                     facts.intent.execution.quality,
                                     facts.intent.execution.run_input_sequence,
                                     facts.fill_time_nanoseconds};
  return {LedgerFailure::None, settlement, committed.transaction_id};
}

LedgerResult LedgerAuthority::reverse(LedgerTransactionId transaction_id,
                                      LedgerCorrectionId correction_id) {
  for (const auto &transaction : transactions_) {
    if (transaction.correction_id == correction_id) {
      if (transaction.original_transaction_id == transaction_id)
        return {LedgerFailure::None, {}, transaction.transaction_id};
      return {LedgerFailure::CorrectionConflict, {}, {}};
    }
  }
  const LedgerTransaction *original = nullptr;
  for (const auto &transaction : transactions_) {
    if (transaction.transaction_id == transaction_id) {
      original = &transaction;
      break;
    }
  }
  if (original == nullptr)
    return {LedgerFailure::UnknownTransaction, {}, {}};
  if (original->kind != LedgerTransactionKind::Fill)
    return {LedgerFailure::InvalidReversal, {}, {}};
  for (const auto &transaction : transactions_) {
    if (transaction.original_transaction_id == transaction_id)
      return {LedgerFailure::AlreadyCompensated, {}, {}};
  }
  std::vector<LedgerEntry> entries;
  entries.reserve(original->entries.size());
  for (const auto &entry : original->entries) {
    if (entry.amount_units == std::numeric_limits<AmountUnits>::min())
      return {LedgerFailure::ArithmeticOverflow, {}, {}};
    entries.push_back({entry.account, entry.unit, -entry.amount_units});
  }
  if (original->signed_fill_quantity_units ==
          std::numeric_limits<AmountUnits>::min() ||
      original->trade_notional_units ==
          std::numeric_limits<AmountUnits>::min() ||
      original->fee_units == std::numeric_limits<AmountUnits>::min())
    return {LedgerFailure::ArithmeticOverflow, {}, {}};
  const auto quantity = -original->signed_fill_quantity_units;
  AmountUnits next_position{};
  if (__builtin_add_overflow(position_units_, quantity, &next_position) ||
      !balanced(entries))
    return {LedgerFailure::ArithmeticOverflow, {}, {}};
  const auto semantics = transaction_semantics(
      policy_, original->source_fill, LedgerTransactionKind::Compensation,
      entries, quantity, -original->trade_notional_units, -original->fee_units,
      transaction_id, correction_id);
  const auto reversal_id =
      LedgerTransactionId::from_sha256_digest(sha256(semantics));
  if (transactions_.size() == std::numeric_limits<std::uint64_t>::max())
    return {LedgerFailure::ArithmeticOverflow, {}, {}};
  const auto next_cursor = *StreamCursor::at_sequence(
      policy_.ledger_stream_id, policy_.ledger_epoch,
      static_cast<std::uint64_t>(transactions_.size() + 1));
  const auto next_checksum =
      chained_checksum(checksum_, semantics, next_cursor);
  LedgerTransaction staged{reversal_id,
                           LedgerTransactionKind::Compensation,
                           policy_,
                           original->source_fill,
                           std::move(entries),
                           quantity,
                           -original->trade_notional_units,
                           -original->fee_units,
                           transaction_id,
                           correction_id,
                           next_cursor,
                           next_checksum};
  static_assert(std::is_nothrow_move_constructible_v<LedgerTransaction>);
  transactions_.reserve(transactions_.size() + 1);
  transactions_.push_back(std::move(staged));
  cursor_ = next_cursor;
  checksum_ = next_checksum;
  position_units_ = next_position;
  return {LedgerFailure::None, {}, reversal_id};
}

const std::vector<LedgerTransaction> &
LedgerAuthority::transactions() const noexcept {
  return transactions_;
}

StreamCursor LedgerAuthority::cursor() const noexcept { return cursor_; }

const Sha256Digest &LedgerAuthority::checksum() const noexcept {
  return checksum_;
}

AmountUnits LedgerAuthority::position_units() const noexcept {
  return position_units_;
}
} // namespace chronos::accounting
