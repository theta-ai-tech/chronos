# Phase 08: Paper Order Simulation

## Purpose

This document defines Chronos's paper-execution planning contract: the authority that consumes an approved risk decision and accepted reservation, creates mode-valid executable paper order intents, simulates paper order lifecycles, and emits immutable paper order and fill facts.

This leaf builds on approved Phases 01-07. It preserves the canonical chain:

```text
TargetPosition
  -> RiskDecision
  -> ReservationOutcome / Reservation
  -> ExecutableOrderIntent
  -> Order
  -> Fill
  -> accounting leaf
```

Paper execution is the first phase that creates executable order intents, orders, and fills. It is still not live trading. It must prove that Chronos can run the full safety and execution lifecycle against a paper broker without live credentials, live venue submission, or human-approval workflow.

## Objectives

Paper order simulation must establish that Chronos can:

1. create executable paper order intents only from approved or modified `RiskDecision`s with accepted, active, unexpired reservations;
2. bind every intent to exactly one paper-capable runtime mode and prohibit live routing;
3. translate absolute approved targets into one or more paper order plans without changing risk authority or reservation authority;
4. simulate acknowledgements, rejections, partial fills, fills, cancellations, replacements, expiries, and unknown states deterministically or with explicitly seeded models;
5. consume, hold, release, or reconcile reservations through explicit reservation lifecycle facts rather than inferred UI/query state;
6. prevent duplicate order submission across retries, replay, crash recovery, and idempotency races;
7. preserve complete lineage from recommendation through reservation to intent, order, and fill;
8. expose paper execution telemetry and performance evidence without letting observability become execution truth;
9. support faithful replay and normalized-fact replay of paper execution outcomes;
10. hand immutable fills and execution outcomes to the accounting leaf without directly posting ledger entries or editing positions.

## Scope

### In scope

- executable paper order-intent semantics and lifecycle;
- paper execution mode gating for `Backtest/paper replay` and `Live paper`;
- reservation eligibility and consumption/release handoff;
- paper order-plan generation from approved or modified targets;
- paper broker simulation contracts, model versions, deterministic seeds, and scenario fixtures;
- order acknowledgement, rejection, cancellation, replacement, expiry, partial-fill, filled, and unknown-state behavior;
- idempotency, retry, recovery, replay, and duplicate-submission prevention;
- order/fill fact taxonomy under Phase 03 namespaces;
- execution-group placeholders for sliced or multi-leg paper work;
- latency, health, audit, and evidence gates.

### Out of scope

- portfolio target construction, risk approval, or reservation issuance;
- live venue submission, live credentials, live acknowledgements, or live execution adapters;
- human approval workflow or `HumanExecutionApproval`;
- ledger posting, positions, P&L, valuation, reconciliation accounting corrections, or tax;
- concrete broker API product choice, storage engine, process topology, programming language, or implementation tickets;
- exact production execution algorithms, alpha claims, or legally binding compliance rules.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Risk-policy authority | Approved/rejected/modified `RiskDecision`s and decision validity | Executable intents, paper orders, fills, ledger |
| Exposure/reservation authority | Accepted reservations, reservation state, reservation consumption/release/reconciliation facts, `risk_sequence` | Paper broker outcomes, order facts, fill invention |
| Execution-planning authority | Executable paper order intents, execution constraints, order-plan identity, intent lifecycle | Risk decisions, reservation outcomes, ledger postings |
| Paper-broker authority | Simulated order lifecycle, acknowledgements, rejections, fills, cancellation/replacement responses, paper broker clock/model evidence | Live venue truth, risk approval, accounting positions |
| Execution-group authority | Coordination policy for related paper intents/orders and partial completion placeholders | Claiming venue atomicity or bypassing per-leg risk/reservation |
| Accounting authority | Later ledger posting from fills and other accounting events | Paper order routing or fill simulation |
| Run/configuration authority | Mode, paper broker model activation, execution policy activation, controls via ordered `ControlOutcome`s | Intent values, order facts, fills, risk values |
| Stream/run-input authority | Ordered controls, timers, selected cuts, replay input order | Execution semantics or broker simulation outcomes |
| Observability | Non-authoritative metrics, traces, logs, health projections, latency points, evidence export | Creating intents, orders, fills, reservation releases, or accounting truth |
| Query/report/UI tools | Disposable projections of paper execution state | Authoritative execution state or behavior-changing controls |

