# M6.2 Task 4 Report

## Status

DONE

## Commits

- `c1fe520 build: enforce M6.2 risk authority boundaries (#35)`

## Scope

The task adds the `chronos_risk` structural authority gate, preserves the M6.1
portfolio gate, and registers both in the aggregate development and Python
distribution checks.

Task 3 introduced the internal `core/risk/src/risk_arithmetic.hpp` seam. To
make the Task 4 undeclared-native-source rule true for the real repository,
`core/risk/CMakeLists.txt` now declares both canonical risk sources and the
native guard verifies their sorted exact set.

## Implementation

- `core/risk/AssertTargetBoundary.cmake` verifies exact sources, direct and
  interface dependencies, include directories, empty target capabilities,
  canonical implementation-source properties, and emits a trace sentinel.
- `tools/development/verify_m6_risk_authority_boundaries.py` verifies static
  ownership/include/dependency rules and configures temporary fixtures under
  expanded JSON CMake tracing to observe executed graph mutations.
- `tests/python/test_m6_risk_authority_boundaries.py` covers 70 ownership,
  mutation, guard-integrity, trace, source-property, and packaging scenarios.
- The root CMake tail is exactly risk guard then portfolio guard. The portfolio
  verifier explicitly permits and tests the preceding risk guard without
  permitting arbitrary target declarations.
- `make m6-risk-authority-check` is part of `m0-check`; the verifier and tests
  are declared in the source distribution and package-content verifier.

## Debugging Evidence

The first standalone temporary-fixture probe hung in `is_generated_path`.
macOS exposed a resolved source path under `/private/var/...` while
`tempfile.mkdtemp()` supplied `/var/...`; the textual ancestor loop reached
filesystem root without ever equalling the supplied root.

The fix resolves both paths and terminates explicitly at filesystem root. A
symlinked logical-root regression reproduces the alias mismatch without relying
on a platform-specific path. The corrected 70-test risk suite completes in
about 15 seconds, and the formerly hung standalone fixture returns no
violations in under one second.

## Verification

Executed individually from the task worktree:

```sh
uv run pytest tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
python3 tools/development/verify_m6_authority_boundaries.py
python3 tools/development/verify_m6_risk_authority_boundaries.py
uv run ruff check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py \
  tools/development/verify_m6_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py
uv run ruff format --check \
  tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py \
  tools/development/verify_m6_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py
make m6-risk-authority-check
make m6-authority-check
uv run python tools/development/verify_python_distribution.py
cmake -S . -B build -G Ninja
cmake --build build
git diff --check
```

Results:

- Cross-gate Python matrix: `157 passed in 28.64s`.
- Risk-only matrix after the alias regression: `70 passed in 15.35s`.
- Portfolio and risk standalone verifiers: both `[OK]`.
- Ruff lint and format: passed.
- Source distribution and wheel exact-content verification: passed.
- Actual repository configure and build: passed.
- Diff integrity: passed.

## Self-Review

- Confirmed the verifier parses actual expanded CMake trace semantics rather
  than emulating includes.
- Confirmed subprocess configure has a finite timeout and temporary build
  directories do not remain in fixtures.
- Confirmed the risk guard executes before the portfolio guard and both reject
  target/source mutations around their sentinels.
- Confirmed only contracts, portfolio, options, and warnings are accepted as
  risk dependencies.
- Confirmed the internal arithmetic header is declared but does not become a
  public include directory or new callable authority surface.
- Confirmed Task 5 documentation is not included in the implementation commit.

## Concerns

None.
