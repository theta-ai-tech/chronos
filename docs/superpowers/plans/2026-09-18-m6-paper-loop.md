# Chronos M6 completion implementation plan

**Goal:** Complete D0 M6.3–M6.7 in /Users/ignatioscharalampidis/Documents/theta/dev/chronos/.claude/worktrees/codex-m6-completion, branch codex/m6-paper-loop.
**Architecture:** See docs/superpowers/specs/2026-09-18-m6-paper-loop-design.md. Implement sequentially with focused tests and review at each issue; no source work belongs in the old M6.2 worktree. User already authorized implementation; do not pause for design permission.

## Global Constraints

- D0: one listing, one portfolio/account, BacktestPaperReplay only.
- C++20 and exact fixed-point arithmetic; no wall time in decisions.
- Source → recommendation → target → risk → reservation → intent → fill → ledger; no bypass.
- Domain authorities stay distinct; core never depends on adapter or accounting implementations.
- Use test-first implementation and focused review per task; full make m0-check and make cpp-profiles-check before final merge.

### Task 1: M6.3 single-writer reservation

- [ ] Add core/risk/include/chronos/core/risk/reservation.hpp and src/reservation.cpp; create chronos_reservation as a separate target within core/risk/CMakeLists.txt, linking chronos_risk and contracts. Keep existing risk authority mint restrictions; extend root/risk CMake and Python guards with narrowly named reservation consumer exemptions. Add neutral settled exposure DTO in contracts/include/chronos/contracts/accounting.hpp, IDs as needed.
- [ ] API: ReservationAuthority(policy, initial_position); snapshot(cut); reserve(request_id, const RiskDecision&, cut); consume(reservation_id,intent_id,cut); release(reservation_id,cut); reconcile(reservation_id,SettledExposureEvidence). Outcomes accepted/rejected/stale plus typed failures/conflicts. Keep factual outputs immutable and authority noncopyable.
- [ ] Require full scope, decision policy/version/scale match, source projected identity/sequence/content, exact authorized delta and cap. Narrow one-unsettled-hold-per-listing policy prevents netting opposite holds from concealing risk. Terminal idempotency cache prevents reusing old decisions or requests after release.
- [ ] tests/unit/reservation_test.cpp: accept, cap, two decisions same sequence (second stale), opposite hold rejection, rejected risk, expiry, mode/scope mismatch, same retry, changed retry, duplicate decision/new key, consume twice, consumed cannot release, settle full listing scope, overflow and no state change. Existing risk tests confirm recorded risk_sequence remains unchanged.
- [ ] Update tests/CMakeLists.txt and strict boundary guards in root CMake and core/risk/AssertTargetBoundary.cmake only as required, preserving disallowed dependency checks. Handoff tested public API and sample fixture to task 2.

### Task 2: M6.4 executable paper intent

- [ ] Add contracts/include/chronos/contracts/paper_execution.hpp for shared intent/fill facts. Add core/execution_planning/include/chronos/core/execution_planning/paper_intent.hpp, src/paper_intent.cpp, CMakeLists.txt and README; add chronos_execution in core/CMakeLists.txt.
- [ ] API: PaperIntentAuthority::create(request_id, RiskDecision, ReservationAuthority&, reservation_id, execution_evidence, cut). Require exact active reservation authority state, not a caller's forged copy. Bind and consume once only after prevalidation. Shared intent records full chain/scope/cut/lineage and authorized side/quantity.
- [ ] tests/unit/paper_intent_test.cpp: approved and modified quantities; no reservation/rejected risk/nonpaper modes; mismatched decision/target/run/listing; expired/reserved-consumed/released; idempotent same intent; conflicting retry; INT64_MIN conversion; failed validation leaves active reservation unchanged.
- [ ] Register library/test and dependency boundary checks. Handoff shared intent plus example to task 3; no accounting dependency.

### Task 3: M6.5 deterministic paper broker

