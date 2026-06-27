# Phase 08: Accounting and Reconciliation

## Purpose

This document defines Chronos's paper accounting and reconciliation planning contract: the authority that consumes immutable paper fills and declared accounting events, posts balanced ledger entries, derives positions/cash/P&L, and records reconciliation status without rewriting execution history.

This leaf builds on approved Phases 01-07 and the companion Phase 08 `paper-order-simulation.md` leaf. Paper accounting is authoritative for paper positions, cash, fees, realized P&L, unrealized P&L, and accounting projections once ledger entries are posted. It does not create orders or fills, approve risk, reserve capacity, or infer execution facts from desired targets.

## Objectives

Accounting and reconciliation must establish that Chronos can:

1. post immutable balanced ledger transactions from paper fills, fees, funding, corrections, opening balances, and other declared paper accounting events;
2. derive paper positions, cash, exposure, cost basis, realized P&L, unrealized P&L, and net/gross components from ledger facts and identified marks;
3. keep execution facts, ledger facts, positions, and P&L semantically separate;
4. support deterministic replay and rebuild of paper accounting state from ordered facts;
5. reconcile paper broker orders/fills against ledger postings and reservation state without mutating source facts;
6. represent discrepancies, unknown states, corrections, reversals, busts, and compensating transactions explicitly;
7. provide authoritative account/capital/position snapshots back to portfolio construction and risk once Phase 08 accounting exists;
8. preserve paper/live mode isolation and prohibit live-accounting claims from paper evidence;
9. expose accounting telemetry and quality checks without making reports authoritative;
10. define evidence gates for balanced posting, replay, reconciliation, and audit reconstruction.

## Scope

### In scope

- paper account, accounting epoch, ledger transaction, ledger entry, posting rule, cost-basis, fee, funding, mark, valuation, position, and P&L semantics;
- opening-balance and reset handling for paper sessions;
- fill-to-ledger posting and idempotency;
- corrections, reversals, fill busts, accounting adjustments, and compensating transactions;
- reconciliation among paper broker facts, ledger postings, reservation state, and derived positions;
- authoritative paper account/capital/position snapshot publication;
- deterministic rebuild, replay, audit, observability, and evidence gates.

### Out of scope

- portfolio target construction, risk decisions, reservations, executable intents, order submission, or fill simulation;
- live venue reconciliation, broker statements, custody feeds, settlement files, or real-money accounting;
- tax reporting, regulatory reports, legal accounting policy, or external audit certification;
- UI workflows beyond projection requirements;
- concrete database products, process topology, programming language, or ticket-level implementation plan.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Paper-broker authority | Paper order and fill facts consumed as accounting inputs | Ledger postings, positions, P&L |
| Accounting authority | Ledger transactions/entries, posting rules, cost basis, paper positions, cash, realized P&L, accounting corrections | Orders, fills, risk decisions, reservation outcomes |
| Mark and valuation authority | Mark-source selection, mark quality/freshness, unrealized valuation policy, valuation snapshots | Editing ledger facts or inventing fills |
| Reconciliation authority | Discrepancy detection/status, comparison workflows, reconciliation-required facts, correction requests | Rewriting source execution facts or ledger history |
| Exposure/reservation authority | Reservation consumption/release/reconciliation facts | Ledger entries, cash, positions, P&L |
| Portfolio/risk consumers | Later consumption of account/capital/position snapshots | Accounting mutation or hidden balance overrides |
| Run/configuration authority | Accounting policy activation, reset/opening-balance policy, controls through ordered `ControlOutcome`s | Posting values, fill facts, reconciliation findings |
| Observability | Non-authoritative metrics, traces, logs, health projections, evidence export | Accounting truth or reconciliation clearance |
| Query/report/UI tools | Disposable account, position, P&L, and discrepancy projections | Authoritative ledger state or accounting corrections |

Accounting, valuation, and reconciliation may initially share a process, but their authority boundaries remain separate. A reconciliation finding may request or justify a compensating accounting fact; it may not edit a fill or ledger entry in place.

## Canonical concepts

### Paper account

A `PaperAccount` is the accounting and execution container for simulated balances, orders, fills, ledger entries, positions, and P&L. It contains:

- `paper_account_id`;
- portfolio/account binding;
- runtime mode: `Backtest/paper replay` or `Live paper`;
- accounting epoch and parent/reset lineage;
- base currency or unit of account;
- permitted instruments/listings/venues simulated by the paper broker;
- opening-balance policy;
- cost-basis policy version;
- fee/funding policy version;
- mark/valuation policy version;
- snapshot publication policy;
- reconciliation status.

A paper account is not a live account, venue account, or broker statement. Its evidence may support development and live-paper operations, but it must not be represented as real-money custody truth.

### Accounting epoch

