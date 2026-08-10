# M6.2 Task 3 Report

## Status

DONE_WITH_CONCERNS

## Commits

- `a9fe6aa test: prove M6.2 fail-closed risk semantics (#35)`

## Changed Files

- `core/risk/src/risk_decision.cpp`
- `tests/unit/risk_decision_test.cpp`
- `.superpowers/sdd/2026-08-01-m6.2-minimal-risk-decision/task-3-report.md`

The implementation commit changes only the evaluator and its C++ unit test.
This report is committed separately.

## Baseline Evidence

The worktree was clean on `feat/m6.2-minimal-risk-decision` at `0603dd7`.

Executed before edits:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
build/tests/chronos_unit_tests
```

Result: the build exited 0, CTest passed 1/1, and the direct suite reported
`RESULT OK: 235 case(s), 0 failed check(s)`.

## RED Evidence

The first admission/run matrix run reported:

```text
RESULT FAIL: 237 case(s), 2 failed check(s)
```

Both failures were test-fixture ordering errors. The two deliberately older
evaluation cuts still had publication, acknowledgement, and lifecycle facts
newer than the cut, so `FutureTargetPublication` correctly took precedence
over `FutureEvaluationCut`. The fixture facts were moved to the selected cut,
after which those cases exercised the intended target-cut branch.

After the arithmetic boundary tests were added, executed:

```sh
cmake --build build
build/tests/chronos_unit_tests
```

Result: the build exited 0 and the direct suite exited 1 with
`RESULT FAIL: 245 case(s), 2 failed check(s)`. Both failed checks came from the
repeated projected-exposure `INT64_MIN` case: the evaluator returned a semantic
rejection rather than `ArithmeticUnrepresentable`. Adding the requested delta
had moved the projected value away from `INT64_MIN`, so the old implementation
never tested the raw projected amount's unrepresentable absolute value.

## GREEN Evidence

The minimal arithmetic fix validates the raw requested target, authoritative
account position, and projected-exposure-before-target amounts before any
calculation. Re-running the focused suite produced:

```text
RESULT OK: 245 case(s), 0 failed check(s)
```

After identity coverage and the explicit phase refactor, the final verification
was:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
uv run clang-format --dry-run --Werror \
  core/risk/include/chronos/core/risk/risk_decision.hpp \
  core/risk/src/risk_decision.cpp tests/unit/risk_decision_test.cpp
git diff --check
build/tests/chronos_unit_tests
```

Result: every command exited 0. CTest passed 1/1 with 0 failures, format and
diff checks produced no output, and the direct suite reported
`RESULT OK: 248 case(s), 0 failed check(s)`. Task 3 adds 13 unit-test cases to
the 235-case baseline.

## Coverage Matrix

| Area | Coverage |
| --- | --- |
| Admission | 29 variants: missing, negative, invalid, future-sequence, future-time, and mismatched publication/acknowledgement/lifecycle facts; superseded/invalidated lifecycle; target key/run/scale/schema mismatch; future cut by each coordinate; expiry; validated capture and live-read-only modes |
| Run context | Missing; all 5 non-valid quality states; wrong run and epoch; future sequence/time; invalid mode; invalid replay class; supported-mode mismatch |
| Policy activation | Missing; all 5 non-valid quality states; wrong run/scope/epoch; each of 5 version mismatches; future sequence/time |
| Mode precedence | Stale live-read-only context is unavailable; quality-valid live-read-only context is admission rejected; run context precedes activation; activation precedes account |
| Account snapshot | 13 unavailable variants plus exact maximum age: missing, all 5 non-valid qualities, 3 scope fields, scale, future sequence/time, one-nanosecond stale |
| Market snapshot | 12 unavailable variants plus exact maximum age: missing, all 5 non-valid qualities, 3 scope fields, future sequence/time, one-nanosecond stale |
| Projected snapshot | 12 unavailable variants plus exact maximum age: missing, all 5 non-valid qualities, risk-scope/target-key mismatch, scale, future sequence/time, one-nanosecond stale |
| Kill-switch snapshot | 10 unavailable variants plus exact maximum age: missing, all 5 non-valid qualities, scope, future sequence/time, one-nanosecond stale |
| Snapshot precedence | Account, market, projected exposure, then kill switch when multiple inputs are absent |
| Invalid policy | Invalid mode/dimension set, negative limit, each negative maximum age, and zero/negative decision duration; all return failure without a domain terminal |
| Arithmetic | Requested subtraction overflow, projected addition overflow, raw account/target/projected `INT64_MIN`, expiry addition overflow, all 4 age-subtraction overflows, and exact positive/negative exposure limits |
| Proof boundaries | Existing reviewed cases retain over-limit binding for a solved movement increase and retain exposure-limit then no-movement findings for a zero solved delta |
| Identity | 52 one-at-a-time valid semantic mutations plus a changed target; obligation ownership sensitivity/invariance; decision/outcome sensitivity; repeated objects at different addresses; intervening call order; five all-zero SHA-256 risk-ID projections |

