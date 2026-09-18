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

## Fix Round 1

Independent review identified two fail-open paths and both were corrected with
regression-first coverage.

### Post-Sentinel Built-In Aliases

The expanded CMake trace can report an overridden built-in invocation as
`_target_link_libraries`. Post-sentinel classification previously compared the
raw command name and missed that alias. The verifier now strips leading
underscores only when the resulting name is one of the known protected CMake
mutation commands. Arbitrary underscore-prefixed functions remain arbitrary
functions rather than being promoted to built-ins.

The real fixture overrides `target_link_libraries`, composes the protected
target and alias command, and defers the expanded
`_target_link_libraries(chronos_risk PRIVATE chronos_recommendation)` call until
after both guards. Removing alias normalization reproduced RED:

```text
1 failed in 0.59s
assert set() == {"LINK_LIBRARIES"}
```

### Missing Configure Prerequisites and Canonical Sources

Static ownership now requires exactly these two canonical paths, independent
of the files merely agreeing with their declarations:

```text
src/risk_arithmetic.hpp
src/risk_decision.cpp
```

A root intended for real configuration now emits an explicit
`missing-cmake-configure-prerequisite:<command>` violation when
`cmake_minimum_required()` or `project()` is absent. Minimal static-only
fixtures remain useful, while the repository and native configurable fixtures
fail closed. The regression deleting `project()` and adding plus declaring
`src/extra.cpp` initially returned no violations and now reports both the
missing prerequisite and noncanonical source set.

### Fix Verification

Executed individually from the task worktree after the final changes:

```text
two new regressions
2 passed in 0.58s

uv run pytest tests/python/test_m6_risk_authority_boundaries.py -q
72 passed in 14.96s

uv run pytest tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
159 passed in 28.53s

python3 tools/development/verify_m6_authority_boundaries.py
[OK] M6 portfolio authority has no forbidden dependencies

python3 tools/development/verify_m6_risk_authority_boundaries.py
[OK] M6 risk authority has no forbidden dependencies

uv run ruff check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!

uv run ruff format --check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
2 files already formatted
```

Implementation commit: `d1aa360`.

Fix-round concerns: none.

## Fix Round 2

Scoped review found that built-in alias normalization was too broad and that a
static prerequisite violation could suppress configured trace analysis. Both
paths were corrected with regression-first coverage.

### Exact Single-Underscore Aliases

Protected CMake mutation commands now recognize only the exact built-in alias
form with one leading underscore. For example, `_target_link_libraries` maps to
`target_link_libraries`, while `__target_link_libraries` and names with further
leading underscores remain arbitrary user functions.

The existing real-fixture regression continues to reject the exact built-in
alias after both guards. A new real fixture defines and defers a harmless
`__target_link_libraries` function after both guards and verifies that it is not
classified as a protected mutation. Before the implementation change, the new
test failed because `lstrip("_")` normalized both forms to the built-in name.

### Configure Eligibility After Missing Prerequisites

A full fixture is now eligible for configured graph analysis when either
canonical configure prerequisite remains. This preserves static-only fixtures,
which contain neither signal, while ensuring that deleting only `project()` or
only `cmake_minimum_required()` cannot disable expanded JSON trace checks.

Configured trace analysis now examines the guard sentinel and post-guard
mutations even when CMake ultimately returns a prerequisite-related failure.
The regressions delete each prerequisite independently and defer a dynamically
composed `target_link_libraries` mutation after the guards; both report the
missing prerequisite and `configured-target-property:LINK_LIBRARIES`.
Canonical static ownership remains fixed to `src/risk_decision.cpp` and
`src/risk_arithmetic.hpp`. The missing-project plus extra-source fixture also
retains its static violations and now reports configured source mutation
evidence.

Initial focused RED had three failures: the double-underscore function was
misclassified, and neither missing-prerequisite fixture reported the configured
post-guard link mutation. The exact single-underscore alias regression already
passed and remained protected throughout.

### Fix Verification

Executed from the task worktree:

```text
four focused round-2 regressions after final formatting
4 passed in 2.40s

uv run pytest tests/python/test_m6_risk_authority_boundaries.py -q
75 passed in 23.16s

uv run pytest tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
162 passed in 36.27s

python3 tools/development/verify_m6_authority_boundaries.py
[OK] M6 portfolio authority has no forbidden dependencies

python3 tools/development/verify_m6_risk_authority_boundaries.py
[OK] M6 risk authority has no forbidden dependencies

uv run ruff check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!

uv run ruff format --check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
2 files already formatted

git diff --check
passed
```

Implementation commit: `8adb608`.

Fix-round concerns: none.

## Fix Round 3

Scoped review found two remaining broad underscore normalizations in static
guard-command interception and expanded-trace critical-command interception.
Both used `lstrip("_")`, so harmless user functions such as `__message` were
classified as CMake built-in aliases.

