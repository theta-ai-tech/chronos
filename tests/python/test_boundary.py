import pytest
from chronos import round_trip_hello_event
from chronos.boundary import BoundaryCallError


def test_hello_event_round_trips_through_native_boundary() -> None:
    assert round_trip_hello_event("hello event") == "chronos.boundary.v0|hello_ack|hello event"


@pytest.mark.parametrize("payload", ["", "hello\x00event", "Καλημέρα", "x" * 4096])
def test_hello_event_preserves_utf8_semantics(payload: str) -> None:
    assert round_trip_hello_event(payload) == f"chronos.boundary.v0|hello_ack|{payload}"


def test_hello_event_rejects_payload_above_native_limit() -> None:
    with pytest.raises(BoundaryCallError, match="maximum is 4096"):
        round_trip_hello_event("x" * 4097)
