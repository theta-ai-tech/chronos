"""Complete vector-clock lineage for immutable state cuts."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from enum import Enum

from chronos.value_objects import ContractValueError, RunId, StreamCursor, StreamId


class LineageRelation(Enum):
    EQUAL = "equal"
    PRECEDES = "precedes"
    SUCCEEDS = "succeeds"
    CONCURRENT = "concurrent"
    INCOMPARABLE = "incomparable"


@dataclass(frozen=True)
class StateLineage:
    run_id: RunId
    run_input_sequence: int
    required_streams: tuple[StreamId, ...]
    cursors: tuple[StreamCursor, ...]

    def __post_init__(self) -> None:
        if not isinstance(self.run_id, RunId):
            raise TypeError("state lineage requires a RunId")
        _require_uint64("run input sequence", self.run_input_sequence)
        if (
            not self.required_streams
            or tuple(sorted(self.required_streams, key=lambda identity: identity.value.int))
            != self.required_streams
        ):
            raise ContractValueError("required streams must be nonempty and canonically ordered")
        if len(self.required_streams) != len(set(self.required_streams)):
            raise ContractValueError("required stream identities must be unique")
        if (
            tuple(sorted(self.cursors, key=lambda cursor: cursor.stream_id.value.int))
            != self.cursors
        ):
            raise ContractValueError("state lineage cursors must be canonically ordered")
        if tuple(cursor.stream_id for cursor in self.cursors) != self.required_streams:
            raise ContractValueError("state lineage cursor set does not match required streams")

    @classmethod
    def from_cursors(
        cls,
        run_id: RunId,
        run_input_sequence: int,
        required_streams: Iterable[StreamId],
        cursors: Iterable[StreamCursor],
    ) -> StateLineage:
        required = tuple(sorted(required_streams, key=lambda identity: identity.value.int))
        ordered = tuple(sorted(cursors, key=lambda cursor: cursor.stream_id.value.int))
        if not required or len(required) != len(set(required)):
            raise ContractValueError("required stream identities must be nonempty and unique")
        if len(ordered) != len(required):
            raise ContractValueError("state lineage must contain every required stream cursor")
        if tuple(cursor.stream_id for cursor in ordered) != required:
            raise ContractValueError("state lineage cursor set does not match required streams")
        return cls(run_id, run_input_sequence, required, ordered)

    def compare(self, other: StateLineage) -> LineageRelation:
        if not isinstance(other, StateLineage):
            raise TypeError("lineage comparison requires StateLineage")
        if self.run_id != other.run_id or len(self.cursors) != len(other.cursors):
            return LineageRelation.INCOMPARABLE

        less = self.run_input_sequence < other.run_input_sequence
        greater = self.run_input_sequence > other.run_input_sequence
        for left, right in zip(self.cursors, other.cursors):
            if left.stream_id != right.stream_id or left.stream_epoch != right.stream_epoch:
                return LineageRelation.INCOMPARABLE
            if left.last_consumed_sequence == right.last_consumed_sequence:
                continue
            if left.last_consumed_sequence is None or (
                right.last_consumed_sequence is not None
                and left.last_consumed_sequence < right.last_consumed_sequence
            ):
                less = True
            else:
                greater = True

        if less and greater:
            return LineageRelation.CONCURRENT
        if less:
            return LineageRelation.PRECEDES
        if greater:
            return LineageRelation.SUCCEEDS
        return LineageRelation.EQUAL


def _require_uint64(name: str, value: int) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an integer")
    if not 0 <= value <= 2**64 - 1:
        raise ContractValueError(f"{name} exceeds unsigned 64-bit range")
