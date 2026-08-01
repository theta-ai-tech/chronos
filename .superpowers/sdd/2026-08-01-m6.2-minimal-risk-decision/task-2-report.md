# M6.2 Task 2 Report

## Status

DONE_WITH_CONCERNS

## Commits

- `07476b9 feat: add minimal risk decision authority (#35)`
- `4833e09 fix: preserve risk proof rejection reasons (#35)`

## Changed Files

- `core/CMakeLists.txt`
- `core/risk/CMakeLists.txt`
- `core/risk/src/risk_decision.cpp`
- `tests/CMakeLists.txt`
- `tests/support/portfolio_runtime_fixture.hpp`
- `tests/unit/feature_runtime_test.cpp`
- `tests/unit/risk_decision_test.cpp`
- `.superpowers/sdd/2026-08-01-m6.2-minimal-risk-decision/task-2-report.md`

The support header and narrow function in `feature_runtime_test.cpp` expose one
authority-produced actionable recommendation. `risk_decision_test.cpp` passes
that recommendation through `PortfolioConstructionAuthority` itself and
asserts that the returned real `TargetPosition` has lineage current position
`10` and target `40`. Fix round 1 replaces the old post-construction payload
mutation with an explicit maximum-exposure authority input, so the actionable
payload and deterministic recommendation ID are derived from the same policy.

## Fix Round 1 RED Evidence

The proof-failure and simultaneous-trigger assertions were added before the
implementation was changed.

Executed:

```sh
cmake --build build
build/tests/chronos_unit_tests
```

Result: the build exited 0; the direct suite exited 1 with
`RESULT FAIL: 234 case(s), 4 failed check(s)`. The two new proof-failure cases
failed because both returned `ModifiedTargetWouldNotChangeExposure` as the
binding reason. The strengthened simultaneous-trigger case already passed with
all five findings in the required order.

The fixture API was then changed to accept the desired authority exposure and
an identity-consistency case was added before changing its implementation.

Executed:

```sh
cmake --build build
```

Result: exit 1 at link time because the new
`positive_portfolio_recommendation(AmountUnits)` fixture declaration had no
definition. This was the expected fixture RED condition.

An initial GREEN attempt exposed that using the small maximum as the minimum
for every call changed the established default strategy threshold. The direct
suite exited 139. The root cause was corrected by preserving the default
minimum as `min(300000, maximum_indicative_exposure)`, which is `300000` for
existing tests and `40` or `41` only for the explicit small-exposure fixture.

## Fix Round 1 GREEN Evidence

Executed after the correction:

```sh
uv run clang-format -i \
  core/risk/src/risk_decision.cpp \
  tests/support/portfolio_runtime_fixture.hpp \
  tests/unit/feature_runtime_test.cpp \
  tests/unit/risk_decision_test.cpp
cmake --build build
set -o pipefail; build/tests/chronos_unit_tests 2>&1 | tail -n 1
ctest --test-dir build --output-on-failure
uv run clang-format --dry-run --Werror \
  core/risk/src/risk_decision.cpp \
  tests/support/portfolio_runtime_fixture.hpp \
  tests/unit/feature_runtime_test.cpp \
  tests/unit/risk_decision_test.cpp
git diff --check
```

Result: exit 0 throughout. The direct suite reported
`RESULT OK: 235 case(s), 0 failed check(s)`; CTest passed 1/1; the format and
diff checks produced no output.

## RED Evidence

Tests were written before `risk_decision.cpp` or the `chronos_risk` target.

Executed:

```sh
cmake -S . -B build -G Ninja
```

Result: exit 0; configuration and generation completed.

The first `cmake --build build` exposed a test-only harness error: the shared
rejection assertion used `CHECK` without receiving microtest's per-case
`mt_fail_count`. The helper was corrected to receive that counter, then the RED
build was repeated.

Executed:

```sh
cmake --build build
```

Result: exit 1 at link time, with the expected sole product failure:

```text
Undefined symbols for architecture arm64:
  chronos::core::risk::MinimalRiskAuthority::evaluate(...)
ld: symbol(s) not found for architecture arm64
```

The new runtime tests compiled before this failure. The missing evaluator
definition was therefore the observed RED condition.

Executed:

```sh
ctest --test-dir build --output-on-failure
```

Result: exit 8; 0/1 tests ran because the preceding expected link failure had
not produced `build/tests/chronos_unit_tests`.

## GREEN Evidence

After adding the implementation and CMake wiring, executed:

```sh
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure
```

Result: both commands exited 0; `chronos_risk` and `chronos_unit_tests` linked,
and 1/1 CTest test passed with 0 failures.

After self-review fixes for mode-admission ordering and outcome-domain
separation, executed the final verification:

```sh
uv run clang-format -i core/risk/src/risk_decision.cpp
cmake --build build
ctest --test-dir build --output-on-failure
uv run clang-format --dry-run --Werror \
  core/risk/src/risk_decision.cpp \
  tests/unit/risk_decision_test.cpp \
  tests/unit/feature_runtime_test.cpp \
  tests/support/portfolio_runtime_fixture.hpp
git diff --check
```

Result: exit 0 throughout; 1/1 CTest test passed with 0 failures, formatting
was clean, and no whitespace errors were reported.

Also executed:

```sh
build/tests/chronos_unit_tests
```

Result: exit 0; `RESULT OK: 232 case(s), 0 failed check(s)`. This included the
seven Task 2 risk cases: contract shape, approval, individual semantic
rejections, simultaneous-rule ordering, positive modification, negative
modification, and modification-disabled rejection.

## Arithmetic Decisions