Execution planning and paper broker simulation may initially share a process, but their authorities remain distinct. Execution planning decides what paper work is authorized; the paper broker simulation decides simulated order outcomes under declared model rules.

## Canonical concepts

### Executable paper order intent

An `ExecutableOrderIntent` is the execution-planning authority's immutable instruction to move from current paper state toward an approved target under execution constraints. In Phase 08, every executable intent is paper-only.

An intent contains at minimum:

- `intent_id`;
- runtime mode: `Backtest/paper replay` or `Live paper`;
- paper account, portfolio, target, risk decision, reservation outcome, reservation, and `risk_sequence` references;
- approved or modified target identity and target checksum;
- intended delta, side, quantity/notional/unit, price/mark basis, and rounding profile;
- execution policy version and order-plan version;
- paper broker model version and eligibility scope;
- reservation consumption/hold policy;
- validity interval, expiry, cancellation policy, and supersession scope;
- idempotency key and replay class;
- complete lineage and explanation references.

An executable paper order intent does not exist in `Live read-only`. It cannot be created in `Human-approved live`, because that mode requires Phase 11 live approval and live execution semantics before a live intent can exist. It cannot be routed to a live adapter even if the order fields resemble live-order fields.

### Paper order plan

A `PaperOrderPlan` translates one executable paper order intent into one or more simulated paper orders. It declares:

- parent intent identity;
- order count, split policy, and sequencing;
- order type, time-in-force, price bounds, slippage/fee-model references, and cancellation/replacement policy;
- execution-group identity when multiple orders must be reasoned about together;
- minimum-fill and residual-handling policy;
- deterministic tie-breaking and arithmetic profiles;
- model fixtures needed for replay.

The order plan must not modify the target, risk decision, or reservation. If the plan cannot safely express the approved work, execution planning rejects or cancels the intent with an explicit reason and releases or reconciles reservation state according to the reservation authority's policy.

### Paper broker

A `PaperBroker` is a simulation authority, not a venue. It receives paper orders and emits simulated lifecycle outcomes. It must declare:

- broker model identity and semantic version;
- supported instruments/listings/order types/time-in-force values;
- acknowledgement model;
- fill model, including price source, queue model, liquidity assumptions, slippage, latency, and seeded randomness if allowed;
- rejection model for invalid, stale, unsupported, or unavailable work;
- cancellation and replacement model;
- unknown-state model for crash/recovery drills;
- compatibility with replay classes and deterministic reconstruction.

If stochastic simulation is used, the pseudorandom algorithm, seed, draw order, and scenario inputs are part of run provenance. Unseeded randomness is prohibited.

### Paper order

An `Order` in Phase 08 is a paper-broker order with Chronos identity and simulated broker identity. It is distinct from its parent intent.

An order contains:

- `order_id`;
- parent `intent_id` and optional execution-group identity;
- paper broker model/version;
- paper account and portfolio references;
- instrument/listing/unit/order-type/time-in-force;
- requested quantity/notional and price constraints;
- submission attempt identity, idempotency key, and sequence;
- current lifecycle facts and terminal status if known;
- lineage, replay class, and semantic checksum.

Supported paper lifecycle is branching, not a terminal-to-terminal chain:

```text
created
  -> submitted
      -> acknowledged
          -> partially_filled
              -> filled
              -> cancel_requested -> cancelled
              -> expired
              -> unknown -> reconciled
          -> filled
          -> cancel_requested -> cancelled
          -> replace_requested -> replaced -> submitted
          -> expired
          -> unknown -> reconciled
      -> rejected
      -> unknown -> reconciled
```

