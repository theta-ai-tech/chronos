# M6.1 CMake-Native Portfolio Boundary Report

Date: 2026-08-01
Branch: `feat/m6.1-target-construction`
Starting commit: `9d457d657e0ece0602ab03eb1a94f49648861d15`
Issue: `#34`

## Outcome

The CMake include-graph interpreter introduced by `9d457d6` has been removed. The
`chronos_portfolio` owner now performs a declarative, configure-time assertion over
the target properties that CMake actually resolved.

The assertion requires:

- exact `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES` values;
- exact `SOURCES`, `INCLUDE_DIRECTORIES`, and `INTERFACE_INCLUDE_DIRECTORIES` values;
- empty direct and interface link options/directories;
- empty consumer-direct link injection/exclusion properties;
- empty direct and interface compile definitions, features, options, precompiled
  headers, system include directories, and interface sources.

Failures use the typed sentinel
`CHRONOS_M6_BOUNDARY_VIOLATION:<PROPERTY>:` so the Python verifier can report a
`configured-target-property:<PROPERTY>` violation.

## Verifier Design

`verify_m6_authority_boundaries.py` retains the static source/include/ownership and
backward-dependency checks. When those pass for a configurable repository, it runs
an out-of-source CMake configure in a temporary directory and removes the directory
afterward.

The structural check requires the canonical top-level property reads, comparisons,
fatal messages, expected values, and empty-property loop in command order after the
target configuration. Whole-guard deletion, partial deletion, inert `if(FALSE)`,
comparison reversal, and fatal-message removal are rejected.

The M5 verifier has a narrow compatibility rule for the two expected-link list
declarations. It accepts each declaration exactly once with its canonical values,
and permits subsequent variable references only from `if()` and `message()`.

## TDD Evidence

Initial real-CMake tests failed because the Python verifier did not configure the
fixture and the native assertion did not exist. Review regressions then failed for:

- `INTERFACE_LINK_LIBRARIES_DIRECT` and resolved source injection;
- inert or partially deleted native assertions;
- reassignment and opaque helper use of the M5 expected-link variables.

After implementation, the focused suite passed all 72 M5/M6 tests. The fixtures
exercise root, core, and owner includes; `cmake_language(CALL)`; callable-local
`CMAKE_MODULE_PATH`; escaped parentheses; unquoted list expansion; resolved sources;
consumer-direct linkage; compile/include properties; guard deletion; and a valid
direct consumer.

## Final Gates

- `uv run ruff format --check tools/development/verify_m5_authority_boundaries.py tools/development/verify_m6_authority_boundaries.py tests/python/test_m5_authority_boundaries.py tests/python/test_m6_authority_boundaries.py`
  - passed; 4 files already formatted
- `uv run ruff check tools/development/verify_m5_authority_boundaries.py tools/development/verify_m6_authority_boundaries.py tests/python/test_m5_authority_boundaries.py tests/python/test_m6_authority_boundaries.py`
  - passed
- `uv run pytest tests/python/test_m5_authority_boundaries.py tests/python/test_m6_authority_boundaries.py -q`
  - passed; 72 tests
- `python3 tools/development/verify_m6_authority_boundaries.py`
  - passed; `[OK] M6 portfolio authority has no forbidden dependencies`
- `make m0-check`
  - passed; CMake build and CTest 1/1, Python distribution verification, 144 Python
    tests, Ruff lint, and Ruff formatting
- `make cpp-profiles-check`
  - passed; Debug, Release, Benchmark, sanitizer, and multi-config Debug/Release/
    Benchmark builds and tests
- `git diff --check`
  - passed

## Review

An independent read-only review found three Important issues in the first native
implementation: missing consumer-direct/source properties, an inert structural-guard
bypass, and an over-broad M5 declaration exception. All three were reproduced with
focused tests and corrected before the final gates above.

## Concerns

- The gate now requires CMake to be available and permits 120 seconds for the
  temporary configure. Configure-toolchain failures are reported as typed gate
  failures rather than silently falling back to static analysis.
- The structural verifier intentionally tracks the canonical owner assertion. A
  legitimate change to the portfolio target's sources, links, includes, or allowed
  capabilities must update the owner assertion, verifier constants, and tests in the
  same change.
- No push or pull request was created.
