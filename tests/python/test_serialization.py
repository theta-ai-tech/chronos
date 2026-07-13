from pathlib import Path

import pytest
from chronos.boundary import BoundaryCallError, ChronosBoundary
from chronos.contracts import DecimalScale, Money, Price, Quantity
from chronos.event_envelope import (
    AcceptanceClass,
    Applicability,
    EffectivePositionPolicy,
    EventEnvelope,
    EventPosition,
    EventTypeRegistration,
    ProducerRef,
    RunMode,
)
from chronos.serialization import (
    ConformanceFrame,
    decode_conformance_frame,
    encode_conformance_frame,
)
from chronos.state_lineage import StateLineage
from chronos.value_objects import (
    AuthorityId,
    CanonicalInstrumentId,
    ClockClass,
    ClockDomainId,
    CommandId,
    ContractValueError,
    CorrelationId,
    DataQuality,
    DecisionId,
    DefinitionId,
    EventId,
    IntegrityId,
    ListingId,
    ProducerId,
    QualityStatus,
    RunId,
    RuntimeId,
    SourceEventId,
    StateViewId,
    StreamCursor,
    StreamId,
    TimePoint,
    VersionRef,
)


def identity(identity_type: type, suffix: str):
    return identity_type.parse(f"018f1f6e-7d3a-7c4b-8a91-0123456789{suffix}")


def version(suffix: str, number: int) -> VersionRef:
    return VersionRef(identity(DefinitionId, suffix), number)


def make_frame() -> ConformanceFrame:
    run_id = identity(RunId, "01")
    stream_one = identity(StreamId, "02")
    stream_two = identity(StreamId, "03")
    cursors = (
        StreamCursor.at_origin(stream_one, 1),
        StreamCursor.at_sequence(stream_two, 2, 17),
    )
    lineage = StateLineage.from_cursors(run_id, 9, (stream_one, stream_two), cursors)
    schema = version("04", 3)
    owner = identity(AuthorityId, "05")
    producer_id = identity(ProducerId, "06")
    registration = EventTypeRegistration(
        event_type="market.book.depth.snapshot_applied",
        semantic_owner=owner,
        envelope_version=1,
        schema_version=schema,
        authorized_producers=(producer_id,),
        allowed_acceptance_classes=(AcceptanceClass.ACCEPTED_TRANSITION,),
        permitted_modes=(RunMode.REPLAY,),
        root_observation=False,
        run_scope=Applicability.REQUIRED,
        event_position=Applicability.REQUIRED,
        run_input_eligible=True,
        source_event=Applicability.REQUIRED,
        subjects=Applicability.REQUIRED,
        mode=Applicability.REQUIRED,
        effective_position=EffectivePositionPolicy.OPTIONAL,
        integrity=Applicability.REQUIRED,
        receive_time=Applicability.REQUIRED,
    )
    source_clock = identity(ClockDomainId, "07")
    chronos_clock = identity(ClockDomainId, "08")
    envelope = EventEnvelope(
        registration=registration,
        event_id=identity(EventId, "09"),
        event_type=registration.event_type,
        envelope_version=1,
        schema_version=schema,
        semantic_owner=owner,
        producer=ProducerRef(producer_id, version("0a", 5), identity(RuntimeId, "0b")),
        acceptance_class=AcceptanceClass.ACCEPTED_TRANSITION,
        run_id=run_id,
        mode=RunMode.REPLAY,
        event_position=EventPosition(stream_one, 1, 22),
        run_input_sequence=9,
        effective_position=10,
        state_lineage=lineage,
        source_event_id=identity(SourceEventId, "0c"),
        causation_refs=(
            identity(CommandId, "0d"),
            identity(EventId, "0e"),
            identity(StateViewId, "0f"),
            identity(DecisionId, "10"),
        ),
        correlation_refs=(identity(CorrelationId, "11"),),
        subject_refs=(
            identity(CanonicalInstrumentId, "12"),
            identity(ListingId, "13"),
        ),
        source_event_time=TimePoint(1_700_000_000, source_clock, ClockClass.SOURCE_WALL, 1_000),
        chronos_receive_time=TimePoint(1_700_000_100, chronos_clock, ClockClass.CHRONOS_WALL, 1),
        accept_time=TimePoint(1_700_000_200, chronos_clock, ClockClass.CHRONOS_WALL, 1),
        recoverability_handoff_time=TimePoint(
            1_700_000_300, chronos_clock, ClockClass.CHRONOS_WALL, 1
        ),
        record_time=TimePoint(1_700_000_400, chronos_clock, ClockClass.CHRONOS_WALL, 1),
        quality=DataQuality(QualityStatus.STALE, 7),
        payload=b"book\x00snapshot",
        integrity=identity(IntegrityId, "14"),
    )
    return ConformanceFrame(
        DecimalScale(8),
        Price(12_345, version("15", 100)),
        Quantity(67, version("16", 101)),
        Money(-890, version("17", 102)),
        registration,
        envelope,
    )


def fixture_bytes() -> bytes:
    fixture = (
        Path(__file__).resolve().parents[2] / "contracts/conformance/fixtures/m1-full-frame.hex"
    )
    return bytes.fromhex(fixture.read_text(encoding="ascii"))


def test_python_codec_matches_shared_fixture_semantics() -> None:
    frame = make_frame()
    encoded = encode_conformance_frame(frame)
    assert encoded == fixture_bytes()
    assert decode_conformance_frame(encoded) == frame


def test_python_to_cpp_to_python_round_trip_preserves_exact_frame() -> None:
    encoded = encode_conformance_frame(make_frame())
    native = ChronosBoundary()
    returned = native.round_trip_contract_frame(encoded)
    assert returned == encoded
    assert decode_conformance_frame(returned) == make_frame()


@pytest.mark.parametrize("mutation", [lambda value: value[:-1], lambda value: value + b"\x00"])
def test_both_boundaries_reject_noncanonical_frames(mutation) -> None:
    invalid = mutation(fixture_bytes())
    with pytest.raises(ContractValueError):
        decode_conformance_frame(invalid)
    with pytest.raises(BoundaryCallError):
        ChronosBoundary().round_trip_contract_frame(invalid)


@pytest.mark.parametrize(
    "mutation",
    [
        lambda value: value[:184] + b"\xff\xff\xff\xff" + value[188:],
        lambda value: value[:474] + b"\xff\xff\xff\xff" + value[478:],
        lambda value: value[:106] + b"\xff" + value[107:],
    ],
)
def test_both_decoders_reject_hostile_counts_and_invalid_utf8(mutation) -> None:
    invalid = mutation(fixture_bytes())
    with pytest.raises(ContractValueError):
        decode_conformance_frame(invalid)
    with pytest.raises(BoundaryCallError):
        ChronosBoundary().round_trip_contract_frame(invalid)
