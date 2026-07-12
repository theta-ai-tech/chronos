from dataclasses import replace

import pytest
from chronos.event_envelope import (
    RESERVED_EVENT_NAMESPACES,
    AcceptanceClass,
    EventEnvelope,
    EventTypeRegistration,
    ProducerRef,
    event_namespace,
    is_valid_event_type,
)
from chronos.value_objects import (
    AuthorityId,
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
    RuntimeId,
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


def registration() -> EventTypeRegistration:
    return EventTypeRegistration(
        event_type="source.capture.payload_captured",
        semantic_owner=identity(AuthorityId, "a6"),
        root_observation=True,
        run_scoped=False,
        ordered=False,
        run_input_eligible=False,
        requires_source_event=False,
        requires_subjects=False,
        mode_sensitive=False,
        requires_effective_position=False,
        requires_integrity=False,
        requires_receive_time=False,
    )


def valid_envelope() -> EventEnvelope:
    return EventEnvelope(
        registration=registration(),
        event_id=identity(EventId, "a1"),
        event_type="source.capture.payload_captured",
        envelope_version=1,
        schema_version=version("a3", 2),
        semantic_owner=identity(AuthorityId, "a6"),
        producer=ProducerRef(
            identity(ProducerId, "a4"),
            version("a5", 3),
            identity(RuntimeId, "a7"),
        ),
        acceptance_class=AcceptanceClass.ACCEPTED_OBSERVATION,
        run_id=None,
        mode=None,
        event_position=None,
        run_input_sequence=None,
        effective_position=None,
        state_lineage=None,
        source_event_id=None,
        causation_refs=None,
        correlation_refs=None,
        subject_refs=None,
        source_event_time=None,
        chronos_receive_time=None,
        accept_time=time_point(10),
        recoverability_handoff_time=None,
        record_time=None,
        quality=DataQuality(QualityStatus.VALID, 0),
        payload=b"\x01\x02",
        integrity=None,
    )


def updated(envelope: EventEnvelope, **changes) -> EventEnvelope:
    return replace(envelope, registration=registration(), **changes)


def test_reserved_taxonomy_accepts_only_canonical_names() -> None:
    assert len(RESERVED_EVENT_NAMESPACES) == 22
    assert event_namespace("market.book.snapshot_applied") == "market.book"
    assert event_namespace("execution.fill.accepted") == "execution.fill"
    assert is_valid_event_type("source.capture.payload_captured")
    assert is_valid_event_type("run.input.event_selected")
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
        updated(valid_envelope(), event_type="unknown.fact")
    with pytest.raises(ContractValueError):
        updated(valid_envelope(), envelope_version=0)
    with pytest.raises(ContractValueError):
        updated(valid_envelope(), causation_refs=())


def test_envelope_rejects_self_or_duplicate_causation() -> None:
    envelope = valid_envelope()
    with pytest.raises(ContractValueError):
        updated(envelope, causation_refs=(envelope.event_id,))
    cause = identity(EventId, "af")
    with pytest.raises(ContractValueError):
        updated(envelope, causation_refs=(cause, cause))


def test_causation_preserves_identity_kind() -> None:
    envelope = updated(
        valid_envelope(),
        causation_refs=(
            identity(CommandId, "ab"),
            identity(StateViewId, "ac"),
            identity(DecisionId, "ad"),
        ),
    )
    assert len(envelope.causation_refs or ()) == 3


def test_run_input_position_cannot_be_unscoped_or_unordered() -> None:
    with pytest.raises(ContractValueError):
        updated(valid_envelope(), run_input_sequence=3)


def test_registry_enforces_owner_and_derived_causation() -> None:
    with pytest.raises(ContractValueError):
        updated(valid_envelope(), semantic_owner=identity(AuthorityId, "af"))
    derived = replace(registration(), root_observation=False)
    with pytest.raises(ContractValueError):
        replace(valid_envelope(), registration=derived)
