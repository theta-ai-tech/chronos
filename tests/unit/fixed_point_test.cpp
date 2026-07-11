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
  CHECK(Price{42} == Price{42});
  CHECK(Price{42} != Price{43});
  CHECK(Quantity{42} == Quantity{42});
  CHECK(Money{-7} == Money{-7});
  CHECK(Price{1} < Price{2});
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
  CHECK(Price{7}.checked_add(Price{5}) == Price{12});
  CHECK(Money{7}.checked_subtract(Money{5}) == Money{2});
  CHECK(!Price{maximum}.checked_add(Price{1}).has_value());
  CHECK(!Quantity{minimum}.checked_subtract(Quantity{1}).has_value());
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