- [ ] Add adapters/paper/include/chronos/adapters/paper/paper_broker.hpp, src/paper_broker.cpp, CMakeLists.txt; wire adapters/CMakeLists.txt. Expand shared paper facts with ack/fill/rejection, fee and price/model provenance.
- [ ] API PaperBroker::submit(intent, market_evidence, model_policy). Immediate full fill at current eligible bid/ask with nonnegative tick slippage, exact quote fee and logical latency offset. Return typed no-fill rejection; no wall-time scheduling. Enforce immutable input identity/payload retry behavior.
- [ ] tests/unit/paper_broker_test.cpp: buy/sell prices, nonzero slippage and fee, deterministic latency, stale/gapped/crossed/missing book, scope/mode mismatch, tick/step validity, overflow atomically rejects, duplicate and conflicting submission, complete target→fill IDs.
- [ ] Link only contracts/core market interfaces where needed; no accounting or portfolio mutation. Document D0 immediate full-fill limitation and latency semantics. Handoff fill DTO with precise fee/notional scale policy to task 4.

### Task 4: M6.6 immutable balanced ledger

- [ ] Inspect accounting/README.md and place owned implementation under accounting/ (or existing declared owner), e.g. accounting/ledger/include/chronos/accounting/ledger.hpp, accounting/ledger/src/ledger.cpp, accounting/ledger/CMakeLists.txt. Do not place ledger in adapters/paper. Shared contract in contracts accounting header remains implementation independent.
- [ ] API LedgerAuthority::post(PaperFill), reverse(transaction_id,correction_id), transactions(), cursor(), checksum(). Each transaction is immutable, full-source scoped and policy-versioned; atomic post/reversal. Economic duplicate key is fill identity; compare complete semantic payload. Append compensation with exact negated entries and original linkage.
- [ ] Balance quantity and quote currency separately; fee expense/cash legs included. Use checked multiply/rescale with named rounding; reject unrepresentable inputs without committing. Neutral settlement evidence can identify posted fill/reservation and ledger cursor.
- [ ] tests/unit/ledger_test.cpp: buys/sells/nonzero fee, per-unit balance, duplicate/conflict, invalid scope/mode/scales, overflow/no partial transaction, reversal immutable original/exact negate/retry/conflict/unknown, deterministic replay and checksum.
- [ ] Wire target/test and boundary guards; handoff ledger immutable transactions plus scope/cursor/provenance to task 5.

### Task 5: M6.7 ledger-derived position/P&L and runnable replay

- [ ] Add accounting/valuation/include/chronos/accounting/valuation.hpp and accounting/valuation/src/valuation.cpp. Expose derive_position(transactions,scope) and value_position(transactions,mark,policy,cut); no fill-stream or target shortcut. FIFO exact lot basis with deterministic partial allocation, long/short/flat/cross-zero, fees and explicit valuation unavailable. Resolve compensation by effective posted economic history under declared reversal policy.
- [ ] tests/unit/valuation_test.cpp: known buy/sell realized, long and short unrealized, partial close with rounding remainder, cross-zero, fee net relation, compensated trade rebuild, stale/missing/future/wrong listing/currency marks, overflow, no trades hit rate unavailable. Rebuilding solely from ledger equals incremental projection, if any.
- [ ] Add applications/replay_runner/CMakeLists.txt and src/paper_replay.cpp executable chronos_paper_replay using existing capture/replay provider API, dispatcher and feature/strategy/recommendation. Wire applications in root CMake at correct point. Feed actual ledger position into next portfolio/account snapshot; reservation reconciliation only after successful posting, with full listing binding. Do not generate risk/targets by hand in production runner.
- [ ] Add tests/end_to_end/paper_loop_test.cpp with deterministic synthetic fixture exercising a fill and close; assert all authority IDs, balanced/idempotent ledger, capacity not double counted after settlement, known nonzero fees/P&L and same semantic digest on repeated runs. Negative run proves missing risk evidence cannot reach broker or ledger.
- [ ] Add documented CLI accepting captured dataset path and printing stable machine-readable semantic summary; exact invocation documented in applications/replay_runner/README.md. Run against available captured dataset if present; otherwise clearly report fixture-only evidence and do not claim real captured-run validation.
- [ ] Validation: make cpp-test. Run repository Python boundary/lint checks using its documented environment; configure/build/test release for final arithmetic/optimized path confidence. Execute documented CLI twice and compare output excluding explicitly nonsemantic timing. Update M6 module READMEs and roadmap completion evidence without claiming M7 benchmark work.

Verified accounting/README.md: accounting accepts contracts and narrow persistence ports only; must not depend on market-state, strategy, or UI. Use existing accounting/ledger and accounting/valuation submodules, and shared mark DTO. Existing replay API is runtime/datasets/include/chronos/runtime/datasets/replay.hpp.
