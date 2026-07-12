"""Validated logical event envelope and reserved event taxonomy."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Union

from chronos.state_lineage import StateLineage
from chronos.value_objects import (
    CanonicalInstrumentId,
    CommandId,
    ContractValueError,
    DataQuality,
    DecisionId,
    EventId,
    ListingId,
    ProducerId,
    RunId,
    SourceEventId,
    StateViewId,
    StreamCursor,
    TimePoint,
    VersionRef,
)

RESERVED_EVENT_NAMESPACES = (
    "source",
    "reference",
    "market.book",
    "market.trade",
    "market.control",
    "external",
    "run.control",
    "feature",
    "strategy",
    "recommendation",
    "opportunity",
    "portfolio",
    "risk",
    "execution.approval",
    "execution.intent",
    "execution.order",
    "execution.fill",
    "ledger",
    "reconciliation",
    "audit",
)

SubjectRef = Union[CanonicalInstrumentId, ListingId]  # noqa: UP007 - Python 3.9 syntax
CausationRef = Union[CommandId, EventId, StateViewId, DecisionId]  # noqa: UP007


def event_namespace(event_type: str) -> Optional[str]:  # noqa: UP045 - Python 3.9 syntax
    if not isinstance(event_type, str):
        raise TypeError("event type must be a string")
    for namespace in RESERVED_EVENT_NAMESPACES:
        if event_type.startswith(f"{namespace}."):
            return namespace
    return None


def is_valid_event_type(event_type: str) -> bool:
    namespace = event_namespace(event_type)
    return (
        namespace is not None
        and not event_type.endswith(".")
        and ".." not in event_type
        and all(
            character.isascii()
            and (character.islower() or character.isdigit() or character in "_.")
            for character in event_type
        )
    )


@dataclass(frozen=True)
class EventEnvelope:
    event_id: EventId
    event_type: str
    envelope_version: int
    schema_version: VersionRef
    producer_id: ProducerId
    producer_version: VersionRef
    run_id: Optional[RunId]  # noqa: UP045 - Python 3.9 syntax
    stream_cursor: Optional[StreamCursor]  # noqa: UP045 - Python 3.9 syntax
    run_input_sequence: Optional[int]  # noqa: UP045 - Python 3.9 syntax
    state_lineage: Optional[StateLineage]  # noqa: UP045 - Python 3.9 syntax
    source_event_id: Optional[SourceEventId]  # noqa: UP045 - Python 3.9 syntax
    causation_refs: Optional[tuple[CausationRef, ...]]  # noqa: UP045
    subject_refs: Optional[tuple[SubjectRef, ...]]  # noqa: UP045
    source_event_time: Optional[TimePoint]  # noqa: UP045 - Python 3.9 syntax
    chronos_receive_time: Optional[TimePoint]  # noqa: UP045 - Python 3.9 syntax
    accept_time: TimePoint
    record_time: Optional[TimePoint]  # noqa: UP045 - Python 3.9 syntax
    quality: DataQuality
    payload: bytes

    def __post_init__(self) -> None:
        if not isinstance(self.event_id, EventId):
            raise TypeError("event envelope requires an EventId")
        if not is_valid_event_type(self.event_type):
            raise ContractValueError("event type is outside the reserved taxonomy")
        _require_positive_uint32("envelope version", self.envelope_version)
        _require_type("schema version", self.schema_version, VersionRef)
        _require_type("producer ID", self.producer_id, ProducerId)
        _require_type("producer version", self.producer_version, VersionRef)
        _require_optional_type("run ID", self.run_id, RunId)
        _require_optional_type("stream cursor", self.stream_cursor, StreamCursor)
        _require_optional_uint64("run input sequence", self.run_input_sequence)
        _require_optional_type("state lineage", self.state_lineage, StateLineage)
        _require_optional_type("source event ID", self.source_event_id, SourceEventId)
        _require_optional_type("source event time", self.source_event_time, TimePoint)
        _require_optional_type("receive time", self.chronos_receive_time, TimePoint)
        _require_type("accept time", self.accept_time, TimePoint)
        _require_optional_type("record time", self.record_time, TimePoint)
        _require_type("quality", self.quality, DataQuality)
        if not isinstance(self.payload, bytes):
            raise TypeError("event payload must be bytes")
        self._validate_lineage()
        self._validate_references()

    def _validate_lineage(self) -> None:
        if self.state_lineage is not None and (
            self.run_id is None or self.state_lineage.run_id != self.run_id
        ):
            raise ContractValueError("state lineage must match the envelope run")
        if (
            self.run_input_sequence is not None
            and self.state_lineage is not None
            and self.state_lineage.run_input_sequence != self.run_input_sequence
        ):
            raise ContractValueError("run input sequence must match state lineage")

    def _validate_references(self) -> None:
        if self.causation_refs is not None:
            if not self.causation_refs:
                raise ContractValueError("present causation references must not be empty")
            if any(
                not isinstance(reference, (CommandId, EventId, StateViewId, DecisionId))
                for reference in self.causation_refs
            ):
                raise TypeError("causation references must use a canonical identity kind")
            if self.event_id in self.causation_refs or len(set(self.causation_refs)) != len(
                self.causation_refs
            ):
                raise ContractValueError("causation references must be unique and not self")
        if self.subject_refs is not None:
            if not self.subject_refs:
                raise ContractValueError("present subject references must not be empty")
            if any(
                not isinstance(reference, (CanonicalInstrumentId, ListingId))
                for reference in self.subject_refs
            ):
                raise TypeError("subject references must be typed canonical identities")
            if len(set(self.subject_refs)) != len(self.subject_refs):
                raise ContractValueError("subject references must be unique")


def _require_type(name: str, value: object, expected: type) -> None:
    if not isinstance(value, expected):
        raise TypeError(f"{name} has the wrong canonical type")


def _require_optional_type(name: str, value: object, expected: type) -> None:
    if value is not None:
        _require_type(name, value, expected)


def _require_positive_uint32(name: str, value: int) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    if not 1 <= value <= 2**32 - 1:
        raise ContractValueError(f"{name} exceeds positive unsigned 32-bit range")


def _require_optional_uint64(name: str, value: Optional[int]) -> None:  # noqa: UP045
    if value is None:
        return
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    if not 0 <= value <= 2**64 - 1:
        raise ContractValueError(f"{name} exceeds unsigned 64-bit range")
