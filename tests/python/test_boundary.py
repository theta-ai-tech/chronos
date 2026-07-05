from chronos import round_trip_hello_event


def test_hello_event_round_trips_through_native_boundary() -> None:
    assert round_trip_hello_event("hello event") == "chronos.boundary.v0|hello_ack|hello event"
