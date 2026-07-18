#include "chronos/contracts/fixed_point.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <limits>

using chronos::contracts::checked_multiply_divide;
using chronos::contracts::checked_weighted_average;
using chronos::contracts::DecimalScale;
using chronos::contracts::DefinitionId;
using chronos::contracts::Money;
using chronos::contracts::Price;
using chronos::contracts::Quantity;
using chronos::contracts::RoundingMode;
using chronos::contracts::VersionRef;

namespace {
VersionRef definition(std::string_view value, std::uint64_t version) {
  return VersionRef::from(DefinitionId::parse(value).value(), version).value();
}

const auto kPriceDefinition =
    definition("018f1f6e-7d3a-7c4b-8a91-012345678901", 7);
const auto kOtherDefinition =
    definition("018f1f6e-7d3a-7c4b-8a91-012345678902", 7);
const auto kMoneyDefinition =
    definition("018f1f6e-7d3a-7c4b-8a91-012345678903", 9);
} // namespace

TEST_CASE("fixed-point values preserve exact type-scoped equality") {
  CHECK(Price::from_units(42, kPriceDefinition) ==
        Price::from_units(42, kPriceDefinition));
  CHECK(Price::from_units(42, kPriceDefinition) !=
        Price::from_units(43, kPriceDefinition));
  CHECK(Price::from_units(42, kPriceDefinition) !=
        Price::from_units(42, kOtherDefinition));
  CHECK(Quantity::from_units(42, kPriceDefinition) ==
        Quantity::from_units(42, kPriceDefinition));
  CHECK(Money::from_units(-7, kMoneyDefinition) ==
        Money::from_units(-7, kMoneyDefinition));
  const auto lower = Price::from_units(1, kPriceDefinition).value();
  const auto higher = Price::from_units(2, kPriceDefinition).value();
  CHECK(lower.checked_compare(higher) == std::strong_ordering::less);
  CHECK(!lower.checked_compare(Price::from_units(2, kOtherDefinition).value())
             .has_value());
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
  const auto price = Price::from_units(7, kPriceDefinition).value();
  const auto money = Money::from_units(7, kMoneyDefinition).value();
  CHECK(price.checked_add(Price::from_units(5, kPriceDefinition).value()) ==
        Price::from_units(12, kPriceDefinition));
  CHECK(
      money.checked_subtract(Money::from_units(5, kMoneyDefinition).value()) ==
      Money::from_units(2, kMoneyDefinition));
  CHECK(!Price::from_units(maximum, kPriceDefinition)
             ->checked_add(Price::from_units(1, kPriceDefinition).value())
             .has_value());
  CHECK(
      !Quantity::from_units(minimum, kPriceDefinition)
           ->checked_subtract(Quantity::from_units(1, kPriceDefinition).value())
           .has_value());
  CHECK(!price.checked_add(Price::from_units(5, kOtherDefinition).value())
             .has_value());
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

TEST_CASE("weighted average rounds the complete translated value once") {
  CHECK(checked_weighted_average(102, 1, 101, 1, 2,
                                 RoundingMode::nearest_ties_to_even) == 102);
  CHECK(checked_weighted_average(101, 1, 100, 1, 2,
                                 RoundingMode::nearest_ties_to_even) == 100);
  CHECK(checked_weighted_average(-101, 1, -102, 1, 2,
                                 RoundingMode::nearest_ties_to_even) == -102);
  CHECK(!checked_weighted_average(std::numeric_limits<std::int64_t>::min(),
                                  std::numeric_limits<std::int64_t>::min(),
                                  std::numeric_limits<std::int64_t>::min(),
                                  std::numeric_limits<std::int64_t>::min(), 1,
                                  RoundingMode::nearest_ties_to_even)
             .has_value());
  CHECK(!checked_weighted_average(1, 1, 2, 1, 0,
                                  RoundingMode::nearest_ties_to_even)
             .has_value());
}