One allowlist-aware `normalize_cmake_builtin_alias` helper now handles all Task
4 command-alias classification: static critical-command interception, trace
critical-command interception, and post-guard target mutation classification.
It maps only a known built-in name with exactly one leading underscore. Names
with two or more leading underscores remain arbitrary user functions. No
security-relevant underscore `lstrip` remains in the risk authority verifier.

Real-fixture regressions cover harmless `function(__message)` definitions in
both static and configured trace paths. Exact `_message` and `_include` aliases
remain rejected in both paths, while the earlier `_target_link_libraries` and
`__target_link_libraries` regressions preserve post-guard mutation behavior.

The initial focused RED run produced the expected result:

```text
2 failed, 1 passed in 1.55s
```

Both harmless `__message` tests failed with command-interception violations;
the exact single-underscore rejection test already passed.

### Fix Verification

Executed from the task worktree:

```text
five focused critical-command and mutation-alias regressions
5 passed in 2.64s

uv run pytest tests/python/test_m6_risk_authority_boundaries.py -q
78 passed in 24.28s

uv run pytest tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
165 passed in 37.41s

python3 tools/development/verify_m6_authority_boundaries.py
[OK] M6 portfolio authority has no forbidden dependencies

python3 tools/development/verify_m6_risk_authority_boundaries.py
[OK] M6 risk authority has no forbidden dependencies

uv run ruff check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!

uv run ruff format --check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
2 files already formatted

git diff --check
passed
```

Implementation commit: `5947415`.

Fix-round concerns: none.

## Final Whole-Branch Fix

Final evaluation identified three owner-scope pre-guard capabilities that were
not represented by the existing target property checks. Real temporary
fixtures demonstrated that directory compile definitions, target custom
commands, and header source properties all configured without a violation.

The initial focused RED run produced:

```text
6 failed in 2.92s
```

The native guard now requires empty `COMPILE_DEFINITIONS`, `COMPILE_OPTIONS`,
and `INCLUDE_DIRECTORIES` properties on the risk owner directory. These are the
bounded directory capabilities that can be inherited by `chronos_risk`; link
capabilities remain covered by the existing target and directory-link checks.

Expanded trace analysis now rejects target-form `add_custom_command` calls
against `chronos_risk` across the complete configure trace. Coverage includes a
direct `POST_BUILD` command, a callable wrapper with an expanded target, and a
dynamically dispatched exact `_add_custom_command` built-in alias. Output-form
custom commands and commands for unrelated targets remain outside this check.

The native source-property loop now covers both canonical sources. A direct
CMake probe established these legitimate baselines:

```text
src/risk_decision.cpp: LANGUAGE=CXX, GENERATED=0
src/risk_arithmetic.hpp: LANGUAGE="", GENERATED=0
```

All guarded source capability properties must remain unset for both files.
Regressions cover header `LANGUAGE CXX` and header `COMPILE_OPTIONS` mutations.
The Python guard-integrity grammar requires the exact directory property list,
canonical source list, baseline comparison, generated check, and nested source
capability loop.

Task 5 documentation Steps 1 and 2 were marked complete in the implementation
plan. Review, final gates, evidence/report, and publish steps remain pending.

### Fix Verification

Executed from the task worktree:

```text
six focused pre-guard fixture groups after final formatting
6 passed in 3.14s

uv run pytest tests/python/test_m6_risk_authority_boundaries.py -q
84 passed in 27.57s

uv run pytest tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
171 passed in 40.53s

python3 tools/development/verify_m6_authority_boundaries.py
[OK] M6 portfolio authority has no forbidden dependencies

python3 tools/development/verify_m6_risk_authority_boundaries.py
[OK] M6 risk authority has no forbidden dependencies

cmake -S . -B build -G Ninja
Configuring done; Generating done

cmake --build build
[OK] strategy capability boundary checked (0 source files)

uv run ruff check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!

uv run ruff format --check tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
2 files already formatted

git diff --check
passed
```

Implementation commit: `4624f01`.

Fix-round concerns: none.

## Final Structural Fix

Fix-round escalation identified a remaining owner-scope capability: direct
mutation of `CMAKE_CXX_FLAGS` configured successfully, changed the
`chronos_risk` compile command, and returned no structural violations. The
reproduction used exactly:

```cmake
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -DCHRONOS_REVIEW_BYPASS -I/unexpected")
```

The regression independently reads `compile_commands.json` and proves both
`-DCHRONOS_REVIEW_BYPASS` and `-I/unexpected` reached
`core/risk/src/risk_decision.cpp` before asserting the verifier rejection.

Expanded trace analysis now treats project-authored `set`/`unset`, mutating
`list` operations, and in-place/output `string` operations as CMake variable
mutations. From the first executed risk-owner command onward, direct and
configuration-specific `CMAKE_CXX_FLAGS` variables are forbidden in repository
trace sources both before and after the risk sentinel. The check classifies
executed built-ins, so callable bodies, `cmake_language(EVAL)`, dynamic argument
expansion, exact built-in aliases, and deferred calls use the same path.

