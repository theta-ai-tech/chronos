"""Canonical Chronos value objects shared with the native hot path."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum

MIN_AMOUNT_UNITS = -(2**63)
MAX_AMOUNT_UNITS = 2**63 - 1


class ContractValueError(ValueError):
    """Raised when a value cannot be represented by a canonical contract."""


class RoundingMode(Enum):
    TOWARD_ZERO = "toward_zero"
    TOWARD_NEGATIVE = "toward_negative"
    TOWARD_POSITIVE = "toward_positive"
    NEAREST_TIES_TO_EVEN = "nearest_ties_to_even"


@dataclass(frozen=True)
class DecimalScale:
    exponent: int

    def __post_init__(self) -> None:
        if isinstance(self.exponent, bool) or not isinstance(self.exponent, int):
            raise TypeError("decimal scale exponent must be an integer")
        if not 0 <= self.exponent <= 18:
            raise ContractValueError("decimal scale exponent must be between 0 and 18")

    @property
    def denominator(self) -> int:
        return 10**self.exponent


@dataclass(frozen=True)
class _FixedPoint:
    units: int
    definition_ref: int

    def __post_init__(self) -> None:
        if isinstance(self.units, bool) or not isinstance(self.units, int):
            raise TypeError("fixed-point units must be an integer")
        if not MIN_AMOUNT_UNITS <= self.units <= MAX_AMOUNT_UNITS:
            raise ContractValueError("fixed-point units exceed signed 64-bit range")
        if isinstance(self.definition_ref, bool) or not isinstance(self.definition_ref, int):
            raise TypeError("unit definition reference must be an integer")
        if not 1 <= self.definition_ref <= 2**64 - 1:
            raise ContractValueError(
                "unit definition reference must be a nonzero unsigned 64-bit value"
            )

    def checked_add(self, other: object) -> _FixedPoint:
        self._require_same_type(other)
        return type(self)(self.units + other.units, self.definition_ref)

    def checked_subtract(self, other: object) -> _FixedPoint:
        self._require_same_type(other)
        return type(self)(self.units - other.units, self.definition_ref)

    def _require_same_type(self, other: object) -> None:
        if type(other) is not type(self):
            raise TypeError("fixed-point arithmetic requires matching amount types")
        if other.definition_ref != self.definition_ref:
            raise ContractValueError("fixed-point arithmetic requires matching unit definitions")


@dataclass(frozen=True)
class Price(_FixedPoint):
    """Signed 64-bit count of listing-defined price ticks."""


@dataclass(frozen=True)
class Quantity(_FixedPoint):
    """Signed 64-bit count of listing-defined quantity steps."""


@dataclass(frozen=True)
class Money(_FixedPoint):
    """Signed 64-bit count of currency-defined minor units."""


def checked_multiply_divide(
    value: int, multiplier: int, divisor: int, rounding: RoundingMode
) -> int:
    for name, operand in (("value", value), ("multiplier", multiplier), ("divisor", divisor)):
        if isinstance(operand, bool) or not isinstance(operand, int):
            raise TypeError(f"{name} must be an integer")
        if not MIN_AMOUNT_UNITS <= operand <= MAX_AMOUNT_UNITS:
            raise ContractValueError(f"{name} exceeds signed 64-bit range")
    if divisor <= 0:
        raise ContractValueError("divisor must be positive")
    if not isinstance(rounding, RoundingMode):
        raise TypeError("rounding must be a RoundingMode")

    product = value * multiplier
    magnitude_quotient, remainder = divmod(abs(product), divisor)
    quotient = -magnitude_quotient if product < 0 else magnitude_quotient

    if remainder:
        if rounding is RoundingMode.TOWARD_NEGATIVE and product < 0:
            quotient -= 1
        elif rounding is RoundingMode.TOWARD_POSITIVE and product >= 0:
            quotient += 1
        elif rounding is RoundingMode.NEAREST_TIES_TO_EVEN:
            if remainder * 2 > divisor or (remainder * 2 == divisor and quotient % 2 != 0):
                quotient += -1 if product < 0 else 1

    if not MIN_AMOUNT_UNITS <= quotient <= MAX_AMOUNT_UNITS:
        raise ContractValueError("fixed-point result exceeds signed 64-bit range")
    return quotient
