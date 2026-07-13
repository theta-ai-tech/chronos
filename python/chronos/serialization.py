"""Canonical M1 binary serialization shared with the native contracts library."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Callable, Optional, TypeVar
from uuid import UUID

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
    OpaqueId,
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

MAGIC = b"CHR1\x02"
MAX_CONFORMANCE_FRAME_BYTES = 64 * 1024
T = TypeVar("T")


@dataclass(frozen=True)
class ConformanceFrame:
    decimal_scale: DecimalScale
    price: Price
    quantity: Quantity
    money: Money
    registration: EventTypeRegistration
    envelope: EventEnvelope


class _Writer:
    def __init__(self) -> None:
        self.data = bytearray()

    def u8(self, value: int) -> None:
        self.data.extend(struct.pack("<B", value))

    def u32(self, value: int) -> None:
        self.data.extend(struct.pack("<I", value))

    def u64(self, value: int) -> None:
        self.data.extend(struct.pack("<Q", value))

    def i64(self, value: int) -> None:
        self.data.extend(struct.pack("<q", value))

    def raw(self, value: bytes) -> None:
        self.data.extend(value)

    def blob(self, value: bytes) -> None:
        self.u32(len(value))
        self.raw(value)

    def text(self, value: str) -> None:
        self.blob(value.encode("utf-8"))

    def identity(self, value: OpaqueId) -> None:
        self.raw(value.value.bytes)

    def optional(self, value: object, encode: Callable[[object], None]) -> None:
        self.u8(0 if value is None else 1)
        if value is not None:
            encode(value)

    def vector(self, values: tuple | list, encode: Callable[[object], None]) -> None:
        self.u32(len(values))
        for value in values:
            encode(value)


class _Reader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.position = 0

    def _read(self, size: int) -> bytes:
        end = self.position + size
        if end > len(self.data):
            raise ContractValueError("truncated conformance frame")
        value = self.data[self.position : end]
        self.position = end
        return value

    def u8(self) -> int:
        return struct.unpack("<B", self._read(1))[0]

    def boolean(self) -> bool:
        value = self.u8()
        if value not in (0, 1):
            raise ContractValueError("noncanonical boolean")
        return value == 1

    def u32(self) -> int:
        return struct.unpack("<I", self._read(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self._read(8))[0]

    def i64(self) -> int:
        return struct.unpack("<q", self._read(8))[0]

    def blob(self) -> bytes:
        return self._read(self.u32())

    def text(self) -> str:
        return self.blob().decode("utf-8")

    def identity(self, identity_type: type[T]) -> T:
        return identity_type(UUID(bytes=self._read(16)))

    def optional(self, decode: Callable[[], T]) -> Optional[T]:  # noqa: UP045
        return decode() if self.boolean() else None

    def vector(self, decode: Callable[[], T]) -> tuple[T, ...]:
        count = self.u32()
        if count > len(self.data) - self.position:
            raise ContractValueError("impossible conformance vector count")
        return tuple(decode() for _ in range(count))

    def finish(self) -> None:
        if self.position != len(self.data):
            raise ContractValueError("trailing bytes in conformance frame")


def _enum_value(enum_type: type[T], index: int) -> T:
    try:
        return tuple(enum_type)[index]
    except IndexError as error:
        raise ContractValueError(f"invalid {enum_type.__name__} value") from error


def _write_version(writer: _Writer, value: VersionRef) -> None:
    writer.identity(value.definition_id)
    writer.u64(value.version)


def _read_version(reader: _Reader) -> VersionRef:
    return VersionRef(reader.identity(DefinitionId), reader.u64())


def _write_time(writer: _Writer, value: TimePoint) -> None:
    writer.i64(value.nanoseconds)
    writer.identity(value.clock_domain_id)
    writer.u8(tuple(ClockClass).index(value.clock_class))
    writer.u32(value.precision_nanoseconds)


def _read_time(reader: _Reader) -> TimePoint:
    return TimePoint(
        reader.i64(),
        reader.identity(ClockDomainId),
        _enum_value(ClockClass, reader.u8()),
        reader.u32(),
    )


def _write_cursor(writer: _Writer, value: StreamCursor) -> None:
    writer.identity(value.stream_id)
    writer.u64(value.stream_epoch)
    writer.optional(value.last_consumed_sequence, lambda item: writer.u64(item))


def _read_cursor(reader: _Reader) -> StreamCursor:
    stream_id = reader.identity(StreamId)
    epoch = reader.u64()
    sequence = reader.optional(reader.u64)
    return (
        StreamCursor.at_origin(stream_id, epoch)
        if sequence is None
        else StreamCursor.at_sequence(stream_id, epoch, sequence)
    )


def _write_lineage(writer: _Writer, value: StateLineage) -> None:
    writer.identity(value.run_id)
    writer.u64(value.run_input_sequence)
    writer.vector(value.cursors, lambda item: _write_cursor(writer, item))


def _read_lineage(reader: _Reader) -> StateLineage:
    run_id = reader.identity(RunId)
    run_input_sequence = reader.u64()
    cursors = reader.vector(lambda: _read_cursor(reader))
    return StateLineage.from_cursors(
        run_id, run_input_sequence, (cursor.stream_id for cursor in cursors), cursors
    )


def _write_registration(writer: _Writer, value: EventTypeRegistration) -> None:
    writer.text(value.event_type)
    writer.identity(value.semantic_owner)
    writer.u32(value.envelope_version)
    _write_version(writer, value.schema_version)
    writer.vector(value.authorized_producers, writer.identity)
    writer.vector(
        value.allowed_acceptance_classes,
        lambda item: writer.u8(tuple(AcceptanceClass).index(item)),
    )
    writer.vector(value.permitted_modes, lambda item: writer.u8(tuple(RunMode).index(item)))
    writer.u8(1 if value.root_observation else 0)
    writer.u8(tuple(Applicability).index(value.run_scope))
    writer.u8(tuple(Applicability).index(value.event_position))
    writer.u8(1 if value.run_input_eligible else 0)
    writer.u8(tuple(Applicability).index(value.source_event))
    writer.u8(tuple(Applicability).index(value.subjects))
    writer.u8(tuple(Applicability).index(value.mode))
    writer.u8(tuple(EffectivePositionPolicy).index(value.effective_position))
    writer.u8(tuple(Applicability).index(value.integrity))
    writer.u8(tuple(Applicability).index(value.receive_time))


def _read_registration(reader: _Reader) -> EventTypeRegistration:
    return EventTypeRegistration(
        event_type=reader.text(),
        semantic_owner=reader.identity(AuthorityId),
        envelope_version=reader.u32(),
        schema_version=_read_version(reader),
        authorized_producers=reader.vector(lambda: reader.identity(ProducerId)),
        allowed_acceptance_classes=reader.vector(lambda: _enum_value(AcceptanceClass, reader.u8())),
        permitted_modes=reader.vector(lambda: _enum_value(RunMode, reader.u8())),
        root_observation=reader.boolean(),
        run_scope=_enum_value(Applicability, reader.u8()),
        event_position=_enum_value(Applicability, reader.u8()),
        run_input_eligible=reader.boolean(),
        source_event=_enum_value(Applicability, reader.u8()),
        subjects=_enum_value(Applicability, reader.u8()),
        mode=_enum_value(Applicability, reader.u8()),
        effective_position=_enum_value(EffectivePositionPolicy, reader.u8()),
        integrity=_enum_value(Applicability, reader.u8()),
        receive_time=_enum_value(Applicability, reader.u8()),
    )


def _write_position(writer: _Writer, value: EventPosition) -> None:
    writer.identity(value.stream_id)
    writer.u64(value.stream_epoch)
    writer.u64(value.stream_sequence)


def _read_position(reader: _Reader) -> EventPosition:
    return EventPosition(reader.identity(StreamId), reader.u64(), reader.u64())


CAUSE_TYPES = (CommandId, EventId, StateViewId, DecisionId)
SUBJECT_TYPES = (CanonicalInstrumentId, ListingId)


def _write_variant(writer: _Writer, value: object, types: tuple[type, ...]) -> None:
    for index, item_type in enumerate(types):
        if type(value) is item_type:
            writer.u8(index)
            writer.identity(value)
            return
    raise TypeError("unsupported conformance variant")


def _read_variant(reader: _Reader, types: tuple[type, ...]):
    index = reader.u8()
    if index >= len(types):
        raise ContractValueError("invalid conformance variant tag")
    return reader.identity(types[index])


def _write_envelope(writer: _Writer, value: EventEnvelope) -> None:
    writer.identity(value.event_id)
    writer.text(value.event_type)
    writer.u32(value.envelope_version)
    _write_version(writer, value.schema_version)
    writer.identity(value.semantic_owner)
    writer.identity(value.producer.component_id)
    _write_version(writer, value.producer.implementation_version)
    writer.identity(value.producer.runtime_incarnation_id)
    writer.u8(tuple(AcceptanceClass).index(value.acceptance_class))
    writer.optional(value.run_id, writer.identity)
    writer.optional(value.mode, lambda item: writer.u8(tuple(RunMode).index(item)))
    writer.optional(value.event_position, lambda item: _write_position(writer, item))
    writer.optional(value.run_input_sequence, writer.u64)
    writer.optional(value.effective_position, writer.u64)
    writer.optional(value.state_lineage, lambda item: _write_lineage(writer, item))
    writer.optional(value.source_event_id, writer.identity)
    writer.optional(
        value.causation_refs,
        lambda items: writer.vector(items, lambda item: _write_variant(writer, item, CAUSE_TYPES)),
    )
    writer.optional(
        value.correlation_refs,
        lambda items: writer.vector(items, writer.identity),
    )
    writer.optional(
        value.subject_refs,
        lambda items: writer.vector(
            items, lambda item: _write_variant(writer, item, SUBJECT_TYPES)
        ),
    )
    writer.optional(value.source_event_time, lambda item: _write_time(writer, item))
    writer.optional(value.chronos_receive_time, lambda item: _write_time(writer, item))
    _write_time(writer, value.accept_time)
    writer.optional(value.recoverability_handoff_time, lambda item: _write_time(writer, item))
    writer.optional(value.record_time, lambda item: _write_time(writer, item))
    writer.u8(tuple(QualityStatus).index(value.quality.status))
    writer.u32(value.quality.reason_code)
    writer.blob(value.payload)
    writer.optional(value.integrity, writer.identity)


def _read_envelope(reader: _Reader, registration: EventTypeRegistration) -> EventEnvelope:
    event_id = reader.identity(EventId)
    event_type = reader.text()
    envelope_version = reader.u32()
    schema_version = _read_version(reader)
    semantic_owner = reader.identity(AuthorityId)
    producer = ProducerRef(
        reader.identity(ProducerId), _read_version(reader), reader.identity(RuntimeId)
    )
    acceptance_class = _enum_value(AcceptanceClass, reader.u8())
    run_id = reader.optional(lambda: reader.identity(RunId))
    mode = reader.optional(lambda: _enum_value(RunMode, reader.u8()))
    event_position = reader.optional(lambda: _read_position(reader))
    run_input_sequence = reader.optional(reader.u64)
    effective_position = reader.optional(reader.u64)
    state_lineage = reader.optional(lambda: _read_lineage(reader))
    source_event_id = reader.optional(lambda: reader.identity(SourceEventId))
    causation_refs = reader.optional(
        lambda: reader.vector(lambda: _read_variant(reader, CAUSE_TYPES))
    )
    correlation_refs = reader.optional(
        lambda: reader.vector(lambda: reader.identity(CorrelationId))
    )
    subject_refs = reader.optional(
        lambda: reader.vector(lambda: _read_variant(reader, SUBJECT_TYPES))
    )
    source_event_time = reader.optional(lambda: _read_time(reader))
    chronos_receive_time = reader.optional(lambda: _read_time(reader))
    accept_time = _read_time(reader)
    handoff_time = reader.optional(lambda: _read_time(reader))
    record_time = reader.optional(lambda: _read_time(reader))
    quality = DataQuality(_enum_value(QualityStatus, reader.u8()), reader.u32())
    payload = reader.blob()
    integrity = reader.optional(lambda: reader.identity(IntegrityId))
    return EventEnvelope(
        registration=registration,
        event_id=event_id,
        event_type=event_type,
        envelope_version=envelope_version,
        schema_version=schema_version,
        semantic_owner=semantic_owner,
        producer=producer,
        acceptance_class=acceptance_class,
        run_id=run_id,
        mode=mode,
        event_position=event_position,
        run_input_sequence=run_input_sequence,
        effective_position=effective_position,
        state_lineage=state_lineage,
        source_event_id=source_event_id,
        causation_refs=causation_refs,
        correlation_refs=correlation_refs,
        subject_refs=subject_refs,
        source_event_time=source_event_time,
        chronos_receive_time=chronos_receive_time,
        accept_time=accept_time,
        recoverability_handoff_time=handoff_time,
        record_time=record_time,
        quality=quality,
        payload=payload,
        integrity=integrity,
    )


def encode_conformance_frame(frame: ConformanceFrame) -> bytes:
    writer = _Writer()
    writer.raw(MAGIC)
    writer.u8(frame.decimal_scale.exponent)
    for amount in (frame.price, frame.quantity, frame.money):
        writer.i64(amount.units)
        _write_version(writer, amount.definition_ref)
    _write_registration(writer, frame.registration)
    _write_envelope(writer, frame.envelope)
    if len(writer.data) > MAX_CONFORMANCE_FRAME_BYTES:
        raise ContractValueError("conformance frame exceeds boundary limit")
    return bytes(writer.data)


def _decode_conformance_frame(data: bytes) -> ConformanceFrame:
    if not isinstance(data, bytes):
        raise TypeError("conformance frame must be bytes")
    if len(data) > MAX_CONFORMANCE_FRAME_BYTES:
        raise ContractValueError("conformance frame exceeds boundary limit")
    reader = _Reader(data)
    if reader._read(len(MAGIC)) != MAGIC:
        raise ContractValueError("unsupported conformance frame version")
    scale = DecimalScale(reader.u8())
    price = Price(reader.i64(), _read_version(reader))
    quantity = Quantity(reader.i64(), _read_version(reader))
    money = Money(reader.i64(), _read_version(reader))
    registration = _read_registration(reader)
    envelope = _read_envelope(reader, registration)
    reader.finish()
    return ConformanceFrame(scale, price, quantity, money, registration, envelope)


def decode_conformance_frame(data: bytes) -> ConformanceFrame:
    if not isinstance(data, bytes):
        raise TypeError("conformance frame must be bytes")
    try:
        return _decode_conformance_frame(data)
    except ContractValueError:
        raise
    except (UnicodeError, ValueError, IndexError, struct.error) as error:
        raise ContractValueError("invalid conformance frame") from error
