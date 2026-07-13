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
from chronos.value_objects import DefinitionId, VersionRef


def definition(suffix: str, version: int) -> VersionRef:
    return VersionRef(DefinitionId.parse(f"018f1f6e-7d3a-7c4b-8a91-0123456789{suffix}"), version)


PRICE_DEFINITION = definition("01", 7)
OTHER_DEFINITION = definition("02", 7)
MONEY_DEFINITION = definition("03", 9)


def test_amounts_are_exact_type_scoped_signed_64_bit_values() -> None:
    assert Price(42, PRICE_DEFINITION) == Price(42, PRICE_DEFINITION)
    assert Price(42, PRICE_DEFINITION) != Price(43, PRICE_DEFINITION)
    assert Price(42, PRICE_DEFINITION) != Price(42, OTHER_DEFINITION)
    assert Price(42, PRICE_DEFINITION) != Quantity(42, PRICE_DEFINITION)
    assert Money(MIN_AMOUNT_UNITS, MONEY_DEFINITION).units == MIN_AMOUNT_UNITS
    assert Money(MAX_AMOUNT_UNITS, MONEY_DEFINITION).units == MAX_AMOUNT_UNITS

    with pytest.raises((TypeError, ContractValueError)):
        Price(1.0, PRICE_DEFINITION)  # type: ignore[arg-type]
    with pytest.raises(ContractValueError):
        Quantity(MAX_AMOUNT_UNITS + 1, PRICE_DEFINITION)
    with pytest.raises(TypeError):
        Price(1, 7)  # type: ignore[arg-type]


def test_scale_is_explicit_and_bounded() -> None:
    assert DecimalScale(8).denominator == 100_000_000
    assert DecimalScale(18).denominator == 1_000_000_000_000_000_000
    with pytest.raises(ContractValueError):
        DecimalScale(19)


def test_checked_arithmetic_rejects_overflow_and_cross_type_operations() -> None:
    assert Price(7, PRICE_DEFINITION).checked_add(Price(5, PRICE_DEFINITION)) == Price(
        12, PRICE_DEFINITION
    )
    assert Money(7, MONEY_DEFINITION).checked_subtract(Money(5, MONEY_DEFINITION)) == Money(
        2, MONEY_DEFINITION
    )
    with pytest.raises(ContractValueError):
        Price(MAX_AMOUNT_UNITS, PRICE_DEFINITION).checked_add(Price(1, PRICE_DEFINITION))
    with pytest.raises(TypeError):
        Price(1, PRICE_DEFINITION).checked_add(
            Quantity(1, PRICE_DEFINITION)  # type: ignore[arg-type]
        )
    with pytest.raises(ContractValueError):
        Price(1, PRICE_DEFINITION).checked_add(Price(1, OTHER_DEFINITION))


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
    with pytest.raises(ContractValueError):
        checked_multiply_divide(MAX_AMOUNT_UNITS + 1, 0, 1, RoundingMode.TOWARD_ZERO)
