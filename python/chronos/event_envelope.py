"""Validated logical event envelope and reserved event taxonomy."""

from __future__ import annotations

from dataclasses import InitVar, dataclass
from enum import Enum
from typing import Optional, Union

from chronos.state_lineage import StateLineage
from chronos.value_objects import (
    AuthorityId,
    CanonicalInstrumentId,
    CommandId,
    ContractValueError,
    CorrelationId,
    DataQuality,
    DecisionId,
    EventId,
    IntegrityId,
    ListingId,
    ProducerId,
    RunId,
    RuntimeId,
    SourceEventId,
    StateViewId,
    StreamId,
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
    "run.timer",
    "run.input",
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
        and event_type.count(".") >= 2
        and all(
            character.isascii()
            and (character.islower() or character.isdigit() or character in "_.")
            for character in event_type
        )
    )


class AcceptanceClass(Enum):
    ACCEPTED_TRANSITION = "accepted_transition"
    ACCEPTED_OBSERVATION = "accepted_observation"
    ACCEPTED_REJECTION = "accepted_rejection"
    ACCEPTED_CORRECTION = "accepted_correction"


class RunMode(Enum):
    CAPTURE = "capture"
    REPLAY = "replay"
    BACKTEST = "backtest"
    LIVE_READ_ONLY = "live_read_only"
    LIVE_PAPER = "live_paper"


@dataclass(frozen=True)
class EventPosition:
    stream_id: StreamId
    stream_epoch: int
    stream_sequence: int

    def __post_init__(self) -> None:
        _require_type("stream ID", self.stream_id, StreamId)
        _require_positive_uint64("stream epoch", self.stream_epoch)
        _require_uint64("stream sequence", self.stream_sequence)


@dataclass(frozen=True)
class ProducerRef:
    component_id: ProducerId
    implementation_version: VersionRef
    runtime_incarnation_id: RuntimeId

    def __post_init__(self) -> None:
        _require_type("producer component", self.component_id, ProducerId)
        _require_type("producer version", self.implementation_version, VersionRef)
        _require_type("producer runtime", self.runtime_incarnation_id, RuntimeId)


@dataclass(frozen=True)
class EventTypeRegistration:
    event_type: str
    semantic_owner: AuthorityId
    root_observation: bool
    run_scoped: bool
    ordered: bool
    run_input_eligible: bool
    requires_source_event: bool
    requires_subjects: bool
    mode_sensitive: bool
    requires_effective_position: bool
    requires_integrity: bool
    requires_receive_time: bool

    def __post_init__(self) -> None:
        if not is_valid_event_type(self.event_type):
            raise ContractValueError("registered event type is outside the stable taxonomy")
        _require_type("semantic owner", self.semantic_owner, AuthorityId)


