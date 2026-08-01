# M6.1 CMake-Native Portfolio Boundary Report

Date: 2026-08-01
Branch: `feat/m6.1-target-construction`
Supersedes: `bb16584`
Issue: `#34`

## Outcome

The M6 gate now delegates CMake semantics to CMake. The target owner contains only
the canonical `chronos_portfolio` declaration, include directory, and dependencies.
The root's literal final command synchronously includes
`core/portfolio/AssertTargetBoundary.cmake`, after every root subdirectory has
finished configuring.

The assertion reads the fully resolved target and requires:

- exact direct and interface link libraries;
- exact source and direct/interface include directories;
- no link options, link directories, consumer-direct links, or unexpected direct
  and interface compile capabilities;
- canonical source language and generated state;
- no source-level compile definitions/options/includes or build-changing source
  properties, including Visual Studio and Xcode settings;
- no `COMPILE_DEFINITIONS_<CONFIG>` for any built-in, active single-config, or
  multi-config configuration name.

This synchronous tail has no callback name or defer ID, so a later subdirectory
cannot redefine or cancel it. Typed failures use
`CHRONOS_M6_BOUNDARY_VIOLATION:<PROPERTY>:`.

## Verifier Design

The Python verifier retains static ownership, include, source, and backward-link
checks. It requires the final root include and the complete declarative assertion
command sequence, so deleting, truncating, making inert, or reordering the guard is
a structural violation even if `project()` is also removed.

For a structurally valid repository, the verifier runs a temporary out-of-source
CMake configure with expanded JSON tracing. The trace makes executed command
overrides visible even when they came from an external module selected through a
callable-local `CMAKE_MODULE_PATH`. Overrides of assertion-critical commands are
reported as `configured-target-property:COMMAND_OVERRIDE`; no Python include-graph
interpreter remains. Temporary configure artifacts are removed automatically.

The M5 verifier has a narrow compatibility rule for the two canonical expected-link
declarations in the assertion module. M6 remains responsible for validating the
whole assertion.

## TDD Evidence

The new tests first failed for all six reviewer categories: two-level `message`
interception, deferred guard replacement, deferred cancellation, Visual Studio/Xcode
source settings, custom-configuration source definitions, and simultaneous removal
of `project()` plus tail registration.

The final real-CMake fixtures also cover root/core/owner included mutations,
`cmake_language(CALL)`, callable-local module selection, escaped parentheses,
unquoted list expansion, after-core target mutation, canonical-source properties,
guard deletion/inert/reorder, exact valid construction, and direct consumer linkage.

## Final Gates

- `uv run pytest tests/python/test_m5_authority_boundaries.py tests/python/test_m6_authority_boundaries.py -q`
  - passed: 83 tests
- `python3 tools/development/verify_m6_authority_boundaries.py`
  - passed: `[OK] M6 portfolio authority has no forbidden dependencies`
- `make m0-check`
  - passed: native configure/build and CTest 1/1; Python distribution validation;
    155 Python tests; repository-wide Ruff lint and format checks
- `make cpp-profiles-check`
  - passed: Debug, Release, Benchmark, sanitizer, and Ninja Multi-Config
    Debug/Release/Benchmark configure, build, and CTest runs
- `uv run ruff check tools/development/verify_m5_authority_boundaries.py tools/development/verify_m6_authority_boundaries.py tests/python/test_m5_authority_boundaries.py tests/python/test_m6_authority_boundaries.py`
  - passed
- `uv run ruff format --check tools/development/verify_m5_authority_boundaries.py tools/development/verify_m6_authority_boundaries.py tests/python/test_m5_authority_boundaries.py tests/python/test_m6_authority_boundaries.py`
  - passed: 4 files already formatted
- `git diff --check`
  - passed

## Concerns

- The gate requires CMake and allows 120 seconds for its temporary configure.
  Toolchain or configure failures are typed gate failures.
- The assertion intentionally treats new target or canonical-source capabilities as
  denied by default. Legitimate graph changes must update the owner assertion,
  verifier constants, and focused fixtures together.
- Expanded CMake tracing adds configure output and runtime to the Python gate, but
  avoids interpreting CMake and is removed with the temporary build directory.
- No push or pull request was created.
