from typing import Optional

import pytest
from chronos.state_lineage import LineageRelation, StateLineage
from chronos.value_objects import ContractValueError, RunId, StreamCursor, StreamId

RUN_ID = RunId.parse("018f1f6e-7d3a-7c4b-8a91-0123456789a0")
STREAM_1 = StreamId.parse("018f1f6e-7d3a-7c4b-8a91-0123456789a1")
STREAM_2 = StreamId.parse("018f1f6e-7d3a-7c4b-8a91-0123456789a2")
REQUIRED = (STREAM_1, STREAM_2)


def cursor(
    stream_id: StreamId,
    sequence: Optional[int],
    epoch: int = 1,  # noqa: UP045
) -> StreamCursor:
    if sequence is None:
        return StreamCursor.at_origin(stream_id, epoch)
    return StreamCursor.at_sequence(stream_id, epoch, sequence)


def lineage(
    one: Optional[int],
    two: Optional[int],
    run_input: int = 1,  # noqa: UP045
) -> StateLineage:
    return StateLineage.from_cursors(
        RUN_ID,
        run_input,
        REQUIRED,
        (cursor(STREAM_2, two), cursor(STREAM_1, one)),
    )


def test_lineage_requires_complete_registered_cursor_vector() -> None:
    assert len(lineage(None, 0).cursors) == 2
    with pytest.raises(ContractValueError):
        StateLineage.from_cursors(RUN_ID, 1, REQUIRED, (cursor(STREAM_1, 0),))
    with pytest.raises(ContractValueError):
        StateLineage.from_cursors(
            RUN_ID,
            1,
            (STREAM_1, STREAM_1),
            (cursor(STREAM_1, 0), cursor(STREAM_2, 0)),
        )
    with pytest.raises(ContractValueError):
        StateLineage(RUN_ID, 1, REQUIRED, (cursor(STREAM_1, 0),))


def test_lineage_implements_vector_clock_ordering() -> None:
    origin = lineage(None, None)
    first = lineage(0, None, 2)
    later = lineage(2, 3, 8)
    assert origin.compare(first) is LineageRelation.PRECEDES
    assert first.compare(origin) is LineageRelation.SUCCEEDS
    assert first.compare(first) is LineageRelation.EQUAL
    assert first.compare(later) is LineageRelation.PRECEDES


def test_concurrent_cuts_do_not_collapse_to_run_input_scalar() -> None:
    left = lineage(5, 2, 10)
    right = lineage(3, 7, 11)
    assert left.run_input_sequence < right.run_input_sequence
    assert left.compare(right) is LineageRelation.CONCURRENT
    assert right.compare(left) is LineageRelation.CONCURRENT


def test_run_input_position_is_one_vector_dimension() -> None:
    earlier = lineage(2, 2, 10)
    later_same_cursors = lineage(2, 2, 11)
    cursor_ahead_input_behind = lineage(3, 3, 9)
    assert earlier.compare(later_same_cursors) is LineageRelation.PRECEDES
    assert later_same_cursors.compare(earlier) is LineageRelation.SUCCEEDS
    assert earlier.compare(cursor_ahead_input_behind) is LineageRelation.CONCURRENT


def test_different_runs_or_epochs_are_incomparable() -> None:
    base = lineage(1, 1)
    other_run = StateLineage.from_cursors(
        RunId.parse("018f1f6e-7d3a-7c4b-8a91-0123456789af"),
        1,
        REQUIRED,
        (cursor(STREAM_1, 1), cursor(STREAM_2, 1)),
    )
    other_epoch = StateLineage.from_cursors(
        RUN_ID,
        1,
        REQUIRED,
        (cursor(STREAM_1, 1, 2), cursor(STREAM_2, 1)),
    )
    assert base.compare(other_run) is LineageRelation.INCOMPARABLE
    assert base.compare(other_epoch) is LineageRelation.INCOMPARABLE