@dataclass(frozen=True)
class EventEnvelope:
    registration: InitVar[EventTypeRegistration]
    event_id: EventId
    event_type: str
    envelope_version: int
    schema_version: VersionRef
    semantic_owner: AuthorityId
    producer: ProducerRef
    acceptance_class: AcceptanceClass
    run_id: Optional[RunId]  # noqa: UP045 - Python 3.9 syntax
    mode: Optional[RunMode]  # noqa: UP045 - Python 3.9 syntax
    event_position: Optional[EventPosition]  # noqa: UP045 - Python 3.9 syntax
    run_input_sequence: Optional[int]  # noqa: UP045 - Python 3.9 syntax
    effective_position: Optional[int]  # noqa: UP045 - Python 3.9 syntax
    state_lineage: Optional[StateLineage]  # noqa: UP045 - Python 3.9 syntax
    source_event_id: Optional[SourceEventId]  # noqa: UP045 - Python 3.9 syntax
    causation_refs: Optional[tuple[CausationRef, ...]]  # noqa: UP045
    correlation_refs: Optional[tuple[CorrelationId, ...]]  # noqa: UP045
    subject_refs: Optional[tuple[SubjectRef, ...]]  # noqa: UP045
    source_event_time: Optional[TimePoint]  # noqa: UP045 - Python 3.9 syntax
    chronos_receive_time: Optional[TimePoint]  # noqa: UP045 - Python 3.9 syntax
    accept_time: TimePoint
    recoverability_handoff_time: Optional[TimePoint]  # noqa: UP045
    record_time: Optional[TimePoint]  # noqa: UP045 - Python 3.9 syntax
    quality: DataQuality
    payload: bytes
    integrity: Optional[IntegrityId]  # noqa: UP045

    def __post_init__(self, registration: EventTypeRegistration) -> None:
        _require_type("event registration", registration, EventTypeRegistration)
        if not isinstance(self.event_id, EventId):
            raise TypeError("event envelope requires an EventId")
        if self.event_type != registration.event_type:
            raise ContractValueError("event type does not match its registry entry")
        _require_positive_uint32("envelope version", self.envelope_version)
        _require_type("schema version", self.schema_version, VersionRef)
        _require_type("semantic owner", self.semantic_owner, AuthorityId)
        if self.semantic_owner != registration.semantic_owner:
            raise ContractValueError("semantic owner does not match its registry entry")
        _require_type("producer", self.producer, ProducerRef)
        _require_type("acceptance class", self.acceptance_class, AcceptanceClass)
        _require_optional_type("run ID", self.run_id, RunId)
        _require_optional_type("mode", self.mode, RunMode)
        _require_optional_type("event position", self.event_position, EventPosition)
        _require_optional_uint64("run input sequence", self.run_input_sequence)
        _require_optional_uint64("effective position", self.effective_position)
        _require_optional_type("state lineage", self.state_lineage, StateLineage)
        _require_optional_type("source event ID", self.source_event_id, SourceEventId)
        _require_optional_type("source event time", self.source_event_time, TimePoint)
        _require_optional_type("receive time", self.chronos_receive_time, TimePoint)
        _require_type("accept time", self.accept_time, TimePoint)
        _require_optional_type(
            "recoverability handoff time", self.recoverability_handoff_time, TimePoint
        )
        _require_optional_type("record time", self.record_time, TimePoint)
        _require_type("quality", self.quality, DataQuality)
        _require_optional_type("integrity", self.integrity, IntegrityId)
        if not isinstance(self.payload, bytes):
            raise TypeError("event payload must be bytes")
        self._validate_registration(registration)
        self._validate_lineage(registration)
        self._validate_references()

    def _validate_registration(self, registration: EventTypeRegistration) -> None:
        checks = (
            (registration.run_scoped, self.run_id is not None),
            (registration.ordered, self.event_position is not None),
            (registration.requires_source_event, self.source_event_id is not None),
            (registration.requires_subjects, self.subject_refs is not None),
            (registration.mode_sensitive, self.mode is not None),
            (registration.requires_effective_position, self.effective_position is not None),
            (registration.requires_integrity, self.integrity is not None),
            (registration.requires_receive_time, self.chronos_receive_time is not None),
        )
        if any(required != present for required, present in checks):
            raise ContractValueError("event fields do not match registry applicability")
        if not registration.root_observation and self.causation_refs is None:
            raise ContractValueError("derived events require direct causation")

    def _validate_lineage(self, registration: EventTypeRegistration) -> None:
        if self.state_lineage is not None and (
            self.run_id is None or self.state_lineage.run_id != self.run_id
        ):
            raise ContractValueError("state lineage must match the envelope run")
        if self.run_input_sequence is not None and (
            not registration.run_input_eligible
            or self.run_id is None
            or self.event_position is None
        ):
            raise ContractValueError("run input position requires scoped selected input")
        if self.effective_position is not None and self.run_id is None:
            raise ContractValueError("effective position requires run scope")
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
        if self.correlation_refs is not None:
            if not self.correlation_refs:
                raise ContractValueError("present correlation references must not be empty")
            if any(not isinstance(reference, CorrelationId) for reference in self.correlation_refs):
                raise TypeError("correlation references must be CorrelationIds")
            if len(set(self.correlation_refs)) != len(self.correlation_refs):
                raise ContractValueError("correlation references must be unique")


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


def _require_uint64(name: str, value: int) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    if not 0 <= value <= 2**64 - 1:
        raise ContractValueError(f"{name} exceeds unsigned 64-bit range")


def _require_positive_uint64(name: str, value: int) -> None:
    _require_uint64(name, value)
    if value == 0:
        raise ContractValueError(f"{name} must be positive")


def _require_optional_uint64(name: str, value: Optional[int]) -> None:  # noqa: UP045
    if value is None:
        return
    _require_uint64(name, value)
