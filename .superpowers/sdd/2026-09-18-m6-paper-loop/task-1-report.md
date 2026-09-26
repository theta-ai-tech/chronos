# Task 1 report: M6.3 single-writer reservation

## Result

Implemented `chronos_reservation` as the risk-owned, single-writer capacity
authority for the D0 `RunMode::backtest` paper replay path. The authority binds
genuine immutable `RiskDecision` facts to full run/portfolio/account/instrument/
listing scope, policy versions, exposure scale, source snapshot identity and
risk sequence before changing capacity.

The authority now:

- publishes deterministic `ProjectedExposureSnapshot` facts;
- accepts one unsettled hold per listing and rejects an opposing concurrent
  hold, so signed deltas cannot net away worst-case exposure;
- enforces an independently configured absolute reservation cap and includes
  that cap in snapshot identity;
- caches request outcomes for exact retry, rejects changed request reuse, and
  prevents a risk decision from being reused under a new request after release;
- binds consumption to one intent without freeing capacity;
- permits release only before consumption;
- replaces held exposure with ledger-derived position only after complete,
  valid, monotonically advancing `SettledExposureEvidence` passes all scope,
  linkage, quality, cursor, checksum, scale and cap checks;
- leaves state unchanged on rejected cap, decision, expiry, scope, scale and
  settlement paths.

`RiskDecision` exposes and stores `run_id` and `exposure_scale` so reservation
can validate those immutable decision bindings directly. Existing
`risk_sequence` behavior is unchanged and is asserted through the genuine risk
fixture.

## Public API handoff

Header: `core/risk/include/chronos/core/risk/reservation.hpp`

Construction and operations:

```cpp
ReservationAuthority(ReservationPolicy policy, AmountUnits initial_position);
ProjectedExposureSnapshot snapshot(const RiskEvaluationCut &cut) const;
ReservationResult reserve(ReservationRequestId request_id,
                          const RiskDecision &decision,
                          const RiskEvaluationCut &cut);
ReservationResult consume(ReservationId reservation_id,
                          ExecutableOrderIntentId intent_id,
                          const RiskEvaluationCut &cut);
ReservationResult release(ReservationId reservation_id,
                          const RiskEvaluationCut &cut);
ReservationResult reconcile(ReservationId reservation_id,
                            const SettledExposureEvidence &evidence);
std::optional<Reservation> find(ReservationId reservation_id) const;
```

`ReservationPolicy` carries the bound `MinimalRiskPolicy`, quote currency,
ledger stream/epoch and the reservation authority's independent absolute
capacity cap. `ReservationResult` reports `Accepted`, `Rejected` or `Stale`
with a typed `ReservationFailure`; accepted results contain an immutable copy
of the reservation fact and its current `Held`, `Consumed`, `Released` or
`Settled` lifecycle state.

Task 2 can use `tests/support/paper_risk_fixture.hpp` to create genuine target
and risk facts, then follow the sample in `tests/unit/reservation_test.cpp`:
take `snapshot(cut)`, evaluate risk against that exact snapshot, call
`reserve`, and call `consume` only after all intent fields have been
prevalidated. A consumed reservation remains capacity-bearing until
`reconcile` accepts ledger evidence.

The neutral accounting handoff is
`contracts/include/chronos/contracts/accounting.hpp`. New opaque IDs are in
`contracts/include/chronos/contracts/value_objects.hpp`.

## Boundary protection

The native risk guard accepts exactly one reservation consumer target with the
single source `src/reservation.cpp` and exact links
`chronos_risk;chronos_contracts;chronos_options;chronos_warnings`. The Python
verifier recognizes that exact block and source while retaining the original
canonical `chronos_risk` source and dependency rules. The shared accounting DTO
is the only new allowed contracts include. Existing negative guard tests still
exercise forbidden includes, links, source injection, property mutation,
deferred mutation and guard bypasses.

## Verification

Fresh checks after formatting:

```text
cmake -S . -B build
cmake --build build --target chronos_unit_tests -j4
./build/tests/chronos_unit_tests
RESULT OK: 256 case(s), 0 failed check(s)

uv run --locked --group dev pytest -q \
  tests/python/test_m6_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
165 passed in 61.66s

uv run --locked --group dev clang-format --dry-run --Werror \
  contracts/include/chronos/contracts/accounting.hpp \
  contracts/include/chronos/contracts/value_objects.hpp \
  core/risk/include/chronos/core/risk/reservation.hpp \
  core/risk/include/chronos/core/risk/risk_decision.hpp \
  core/risk/src/reservation.cpp tests/support/paper_risk_fixture.hpp \
  tests/unit/reservation_test.cpp
exit 0

uv run --locked --group dev ruff check \
  tools/development/verify_m6_risk_authority_boundaries.py \
  tests/python/test_m6_risk_authority_boundaries.py
All checks passed!
```

The reservation implementation retains checked-add/subtract and sequence-wrap
guards. With a genuine `RiskDecision`, numeric overflow at reservation or
settlement is unreachable earlier than risk admission: the private risk mint
has already checked target/delta arithmetic, and reservation requires the exact
same position, projected snapshot, delta and target. Behavioral tests therefore
exercise the reachable boundary failures and verify no state change rather than
forging a private decision to force the defensive overflow branch.

## Post-review allocation atomicity fix

Review identified that successful transitions still copied nested
`RiskDecision` findings after mutation. An allocation failure in the successful
`reserve` return or request-cache copy could therefore leave held capacity
without the cached successful result.

The corrected commit protocol stages the complete `ReservationResult`,
`Request`, `Record`, lifecycle fact and settlement evidence before changing
authority state. `reserve` also reserves both outer containers before staging.
After staging succeeds, the commit uses only moves whose `noexcept` properties
are enforced with `static_assert`, scalar assignments, and a statically checked
no-throw cut assignment. `consume`, `release`, and `reconcile` use the same
pattern, so an exception cannot escape after an unreported successful
transition.

A runtime allocation-failure test would require replacing global allocation or
adding a general fault-injection framework. The compile-time move assertions
prove the narrow commit property without introducing that infrastructure.

Fresh post-review verification:

```text
cmake --build build --target chronos_unit_tests -j4
./build/tests/chronos_unit_tests
RESULT OK: 256 case(s), 0 failed check(s)

uv run --locked --group dev clang-format --dry-run --Werror \
  core/risk/src/reservation.cpp
exit 0
```
