# Building Chronos

Chronos currently has two build surfaces:

- The C++ hot path (`core/`) builds with CMake. This covers the toolchain added in **M0.2**.
- The Python application/research layer is managed with `uv`. This covers the toolchain added in
  **M0.3**.

## Prerequisites

| Tool | Version used | Notes |
|---|---|---|
| CMake | ≥ 3.24 (tested 4.3.1) | build configuration |
| C++ compiler | C++20 (tested AppleClang 16) | Clang or GCC |
| Ninja | tested 1.13.2 | recommended generator |
| Python | ≥ 3.9 (tested 3.9.6) | Python runtime |
| uv | tested 0.11.16 | Python dependency lock and tool runner |
| make | POSIX make | aggregate local commands |

No third-party libraries are fetched during the C++ build (the test harness is vendored), so a
C++ configure + build works without network access. Python tooling is resolved from `uv.lock`;
the first `uv sync` needs access to the configured Python package index unless the packages are
already cached.

## Quick start

### C++

```sh
cmake -S . -B build -G Ninja      # configure (Debug by default)
cmake --build build               # compile
ctest --test-dir build --output-on-failure   # run tests
```

### Python

```sh
uv sync --group dev
make m0-check
```

`make m0-check` builds the C++ targets, runs CTest, then runs the locked Python test, lint, and
format-check tools through `uv`; contributors do not need globally installed `pytest` or `ruff`.

## C++ build profiles

Select with `-DCMAKE_BUILD_TYPE=<profile>`:

| Profile | Flags | Use |
|---|---|---|
| `Debug` (default) | unoptimized, asserts on | development |
| `Release` | `-O3 -DNDEBUG` | general optimized build |
| `Benchmark` | `-O3 -DNDEBUG`, no `-march=native` | latency benchmarking (ADR-0002) — reproducible, host-neutral |

```sh
cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Benchmark
cmake --build build-bench
```

## C++ options

| Option | Default | Effect |
|---|---|---|
| `CHRONOS_WERROR` | `ON` | compiler warnings are errors |
| `CHRONOS_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer |

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCHRONOS_SANITIZE=ON
```

## What's here in M0.2

- `chronos_core` — placeholder library (`core/`) proving the toolchain links.
- `chronos_unit_tests` — unit-test executable registered with CTest.
- `tests/support/microtest.hpp` — vendored ~60-line, zero-dependency test harness.
- `chronos_warnings` / `chronos_options` — shared CMake interface targets for the warning set
  and language standard / sanitizers.

## What's here in M0.3

- `pyproject.toml` — Python project metadata plus pytest and Ruff configuration.
- `uv.lock` — locked dependency graph for Python dev tools.
- `Makefile` — `make python-check` aggregate for the Python test/lint/format gate.
- `python/chronos/` — placeholder Python package proving packaging/imports work.
- `tests/python/` — pytest suite proving the package is installed through project metadata.

## What's here in M0.4

- `chronos_boundary` — shared C ABI library built from C++ for Python to load.
- `chronos::round_trip_hello_event()` — native scaffold call returning a versioned hello-event
  acknowledgement.
- `python/chronos/boundary.py` — `ctypes` wrapper for the native boundary.
- `tests/unit/boundary_test.cpp` and `tests/python/test_boundary.py` — C++ and Python coverage
  proving the hello event crosses Python→C++→Python.
- `make m0-check` — aggregate local gate for the current M0 scaffold.

Real domain modules and canonical cross-boundary contracts arrive in later milestones.
