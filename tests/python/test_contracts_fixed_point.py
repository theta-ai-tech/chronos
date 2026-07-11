import pytest
from chronos.contracts import (
    MAX_AMOUNT_UNITS,
    MIN_AMOUNT_UNITS,
    ContractValueError,
    DecimalScale,
    Money,
    Price,
    Quantity,
    RoundingMode,
    checked_multiply_divide,
)


def test_amounts_are_exact_type_scoped_signed_64_bit_values() -> None:
    assert Price(42) == Price(42)
    assert Price(42) != Price(43)
    assert Price(42) != Quantity(42)
    assert Money(MIN_AMOUNT_UNITS).units == MIN_AMOUNT_UNITS
    assert Money(MAX_AMOUNT_UNITS).units == MAX_AMOUNT_UNITS

    with pytest.raises((TypeError, ContractValueError)):
        Price(1.0)  # type: ignore[arg-type]
    with pytest.raises(ContractValueError):
        Quantity(MAX_AMOUNT_UNITS + 1)


def test_scale_is_explicit_and_bounded() -> None:
    assert DecimalScale(8).denominator == 100_000_000
    assert DecimalScale(18).denominator == 1_000_000_000_000_000_000
    with pytest.raises(ContractValueError):
        DecimalScale(19)


def test_checked_arithmetic_rejects_overflow_and_cross_type_operations() -> None:
    assert Price(7).checked_add(Price(5)) == Price(12)
    assert Money(7).checked_subtract(Money(5)) == Money(2)
    with pytest.raises(ContractValueError):
        Price(MAX_AMOUNT_UNITS).checked_add(Price(1))
    with pytest.raises(TypeError):
        Price(1).checked_add(Quantity(1))  # type: ignore[arg-type]


@pytest.mark.parametrize(
    ("value", "rounding", "expected"),
    [
        (5, RoundingMode.TOWARD_ZERO, 2),
        (-5, RoundingMode.TOWARD_ZERO, -2),
        (5, RoundingMode.TOWARD_NEGATIVE, 2),
        (-5, RoundingMode.TOWARD_NEGATIVE, -3),
        (5, RoundingMode.TOWARD_POSITIVE, 3),
        (-5, RoundingMode.TOWARD_POSITIVE, -2),
        (5, RoundingMode.NEAREST_TIES_TO_EVEN, 2),
        (7, RoundingMode.NEAREST_TIES_TO_EVEN, 4),
        (-5, RoundingMode.NEAREST_TIES_TO_EVEN, -2),
        (-7, RoundingMode.NEAREST_TIES_TO_EVEN, -4),
    ],
)
def test_multiply_divide_uses_declared_rounding(
    value: int, rounding: RoundingMode, expected: int
) -> None:
    assert checked_multiply_divide(value, 1, 2, rounding) == expected


def test_multiply_divide_rejects_invalid_or_out_of_range_results() -> None:
    assert (
        checked_multiply_divide(
            MAX_AMOUNT_UNITS, MAX_AMOUNT_UNITS, MAX_AMOUNT_UNITS, RoundingMode.TOWARD_ZERO
        )
        == MAX_AMOUNT_UNITS
    )
    with pytest.raises(ContractValueError):
        checked_multiply_divide(MAX_AMOUNT_UNITS, 2, 1, RoundingMode.TOWARD_ZERO)
    with pytest.raises(ContractValueError):
        checked_multiply_divide(1, 1, 0, RoundingMode.TOWARD_ZERO)
