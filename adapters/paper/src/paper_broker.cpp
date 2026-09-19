#include "chronos/adapters/paper/paper_broker.hpp"
#include <limits>
#include <type_traits>
namespace chronos::adapters::paper {
using namespace contracts;
namespace {
std::optional<AmountUnits> notional(AmountUnits price, AmountUnits quantity,
                                    const PaperExecutionEvidence &e) {
  __int128 value = static_cast<__int128>(price) * quantity;
  const int shift = static_cast<int>(e.money_scale.exponent()) -
                    e.price_scale.exponent() - e.quantity_scale.exponent();
  if (shift > 0) {
    for (int i = 0; i < shift; ++i)
      if (__builtin_mul_overflow(value, static_cast<__int128>(10), &value))
        return {};
  } else {
    __int128 divisor = 1;
    for (int i = 0; i < -shift; ++i)
      divisor *= 10;
    value = value / divisor + (value % divisor != 0 ? 1 : 0);
  }
  if (value > std::numeric_limits<AmountUnits>::max())
    return {};
  return static_cast<AmountUnits>(value);
}
} // namespace
PaperBrokerResult PaperBroker::submit(const PaperIntent &intent,
                                      const PaperExecutionEvidence &market,
                                      const PaperBrokerPolicy &policy) {
  const auto &f = intent.facts();
  for (const auto &r : records_) {
    if (r.intent.facts().intent_id == f.intent_id) {
      if (r.intent == intent && r.market == market && r.policy == policy)
        return r.result;
      return {PaperBrokerFailure::IntentConflict, {}};
    }
  }
  auto evaluate = [&]() -> PaperBrokerResult {
    if (market != f.execution || market.run_mode != RunMode::backtest ||
        market.bid_price_units <= 0 ||
        market.ask_price_units < market.bid_price_units ||
        market.price_tick_units <= 0 || market.quantity_step_units <= 0 ||
        market.bid_price_units % market.price_tick_units != 0 ||
        market.ask_price_units % market.price_tick_units != 0 ||
        f.quantity_units <= 0 ||
        f.quantity_units % market.quantity_step_units != 0)
      return {PaperBrokerFailure::InvalidEvidence, {}};
    if (policy.model_version != market.broker_model_version ||
        policy.slippage_ticks < 0 || policy.fee_basis_points < 0 ||
        policy.acknowledgement_latency_nanoseconds < 0 ||
        policy.fill_latency_nanoseconds <
            policy.acknowledgement_latency_nanoseconds)
      return {PaperBrokerFailure::InvalidPolicy, {}};
    AmountUnits slippage{}, price{};
    std::int64_t ack{}, fill{};
    if (__builtin_mul_overflow(policy.slippage_ticks, market.price_tick_units,
                               &slippage) ||
        __builtin_add_overflow(market.logical_time_nanoseconds,
                               policy.acknowledgement_latency_nanoseconds,
                               &ack) ||
        __builtin_add_overflow(market.logical_time_nanoseconds,
                               policy.fill_latency_nanoseconds, &fill))
      return {PaperBrokerFailure::ArithmeticOverflow, {}};
    if (fill >= f.logical_expiry_nanoseconds)
      return {PaperBrokerFailure::Expired, {}};
    const bool overflow =
        f.side == PaperSide::Buy
            ? __builtin_add_overflow(market.ask_price_units, slippage, &price)
            : __builtin_sub_overflow(market.bid_price_units, slippage, &price);
    if (overflow || price <= 0)
      return {PaperBrokerFailure::ArithmeticOverflow, {}};
    auto amount = notional(price, f.quantity_units, market);
    if (!amount)
      return {PaperBrokerFailure::ArithmeticOverflow, {}};
    auto fee = checked_multiply_divide(*amount, policy.fee_basis_points, 10000,
                                       RoundingMode::toward_positive);
    if (!fee)
      return {PaperBrokerFailure::ArithmeticOverflow, {}};
    return {PaperBrokerFailure::None,
            PaperFill({*PaperOrderId::from_bytes(f.intent_id.bytes()),
                       *PaperFillId::from_bytes(f.intent_id.bytes()), f, policy,
                       price, *amount, *fee, ack, fill})};
  };
  auto result = evaluate();
  Record staged{intent, market, policy, result};
  records_.reserve(records_.size() + 1);
  static_assert(std::is_nothrow_move_constructible_v<Record>);
  static_assert(std::is_nothrow_move_constructible_v<PaperBrokerResult>);
  records_.push_back(std::move(staged));
  return result;
}
} // namespace chronos::adapters::paper
