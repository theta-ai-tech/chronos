from dataclasses import replace

import pytest
from chronos.event_envelope import (
    RESERVED_EVENT_NAMESPACES,
    EventEnvelope,
    event_namespace,
    is_valid_event_type,
)
from chronos.value_objects import (
    ClockClass,
    ClockDomainId,
    CommandId,
    ContractValueError,
    DataQuality,
    DecisionId,
    DefinitionId,
    EventId,
    ProducerId,
    QualityStatus,
    StateViewId,
    TimePoint,
    VersionRef,
)


def identity(identity_type: type, suffix: str):
    return identity_type.parse(f"018f1f6e-7d3a-7c4b-8a91-0123456789{suffix}")


def version(suffix: str, number: int) -> VersionRef:
    return VersionRef(identity(DefinitionId, suffix), number)


def time_point(value: int) -> TimePoint:
    return TimePoint(value, identity(ClockDomainId, "a2"), ClockClass.CHRONOS_WALL, 1)


def valid_envelope() -> EventEnvelope:
    return EventEnvelope(
        event_id=identity(EventId, "a1"),
        event_type="market.book.snapshot_applied",
        envelope_version=1,
        schema_version=version("a3", 2),
        producer_id=identity(ProducerId, "a4"),
        producer_version=version("a5", 3),
        run_id=None,
        stream_cursor=None,
        run_input_sequence=None,
        state_lineage=None,
        source_event_id=None,
        causation_refs=None,
        subject_refs=None,
        source_event_time=None,
        chronos_receive_time=None,
        accept_time=time_point(10),
        record_time=None,
        quality=DataQuality(QualityStatus.VALID, 0),
        payload=b"\x01\x02",
    )


def test_reserved_taxonomy_accepts_only_canonical_names() -> None:
    assert len(RESERVED_EVENT_NAMESPACES) == 20
    assert event_namespace("market.book.snapshot_applied") == "market.book"
    assert event_namespace("execution.fill.accepted") == "execution.fill"
    assert is_valid_event_type("source.payload_captured")
    assert not is_valid_event_type("unknown.payload_captured")
    assert not is_valid_event_type("market.book..snapshot")
    assert not is_valid_event_type("market.book.Snapshot")


def test_minimum_envelope_preserves_genuinely_absent_fields() -> None:
    envelope = valid_envelope()
    assert envelope.run_id is None
    assert envelope.state_lineage is None
    assert envelope.source_event_time is None
    assert envelope.record_time is None
    assert envelope.causation_refs is None
    assert envelope.payload == b"\x01\x02"


def test_envelope_rejects_invalid_taxonomy_versions_and_fake_empty_refs() -> None:
    with pytest.raises(ContractValueError):
        replace(valid_envelope(), event_type="unknown.fact")
    with pytest.raises(ContractValueError):
        replace(valid_envelope(), envelope_version=0)
    with pytest.raises(ContractValueError):
        replace(valid_envelope(), causation_refs=())


def test_envelope_rejects_self_or_duplicate_causation() -> None:
    envelope = valid_envelope()
    with pytest.raises(ContractValueError):
        replace(envelope, causation_refs=(envelope.event_id,))
    cause = identity(EventId, "af")
    with pytest.raises(ContractValueError):
        replace(envelope, causation_refs=(cause, cause))


def test_causation_preserves_identity_kind_and_input_position_can_stand_alone() -> None:
    envelope = replace(
        valid_envelope(),
        causation_refs=(
            identity(CommandId, "ab"),
            identity(StateViewId, "ac"),
            identity(DecisionId, "ad"),
        ),
        run_input_sequence=3,
    )
    assert envelope.run_input_sequence == 3
    assert envelope.state_lineage is None
