"""Canonical identities and core value objects."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Optional
from uuid import UUID


class ContractValueError(ValueError):
    """Raised when a canonical value object is invalid."""


@dataclass(frozen=True)
class OpaqueId:
    value: UUID

    def __post_init__(self) -> None:
        if not isinstance(self.value, UUID):
            raise TypeError("opaque identity value must be a UUID")
        if self.value.int == 0:
            raise ContractValueError("opaque identity must not be the nil UUID")

    @classmethod
    def parse(cls, value: str) -> OpaqueId:
        if not isinstance(value, str):
            raise TypeError("opaque identity text must be a string")
        try:
            parsed = UUID(value)
        except (ValueError, AttributeError) as error:
            raise ContractValueError("opaque identity must be a UUID") from error
        if str(parsed) != value.lower():
            raise ContractValueError("opaque identity must use canonical UUID text")
        return cls(parsed)

    def __str__(self) -> str:
        return str(self.value)


@dataclass(frozen=True)
class EventId(OpaqueId):
    pass


@dataclass(frozen=True)
class CommandId(OpaqueId):
    pass


@dataclass(frozen=True)
class StateViewId(OpaqueId):
    pass


@dataclass(frozen=True)
class DecisionId(OpaqueId):
    pass


@dataclass(frozen=True)
class AuthorityId(OpaqueId):
    pass


@dataclass(frozen=True)
class RuntimeId(OpaqueId):
    pass


@dataclass(frozen=True)
class CorrelationId(OpaqueId):
    pass


@dataclass(frozen=True)
class IntegrityId(OpaqueId):
    pass


@dataclass(frozen=True)
class StreamId(OpaqueId):
    pass


@dataclass(frozen=True)
class RunId(OpaqueId):
    pass


@dataclass(frozen=True)
class SourceEventId(OpaqueId):
    pass


@dataclass(frozen=True)
class CanonicalInstrumentId(OpaqueId):
    pass


@dataclass(frozen=True)
class ListingId(OpaqueId):
    pass


@dataclass(frozen=True)
class ProducerId(OpaqueId):
    pass


@dataclass(frozen=True)
class DefinitionId(OpaqueId):
    pass


@dataclass(frozen=True)
class ClockDomainId(OpaqueId):
    pass


@dataclass(frozen=True)
class StreamCursor:
    stream_id: StreamId
    stream_epoch: int
    last_consumed_sequence: Optional[int]  # noqa: UP045 - Python 3.9 syntax

    def __post_init__(self) -> None:
        if not isinstance(self.stream_id, StreamId):
            raise TypeError("stream cursor requires a StreamId")
        _require_uint64("stream epoch", self.stream_epoch, nonzero=True)
        if self.last_consumed_sequence is not None:
            _require_uint64("last consumed sequence", self.last_consumed_sequence)

    @classmethod
    def at_origin(cls, stream_id: StreamId, stream_epoch: int) -> StreamCursor:
        return cls(stream_id, stream_epoch, None)

    @classmethod
    def at_sequence(cls, stream_id: StreamId, stream_epoch: int, sequence: int) -> StreamCursor:
        return cls(stream_id, stream_epoch, sequence)

    @property
    def is_origin(self) -> bool:
        return self.last_consumed_sequence is None


@dataclass(frozen=True)
class VersionRef:
    definition_id: DefinitionId
    version: int

    def __post_init__(self) -> None:
        if not isinstance(self.definition_id, DefinitionId):
            raise TypeError("version reference requires a DefinitionId")
        _require_uint64("version", self.version, nonzero=True)


class ClockClass(Enum):
    SOURCE_WALL = "source_wall"
    CHRONOS_WALL = "chronos_wall"
    MONOTONIC = "monotonic"
    REPLAY_LOGICAL = "replay_logical"


@dataclass(frozen=True)
class TimePoint:
    nanoseconds: int
    clock_domain_id: ClockDomainId
    clock_class: ClockClass
    precision_nanoseconds: int

    def __post_init__(self) -> None:
        _require_int64("time point", self.nanoseconds)
        if not isinstance(self.clock_domain_id, ClockDomainId):
            raise TypeError("time point requires a ClockDomainId")
        if not isinstance(self.clock_class, ClockClass):
            raise TypeError("time point requires a ClockClass")
        _require_uint32("time precision", self.precision_nanoseconds, nonzero=True)

    def checked_compare(self, other: TimePoint) -> int:
        if not isinstance(other, TimePoint):
            raise TypeError("time comparison requires a TimePoint")
        if (
            self.clock_domain_id != other.clock_domain_id
            or self.clock_class is not other.clock_class
        ):
            raise ContractValueError("time points in different clock domains are incomparable")
        return (self.nanoseconds > other.nanoseconds) - (self.nanoseconds < other.nanoseconds)


class QualityStatus(Enum):
    VALID = "valid"
    STALE = "stale"
    GAPPED = "gapped"
    RECOVERING = "recovering"
    INVALID = "invalid"
    UNAVAILABLE = "unavailable"


@dataclass(frozen=True)
class DataQuality:
    status: QualityStatus
    reason_code: int

    def __post_init__(self) -> None:
        if not isinstance(self.status, QualityStatus):
            raise TypeError("data quality requires a QualityStatus")
        _require_uint32("quality reason code", self.reason_code)
        if (self.status is QualityStatus.VALID) != (self.reason_code == 0):
            raise ContractValueError("valid quality has no reason; non-valid quality requires one")


def _require_int64(name: str, value: int) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    if not -(2**63) <= value <= 2**63 - 1:
        raise ContractValueError(f"{name} exceeds signed 64-bit range")


def _require_uint64(name: str, value: int, *, nonzero: bool = False) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    minimum = 1 if nonzero else 0
    if not minimum <= value <= 2**64 - 1:
        raise ContractValueError(f"{name} exceeds unsigned 64-bit range")


def _require_uint32(name: str, value: int, *, nonzero: bool = False) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    minimum = 1 if nonzero else 0
    if not minimum <= value <= 2**32 - 1:
        raise ContractValueError(f"{name} exceeds unsigned 32-bit range")
