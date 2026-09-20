#include "chronos/accounting/ledger.hpp"
#include "chronos/adapters/paper/paper_broker.hpp"
#include "microtest.hpp"
#include "paper_broker_fixture.hpp"

#include <array>
#include <limits>
#include <type_traits>

namespace {
using namespace chronos::test_support::paper;
namespace accounting = chronos::accounting;
namespace paper = chronos::adapters::paper;

accounting::LedgerPolicy policy() {
  const auto key = target_key();
  return {.run_id = id<contracts::RunId>(30),
          .portfolio_id = key.portfolio_id(),
          .account_id = key.account_id(),
          .canonical_instrument_id = key.canonical_instrument_id(),
          .listing_id = key.listing_id(),
          .quote_currency = version(120),
          .run_mode = contracts::RunMode::backtest,
          .ledger_stream_id = id<contracts::StreamId>(121),
          .ledger_epoch = 1,
          .posting_policy_version = version(124),
          .quantity_scale = exposure_scale(),
          .money_scale = contracts::DecimalScale::from_exponent(6).value(),
          .notional_rounding = contracts::RoundingMode::toward_positive,
          .fee_rounding = contracts::RoundingMode::toward_positive};
}

contracts::PaperFill fill(bool sell = false,
                          contracts::AmountUnits fee_bps = 100) {
  auto intent = broker_intent(sell);
  auto evidence = intent.facts().execution;
  paper::PaperBroker broker;
  auto result = broker.submit(
      intent, evidence, {evidence.broker_model_version, 2, fee_bps, 1, 3});
  if (!result.accepted())
    std::abort();
  return *result.fill;
}

contracts::AmountUnits sum(const accounting::LedgerTransaction &transaction,
                           accounting::LedgerUnit unit) {
  contracts::AmountUnits result{};
  for (const auto &entry : transaction.entries) {
    if (entry.unit == unit)
      result += entry.amount_units;
  }
  return result;
}

TEST_CASE("ledger posts balanced buy with fee and settlement provenance") {
  accounting::LedgerAuthority ledger(policy());
  const auto source = fill();
  const auto result = ledger.post(source);
  CHECK(result.accepted());
  CHECK(ledger.transactions().size() == 1);
  const auto &transaction = ledger.transactions().front();
  CHECK(transaction.source_fill == source.facts());
  CHECK(transaction.source_fill.order_id == source.facts().order_id);
  CHECK(transaction.source_fill.fill_id == source.facts().fill_id);
  CHECK(transaction.source_fill.intent.intent_id ==
        source.facts().intent.intent_id);
  CHECK(transaction.source_fill.model_policy.model_version ==
        source.facts().model_policy.model_version);
  CHECK(transaction.source_fill.order_id.bytes() ==
        transaction.source_fill.intent.intent_id.bytes());
  CHECK(transaction.source_fill.fill_id.bytes() ==
        transaction.source_fill.intent.intent_id.bytes());
  CHECK(transaction.source_fill.acknowledgement_time_nanoseconds == 121);
  CHECK(transaction.source_fill.fill_time_nanoseconds == 123);
  CHECK(sum(transaction, accounting::LedgerUnit::BaseQuantity) == 0);
  CHECK(sum(transaction, accounting::LedgerUnit::QuoteCurrency) == 0);
  CHECK(transaction.signed_fill_quantity_units == 40);
  CHECK(transaction.trade_notional_units == 4120);
  CHECK(transaction.fee_units == 42);
  CHECK(result.settlement->posted_delta_units == 40);
  CHECK(result.settlement->position_units == 40);
  CHECK(result.settlement->reservation_id ==
        source.facts().intent.reservation_id);
  CHECK(result.settlement->intent_id == source.facts().intent.intent_id);
  CHECK(result.settlement->fill_id == source.facts().fill_id);
  CHECK(result.settlement->transaction_id == transaction.transaction_id);
  CHECK(result.settlement->ledger_cursor == ledger.cursor());
  CHECK(result.settlement->ledger_checksum == ledger.checksum());
  CHECK(result.settlement->run_input_sequence ==
        source.facts().intent.execution.run_input_sequence);
  CHECK(result.settlement->logical_time_nanoseconds ==
        source.facts().fill_time_nanoseconds);
}

TEST_CASE("ledger posts sell with fee using genuine settled broker fixture") {
  accounting::LedgerAuthority ledger(policy());
  const auto source = fill(true);
  const auto result = ledger.post(source);
  CHECK(result.accepted());
  const auto &transaction = ledger.transactions().front();
  CHECK(transaction.signed_fill_quantity_units == -20);
  CHECK(transaction.trade_notional_units == 1960);
  CHECK(transaction.fee_units == 20);
  CHECK(result.settlement->position_units == -20);
  CHECK(sum(transaction, accounting::LedgerUnit::BaseQuantity) == 0);
  CHECK(sum(transaction, accounting::LedgerUnit::QuoteCurrency) == 0);
}

TEST_CASE("ledger exact retry is idempotent and changed fill conflicts") {
  accounting::LedgerAuthority ledger(policy());
  const auto source = fill();
  const auto accepted = ledger.post(source);
  CHECK(accepted.accepted());
  CHECK(ledger.post(source) == accepted);
  CHECK(ledger.transactions().size() == 1);

  auto intent = broker_intent();
  auto evidence = intent.facts().execution;
  paper::PaperBroker alternate;
  auto changed = alternate.submit(
      intent, evidence, {evidence.broker_model_version, 3, 100, 1, 3});
  CHECK(changed.accepted());
  const auto before_cursor = ledger.cursor();
  const auto before_checksum = ledger.checksum();
  CHECK(ledger.post(*changed.fill).failure ==
        accounting::LedgerFailure::FillConflict);
  CHECK(ledger.transactions().size() == 1);
  CHECK(ledger.cursor() == before_cursor);
  CHECK(ledger.checksum() == before_checksum);
}

TEST_CASE("ledger rejects scope scale mode and policy mismatches atomically") {
  const auto source = fill();
  for (int scenario = 0; scenario < 5; ++scenario) {
    auto invalid = policy();
    switch (scenario) {
    case 0:
      invalid.account_id = id<contracts::AccountId>(200);
      break;
    case 1:
      invalid.run_mode = contracts::RunMode::live_paper;
      break;
    case 2:
      invalid.quantity_scale =
          contracts::DecimalScale::from_exponent(5).value();
      break;
    case 3:
      invalid.money_scale = contracts::DecimalScale::from_exponent(5).value();
      break;
    case 4:
      invalid.ledger_epoch = 0;
      break;
    }
    accounting::LedgerAuthority ledger(invalid);
    const auto before_cursor = ledger.cursor();
    const auto before_checksum = ledger.checksum();
    CHECK(!ledger.post(source).accepted());
    CHECK(ledger.transactions().empty());
    CHECK(ledger.cursor() == before_cursor);
    CHECK(ledger.checksum() == before_checksum);
  }
}

TEST_CASE("ledger rejects genuine broker cash overflow without an append") {
  const auto price =
      std::numeric_limits<contracts::AmountUnits>::max() / 40 - 1;
  auto intent = broker_intent(false, price);
  auto evidence = intent.facts().execution;
  paper::PaperBroker broker;
  const auto generated = broker.submit(
      intent, evidence, {evidence.broker_model_version, 0, 1, 0, 0});
  CHECK(generated.accepted());
  accounting::LedgerAuthority ledger(policy());
  const auto before_cursor = ledger.cursor();
  const auto before_checksum = ledger.checksum();
  CHECK(ledger.post(*generated.fill).failure ==
        accounting::LedgerFailure::ArithmeticOverflow);
  CHECK(ledger.transactions().empty());
  CHECK(ledger.cursor() == before_cursor);
  CHECK(ledger.checksum() == before_checksum);
  CHECK(ledger.position_units() == 0);
}

TEST_CASE("paper fill identity is broker-bound and model mismatch rejects") {
  auto intent = broker_intent();
  auto evidence = intent.facts().execution;
  paper::PaperBroker broker;
  auto wrong_model = version(250);
  const auto rejected =
      broker.submit(intent, evidence, {wrong_model, 0, 0, 0, 0});
  CHECK(rejected.failure == contracts::PaperBrokerFailure::InvalidPolicy);
  CHECK(!rejected.fill);

  paper::PaperBroker accepted_broker;
  const auto accepted = accepted_broker.submit(
      intent, evidence, {evidence.broker_model_version, 0, 0, 1, 2});
  CHECK(accepted.accepted());
  CHECK(accepted.fill->facts().order_id.bytes() ==
        intent.facts().intent_id.bytes());
  CHECK(accepted.fill->facts().fill_id.bytes() ==
        intent.facts().intent_id.bytes());
  CHECK(accepted.fill->facts().acknowledgement_time_nanoseconds == 121);
  CHECK(accepted.fill->facts().fill_time_nanoseconds == 122);
}

TEST_CASE("ledger appends an exact immutable compensating reversal") {
  accounting::LedgerAuthority ledger(policy());
  const auto posted = ledger.post(fill());
  CHECK(posted.accepted());
  const auto original = ledger.transactions().front();
  const auto correction = id<contracts::LedgerCorrectionId>(201);
  const auto reversed = ledger.reverse(original.transaction_id, correction);
  CHECK(reversed.accepted());
  CHECK(ledger.transactions().size() == 2);
  CHECK(ledger.transactions().front() == original);
  const auto &compensation = ledger.transactions().back();
  CHECK(compensation.kind == accounting::LedgerTransactionKind::Compensation);
  CHECK(compensation.original_transaction_id == original.transaction_id);
  CHECK(compensation.correction_id == correction);
  CHECK(compensation.entries.size() == original.entries.size());
  for (std::size_t index = 0; index < original.entries.size(); ++index) {
    CHECK(compensation.entries[index].account ==
          original.entries[index].account);
    CHECK(compensation.entries[index].unit == original.entries[index].unit);
    CHECK(compensation.entries[index].amount_units ==
          -original.entries[index].amount_units);
  }
  CHECK(ledger.position_units() == 0);
  CHECK(ledger.reverse(original.transaction_id, correction) == reversed);
  CHECK(ledger.transactions().size() == 2);
  CHECK(ledger
            .reverse(original.transaction_id,
                     id<contracts::LedgerCorrectionId>(202))
            .failure == accounting::LedgerFailure::AlreadyCompensated);
  CHECK(ledger
            .reverse(id<contracts::LedgerTransactionId>(203),
                     id<contracts::LedgerCorrectionId>(204))
            .failure == accounting::LedgerFailure::UnknownTransaction);
  CHECK(ledger
            .reverse(compensation.transaction_id,
                     id<contracts::LedgerCorrectionId>(205))
            .failure == accounting::LedgerFailure::InvalidReversal);
  CHECK(ledger.reverse(id<contracts::LedgerTransactionId>(206), correction)
            .failure == accounting::LedgerFailure::CorrectionConflict);
}

TEST_CASE("ledger cursor checksum and transactions replay deterministically") {
  accounting::LedgerAuthority first(policy());
  accounting::LedgerAuthority second(policy());
  const auto source = fill();
  CHECK(first.post(source).accepted());
  CHECK(second.post(source).accepted());
  const auto correction = id<contracts::LedgerCorrectionId>(210);
  CHECK(first.reverse(first.transactions().front().transaction_id, correction)
            .accepted());
  CHECK(second.reverse(second.transactions().front().transaction_id, correction)
            .accepted());
  CHECK(first.transactions() == second.transactions());
  CHECK(first.cursor() == second.cursor());
  CHECK(first.checksum() == second.checksum());
}

static_assert(!std::is_copy_constructible_v<accounting::LedgerAuthority>);
static_assert(!std::is_copy_assignable_v<accounting::LedgerAuthority>);
} // namespace
