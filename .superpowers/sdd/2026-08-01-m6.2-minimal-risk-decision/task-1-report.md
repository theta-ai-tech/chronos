# M6.2 Task 1 Report

## Status

DONE_WITH_CONCERNS

## Commits

- `ec7e587 feat: define M6.2 risk decision contract (#35)`

## Changed Files

- `contracts/include/chronos/contracts/value_objects.hpp`
- `core/risk/include/chronos/core/risk/risk_decision.hpp`
- `tests/unit/value_objects_test.cpp`
- `tests/unit/risk_decision_test.cpp`
- `tests/CMakeLists.txt`

## TDD Evidence

1. Added opaque-ID compile-time and UUID round-trip checks before defining the
   aliases.
2. Ran:

   ```sh
   cmake -S . -B build -G Ninja
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

   Result: expected build failure in `tests/unit/value_objects_test.cpp` for
   missing `RiskScopeId`, `RiskObligationId`, `RiskEvaluationOutcomeId`, and
   `RiskDecisionId`.
3. Added the opaque aliases, declaration-only risk contract, and contract-shape
   tests. The first header build exposed only a non-default-constructible
   `RiskEvaluationCut` member initializer; removing the invalid defaults made
   authority-only construction explicit.
4. Added a second contract-shape assertion for policy target-schema retention.
   It failed as expected before the decision and unavailable terminals retained
   that version, then passed after the contract was extended.

## Final Verification

Executed from the shared M6.2 worktree:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Result: exit 0; `chronos_unit_tests` passed; 1/1 CTest test passed with 0
failures.

```sh
uv run clang-format --dry-run --Werror \
  contracts/include/chronos/contracts/value_objects.hpp \
  core/risk/include/chronos/core/risk/risk_decision.hpp \
  tests/unit/value_objects_test.cpp \
  tests/unit/risk_decision_test.cpp
git diff --check
```

Result: exit 0; no formatting or whitespace errors.

## Contract Decisions

- Added distinct opaque tags for `RiskScopeId`, `RiskObligationId`,
  `RiskDecisionId`, `RiskEvaluationOutcomeId`, and `ProjectedExposureId`, while
  retaining the existing total SHA-256 projection behavior.
- Made `risk_decision.hpp` a declaration-only public contract. It introduces no
  risk CMake target, evaluator definition, source file, reservation behavior,
  execution behavior, or `risk_sequence` mutation.
- Represented the three terminal categories as distinct immutable types in
  `RiskEvaluationTerminal`. `RiskDecision` alone has a decision ID;
  `RiskAdmissionRejected` has neither a decision nor obligation ID;
  unavailable outcomes retain an obligation but no decision ID.
- Made terminal constructors private to `MinimalRiskAuthority`. All inputs have
  value accessors and defaulted equality. Evidence and authorized/modification
  fields use `std::optional` only where absence is semantically valid.
- Defined closed admission, unavailable, and semantic reason enums plus
  `is_valid` overloads. `RiskDecision::authorizes_target()` is true only for
  approved or modified dispositions, and every terminal's `executable()` is
  false.
- Retained policy versions, target-schema version, immutable cut data, and the
  evidence identities required for semantic decisions, unavailable outcomes,
  and admission rejections.

## Self-Review

- Reviewed the staged diff against Task 1 and the approved design.
- Confirmed the five requested production/test/CMake files are the only code
  changes in `ec7e587`.
- Confirmed no Task 2 risk library registration, implementation source,
  `MinimalRiskAuthority::evaluate` definition, reservation path, or executable
  output was introduced.

## Concerns

- `risk_decision_test.cpp` validates the evaluator signature with a real
  `portfolio::TargetPosition` type via `std::declval`, rather than constructing
  a target through `PortfolioConstructionAuthority`. The existing real-target
  fixture is TU-local in `feature_runtime_test.cpp`; duplicating its complete
  market-to-recommendation path in this declaration-only slice would expand
  Task 1 into M6.1 integration-fixture work. Task 2 must add its required real
  target fixture when it links the risk authority and executes behavior.