The concrete state machine may refine these states, but filled, cancelled, rejected, and expired are terminal for that order identity unless a separate replacement order is created. Cancellation and replacement are requested/pending branches before authoritative broker response; a fill may race with a cancel request according to the paper broker model. `unknown` is an ambiguity state that must be reconciled before reservation release or duplicate retry decisions are safe. The state machine must not hide unknown states by fabricating cancellation or fill evidence.

### Fill

A `Fill` is an immutable simulated execution fact emitted by the paper broker. It contains:

- `fill_id`;
- order, intent, execution-group, paper account, and portfolio references;
- instrument/listing/unit;
- filled quantity, price, notional, side, and timestamp/logical-time basis;
- fee/funding estimates when the fill model owns them, or references for the accounting leaf to calculate them;
- liquidity/slippage/mark source used by the model;
- paper broker model version, seed/draw references if applicable, and replay class;
- deduplication key and semantic checksum.

Fills are never edited or deleted. Corrections, busts, or model replay replacements are compensating facts under explicit policy. The accounting leaf consumes fills; paper execution does not mutate positions, cash, balances, ledger entries, or P&L.

## Fact taxonomy activated by this leaf

This leaf activates execution order and fill namespaces reserved by Phase 03.

Minimum execution-planning fact types:

- `execution.intent.accepted`;
- `execution.intent.rejected`;
- `execution.intent.expired`;
- `execution.intent.cancelled_by_control`;
- `execution.intent.superseded`;
- `execution.intent.partially_satisfied`;
- `execution.intent.satisfied`;
- `execution.intent.reconciliation_required`;
- execution publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Minimum paper order fact types:

- `execution.order.created`;
- `execution.order.submitted`;
- `execution.order.acknowledged`;
- `execution.order.rejected`;
- `execution.order.partially_filled`;
- `execution.order.filled`;
- `execution.order.cancel_requested`;
- `execution.order.cancelled`;
- `execution.order.replace_requested`;
- `execution.order.replaced`;
- `execution.order.expired`;
- `execution.order.unknown`;
- `execution.order.reconciled`;
- order publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Minimum fill fact types:

- `execution.fill.accepted`;
- `execution.fill.corrected`;
- `execution.fill.busted`;
- `execution.fill.duplicate_rejected`;
- fill publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Reservation consumption, release, expiry, and reconciliation facts remain `risk.*` facts owned by the reservation authority. Ledger facts remain owned by the accounting leaf.

Behavior-changing paper execution policy activation, paper broker model activation, pause/resume, stop, abort, reset, and emergency disable controls remain accepted ordered `ControlOutcome`s under `run.control.*`. Runtime mode is fixed by the run manifest and cannot be changed in place.

## Intent eligibility

Execution planning may create an executable paper order intent only when all conditions hold:

1. runtime mode is `Backtest/paper replay` or `Live paper`;
2. the source `RiskDecision` is `approved` or `modified`, active, unexpired, and unsuperseded;
3. the source `ReservationOutcome` is `accepted`;
4. the linked `Reservation` is active, unexpired, not consumed, not released, not cancelled, not invalidated, and paper-eligible;
5. the reservation's `risk_sequence` and projected-exposure basis match the decision lineage;
6. target, portfolio, account, instrument/listing, unit, and policy versions are supported by the execution policy and paper broker model;
7. required account and market-state inputs are fresh enough under execution policy;
8. kill switch and execution controls permit the intent at the selected effective position;
9. idempotency preconditions prove the same intent has not already been accepted or terminally rejected;
10. the replay class supports the execution claim being made.

Missing, stale, recovering, incompatible, or unknown evidence fails closed into an explicit intent rejection, cancellation, or reconciliation-required state according to policy. Execution planning must not infer approval from a visible target, UI card, telemetry health, or prior paper order.

## Reservation consumption and release

