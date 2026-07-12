"""ctypes bridge for the M0.4 Chronos Python to C++ boundary."""

from __future__ import annotations

import ctypes
import os
from pathlib import Path

_BOUNDARY_LIBRARY_ENV = "CHRONOS_BOUNDARY_LIBRARY"
_STATUS_OK = 0
_STATUS_INVALID_ARGUMENT = 1
_STATUS_OUTPUT_TOO_SMALL = 2
_STATUS_INPUT_TOO_LARGE = 3
_STATUS_RESOURCE_EXHAUSTED = 4
_STATUS_INTERNAL_ERROR = 5
_STATUS_INVALID_CONTRACT = 6


class BoundaryLibraryNotFound(RuntimeError):
    """Raised when the native Chronos boundary library has not been built."""


class BoundaryCallError(RuntimeError):
    """Raised when the native boundary returns an unexpected failure."""


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _library_names() -> tuple[str, ...]:
    return (
        "libchronos_boundary.dylib",
        "libchronos_boundary.so",
        "chronos_boundary.dll",
    )


def _candidate_library_paths() -> tuple[Path, ...]:
    env_path = os.environ.get(_BOUNDARY_LIBRARY_ENV)
    candidates = []
    if env_path:
        candidates.append(Path(env_path))

    root = _repo_root()
    for name in _library_names():
        candidates.append(root / "build" / "core" / name)
        candidates.append(root / "build" / name)
    return tuple(candidates)


def _resolve_library_path(library_path: Path | None) -> Path:
    if library_path is not None:
        path = Path(library_path)
        if path.exists():
            return path
        raise BoundaryLibraryNotFound(f"Chronos boundary library not found: {path}")

    for candidate in _candidate_library_paths():
        if candidate.exists():
            return candidate

    searched = ", ".join(str(path) for path in _candidate_library_paths())
    raise BoundaryLibraryNotFound(
        "Chronos boundary library has not been built. "
        "Run `cmake -S . -B build -G Ninja && cmake --build build`; "
        f"searched: {searched}"
    )


class ChronosBoundary:
    """Thin Python wrapper around the native M0.4 C ABI."""

    def __init__(self, library_path: Path | None = None) -> None:
        self.library_path = _resolve_library_path(library_path)
        self._library = ctypes.CDLL(str(self.library_path))
        self._round_trip = self._library.chronos_boundary_hello_event_round_trip
        self._round_trip.argtypes = [
            ctypes.c_char_p,
            ctypes.c_size_t,
            ctypes.c_char_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self._round_trip.restype = ctypes.c_int
        self._max_payload = self._library.chronos_boundary_max_hello_payload_bytes
        self._max_payload.argtypes = []
        self._max_payload.restype = ctypes.c_size_t
        self._contract_round_trip = self._library.chronos_boundary_contract_round_trip
        self._contract_round_trip.argtypes = [
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self._contract_round_trip.restype = ctypes.c_int
        self._max_contract_frame = self._library.chronos_boundary_max_contract_frame_bytes
        self._max_contract_frame.argtypes = []
        self._max_contract_frame.restype = ctypes.c_size_t

    def round_trip_hello_event(self, payload: str) -> str:
        request = payload.encode("utf-8")
        max_payload = self._max_payload()
        if len(request) > max_payload:
            raise BoundaryCallError(
                f"hello event payload is {len(request)} bytes; maximum is {max_payload}"
            )
        capacity = max(len(request) + 64, 256)

        while True:
            output = ctypes.create_string_buffer(capacity)
            output_len = ctypes.c_size_t()
            status = self._round_trip(
                request,
                len(request),
                output,
                capacity,
                ctypes.byref(output_len),
            )

            if status == _STATUS_OK:
                return bytes(output.raw[: output_len.value]).decode("utf-8")
            if status == _STATUS_OUTPUT_TOO_SMALL:
                capacity = output_len.value + 1
                continue
            if status == _STATUS_INVALID_ARGUMENT:
                raise BoundaryCallError("native boundary rejected the hello event arguments")
            if status == _STATUS_INPUT_TOO_LARGE:
                raise BoundaryCallError("native boundary rejected an oversized hello event")
            if status == _STATUS_RESOURCE_EXHAUSTED:
                raise BoundaryCallError("native boundary exhausted resources")
            if status == _STATUS_INTERNAL_ERROR:
                raise BoundaryCallError("native boundary failed internally")
            raise BoundaryCallError(f"native boundary returned unknown status {status}")

    def round_trip_contract_frame(self, frame: bytes) -> bytes:
        if not isinstance(frame, bytes):
            raise TypeError("contract frame must be bytes")
        maximum = self._max_contract_frame()
        if len(frame) > maximum:
            raise BoundaryCallError(f"contract frame is {len(frame)} bytes; maximum is {maximum}")
        request = (ctypes.c_uint8 * len(frame)).from_buffer_copy(frame)
        capacity = max(len(frame), 256)
        while True:
            output = (ctypes.c_uint8 * capacity)()
            output_len = ctypes.c_size_t()
            status = self._contract_round_trip(
                request,
                len(frame),
                output,
                capacity,
                ctypes.byref(output_len),
            )
            if status == _STATUS_OK:
                return bytes(output[: output_len.value])
            if status == _STATUS_OUTPUT_TOO_SMALL:
                capacity = output_len.value
                continue
            if status == _STATUS_INVALID_CONTRACT:
                raise BoundaryCallError("native boundary rejected an invalid contract frame")
            if status == _STATUS_INPUT_TOO_LARGE:
                raise BoundaryCallError("native boundary rejected an oversized contract frame")
            if status == _STATUS_RESOURCE_EXHAUSTED:
                raise BoundaryCallError("native boundary exhausted resources")
            if status == _STATUS_INVALID_ARGUMENT:
                raise BoundaryCallError("native boundary rejected contract-frame arguments")
            if status == _STATUS_INTERNAL_ERROR:
                raise BoundaryCallError("native contract boundary failed internally")
            raise BoundaryCallError(f"native boundary returned unknown status {status}")


def round_trip_hello_event(payload: str, library_path: Path | None = None) -> str:
    return ChronosBoundary(library_path).round_trip_hello_event(payload)
