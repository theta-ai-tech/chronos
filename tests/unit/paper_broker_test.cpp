#include "chronos/adapters/paper/paper_broker.hpp"
#include "microtest.hpp"
#include "paper_broker_fixture.hpp"
#include <limits>
#include <type_traits>
namespace {
using namespace chronos::test_support::paper;
namespace paper = chronos::adapters::paper;
TEST_CASE("paper broker deterministic full fill and semantic retry") {
  auto intent = broker_intent();
  auto evidence = intent.facts().execution;
  paper::PaperBroker broker;
  contracts::PaperBrokerPolicy policy{evidence.broker_model_version, 2, 100, 1,
                                      3};
  auto result = broker.submit(intent, evidence, policy);
  CHECK(result.accepted());
  CHECK(result.fill->facts().price_units == 103);
  CHECK(result.fill->facts().notional_units == 4120);
  CHECK(result.fill->facts().fee_units == 42);
  CHECK(result.fill->facts().fill_time_nanoseconds == 123);
  CHECK(result.fill->facts().intent == intent.facts());
  CHECK(broker.submit(intent, evidence, policy) == result);
  evidence.ask_price_units++;
  CHECK(broker.submit(intent, evidence, policy).failure ==
        contracts::PaperBrokerFailure::IntentConflict);
}
} // namespace

namespace {
TEST_CASE("paper broker sell uses genuine settled opening reservation") {
  auto intent = broker_intent(true);
  auto e = intent.facts().execution;
  paper::PaperBroker broker;
  auto result =
      broker.submit(intent, e, {e.broker_model_version, 2, 100, 0, 0});
  CHECK(result.accepted());
  CHECK(intent.facts().authorized_delta_units == -20);
  CHECK(intent.facts().side == contracts::PaperSide::Sell);
  CHECK(result.fill->facts().price_units == 98);
  CHECK(result.fill->facts().notional_units == 1960);
  CHECK(result.fill->facts().fee_units == 20);
}
TEST_CASE("paper broker rejects changed market and caches no fill atomically") {
  for (int scenario = 0; scenario < 13; ++scenario) {
    auto intent = broker_intent();
    auto e = intent.facts().execution;
    paper::PaperBroker broker;
    contracts::PaperBrokerPolicy policy{e.broker_model_version, 0, 0, 0, 0};
    switch (scenario) {
    case 0:
      e.quality = non_valid_quality(contracts::QualityStatus::stale);
      break;
    case 1:
      e.run_mode = contracts::RunMode::live_paper;
      break;
    case 2:
      e.listing_id = id<contracts::ListingId>(200);
      break;
    case 3:
      e.bid_price_units = 102;
      break;
    case 4:
      e.ask_price_units = 0;
      break;
    case 5:
      e.price_tick_units = 3;
      break;
    case 6:
      e.quantity_step_units = 3;
      break;
    case 7:
      e.run_input_sequence++;
      break;
    case 8:
      policy.slippage_ticks = -1;
      break;
    case 9:
      policy.slippage_ticks = std::numeric_limits<std::int64_t>::max();
      break;
    case 10:
      policy.fill_latency_nanoseconds = 100;
      break;
    case 11:
      policy.fill_latency_nanoseconds =
          std::numeric_limits<std::int64_t>::max();
      break;
    case 12:
      policy.acknowledgement_latency_nanoseconds = 1;
      break;
    }
    auto rejected = broker.submit(intent, e, policy);
    CHECK(!rejected.accepted());
    CHECK(!rejected.fill);
    CHECK(broker.submit(intent, e, policy) == rejected);
    policy.fee_basis_points++;
    CHECK(broker.submit(intent, e, policy).failure ==
          contracts::PaperBrokerFailure::IntentConflict);
  }
}
static_assert(!std::is_default_constructible_v<contracts::PaperFill>);
static_assert(!std::is_aggregate_v<contracts::PaperFill>);
} // namespace

namespace {
TEST_CASE("paper broker rejects notional fee and scaling overflow without "
          "partial fill") {
  for (int scenario = 0; scenario < 3; ++scenario) {
    auto intent = broker_intent(
        false,
        scenario == 0 ? std::numeric_limits<std::int64_t>::max() : 1000000000,
        0, scenario == 2 ? 18 : 6);
    auto e = intent.facts().execution;
    paper::PaperBroker broker;
    contracts::PaperBrokerPolicy policy{
        e.broker_model_version, 0,
        scenario == 1 ? std::numeric_limits<std::int64_t>::max() : 0, 0, 0};
    // Use a sufficiently large notional for fee overflow.
    if (scenario == 1)
      intent = broker_intent(false, 1000000);
    e = intent.facts().execution;
    auto result = broker.submit(intent, e, policy);
    CHECK(result.failure == contracts::PaperBrokerFailure::ArithmeticOverflow);
    CHECK(!result.fill);
    CHECK(broker.submit(intent, e, policy) == result);
  }
}
TEST_CASE("paper broker decimal notional rounds toward positive once") {
  auto intent = broker_intent(false, 100, 6, 6);
  paper::PaperBroker broker;
  auto e = intent.facts().execution;
  auto result = broker.submit(intent, e, {e.broker_model_version, 0, 0, 0, 0});
  CHECK(result.accepted());
  CHECK(result.fill->facts().notional_units == 1);
  CHECK(result.fill->facts().fee_units == 0);
}
} // namespace