An `AccountingEpoch` defines the bounded ledger scope for a paper account. It starts with either:

- an explicit opening-balance transaction;
- an empty initial balance policy;
- a child-run/reset transaction carrying declared balances from a prior epoch;
- a replay fixture manifest.

Reset creates a new epoch or child run; it never deletes prior ledger entries, positions, P&L, orders, fills, or reconciliation history. A position snapshot may initialize a new epoch only if it is posted as an explicit opening-balance or transfer transaction with lineage.

### Ledger transaction and ledger entry

A `LedgerTransaction` groups balanced accounting entries caused by one economic event or correction. A `LedgerEntry` is one immutable posting line.

Minimum transaction fields:

- `ledger_transaction_id`;
- paper account, portfolio, accounting epoch, and run identity;
- causation: fill, fee, funding, opening balance, correction, reversal, bust, mark adjustment if applicable, or manual accounting adjustment when later approved;
- posting-rule version and idempotency key;
- transaction timestamp/logical-time basis;
- balanced-entry checksum;
- replay class and lineage.

Minimum entry fields:

- `ledger_entry_id`;
- transaction identity;
- account/book/category: cash, position, fee, funding, realized P&L, unrealized valuation memo, clearing, suspense, or policy-specific account;
- instrument/listing/unit/currency;
- debit/credit or signed amount under the accounting policy;
- quantity and monetary amount where applicable;
- cost-basis lot reference where applicable;
- semantic checksum.

Transactions must balance under the declared posting policy. If a fill cannot be posted because required policy or mark evidence is unavailable, accounting records an explicit posting rejection or suspense/reconciliation-required state; it must not silently update positions.

### Posting rule

A `PostingRule` maps one input fact type to one balanced transaction pattern. Posting rules are versioned semantic contracts. They define:

- eligible source fact types and deduplication keys;
- required source fields;
- account categories and balancing rules;
- cost-basis interaction;
- fee/funding treatment;
- currency/unit conversion policy;
- rounding and residual handling;
- correction/reversal behavior;
- replay and migration requirements.

Changing posting meaning is a semantic version change.

### Position and cash

Paper positions and cash are derived views over posted ledger entries, not independently mutable facts. A position view identifies:

- paper account, portfolio, instrument/listing/unit, and accounting epoch;
- quantity, average cost or lot structure, realized quantities, and open lots under cost-basis policy;
- source ledger cursor and semantic checksum;
- stale/unavailable status where ledger inputs are incomplete or reconciliation is unresolved.

Cash is likewise derived from ledger entries and declared currency conversion policy. No component may overwrite cash or position to match a target, order, or UI display.

### P&L

P&L is derived under a valuation policy:

- realized P&L from posted closing activity and cost-basis rules;
- unrealized P&L from open positions valued against identified marks;
- gross P&L before declared costs;
- net P&L after included fees, funding, slippage, and other costs;
- stale or unavailable valuation when marks are missing or stale.

Every P&L value must identify account/portfolio scope, accounting epoch, valuation time, mark source, policy version, currency/unit, and ledger cursor. P&L is never a directly editable field.

### Reconciliation case

A `ReconciliationCase` records a discrepancy or unresolved state among execution, reservation, ledger, and derived accounting views. Examples:

- fill exists but ledger posting is missing;
- ledger transaction references an unknown fill;
- duplicate fill was rejected but appears in a projection;
- reservation remains active after terminal order/fill state;
- unknown order state prevents safe reservation release;
- accounting transaction is unbalanced under policy;
- derived position differs from rebuilt position;
- mark source is stale or missing for valuation.

A case has identity, severity, owner, source facts, expected resolution paths, status, and audit trail. Resolution must occur through source facts, compensating ledger transactions, reservation reconciliation facts, or declared projection rebuilds. It must not edit historical facts in place.

## Fact taxonomy activated by this leaf

This leaf activates accounting and reconciliation fact namespaces reserved by Phase 03.

Minimum ledger/accounting fact types:

- `ledger.account.registered`;
- `ledger.epoch.opened`;
- `ledger.epoch.closed`;
- `ledger.posting_rule.registered`;
- `ledger.transaction.posted`;
- `ledger.transaction.reversed`;
- `ledger.transaction.corrected`;
- `ledger.transaction.rejected`;
- `ledger.entry.posted`;
- `ledger.opening_balance.posted`;
- `ledger.fill_posting.accepted`;
- `ledger.fill_posting.duplicate_rejected`;
- `ledger.fill_posting.suspense`;
- ledger publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Minimum valuation/projection fact types:

- `ledger.mark_policy.registered`;
- `ledger.valuation.mark_accepted`;
- `ledger.valuation.unavailable`;
- `ledger.position_snapshot.published`;
- `ledger.cash_snapshot.published`;
- `ledger.pnl_snapshot.published`;