Paper execution cannot create or modify reservations. It requests reservation lifecycle transitions from the reservation authority using explicit causation:

- intent accepted may hold the reservation for execution;
- order acknowledged may consume some or all of the reservation under declared transfer policy;
- partial fill may consume reserved quantity and leave residual reservation active;
- order rejected before exposure is opened may release eligible reservation;
- order cancellation may release only the unfilled residual after cancellation is authoritative;
- unknown order state keeps worst-case reservation until reconciliation proves otherwise;
- intent expiry/cancellation releases or reconciles under policy;
- terminal completion consumes, releases, or reconciles exactly the covered reservation amount.

No reservation transition may be inferred solely from missing orders, lagging query projections, or process shutdown. Unknown state is a first-class safety condition.

## Paper broker behavior

The paper broker may be configured for multiple deterministic profiles, such as:

- immediate acknowledgement and fill at reference mark;
- acknowledgement latency with fill at next eligible market-state cut;
- partial fills based on declared liquidity model;
- deterministic slippage and fees;
- rejection for stale market, unsupported instrument, insufficient simulated liquidity, invalid order type, halted market, or disabled account;
- cancellation races where fill may occur before cancellation acknowledgement;
- unknown acknowledgement/fill state for recovery drills.

Every profile must define its inputs, output state machine, deterministic ordering, and replay requirements. A profile may be unrealistic if it is clearly labeled for early testing; it may not be presented as live execution evidence.

## Mode behavior

- `Replay analysis` creates no executable paper order intent unless the run manifest explicitly selects `Backtest/paper replay` semantics.
- `Backtest/paper replay` may create deterministic executable paper intents, paper orders, fills, and accounting inputs from replayed data and paper policies.
- `Live read-only` cannot create risk decisions, reservations, intents, orders, or fills.
- `Live paper` may create executable paper intents, paper orders, fills, and accounting inputs without human approval and without live adapter access.
- `Human-approved live` is out of scope for Phase 08; paper execution must not treat a live reservation awaiting approval as paper-executable.
- `Guarded automated live` is deferred and cannot be inferred from paper-execution evidence.
- `paper_candidate` remains a Phase 06 promotion state and may gate upstream portfolio/risk eligibility; it is not a runtime mode.

## Pause, stop, abort, and reset

Paper execution controls take effect only through accepted ordered `ControlOutcome`s and effective positions.

Pause:

- blocks new intents and order submissions after the effective position;
- may allow safety-critical cancellation, fill capture, reservation reconciliation, and accounting handoff to continue according to policy;
- does not erase active orders or fills.

Stop/abort:

- prevents new intents and submissions;
- attempts mode-safe cancellation where policy requires it;
- preserves unknown order states for reconciliation rather than fabricating terminal outcomes;
- closes the run only after required recovery/reconciliation status is recorded.

Reset:

- creates a new run or child run and a new paper execution/accounting epoch as declared by Phase 01/03;
- never deletes prior intents, orders, fills, reservations, ledger inputs, or audit history;
- cannot convert paper work into live work or live mode into paper mode in place.

## Persistence, recovery, and replay

Paper execution must persist or reconstruct:

- accepted and rejected intent obligations;
- idempotency keys and submission attempts;
- paper broker input/output facts;
- order lifecycle and fill facts;
- reservation transition requests and acknowledgements;
- execution-group state;
- replay seeds, model versions, draw positions, and deterministic scenario inputs;
- publication and consumer-acknowledgement facts.

Recovery rules:

- if an intent acceptance was durable but order submission was not attempted, recovery may submit once using the same idempotency key;
- if submission may have occurred but acknowledgement is unknown, recovery records unknown state and follows reconciliation policy;
- if fills were emitted but accounting did not consume them, fills are replayed to accounting with original identities;
- if duplicate broker outcomes are observed, deduplication rejects duplicates without changing original fills;
- if deterministic replay reproduces different paper broker output for the same model/seed/input, the run is failed or marked unreproducible rather than patched silently.

