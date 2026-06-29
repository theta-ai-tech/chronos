# Deliverable 0 — Thin Vertical Slice

**Status:** Draft for review (issues to be created after approval)
**Date:** 2026-06-29

## Goal

Prove the Chronos architecture end-to-end with the narrowest possible path that is
nonetheless a **clean cross-section of the general design** — not a throwaway prototype.
One venue (Bybit), one listing, one strategy. Real data is **captured live** from Bybit into
an immutable dataset, then **replayed deterministically** to produce paper fills, P&L, and a
reproducible hot-path latency distribution.

Deliverable 0 exists to:

1. Validate the domain-model contracts against real Bybit data before deepening the 28
   authorities.
2. Produce the optimized-C++ hot-path benchmark artifact (the sellable thing) soonest.
3. Prove the seams hold, so later phases (multi-venue, L3, multi-leg, live) are confident
   additions rather than leaps of faith.

## Capture vs. live processing

D0 includes **capture-to-dataset** (record mode): a real Bybit source adapter connects, frames
and timestamps raw market data, and persists immutable source events as a dataset. D0 does
**not** include the live *processing/decision* path — all state, strategy, and paper decisions
run in **replay** over the captured dataset, preserving determinism. Capture feeds replay; it
never drives live decisions in D0.

## Non-negotiable seams (honored in every milestone)

These cost ~nothing at N=1 and save a rewrite at N>1. A milestone that violates one is not
done, even if it "works."

- **S1** — `canonical_instrument` and `listing` stay distinct IDs (1:1 today).
- **S2** — Market state is **listing-scoped**, never a single global book.
- **S3** — Use the `StateLineage` **vector** cut; never collapse to a scalar sequence.
- **S4** — Everything flows through the single `run_input_sequence` dispatch (one stream is
  the degenerate merge).
- **S5** — Strategies implement the **C++ strategy SDK** interface from day one.
- **S6** — The decision path `recommendation → target → risk → reservation → intent` is never
  bypassed, even where a stage is minimal/permissive in D0.
- **S7** — Hot path is **optimized C++** (ADR-0001); control/research is Python; amounts are
  **fixed-point integers** (ADR-0003).
- **S8** — Source adapters implement the **adapter SDK**; capture preserves the raw payload and
  retains malformed/unsupported input rather than discarding it; receive time + capture
  sequence are assigned at ingress.

## Scope boundary for Deliverable 0

In scope: a real Bybit capture (record mode) for one listing; replay mode for all
state/strategy/paper decisions; L2 only; one reference strategy; minimal but
structurally-correct risk/reservation/paper/ledger; latency instrumentation + benchmark.

Explicitly **out** of D0 (later phases, seams reserved): live *processing/decision* path,
multi-venue, L3, multi-leg/opportunity resolution, human-approved live execution,
reconciliation beyond a trivial self-check, full Phase-01 closure evidence machinery, full
observability stack.

D0 deliberately takes a *thin structural slice* of phases 07 (risk) and 08 (paper/accounting)
to close the loop end-to-end. These slices must honor S6 but are not the full phases.

---

## Milestones

