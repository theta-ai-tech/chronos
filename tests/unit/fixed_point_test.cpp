#include "chronos/contracts/fixed_point.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <limits>

using chronos::contracts::checked_multiply_divide;
using chronos::contracts::DecimalScale;
using chronos::contracts::Money;
using chronos::contracts::Price;
using chronos::contracts::Quantity;
using chronos::contracts::RoundingMode;

TEST_CASE("fixed-point values preserve exact type-scoped equality") {
  CHECK(Price::from_units(42, 7) == Price::from_units(42, 7));
  CHECK(Price::from_units(42, 7) != Price::from_units(43, 7));
  CHECK(Price::from_units(42, 7) != Price::from_units(42, 8));
  CHECK(Quantity::from_units(42, 7) == Quantity::from_units(42, 7));
  CHECK(Money::from_units(-7, 9) == Money::from_units(-7, 9));
  const auto lower = Price::from_units(1, 7).value();
  const auto higher = Price::from_units(2, 7).value();
  CHECK(lower.checked_compare(higher) == std::strong_ordering::less);
  CHECK(!lower.checked_compare(Price::from_units(2, 8).value()).has_value());
  CHECK(!Price::from_units(1, 0).has_value());
}

TEST_CASE("decimal scale is bounded and exact") {
  const auto scale = DecimalScale::from_exponent(8);
  CHECK(scale.has_value());
  CHECK(scale->exponent() == 8);
  CHECK(scale->denominator() == 100'000'000);
  CHECK(DecimalScale::from_exponent(18)->denominator() ==
        1'000'000'000'000'000'000LL);
  CHECK(!DecimalScale::from_exponent(19).has_value());
}

TEST_CASE("checked addition and subtraction reject overflow") {
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
  const auto price = Price::from_units(7, 1).value();
  const auto money = Money::from_units(7, 2).value();
  CHECK(price.checked_add(Price::from_units(5, 1).value()) ==
        Price::from_units(12, 1));
  CHECK(money.checked_subtract(Money::from_units(5, 2).value()) ==
        Money::from_units(2, 2));
  CHECK(!Price::from_units(maximum, 1)
             ->checked_add(Price::from_units(1, 1).value())
             .has_value());
  CHECK(!Quantity::from_units(minimum, 1)
             ->checked_subtract(Quantity::from_units(1, 1).value())
             .has_value());
  CHECK(!price.checked_add(Price::from_units(5, 2).value()).has_value());
}

TEST_CASE("multiply-divide applies every declared rounding direction") {
  CHECK(checked_multiply_divide(5, 1, 2, RoundingMode::toward_zero) == 2);
  CHECK(checked_multiply_divide(-5, 1, 2, RoundingMode::toward_zero) == -2);
  CHECK(checked_multiply_divide(5, 1, 2, RoundingMode::toward_negative) == 2);
  CHECK(checked_multiply_divide(-5, 1, 2, RoundingMode::toward_negative) == -3);
  CHECK(checked_multiply_divide(5, 1, 2, RoundingMode::toward_positive) == 3);
  CHECK(checked_multiply_divide(-5, 1, 2, RoundingMode::toward_positive) == -2);
  CHECK(checked_multiply_divide(5, 1, 2, RoundingMode::nearest_ties_to_even) ==
        2);
  CHECK(checked_multiply_divide(7, 1, 2, RoundingMode::nearest_ties_to_even) ==
        4);
  CHECK(checked_multiply_divide(-5, 1, 2, RoundingMode::nearest_ties_to_even) ==
        -2);
  CHECK(checked_multiply_divide(-7, 1, 2, RoundingMode::nearest_ties_to_even) ==
        -4);
}

TEST_CASE("multiply-divide widens intermediates and rejects invalid output") {
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  CHECK(checked_multiply_divide(maximum, maximum, maximum,
                                RoundingMode::toward_zero) == maximum);
  CHECK(!checked_multiply_divide(maximum, 2, 1, RoundingMode::toward_zero)
             .has_value());
  CHECK(
      !checked_multiply_divide(1, 1, 0, RoundingMode::toward_zero).has_value());
  CHECK(!checked_multiply_divide(1, 1, -1, RoundingMode::toward_zero)
             .has_value());
}