## Lineage and audit

Every intent, order, and fill must reconstruct:

- source recommendation, signal, feature, market-state, target, risk decision, reservation outcome, and reservation;
- portfolio/account/risk scope and `risk_sequence`;
- execution policy and paper broker model versions;
- controls and kill-switch state at the selected effective position;
- replay class, seed/draw references where applicable, and input cursor vectors;
- submission attempt and idempotency lineage;
- reservation consumption/release/reconciliation linkage;
- accounting handoff acknowledgement where applicable.

Audit must be possible without reading mutable current projections.

## Observability and performance

Observability must include:

- intent eligibility decisions and reason codes;
- intent-to-submit, submit-to-acknowledge, acknowledge-to-fill, fill-to-accounting-handoff latency segments using Phase 02 semantics;
- order lifecycle counts by mode, broker model, account, instrument, and status;
- cancellation/replacement race outcomes;
- unknown-state and reconciliation-required counts;
- duplicate-submission prevention counters;
- reservation held/consumed/released/reconciled measurements;
- deterministic replay comparison reports.

Metrics and traces are not authoritative execution facts. Missing telemetry cannot create or clear orders, fills, or reservations.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **PE-E01 — Intent eligibility suite** | Approved/rejected/modified decisions, accepted/rejected/stale reservations, expired/superseded targets, live-read-only mode, human-approved-live reservation, stale inputs | Intents are created only for active paper-eligible approved/modified decisions with accepted active reservations |
| **PE-E02 — Mode isolation suite** | `Backtest/paper replay`, `Live paper`, `Live read-only`, `Human-approved live`, adapter capability matrix | Paper intents cannot route live; live-read-only creates no intent/order/fill; live reservations awaiting approval are not paper-executable |
| **PE-E03 — Order lifecycle model tests** | Ack, reject, partial fill, fill, cancel, replace, expiry, unknown, duplicate outcomes | Lifecycle transitions are valid, terminal states are stable, unknown remains explicit |
| **PE-E04 — Reservation transfer model check** | Concurrent intents, partial fills, cancels, rejects, unknown orders, terminal completion | Reservation capacity is never double-used or silently released |
| **PE-E05 — Idempotency/retry suite** | Crashes before/after acceptance, submission, acknowledgement, fill publication, accounting handoff | Recovery never duplicates intents, orders, fills, or reservation consumption |
| **PE-E06 — Paper broker determinism suite** | Model versions, seeds, replay inputs, stochastic profiles, corrected/faithful replay cases | Same declared inputs reproduce identical semantic order/fill outcomes |
| **PE-E07 — Control effective-position suite** | Pause, resume, stop, abort, reset, kill switch around intent/order cuts | Controls affect only post-effective work and preserve audit history |
| **PE-E08 — Audit reconstruction drill** | End-to-end reconstruction from target through fill | Audit reaches all lineage, model versions, controls, idempotency keys, and reservation transitions |
| **PE-E09 — Observability/performance suite** | Latency profiles, health loss, missing metrics, high-throughput replay | Measurements are non-authoritative and budgets are evidence-derived |

## Phase exit criteria

Paper order-simulation planning is complete when:

- executable paper intent semantics and eligibility are unambiguous;
- mode boundaries prohibit live routing and human-approval leakage;
- paper order and fill lifecycle facts are immutable, replayable, and recoverable;
- reservation consumption/release/reconciliation handoff is explicit and safe under unknown states;
- idempotency and recovery rules prevent duplicate execution facts;
- paper broker models are versioned and deterministic under declared inputs;
- accounting receives fills as immutable inputs without paper execution mutating ledger state.

## Deferred decisions

Deferred to later implementation or phases:

- first concrete paper broker model and fill profile defaults;
- exact order types and time-in-force values in the first release;
- detailed execution algorithms beyond safe target-to-order translation;
- production-like live-paper soak targets, handled in Phase 10;
- live venue adapter contracts, handled in Phase 11.