The verifier does not compare against assumed initial flag values. Project
toolchain/profile initialization before risk ownership remains the ambient
baseline, preserving the declared Benchmark profile, and commands traced to
CMake/toolchain/module sources outside the repository are excluded.

The corrected RED run produced:

```text
4 failed, 84 deselected in 2.17s
```

All four groups failed because the old verifier returned no CMake-variable
violation. Together they cover seven fixtures: exact direct owner mutation,
Debug configuration mutation, `string(APPEND)`, `list(APPEND)`, callable,
`cmake_language(EVAL)`, and deferred post-guard execution.

### Fix Verification

Executed from the final formatted implementation state:

```text
focused CXX-flags regressions
4 passed, 84 deselected in 3.68s

uv run --locked --group dev pytest \
  tests/python/test_m6_risk_authority_boundaries.py -q
88 passed in 39.44s

uv run --locked --group dev pytest \
  tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
175 passed in 52.52s

python3 tools/development/verify_m6_authority_boundaries.py
[OK] M6 portfolio authority has no forbidden dependencies

python3 tools/development/verify_m6_risk_authority_boundaries.py
[OK] M6 risk authority has no forbidden dependencies

cmake -S . -B build -G Ninja
Configuring done; Generating done

cmake --build build
[OK] strategy capability boundary checked (0 source files)

uv run --locked --group dev ruff check \
  tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!

uv run --locked --group dev ruff format --check \
  tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
2 files already formatted

git diff --check
passed
```

Implementation commit: `55c49ae`.

Fix-round concerns: none.

## Final Structural Verifier Fix Round

Final verifier review found that command-specific variable-position tables
still allowed output-producing CMake forms to mutate the risk compile command.
The confirmed bypasses included:

```cmake
string(REPLACE X "-DCHRONOS_STRING_REPLACE_BYPASS" CMAKE_CXX_FLAGS X)
list(JOIN _risk_flags " " CMAKE_CXX_FLAGS)
```

Regressions also cover `string(REGEX REPLACE ...)` and
`string(CONFIGURE ...)`, exercising protected output-variable positions 4 and
2 in the zero-based JSON argument array, in addition to the plain `REPLACE`
and `list(JOIN)` position 3. Every case configures a real fixture, reads
`compile_commands.json`, proves its unique
`-DCHRONOS_*_BYPASS` flag reached `core/risk/src/risk_decision.cpp`, and only
then asserts verifier rejection.

Expanded JSON trace represents `${CMAKE_CXX_FLAGS}` reads as the expanded flag
value, while variable names used as output arguments remain literal
`CMAKE_CXX_FLAGS` arguments. The verifier therefore no longer classifies
commands or output positions. From the first executed risk-owner command, it
rejects any exact protected CXX-flags variable argument retained by an
executed repository-owned trace entry. This covers every `string`/`list`
output position and future output-producing commands without another command
table.

A benign read/copy fixture confirms expanded `${CMAKE_CXX_FLAGS}` values remain
allowed. Separate real fixtures prove CMake commands sourced from an external
module and an external toolchain can change the compile command without being
classified as repository-owned. CMake system-module traces remain excluded by
the same resolved source-ownership check. Repository build-profile setup that
executes before risk ownership remains the accepted ambient baseline.

The focused RED run against the previous verifier produced:

```text
4 failed, 3 passed, 88 deselected in 5.73s
```

All four output forms changed the compile command but returned no configured
CMake-variable violation. The benign read/copy, external-module, and external-
toolchain controls passed during RED.

### Fix Verification

Executed from the final formatted implementation state:

```text
focused CXX-flags invariant regressions
11 passed, 84 deselected in 8.75s

uv run --locked --group dev pytest \
  tests/python/test_m6_risk_authority_boundaries.py -q
95 passed in 44.61s

uv run --locked --group dev pytest \
  tests/python/test_m5_authority_boundaries.py \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py -q
182 passed in 57.20s

python3 tools/development/verify_m6_authority_boundaries.py
[OK] M6 portfolio authority has no forbidden dependencies

python3 tools/development/verify_m6_risk_authority_boundaries.py
[OK] M6 risk authority has no forbidden dependencies

cmake -S . -B build -G Ninja
Configuring done; Generating done

cmake --build build
[OK] strategy capability boundary checked (0 source files)

uv run --locked --group dev ruff check \
  tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!

uv run --locked --group dev ruff format --check \
  tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
2 files already formatted

git diff --check
passed
```

Implementation commit: `f9dfae7`.

Fix-round concern: the requested literal-name invariant is intentionally
conservative. A repository command executed after risk ownership that retains
`CMAKE_CXX_FLAGS` as a literal argument for introspection will be rejected;
ordinary `${CMAKE_CXX_FLAGS}` reads and copies are expanded and remain allowed.