Minimum reconciliation fact types use the separate `reconciliation.*` namespace reserved by Phase 03. Reconciliation status is not ledger authority; ledger corrections and reversals remain `ledger.*` postings, while discrepancy workflow and resolution facts live here:

- `reconciliation.case_opened`;
- `reconciliation.case_updated`;
- `reconciliation.case_resolved`;
- `reconciliation.required`;
- `reconciliation.failed`;

Paper order and fill facts remain `execution.*` facts owned by paper execution. Reservation lifecycle facts remain `risk.*` facts owned by reservation. Behavior-changing accounting-policy activation, mark-policy activation, pause/resume, stop, abort, reset, and emergency controls remain accepted ordered `ControlOutcome`s under `run.control.*`. Runtime mode remains fixed by the run manifest.

## Posting eligibility

Accounting may post from a fill only when all conditions hold:

1. the fill is an accepted immutable `execution.fill.accepted` fact;
2. fill identity, order identity, intent identity, paper account, portfolio, instrument/listing, quantity, price, and side are complete;
3. the fill belongs to `Backtest/paper replay` or `Live paper`;
4. the paper account and accounting epoch are active for the fill's run lineage;
5. posting rule, cost-basis policy, fee/funding policy, and currency/unit policy support the fill;
6. the fill has not already been posted under the same idempotency key;
7. required fee, funding, and conversion inputs are present or explicitly deferrable under suspense policy;
8. accounting controls permit posting at the selected effective position.

If eligibility fails, accounting records a rejected, duplicate-rejected, suspense, or reconciliation-required fact. It must not update derived positions without ledger entries.

## Reconciliation behavior

Reconciliation compares:

- paper broker order/fill facts;
- execution intent and order terminal states;
- reservation active/consumed/released/reconciliation-required state;
- ledger postings and rejected/suspense postings;
- derived positions/cash/P&L;
- expected state from deterministic rebuild.

Rules:

- source facts are immutable;
- discrepancies open cases with causal references;
- cases have explicit status and owner;
- resolution is via compensating facts, missing-fact ingestion, reservation reconciliation, ledger correction/reversal, or projection rebuild;
- unresolved severe cases make downstream account/capital snapshots unavailable or degraded according to policy;
- unknown order/fill state remains worst-case until resolved.

Reconciliation does not substitute for broker simulation. It cannot invent a fill to close an order or fabricate a ledger entry to make P&L look correct.

## Snapshot publication

Once Phase 08 accounting exists, portfolio construction and risk may consume authoritative paper snapshots from accounting rather than assumption-backed placeholders.

Published snapshots include:

- `ledger.position_snapshot.published`;
- `ledger.cash_snapshot.published`;
- `ledger.pnl_snapshot.published`;
- account/capital snapshot projections for portfolio/risk consumption.

Each snapshot must include:

- paper account, portfolio, epoch, and run identity;
- source ledger cursor and semantic checksum;
- mark/valuation policy and mark freshness where applicable;
- reconciliation status and unavailable/degraded flags;
- completeness status;
- publication time/logical position;
- replay class and rebuild lineage.

Snapshots are read-only evidence. If a downstream component needs a different view, it must request or derive a new projection from ledger facts; it must not mutate the snapshot.

## Mode behavior

- `Replay analysis` may inspect historical accounting projections only when supplied by a replay dataset; it does not create new paper ledger facts unless the manifest selects `Backtest/paper replay`.
- `Backtest/paper replay` may post deterministic paper ledger facts, positions, cash, and P&L from replayed paper fills.
- `Live read-only` cannot create fills or authoritative paper accounting. It may display non-authoritative projections labeled as such.
- `Live paper` may post authoritative paper ledger facts from paper fills without human approval and without live custody claims.
- `Human-approved live` and `Guarded automated live` require later live accounting/reconciliation planning; Phase 08 paper accounting must not claim live settlement or custody truth.
- `paper_candidate` remains a research promotion state, not an accounting mode.

## Pause, stop, abort, and reset

Pause:

- may block new non-safety accounting work after the effective position;
- must continue required fill posting or reconciliation where policy says safety/accounting completeness requires it;
- does not change prior ledger entries.

Stop/abort:

- records incomplete or failed accounting scope when required postings cannot be recovered;
- opens reconciliation cases for unresolved fills, unbalanced transactions, or unknown execution states;
- preserves source facts and ledger history.

Reset:

- opens a new accounting epoch or child run with explicit opening-balance or transfer transaction;
- never deletes prior accounting facts;
- cannot change runtime mode in place;
- requires downstream portfolio/risk consumers to use snapshots from the new epoch only after publication and completeness checks.

## Persistence, recovery, and replay

Accounting must persist or reconstruct:

