#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace chronos::contracts {

using AmountUnits = std::int64_t;

static_assert(sizeof(AmountUnits) == 8);

enum class RoundingMode : std::uint8_t {
  toward_zero,
  toward_negative,
  toward_positive,
  nearest_ties_to_even,
};

class DecimalScale final {
public:
  static constexpr std::uint8_t kMaxExponent = 18;

  [[nodiscard]] static constexpr std::optional<DecimalScale>
  from_exponent(std::uint8_t exponent) noexcept {
    if (exponent > kMaxExponent) {
      return std::nullopt;
    }
    return DecimalScale(exponent);
  }

  [[nodiscard]] constexpr std::uint8_t exponent() const noexcept {
    return exponent_;
  }

  [[nodiscard]] constexpr AmountUnits denominator() const noexcept {
    AmountUnits result = 1;
    for (std::uint8_t index = 0; index < exponent_; ++index) {
      result *= 10;
    }
    return result;
  }

  auto operator<=>(const DecimalScale &) const = default;

private:
  explicit constexpr DecimalScale(std::uint8_t exponent) noexcept
      : exponent_(exponent) {}

  std::uint8_t exponent_;
};

template <typename Tag> class FixedPoint final {
public:
  using units_type = AmountUnits;
  using definition_ref_type = std::uint64_t;

  [[nodiscard]] static constexpr std::optional<FixedPoint>
  from_units(AmountUnits units, definition_ref_type definition_ref) noexcept {
    if (definition_ref == 0) {
      return std::nullopt;
    }
    return FixedPoint(units, definition_ref);
  }

  [[nodiscard]] constexpr AmountUnits units() const noexcept { return units_; }
  [[nodiscard]] constexpr definition_ref_type definition_ref() const noexcept {
    return definition_ref_;
  }

  bool operator==(const FixedPoint &) const = default;

  [[nodiscard]] constexpr std::optional<std::strong_ordering>
  checked_compare(FixedPoint other) const noexcept {
    if (definition_ref_ != other.definition_ref_) {
      return std::nullopt;
    }
    return units_ <=> other.units_;
  }

  [[nodiscard]] constexpr std::optional<FixedPoint>
  checked_add(FixedPoint other) const noexcept {
    if (definition_ref_ != other.definition_ref_) {
      return std::nullopt;
    }
    AmountUnits result{};
    if (__builtin_add_overflow(units_, other.units_, &result)) {
      return std::nullopt;
    }
    return FixedPoint(result, definition_ref_);
  }

  [[nodiscard]] constexpr std::optional<FixedPoint>
  checked_subtract(FixedPoint other) const noexcept {
    if (definition_ref_ != other.definition_ref_) {
      return std::nullopt;
    }
    AmountUnits result{};
    if (__builtin_sub_overflow(units_, other.units_, &result)) {
      return std::nullopt;
    }
    return FixedPoint(result, definition_ref_);
  }

private:
  explicit constexpr FixedPoint(AmountUnits units,
                                definition_ref_type definition_ref) noexcept
      : units_(units), definition_ref_(definition_ref) {}

  AmountUnits units_;
  definition_ref_type definition_ref_;
};

struct PriceTag;
struct QuantityTag;
struct MoneyTag;

using Price = FixedPoint<PriceTag>;
using Quantity = FixedPoint<QuantityTag>;
using Money = FixedPoint<MoneyTag>;

static_assert(sizeof(Price) == 16);
static_assert(sizeof(Quantity) == 16);
static_assert(sizeof(Money) == 16);
static_assert(std::is_trivially_copyable_v<Price>);
static_assert(std::is_trivially_copyable_v<Quantity>);
static_assert(std::is_trivially_copyable_v<Money>);

[[nodiscard]] constexpr std::optional<AmountUnits>
checked_multiply_divide(AmountUnits value, AmountUnits multiplier,
                        AmountUnits divisor, RoundingMode rounding) noexcept {
  if (divisor <= 0) {
    return std::nullopt;
  }

  const __int128 product = static_cast<__int128>(value) * multiplier;
  __int128 quotient = product / divisor;
  const __int128 remainder = product % divisor;

  if (remainder != 0) {
    const bool negative = product < 0;
    switch (rounding) {
    case RoundingMode::toward_zero:
      break;
    case RoundingMode::toward_negative:
      if (negative) {
        --quotient;
      }
      break;
    case RoundingMode::toward_positive:
      if (!negative) {
        ++quotient;
      }
      break;
    case RoundingMode::nearest_ties_to_even: {
      const __int128 magnitude = remainder < 0 ? -remainder : remainder;
      const __int128 twice_remainder = magnitude * 2;
      if (twice_remainder > divisor ||
          (twice_remainder == divisor && quotient % 2 != 0)) {
        quotient += negative ? -1 : 1;
      }
      break;
    }
    }
  }

  if (quotient < std::numeric_limits<AmountUnits>::min() ||
      quotient > std::numeric_limits<AmountUnits>::max()) {
    return std::nullopt;
  }
  return static_cast<AmountUnits>(quotient);
}

} // namespace chronos::contracts
