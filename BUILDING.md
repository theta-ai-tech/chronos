# Building Chronos

Chronos currently has two build surfaces:

- The C++ hot path (`core/`) builds with CMake. This covers the toolchain added in **M0.2**.
- The Python application/research layer is managed with `uv`. This covers the toolchain added in
  **M0.3**.

## Prerequisites

| Tool | Version used | Notes |
|---|---|---|
| CMake | ≥ 3.24 (tested 4.3.1) | build configuration |
| C++ compiler | Clang 18.x (canonical), AppleClang 16.x (macOS development) | enforced by CMake |
| Ninja | tested 1.13.2 | recommended generator |
| Python | ≥ 3.9 (tested 3.9.6) | Python runtime |
| uv | tested 0.11.16 | Python dependency lock and tool runner |
| make | POSIX make | aggregate local commands |

No third-party libraries are fetched during the C++ build (the test harness is vendored), so a
C++ configure + build works without network access. Python tooling is resolved from `uv.lock`;
the first `uv sync` needs access to the configured Python package index unless the packages are
already cached.

## Quick start

### One-command M0 bootstrap

```sh
make bootstrap
```

This checks required host tools, syncs locked Python dev dependencies, configures and builds the
C++ targets, runs C++ and Python tests, runs Ruff lint/format gates, and executes the M0.4
Python→C++ hello-event round trip.

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

To verify that CI gates fail when their checks are violated:

```sh
make m0-gate-proof
```

The proof command works on temporary copies of the repository and injects one representative
violation per gate. It succeeds only when the C++ warning/build gate, C++ unit-test gate, Python
unit-test gate, Python lint gate, and Python format gate each fail with the expected failure
signature.

To run only the M0.4 boundary smoke check after the C++ library has been built:

```sh
make m0-round-trip
```

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

Checked-in CMake presets provide the supported single-config builds. Run
`cmake --list-presets` to inspect them, or `make cpp-profiles-check` to build and test all
four presets plus Debug, Release, and Benchmark through Ninja Multi-Config.

CI installs and selects `clang++-18` explicitly before running the complete profile verifier.
AppleClang 16.x is accepted for local macOS development so contributors can use the platform
toolchain, but Clang 18.x is the pinned build-of-record compiler.

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
- `make python-build` — builds the sdist and wheel and rejects local/worktree/build leakage.
- `Makefile` — `make python-check` aggregate for the Python test/lint/format gate.
- `python/chronos/` — placeholder Python package proving packaging/imports work.
- `tests/python/` — pytest suite proving the package is installed through project metadata.

## What's here in M0.4

- `chronos_boundary` — shared C ABI library built from C++ for Python to load.
- `chronos::round_trip_hello_event()` — native scaffold call returning a versioned hello-event
  acknowledgement.
- The C ABI accepts at most 4096 UTF-8 payload bytes, contains exceptions as status codes, and
  exports only the two documented C entry points.
- `python/chronos/boundary.py` — `ctypes` wrapper for the native boundary.
- `tests/unit/boundary_test.cpp` and `tests/python/test_boundary.py` — C++ and Python coverage
  proving the hello event crosses Python→C++→Python.
- `make m0-check` — aggregate local gate for the current M0 scaffold.

## What's here in M0.5

- `.github/workflows/ci.yml` — GitHub Actions baseline for the M0 mixed-runtime scaffold.
- `make m0-check` — positive CI gate: C++ build/test plus Python test/lint/format.
- `make m0-gate-proof` — negative proof that representative violations fail their gates.
- `tools/development/prove_m0_gates.py` — temporary-copy proof harness used by local and CI runs.

## What's here in M0.6

- `make bootstrap` — one command to sync, build, test, lint, format-check, and run the boundary
  round trip on a supported machine.
- `make m0-round-trip` — targeted manual smoke check for the M0.4 Python→C++ boundary.
- `tools/development/bootstrap_m0.py` — bootstrap orchestration with missing-tool diagnostics.
- `tools/development/run_m0_round_trip.py` — explicit hello-event runner for docs and manual tests.

Real domain modules and canonical cross-boundary contracts arrive in later milestones.
