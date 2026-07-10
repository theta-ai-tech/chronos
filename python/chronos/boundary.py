"""ctypes bridge for the M0.4 Chronos Python to C++ boundary."""

from __future__ import annotations

import ctypes
import os
from pathlib import Path

_BOUNDARY_LIBRARY_ENV = "CHRONOS_BOUNDARY_LIBRARY"
_STATUS_OK = 0
_STATUS_INVALID_ARGUMENT = 1
_STATUS_OUTPUT_TOO_SMALL = 2


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

    def round_trip_hello_event(self, payload: str) -> str:
        request = payload.encode("utf-8")
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
            raise BoundaryCallError(f"native boundary returned unknown status {status}")


def round_trip_hello_event(payload: str, library_path: Path | None = None) -> str:
    return ChronosBoundary(library_path).round_trip_hello_event(payload)
