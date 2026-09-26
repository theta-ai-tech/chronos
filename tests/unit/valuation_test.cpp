#include "chronos/accounting/ledger.hpp"
#include "chronos/accounting/valuation.hpp"
#include "chronos/adapters/paper/paper_broker.hpp"
#include "microtest.hpp"
#include "paper_broker_fixture.hpp"
#include <limits>
namespace {
using namespace chronos::test_support::paper;
namespace accounting = chronos::accounting;
accounting::LedgerPolicy scope() {
  const auto key = target_key();
  return {id<contracts::RunId>(30),
          key.portfolio_id(),
          key.account_id(),
          key.canonical_instrument_id(),
          key.listing_id(),
          version(120),
          contracts::RunMode::backtest,
          id<contracts::StreamId>(121),
          1,
          version(124),
          contracts::DecimalScale::from_exponent(0).value(),
          contracts::DecimalScale::from_exponent(0).value(),
          contracts::RoundingMode::toward_positive,
          contracts::RoundingMode::toward_positive};
}
// Independent ledger-history vectors: source provenance comes from a real
// authority-minted fill, economic amounts are explicit numeric ledger fixtures.
accounting::LedgerTransaction tx(std::uint8_t seed, std::int64_t qty,
                                 std::int64_t notional, std::int64_t fee) {
  auto intent = broker_intent();
  chronos::adapters::paper::PaperBroker broker;
  auto fill = broker.submit(
      intent, intent.facts().execution,
      {intent.facts().execution.broker_model_version, 0, 0, 0, 0});
  const auto cash = qty > 0 ? -notional : notional;
  return {
      id<contracts::LedgerTransactionId>(seed),
      accounting::LedgerTransactionKind::Fill,
      scope(),
      fill.fill->facts(),
      {{accounting::LedgerAccount::PositionQuantity,
        accounting::LedgerUnit::BaseQuantity, qty},
       {accounting::LedgerAccount::QuantityClearing,
        accounting::LedgerUnit::BaseQuantity, -qty},
       {accounting::LedgerAccount::QuoteCash,
        accounting::LedgerUnit::QuoteCurrency, cash - fee},
       {accounting::LedgerAccount::TradeClearing,
        accounting::LedgerUnit::QuoteCurrency, -cash},
       {accounting::LedgerAccount::FeeExpense,
        accounting::LedgerUnit::QuoteCurrency, fee}},
      qty,
      notional,
      fee,
      {},
      {},
      contracts::StreamCursor::at_sequence(scope().ledger_stream_id, 1, seed)
          .value(),
      {}};
}
contracts::PositionMark mark(std::int64_t price) {
  const auto s = scope();
  auto intent = broker_intent();
  return {s.run_id,
          s.portfolio_id,
          s.account_id,
          s.canonical_instrument_id,
          s.listing_id,
          s.quote_currency,
          s.run_mode,
          id<contracts::StateViewId>(201),
          intent.facts().execution.market_lineage,
          version(204),
          price,
          contracts::DecimalScale::from_exponent(0).value(),
          s.quantity_scale,
          s.money_scale,
          valid_quality(),
          5,
          120,
          version(202)};
}
accounting::ValuationPolicy policy() {
  return {scope(),      version(203),
          version(204), contracts::DecimalScale::from_exponent(0).value(),
          version(202), 10};
}
TEST_CASE("valuation independent long short cross-zero fee vectors") {
  std::vector history{tx(1, 10, 1000, 2), tx(2, -4, 440, 1)};
  auto result =
      accounting::value_position(history, mark(120), policy(), {5, 120});
  CHECK(result.available());
  CHECK(result.position->position_units == 6);
  CHECK(result.position->remaining_basis_units == 600);
  CHECK(result.position->realized_gross_units == 40);
  CHECK(*result.unrealized_gross_units == 120);
  CHECK(result.position->fees_units == 3);
  CHECK(*result.total_net_units == 157);
  history.push_back(tx(3, -8, 720, 2));
  result = accounting::value_position(history, mark(80), policy(), {5, 120});
  CHECK(result.available());
  CHECK(result.position->position_units == -2);
  CHECK(result.position->remaining_basis_units == 180);
  CHECK(result.position->realized_gross_units == -20);
  CHECK(*result.unrealized_gross_units == 20);
  CHECK(*result.total_net_units == -5);
  CHECK(result.position->closing_events == 2);
  CHECK(result.position->profitable_gross_closing_events == 1);
}
TEST_CASE("valuation FIFO differentiates lots and conserves partial money "
          "remainder") {
  std::vector fifo{tx(1, 2, 200, 0), tx(2, 3, 330, 0), tx(3, -4, 480, 0)};
  auto v = accounting::value_position(fifo, mark(115), policy(), {5, 120});
  CHECK(v.available());
  CHECK(v.position->realized_gross_units == 60);
  CHECK(v.position->remaining_basis_units == 110);
  CHECK(*v.unrealized_gross_units == 5);
  std::vector partial{tx(1, 3, 10, 0)};
  for (std::uint8_t i = 2; i <= 4; ++i) {
    partial.push_back(tx(i, -1, 4, 0));
    auto p = accounting::derive_position(partial, scope());
    CHECK(p.available());
    CHECK(p.position->realized_gross_units == (i == 4 ? 2 : i - 1));
  }
  auto p = accounting::derive_position(partial, scope());
  CHECK(p.position->position_units == 0);
  CHECK(p.position->remaining_basis_units == 0);
}
TEST_CASE("valuation rebuild excludes compensated economics and fees") {
  auto first = tx(1, 10, 1000, 2);
  auto second = tx(2, -4, 440, 1);
  auto reversal = first;
  reversal.transaction_id = id<contracts::LedgerTransactionId>(3);
  reversal.kind = accounting::LedgerTransactionKind::Compensation;
  reversal.original_transaction_id = first.transaction_id;
  reversal.correction_id = id<contracts::LedgerCorrectionId>(3);
  for (auto &entry : reversal.entries)
    entry.amount_units = -entry.amount_units;
  reversal.signed_fill_quantity_units = -first.signed_fill_quantity_units;
  reversal.trade_notional_units = -first.trade_notional_units;
  reversal.fee_units = -first.fee_units;
  const std::vector history{first, second, reversal};
  auto p = accounting::derive_position(history, scope());
  CHECK(p.available());
  CHECK(p.position->position_units == -4);
  CHECK(p.position->remaining_basis_units == 440);
  CHECK(p.position->fees_units == 1);
  CHECK(p.position->realized_gross_units == 0);
}
TEST_CASE(
    "valuation unavailable marks and arithmetic never masquerade as zero") {
  const std::vector history{tx(1, 10, 1000, 2)};
  for (int i = 0; i < 11; ++i) {
    auto m = mark(120);
    if (i == 0)
      m.logical_time_nanoseconds = 100;
    if (i == 1)
      m.logical_time_nanoseconds = 121;
    if (i == 2)
      m.listing_id = id<contracts::ListingId>(231);
    if (i == 3)
      m.quote_currency = version(231);
    if (i == 4)
      m.quality = non_valid_quality(contracts::QualityStatus::stale);
    if (i == 5)
      m.mark_policy_version = version(231);
    if (i == 6)
      m.run_input_sequence = 6;
    if (i == 7)
      m.price_definition = version(232);
    if (i == 8)
      m.price_scale = contracts::DecimalScale::from_exponent(1).value();
    if (i == 9 || i == 10) {
      const std::array streams{id<contracts::StreamId>(150)};
      const std::array cursors{
          *contracts::StreamCursor::at_sequence(streams[0], 1, 5)};
      m.source_lineage = *contracts::StateLineage::from(
          i == 9 ? id<contracts::RunId>(231) : m.run_id,
          i == 10 ? m.run_input_sequence - 1 : m.run_input_sequence, streams,
          cursors);
    }
    auto v = accounting::value_position(history, m, policy(), {5, 120});
    CHECK(!v.available());
    CHECK(!v.unrealized_gross_units);
    CHECK(!v.total_net_units);
  }
  CHECK(!accounting::value_position(history, std::nullopt, policy(), {5, 120})
             .available());
  auto v = accounting::value_position(
      history, mark(std::numeric_limits<std::int64_t>::max()), policy(),
      {5, 120});
  CHECK(v.failure == accounting::ValuationFailure::ArithmeticOverflow);
  CHECK(!v.total_net_units);
  auto empty = accounting::derive_position({}, scope());
  CHECK(empty.available());
  CHECK(empty.position->closing_events == 0);
  auto overflow = std::vector{
      tx(1, std::numeric_limits<std::int64_t>::max(), 1, 0), tx(2, 1, 1, 0)};
  CHECK(accounting::derive_position(overflow, scope()).failure ==
        accounting::ValuationFailure::ArithmeticOverflow);
}
} // namespace