- ledger transactions and entries;
- posting idempotency keys;
- rejected/suspense postings;
- cost-basis lots and derived position cursors;
- valuation mark inputs and mark-quality status;
- reconciliation cases and resolutions;
- snapshot publication facts;
- projection rebuild manifests.

Recovery rules:

- if a fill was accepted but ledger posting did not complete, recovery posts once using the original idempotency key;
- if posting completed but snapshot publication did not, recovery republishes the snapshot from ledger facts;
- if an unbalanced transaction is detected, recovery rejects or reverses through explicit facts rather than editing entries;
- if accounting rebuild differs from stored projections, projections are invalidated and rebuilt or the run is marked failed/unreconciled;
- if a required mark is unavailable, valuation is unavailable or stale, not silently reused as current.

Replay must reproduce the same ledger transactions, entries, positions, and P&L given the same fills, accounting policies, marks, controls, initial epoch, and replay class.

## Lineage and audit

Every ledger transaction, snapshot, and reconciliation case must reconstruct:

- source fill/order/intent/reservation/risk/target lineage where applicable;
- paper account, portfolio, accounting epoch, and run;
- posting, cost-basis, fee/funding, mark, valuation, and rounding policy versions;
- controls at effective positions;
- ledger cursor and semantic checksum;
- correction/reversal/bust lineage;
- reconciliation status and resolution facts;
- downstream snapshot consumers.

Audit must be possible from immutable facts and declared snapshots without reading mutable current tables.

## Observability and performance

Observability must include:

- fill-to-ledger-posting latency;
- ledger-posting-to-snapshot latency;
- snapshot-to-portfolio/risk-consumption latency;
- posting throughput and rebuild throughput;
- duplicate/suspense/rejected posting counts;
- unbalanced transaction detection;
- reconciliation case counts by severity/status/source;
- mark freshness and valuation-unavailable counts;
- projection rebuild duration and mismatch rates.

Metrics and dashboards are not accounting truth. Missing metrics cannot post or clear ledger entries.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **AC-E01 — Balanced posting suite** | Buys, sells, partial fills, fees, funding, opening balances, rounding residuals, corrections | Every posted transaction balances under policy or is explicitly rejected/suspense |
| **AC-E02 — Fill idempotency suite** | Duplicate fills, retry after crash, fill correction, bust, delayed fill | Each economic input posts once; corrections are compensating facts |
| **AC-E03 — Position/P&L derivation suite** | Long/short/flat transitions, cost-basis policies, stale marks, missing marks, fees | Positions and P&L derive from ledger plus valuation policy; stale/unavailable is explicit |
| **AC-E04 — Reconciliation suite** | Missing ledger posting, unknown order, reservation mismatch, projection mismatch, unbalanced transaction | Cases open with lineage and resolve only through authoritative facts |
| **AC-E05 — Snapshot publication suite** | Complete, degraded, unresolved, stale-mark, reset/new-epoch snapshots | Downstream snapshots carry ledger cursor, checksum, quality, and reconciliation status |
| **AC-E06 — Recovery/replay suite** | Crash before/after posting, before snapshot, during reconciliation, rebuild from facts | Recovered/replayed accounting state is semantically identical or explicitly failed/unreconciled |
| **AC-E07 — Mode isolation suite** | Backtest/paper replay, live paper, live read-only, human-approved live | Paper accounting never claims live custody truth and live read-only posts no ledger |
| **AC-E08 — Audit reconstruction drill** | End-to-end fill to ledger to position/P&L to snapshot | Audit reaches all source facts, policy versions, controls, corrections, and consumers |
| **AC-E09 — Observability/performance suite** | Throughput, rebuild, stale metrics, missing telemetry | Measurements are non-authoritative and budgets are evidence-derived |

## Phase exit criteria

Accounting and reconciliation planning is complete when:

- ledger transaction, ledger entry, posting rule, position, cash, P&L, valuation, snapshot, and reconciliation semantics are unambiguous;
- paper fills are the execution input boundary and accounting cannot invent or edit fills;
- posted transactions are balanced, immutable, idempotent, and replayable;
- positions and P&L are derived from ledger and valuation policies;
- reconciliation uses explicit cases and compensating facts rather than mutable rewrites;
- account/capital snapshots are suitable for portfolio/risk consumption with quality flags;
- mode isolation prevents paper accounting from becoming live-accounting evidence.

## Deferred decisions

Deferred to later implementation or phases:

- exact first cost-basis policy and accounting chart;
- first fee/funding/slippage policy defaults;
- exact mark-source selection for unrealized P&L;
- UI reconciliation workflow details, handled in Phase 09;
- production-like live-paper operations and soak, handled in Phase 10;
- live custody/venue reconciliation, handled in Phase 11 or later.
