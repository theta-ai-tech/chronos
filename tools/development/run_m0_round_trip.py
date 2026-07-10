"""Run the M0.4 Python to C++ boundary round trip."""

from __future__ import annotations

import sys

from chronos import round_trip_hello_event

REQUEST = "hello event"
EXPECTED_RESPONSE = "chronos.boundary.v0|hello_ack|hello event"


def main() -> int:
    response = round_trip_hello_event(REQUEST)
    if response != EXPECTED_RESPONSE:
        message = (
            f"unexpected M0.4 round-trip response: expected {EXPECTED_RESPONSE!r}, got {response!r}"
        )
        print(
            message,
            file=sys.stderr,
        )
        return 1

    print(response)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
