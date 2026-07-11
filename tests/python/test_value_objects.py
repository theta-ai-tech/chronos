import pytest
from chronos.value_objects import (
    CanonicalInstrumentId,
    ClockClass,
    ClockDomainId,
    ContractValueError,
    DataQuality,
    DefinitionId,
    EventId,
    ListingId,
    QualityStatus,
    StreamCursor,
    StreamId,
    TimePoint,
    VersionRef,
)

IDENTITY = "018f1f6e-7d3a-7c4b-8a91-0123456789ab"


def test_opaque_ids_have_no_business_meaning_and_remain_strongly_typed() -> None:
    assert str(EventId.parse(IDENTITY)) == IDENTITY
    assert EventId.parse(IDENTITY) != ListingId.parse(IDENTITY)
    assert CanonicalInstrumentId.parse(IDENTITY) != ListingId.parse(IDENTITY)
    with pytest.raises(ContractValueError):
        EventId.parse("BTCUSDT")
    with pytest.raises(ContractValueError):
        EventId.parse("00000000-0000-0000-0000-000000000000")
    with pytest.raises(ContractValueError):
        EventId.parse("-18f1f6e-7d3a-7c4b-8a91-0123456789ab")
    with pytest.raises(ContractValueError):
        EventId.parse("018f1f6e-7d3a-7c4b-8a91-0123456789a-")


def test_stream_cursor_distinguishes_origin_from_sequence_zero() -> None:
    stream_id = StreamId.parse(IDENTITY)
    origin = StreamCursor.at_origin(stream_id, 1)
    first = StreamCursor.at_sequence(stream_id, 1, 0)
    assert origin.is_origin
    assert not first.is_origin
    assert origin != first
    with pytest.raises(ContractValueError):
        StreamCursor.at_origin(stream_id, 0)


def test_version_ref_requires_positive_version() -> None:
    definition_id = DefinitionId.parse(IDENTITY)
    assert VersionRef(definition_id, 3).version == 3
    with pytest.raises(ContractValueError):
        VersionRef(definition_id, 0)


def test_time_points_compare_only_inside_one_clock_domain() -> None:
    domain = ClockDomainId.parse(IDENTITY)
    other_domain = ClockDomainId.parse("018f1f6e-7d3a-7c4b-8a91-0123456789ac")
    earlier = TimePoint(10, domain, ClockClass.MONOTONIC, 1)
    later = TimePoint(20, domain, ClockClass.MONOTONIC, 1)
    restarted = TimePoint(20, other_domain, ClockClass.MONOTONIC, 1)
    assert earlier.checked_compare(later) == -1
    with pytest.raises(ContractValueError):
        earlier.checked_compare(restarted)
    with pytest.raises(ContractValueError):
        TimePoint(10, domain, ClockClass.CHRONOS_WALL, 0)


def test_data_quality_requires_reasons_for_non_valid_states() -> None:
    assert DataQuality(QualityStatus.VALID, 0).status is QualityStatus.VALID
    assert DataQuality(QualityStatus.STALE, 7).reason_code == 7
    with pytest.raises(ContractValueError):
        DataQuality(QualityStatus.VALID, 7)
    with pytest.raises(ContractValueError):
        DataQuality(QualityStatus.GAPPED, 0)