- Approval uses the authoritative account snapshot rather than M6.1 lineage:
  `requested_delta = 40 - 10 = 30`, then
  `requested_projected = 15 + 30 = 45`. The authorized target, delta, and
  projected exposure are `40`, `30`, and `45`.
- Approval expiry is `min(120 + 50, 150) = 150`, so the risk decision cannot
  outlive the source target.
- Positive modification uses projected exposure before target `90`:
  requested exposure is `90 + 30 = 120`; clamping gives `100`; solving gives
  authorized delta `100 - 90 = 10` and authorized target `10 + 10 = 20`.
  Absolute projected exposure falls `120 -> 100`, and absolute movement falls
  `30 -> 10`.
- Negative modification uses account position `100` and projected exposure
  before target `-70`: requested delta is `40 - 100 = -60`; requested exposure
  is `-70 + -60 = -130`; clamping gives `-100`; solving gives authorized delta
  `-100 - -70 = -30` and authorized target `100 + -30 = 70`. Absolute projected
  exposure falls `130 -> 100`, and absolute movement falls `60 -> 30` while
  retaining the requested negative direction.
- Checked add, subtract, and absolute operations reject overflow and
  `INT64_MIN`. Expiry addition and modification solving use the same checked
  operations. Arithmetic inability returns unavailable rather than a semantic
  authorization.
- Modification is accepted only when projected exposure is strictly reduced
  to at most the limit, movement does not increase, movement is sign-preserving
  or zero, and the solved target changes the authoritative account position.
- Fix round 1 zero-movement proof failure starts from account `10`, target `40`,
  projected-before `100`, and limit `100`: requested delta is `30`, requested
  projected exposure is `130`, and the clamp solves delta `0`. The rejection
  retains `ProjectedExposureLimitExceeded` first and appends the truthful
  `ModifiedTargetWouldNotChangeExposure` finding.
- Fix round 1 movement-increase proof failure starts from account `10`, target
  `40`, projected-before `-150`, and limit `100`: requested delta is `30`,
  requested projected exposure is `-120`, and the clamp solves movement `50`.
  The rejection retains only the binding
  `ProjectedExposureLimitExceeded` finding because the modified target would
  change exposure and no other existing reason is truthful.
- Semantic findings are collected in fixed order: kill switch, account
  disabled, market non-tradeable, target already satisfied, then exposure
  limit. The first finding is the binding reason.

## Identity Decisions

- Canonical integers use fixed-width big-endian bytes; enums use their fixed
  underlying values; IDs use their 16 canonical bytes; versions encode
  definition ID then version number; and optionals always include an explicit
  presence byte.
- Target lineage, policy, cut, admission facts, run context, activation,
  account, market, projected exposure, and kill-switch snapshots are encoded in
  fixed field order. Decision encodings additionally include all computed
  amounts, authorized optionals, findings, proof, and expiry.
- The five domains have separate roles:
  `chronos.risk-obligation.v1`, `chronos.risk-decision.v1`,
  `chronos.risk-obligation-unavailable.v1`,
  `chronos.risk-admission-rejected.v1`, and
  `chronos.risk-evaluation-outcome.v1`. Every terminal semantic encoding is
  wrapped by the outcome domain before projection to `RiskEvaluationOutcomeId`.
- Obligation identity is derived after target admission and binds the target,
  policy, successful publication fact, and cut. Decision identity binds the
  obligation plus complete semantic inputs and result fields.
- The evaluator copies the projected-exposure snapshot's `risk_sequence`
  unchanged. No input or sequence is mutated.
- The shared positive recommendation fixture now passes its requested maximum
  exposure through `StrategyRuntimeConfig` and `RecommendationAuthority`.
  Repeated exposure `40` inputs produce the same ID; exposure `40` and `41`
  produce different payloads and IDs. The fixture contains no mutation,
  cached object, or cross-translation-unit mutable state.

## CMake Wiring

- Added static library `chronos_risk` with public risk includes.
- Linked `chronos_contracts` and `chronos_portfolio` publicly, and
  `chronos_options` and `chronos_warnings` privately.
- Registered risk after portfolio and linked `chronos_risk` into
  `chronos_unit_tests`.

## Self-Review

- Reviewed the staged seven-file implementation diff against the Task 2 brief,
  approved spec, and Task 1 public contract.
- Corrected mode admission to run after validated run context and before policy
  activation/safety evidence, matching the approved deterministic order.
- Corrected admission and unavailable terminal IDs to pass through the common
  outcome domain rather than projecting their terminal domains directly.
- Confirmed approved and modified decisions alone retain authorized values;
  every semantic rejection has a decision ID and no authorized target or proof.
- Confirmed over-limit proof failures preserve the original exposure-limit
  decision and finding; no-change is appended only for an actual zero-movement
  clamp, while movement-increase is not mislabeled.
- Confirmed simultaneous safety evidence is exactly kill switch, account,
  market, already-satisfied, and exposure-limit order.
- Confirmed the risk fixture constructs its recommendation through runtime and
  recommendation authorities with one explicit policy value governing both
  canonical payload and identity; it no longer calls the corruption helper.
- Confirmed no reservation, executable output, `risk_sequence` mutation,
  Python boundary gate, or exhaustive Task 3 test matrix was added.

## Concerns

- Apple `ld` emits a non-failing duplicate-library warning for transitive
  portfolio/recommendation/strategy libraries after adding the exact required
  public `chronos_risk` links alongside the unit target's existing direct
  links. Build and tests exit 0; changing the established test link graph is
  outside Task 2.
- The exhaustive admission/unavailable/identity/arithmetic matrix remains Task
  3 by explicit scope. Task 2 proves valid complete evidence and the requested
  semantic approval, rejection, and modification behavior only.