Every admission result asserts its exact reason, deterministic nonzero outcome
identity, non-executable state, and absence of obligation, decision, and
authorized-target APIs. Unavailable results assert exact reasons,
deterministic nonzero obligation/outcome identities, and no decision or
authorized-target API.

## Validation Order

The evaluator now exposes one named function for each phase:

1. `valid_policy`
2. `admission_rejection`
3. `run_context_unavailable`
4. `policy_activation_unavailable`
5. `account_unavailable`
6. `market_unavailable`
7. `projected_exposure_unavailable`
8. `kill_switch_unavailable`
9. `evaluate_arithmetic`
10. `semantic_findings`

`MinimalRiskAuthority::evaluate` calls these in that order. Existing closed
reason enums remain unchanged; no generic invalid-input fallback was added.

## Arithmetic Decisions

- `INT64_MIN` is rejected before arithmetic when present as requested target,
  authoritative account position, or projected exposure before target. This
  prevents later arithmetic from moving the value away from the
  unrepresentable absolute boundary.
- Requested delta, requested projected exposure, evidence age, decision
  expiry, clamp solving, and proof absolutes remain checked signed arithmetic.
  Any inability to represent them returns
  `RiskObligationUnavailableReason::ArithmeticUnrepresentable`.
- Evidence age exactly at the configured maximum remains valid; one
  nanosecond beyond it is stale for every snapshot family.
- Requested projected exposure exactly `+limit` or `-limit` approves. Only a
  strict excess enters modification/rejection logic.
- The reviewed proof-failure behavior remains unchanged: a solved target that
  increases movement is rejected with the exposure-limit reason, and a solved
  zero movement is rejected with exposure-limit binding plus the specific
  no-change finding.

## Identity Decisions

- The obligation ID changes for target, policy, cut, and successful
  publication semantics. It intentionally remains invariant under
  acknowledgement, lifecycle, run, activation, and safety evidence changes.
- The decision and terminal outcome IDs change for every covered semantic
  mutation, including evidence IDs, sequences, times, amounts, safety states,
  policy versions/limits/ages, and admission facts.
- Repeated evaluation of separately allocated but equal objects returns equal
  complete decisions and identical obligation, decision, and outcome IDs.
  An intervening evaluation does not affect those values.
- No unordered storage, host time, object address, or call order enters the
  canonical identity inputs.
- The shared `OpaqueId::from_sha256_digest` helper maps an all-zero digest to
  the stable nonzero fallback for `RiskScopeId`, `RiskObligationId`,
  `RiskDecisionId`, `RiskEvaluationOutcomeId`, and `ProjectedExposureId`.

## Self-Review

- Reviewed the implementation diff against the Task 3 brief, approved design,
  and the Task 2 report.
- Corrected two evaluation-cut fixtures that accidentally triggered the
  earlier publication phase.
- Corrected four test loops that retained pointers into temporary evaluation
  results; each loop now owns the result for the full pointer lifetime.
- Confirmed unavailable precedence remains run context, policy activation,
  account, market, projected exposure, then kill switch.
- Confirmed valid forbidden modes are checked only after complete run-context
  validation and before policy activation.
- Confirmed Task 4 CMake/Python gates and Task 5 documentation were not added.

## Concerns

- Apple `ld` continues to emit the pre-existing non-failing duplicate-library
  warning for transitive portfolio/recommendation/strategy libraries. Build,
  CTest, and the direct suite all exit 0; changing that link graph is outside
  Task 3.
- Reaching the otherwise immutable `INT64_MIN` target boundary requires a
  test-only recommendation-payload mutation before invoking the real portfolio
  authority. The resulting `TargetPosition`, including its target identity and
  arithmetic, is still produced by the production authority; no production
  constructor or test-only risk API was added.