### M0 — Repo, toolchain & cross-language boundary
**Goal:** A clean machine can build and run a trivial event through the C++ core invoked from
Python.
**Scope:** Monorepo skeleton (`contracts/`, `core/`, `adapters/`, `applications/`, `tests/`,
`tools/`) per the architecture's structure; pinned C++ toolchain (CMake + compiler) and Python
toolchain; build graph mirroring dependency direction; CI baseline (build, lint, unit test);
a "hello event" that crosses the Python→C++ boundary and back.
**Seams:** S7. Establishes the FFI/IPC mechanism (the open part of deferred decision #1).
**Exit:** Documented one-command bootstrap + build; trivial round-trip event through the C++
core from Python; CI green on a representative failure of each gate.
**Labels:** `deliverable-0`, `area:scaffold`, `type:infra`

### M1 — Canonical contracts (envelope, value objects, fixed-point)
**Goal:** The minimum stable message contract and value objects exist and round-trip across
the boundary with semantic equivalence.
**Scope:** Minimum event envelope; fixed-point `Price`/`Quantity`/`Money` (ADR-0003) with
explicit width/scale/rounding; `StreamCursor`, `StateLineage`, `VersionRef`, `DataQuality`,
`TimePoint`/clock-domain; reserved taxonomy namespaces; conformance fixtures.
**Seams:** S1, S3, S7.
**Exit:** Contracts compile in both runtimes; fixed-point arithmetic unit-tested for rounding,
overflow-as-failure, and exact equality semantics; envelope round-trips C++↔Python proving
semantic equivalence.
**Labels:** `deliverable-0`, `area:contracts`, `type:feature`

### M2 — Bybit source adapter & capture (record mode)
**Goal:** A real Bybit session for one listing is captured into an immutable, replayable
dataset.
**Scope:** Bybit public WebSocket source adapter (connect, subscribe, frame, receive
timestamps, capture sequence) implementing the adapter SDK; immutable source-event capture with
raw payload + integrity status; source-session health and minimal reconnect/epoch handling;
persistence of the capture dataset; retain malformed/unsupported frames.
**Seams:** S8. Capture is record-only; it does not drive live decisions (see "Capture vs. live
processing").
**Exit:** A live Bybit run records one listing's L2 + public-trade frames into a persisted
capture dataset with receive time, capture sequence, and integrity status; malformed frames are
captured and flagged, not dropped; a captured session can be re-read identically.
**Labels:** `deliverable-0`, `area:market-data`, `type:feature`

### M3 — Normalization, reference data & replay provider
**Goal:** The captured dataset replays into a deterministic, fully-lineaged normalized-event
stream.
**Scope:** Normalization of captured frames to `market.book.*` / `market.trade.*` normalized
events; reference data for one canonical instrument + one listing definition
(tick/step/status); faithful capture-order and normalized-fact replay providers feeding
dispatch.
**Seams:** S1, S2, S4.
**Exit:** Captured session → deterministic ordered normalized stream with complete lineage;
re-run yields identical output (golden); a frame that fails normalization fails visibly, not
silently; replay class is explicit on the run manifest.
**Labels:** `deliverable-0`, `area:market-data`, `type:feature`

### M4 — Dispatch & L2 market state (C++ hot path)
**Goal:** Replay produces deterministic immutable market-state views with complete lineage,
in the C++ core.
**Scope:** Single `run_input_sequence` dispatcher (degenerate single-stream merge);
market-state authority — L2 book transitions, best bid/ask/spread/crossed status, recent-trade
window, freshness/sync status, `StateLineage` vector cut, immutable view publication;
allocation-controlled.
**Seams:** S2, S3, S4, S7.
**Exit:** Deterministic immutable views over the replay; book invariants (one qty per
side/level, non-negative, zero removes level, non-crossed unless permitted) tested;
deterministic replay golden test on view identities + content.
**Labels:** `deliverable-0`, `area:engine`, `type:feature`

### M5 — Features, strategy & recommendation
**Goal:** Replay yields deterministic evaluations, signals/abstentions, and one recommendation
per valid signal with explanations.
**Scope:** Feature runtime (order-book imbalance, microprice, spread — a small set); one
reference strategy via the C++ strategy SDK; `StrategyEvaluation` → one `signal_emitted` or
`abstained`; recommendation authority → exactly one actionable/hold `TradeRecommendation` per
valid signal with ranked explanation factors.
**Seams:** S5, S6 (recommendation end). Honors abstention≠signal and hold-as-first-class.
**Exit:** Deterministic features/evaluations/signals/recommendations with explanation lineage;
stale/gapped state produces `abstained` (tested); one-recommendation-per-signal cardinality
tested.
**Labels:** `deliverable-0`, `area:strategy`, `type:feature`

### M6 — Minimal risk, paper execution, ledger & P&L
**Goal:** A replayed session closes the loop to paper fills, a balanced ledger, position and
P&L — without bypassing the decision path.
**Scope:** Minimal single-portfolio target construction; **structurally-correct but
permissive** risk decision + single-writer reservation + executable paper intent; simple paper
broker fill model; immutable balanced ledger; derived position; realized/unrealized P&L
against a simple mark; basic hit-rate/P&L summary.
**Seams:** S6 (full chain), S7 (exact arithmetic, idempotent ledger).
**Exit:** Replay produces paper fills, a balanced idempotent ledger, position, and P&L with
mark/currency/policy provenance; `recommendation→target→risk→reservation→intent→order→fill→
ledger` chain auditable end-to-end; no UI/manual mutation of derived facts.
**Labels:** `deliverable-0`, `area:paper-execution`, `type:feature`

### M7 — Latency instrumentation & benchmark harness
**Goal:** A reproducible hot-path latency distribution, recorded with its reference
environment.
**Scope:** `LatencyPoint`/`LatencySegment` capture across the hot path (≥
ingress→state→feature→strategy→recommendation) with bounded overhead; benchmark runner
producing p50/p95/p99/p99.9 over a recorded workload; reference-environment manifest
(ADR-0002); histogram artifact.
**Seams:** S7. Replay logical time kept strictly separate from processing latency; demo-host
numbers never presented as benchmark-of-record.
**Exit:** Reproducible latency distribution + profile from the benchmark profile, with
workload + reference-environment manifests; instrumentation overhead measured.
**Labels:** `deliverable-0`, `area:observability`, `type:feature`

### M8 — Minimal operator surface *(optional within D0)*
**Goal:** A thin surface to see the slice run: top-of-book, recent trades, active signals,
suggested paper positions, latency histogram, replay controls.
**Scope:** A minimal local web console (or CLI/static render first); read models exposing
source position/age/completeness/mode; the always-on demo surface (ADR-0002), labeled
non-authoritative for performance.
**Seams:** Console is non-authoritative; commands go through typed contracts.
**Exit:** Can start/stop a replay and observe state, signals, paper positions, and the latency
histogram; demo clearly labeled non-benchmark.
**Labels:** `deliverable-0`, `area:ui`, `type:feature`, `optional`

---

## Dependency order

```
M0 → M1 → M2 → M3 → M4 → M5 → M6 → M7
                            └──────────→ M8 (optional, can start after M5)
```

## Definition of Done for Deliverable 0

- A real Bybit session is captured into an immutable dataset, then replays deterministically
  end-to-end (re-run = identical domain output).
- The hot path is C++, amounts are fixed-point, the decision chain is unbypassed.
- A reproducible latency distribution exists with its reference-environment manifest.
- All seams S1–S8 are honored and covered by tests.

## Issue breakdown

Each line is one narrow, single-PR issue. Every issue carries its milestone label plus the
area/type shown. Issues marked `seam` exist mainly to honor a non-negotiable seam.

### M0 — Repo, toolchain & boundary
- **M0.1** Monorepo skeleton + per-module owner stubs (`contracts/ core/ adapters/ applications/ tests/ tools/`). `area:scaffold` `type:infra`
- **M0.2** C++ toolchain: CMake, pinned compiler, debug/release/benchmark profiles, one lib+test target. `area:scaffold` `type:infra`
- **M0.3** Python toolchain: project, dependency lock, lint/format, test runner. `area:scaffold` `type:infra`
- **M0.4** Python↔C++ boundary: bind a trivial call, round-trip a "hello event". `area:scaffold` `type:feature` `seam`
- **M0.5** CI baseline: build+lint+unit for both runtimes; prove each gate fails on a representative violation. `area:scaffold` `type:infra`
- **M0.6** One-command bootstrap + build/run docs. `area:scaffold` `type:infra`

### M1 — Canonical contracts
- **M1.1** Fixed-point `Price`/`Quantity`/`Money` (widths, scale, rounding, overflow-as-failure) + unit tests. `area:contracts` `type:feature` `seam`
- **M1.2** Identities + value objects (IDs, `StreamCursor`, `VersionRef`, `TimePoint`/clock-domain, `DataQuality`). `area:contracts` `type:feature`
- **M1.3** `StateLineage` vector type + ordering/compare semantics. `area:contracts` `type:feature` `seam`
- **M1.4** Minimum event envelope + reserved taxonomy namespaces. `area:contracts` `type:feature`
- **M1.5** Cross-boundary serialization + round-trip semantic-equivalence conformance fixtures. `area:contracts` `type:test`

### M2 — Bybit source adapter & capture
- **M2.1** Adapter SDK seam: lifecycle/health/capability contracts (minimal). `area:market-data` `type:feature` `seam`
- **M2.2** Bybit WS connectivity: connect, subscribe one listing (L2 + trades), framing. `area:market-data` `type:feature`
- **M2.3** Source-event capture: immutable events, receive time, capture sequence, integrity status, retain malformed. `area:market-data` `type:feature` `seam`
- **M2.4** Reconnect / session-health / epoch handling (minimal). `area:market-data` `type:feature`
- **M2.5** Capture dataset persistence + deterministic re-read. `area:market-data` `type:feature`

### M3 — Normalization, reference data & replay
- **M3.1** Reference data: one canonical instrument + one listing definition (tick/step/status), versioned. `area:market-data` `type:feature` `seam`
- **M3.2** Normalizer: book frames → `market.book.*` normalized events. `area:market-data` `type:feature`
- **M3.3** Normalizer: trade frames → `market.trade.*` normalized events. `area:market-data` `type:feature`
- **M3.4** Replay providers (faithful capture-order + normalized-fact) + run-manifest replay class. `area:market-data` `type:feature`
- **M3.5** Determinism golden test on the normalized stream. `area:market-data` `type:test`

### M4 — Dispatch & L2 market state
- **M4.1** Run-input dispatcher: single `run_input_sequence`, degenerate single-stream merge. `area:engine` `type:feature` `seam`
- **M4.2** L2 book transitions (apply snapshot/delta) + book invariants. `area:engine` `type:feature`
- **M4.3** Derived top-of-book: best bid/ask/spread/crossed/locked status. `area:engine` `type:feature`
- **M4.4** Recent-trade window + freshness/sync status. `area:engine` `type:feature`
- **M4.5** Immutable view publication with `StateLineage` vector cut. `area:engine` `type:feature` `seam`
- **M4.6** Deterministic replay golden test on views. `area:engine` `type:test`

### M5 — Features, strategy & recommendation
- **M5.1** Feature runtime + features (imbalance, microprice, spread) with provenance. `area:strategy` `type:feature`
- **M5.2** C++ strategy SDK interface (deterministic, resource/deadline hooks, explanation output). `area:strategy` `type:feature` `seam`
- **M5.3** One reference strategy implementation. `area:strategy` `type:feature`
- **M5.4** `StrategyEvaluation` outcome (signal_emitted / abstained); abstain on stale/gapped. `area:strategy` `type:feature`
- **M5.5** Recommendation authority: one actionable/hold rec per valid signal + ranked explanations. `area:strategy` `type:feature`
- **M5.6** Cardinality + abstention tests. `area:strategy` `type:test`

### M6 — Minimal risk, paper execution, ledger & P&L
- **M6.1** Target construction (single portfolio, absolute target, no-change outcome). `area:paper-execution` `type:feature` `seam`
- **M6.2** Minimal risk decision (permissive but structurally complete; fail-closed on missing inputs). `area:paper-execution` `type:feature` `seam`
- **M6.3** Single-writer reservation + `risk_sequence` + reservation outcome. `area:paper-execution` `type:feature` `seam`
- **M6.4** Executable paper order intent (mode-bound). `area:paper-execution` `type:feature`
- **M6.5** Paper broker fill model (ack/fill, simple latency/cost). `area:paper-execution` `type:feature`
- **M6.6** Immutable balanced ledger + idempotent posting. `area:paper-execution` `type:feature`
- **M6.7** Derived position + realized/unrealized P&L + mark + summary. `area:paper-execution` `type:feature`

### M7 — Latency instrumentation & benchmark
- **M7.1** `LatencyPoint`/`LatencySegment` capture on the hot path (bounded overhead). `area:observability` `type:feature`
- **M7.2** Benchmark runner: p50/p95/p99/p99.9 over a recorded workload. `area:observability` `type:feature`
- **M7.3** Reference-environment + workload manifests (ADR-0002). `area:observability` `type:feature`
- **M7.4** Histogram artifact + overhead measurement; logical-time vs processing-time separation. `area:observability` `type:test`

### M8 — Minimal operator surface *(optional)*
- **M8.1** Read models (source position/age/completeness/mode). `area:ui` `type:feature` `optional`
- **M8.2** Replay controls (start/stop) via typed commands. `area:ui` `type:feature` `optional`
- **M8.3** Console views: top-of-book, trades, signals, paper positions, latency histogram. `area:ui` `type:feature` `optional`
- **M8.4** Demo deploy labeled non-authoritative for performance. `area:ui` `type:infra` `optional`

**Total: ~48 issues** across 9 milestones (M8's 4 optional). Each sized for a single
focused PR.

## Proposed label taxonomy (for review)

- **Milestone:** `m0`…`m8`
- **Deliverable:** `deliverable-0`
- **Area:** `area:scaffold`, `area:contracts`, `area:market-data`, `area:engine`,
  `area:strategy`, `area:paper-execution`, `area:observability`, `area:ui`
- **Type:** `type:feature`, `type:test`, `type:infra`
- **Other:** `optional`, `seam` (for issues whose main purpose is honoring a seam)
