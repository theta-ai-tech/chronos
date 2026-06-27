# Phase 07: Risk and Reservations

## Purpose

This document defines the planning contract for Chronos's risk-policy and exposure-reservation path: the components that consume portfolio-scoped `TargetPosition`s, decide whether those targets are allowed, and serialize projected exposure before any later execution authority may create executable order intents.

This leaf builds on approved Phases 01-06 and the companion Phase 07 `portfolio-construction.md` leaf. It preserves the Phase 01 domain chain:

```text
TradeRecommendation
  -> TargetPosition
  -> RiskDecision
  -> ReservationOutcome / Reservation
  -> later execution phase
```

Risk approval and reservation are safety gates. They do not create orders, fills, account balances, ledger entries, P&L, or live venue submissions. A recommendation is not executable. A target is not executable. A risk decision is not executable unless the reservation path accepts capacity and a later execution phase consumes it under its own mode-specific rules.

## Objectives

Risk and reservation planning must establish that Chronos can:

1. evaluate each eligible `TargetPosition` against immutable risk policies, portfolio/account snapshots, control state, market-state lineage, kill-switch state, and mode restrictions;
2. terminate each admitted risk obligation exactly once as either one semantic `RiskDecision`, one typed fail-closed unavailable disposition, or one unrecoverable incomplete/failed run-scope disposition under Phase 03 recovery rules;
3. distinguish approved, rejected, modified, unavailable, and operationally interrupted risk obligations without hidden fallbacks;
4. allow risk modification only when the modified target is equal-or-less-risky under the declared policy;
5. serialize projected exposure with authoritative `risk_sequence` values before downstream execution can consume approved capacity;
6. emit exactly one `ReservationOutcome` for each reservation request: `accepted`, `rejected`, or `stale`;
7. ensure the reservation authority never issues, modifies, or replaces a `RiskDecision`;
8. prevent double use of exposure across concurrent targets, unknown orders, active reservations, open execution groups, and unresolved fills;
9. fail closed for stale market state, stale portfolio snapshots, missing account/capital evidence, missing kill-switch evidence, incompatible lineage, unavailable risk policy, or degraded execution mode;
10. preserve exact replay, recovery, audit, and explanation lineage from source data through target, risk, reservation, and later execution handoff;
11. expose non-authoritative observability and performance evidence for risk and reservation while keeping safety decisions domain-owned;
12. leave paper broker simulation, order lifecycle, fills, ledger, accounting, and human-approved live execution to later phases.

## Scope

### In scope

- risk-scope, risk-policy, limit-set, kill-switch, exposure-model, and reservation-policy definitions;
- admitted risk obligations from accepted portfolio targets;
- target eligibility, target freshness, lineage, and downstream mode checks;
- `RiskDecision` semantics: approved, rejected, and modified;
- fail-closed risk-obligation dispositions for unavailable evaluations that do not produce `RiskDecision`s;
- runtime-interruption audit/recovery facts that are non-terminal unless recovery fails and Phase 03 marks the affected run scope incomplete or failed;
- modified-risk constraints and proof that modifications cannot increase risk under the policy;
- projected-exposure state, risk serialization partitions, `risk_sequence`, and reservation identity;
- `ReservationOutcome` semantics: accepted, rejected, and stale;
- reservation lifecycle: active, consumed, released, expired, cancelled by lifecycle, invalidated, and reconciled;
- concurrent target handling, exposure double-use prevention, unknown-order handling, and multi-leg execution-group placeholders;
- deterministic scheduling, ordered controls, pause/stop/reset behavior, recovery, and replay;
- risk/reservation lineage, explanations, audit records, observability, performance workloads, and evidence gates.

### Out of scope

- portfolio construction, recommendation aggregation, strategy evaluation, or target creation;
- executable order-intent creation, order routing, paper broker simulation, fills, marks, ledger, accounting, P&L, reconciliation, or tax;
- human approval workflow for live execution, except preserving the upstream reservation state that Phase 11 must consume;
- live venue submission, account credentials, exchange acknowledgement, cancellation, replacement, or order slicing;
- exact production risk limits, final capital values, trading formulas, alpha claims, or legal/compliance policy text;
- operator override UI, manual risk approvals, or emergency execution workflows unless a later phase defines them as commands;
- concrete storage products, process topology, programming language, scheduler implementation, or ticket-level implementation plan.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Portfolio-construction authority | Accepted, published, active `TargetPosition`s, no-change outcomes, construction rejections, target validity, and target lifecycle | Risk approval, projected exposure, reservations, execution authorization |
| Risk-policy authority | Risk policy, limit interpretation, target risk evaluation, approved/rejected/modified `RiskDecision`s, fail-closed risk-obligation dispositions, risk explanations, and risk-decision lifecycle | Reservation serialization, reservation outcome issuance, executable intents, fills, ledger |
| Exposure/reservation authority | Serialized projected-exposure state, `risk_sequence`, reservation requests, `ReservationOutcome`s, active reservations, release/expiry/consumption state, and reservation audit | Issuing or changing `RiskDecision`s, creating targets, creating executable intents |
| Run/configuration authority | Risk-policy activation, mode, risk-scope binding, kill-switch controls, pauses, stops, resets, and behavior-changing controls through ordered `ControlOutcome`s | Risk values, reservation values, target values, order state |
| Account/capital state owner | Authoritative account, capital, holdings, open-order, and later accounting snapshots exposed through approved state contracts | Risk-policy semantics or reservation issuance |
| Stream/run-input authority | Ordered controls, timer facts, effective positions, and selected run-input cuts | Risk interpretation, exposure arithmetic, reservation capacity |
| Market-state/reference authorities | Listing/reference/market-state lineage used to validate target and risk interpretation | Risk decisions, reservations, order authorization |
| Execution authority | Later consumption of accepted reservation and approved risk decision to create mode-valid executable intents | Risk evaluation, reservation issuance, capacity serialization |
| Observability | Non-authoritative metrics, traces, logs, health projections, latency points, and evidence export | Creating safety facts, approving risk, inferring missing reservation state |
| Query/report/UI tools | Disposable projections of targets, risk decisions, reservations, and safety state | Authoritative risk state, reservation state, or behavior-changing controls |

Risk and reservation may initially run in one process, but they are separate authorities. The risk-policy authority decides whether a target is acceptable. The reservation authority serializes capacity and emits reservation outcomes. This separation is mandatory because a stale reservation returns to risk for a new decision rather than mutating the prior decision.

## Canonical concepts

### Risk scope

A `RiskScope` defines the boundary over which limits and capacity are evaluated. It may cover:

- one portfolio;
- one account or sleeve;
- one canonical instrument or listing;
- one opportunity or execution group placeholder;
- one venue or market class;
- a strategy group, family, or budget;
- a global run-level safety envelope.

A risk scope record contains:

- `risk_scope_id`;
- portfolio/account/sleeve binding;
- mode: `Replay analysis`, `Backtest/paper replay`, `Live read-only`, `Live paper`, `Human-approved live`, or later `Guarded automated live`, with `paper_candidate` represented separately as a Phase 06 promotion state rather than a runtime mode;
- eligible target-key schemas and instruments/listings;
- limit-set identity and version;
- exposure-model identity and version;
- reservation partitioning policy;
- kill-switch and stale-data policy references;
- snapshot freshness requirements;
- reset and lifecycle lineage.

Risk scope is not a portfolio, account, order book, venue account, or UI watchlist. It is the safety boundary for one or more decisions.

### Risk policy and limit set

A `RiskPolicy` is an immutable semantic contract for evaluating targets. It references one or more `LimitSet`s and declares:

- target eligibility rules;
- maximum and minimum exposure constraints;
- concentration, notional, quantity, leverage, drawdown-proxy, and per-instrument constraints where applicable;
- stale market-state and stale portfolio/account snapshot behavior;
- market halted, locked/crossed, missing reference, and unavailable liquidity behavior;
- policy for pending targets, active reservations, unknown orders, unresolved fills, and execution groups;
- kill-switch interpretation;
- allowed modification rules;
- rejection and fail-closed unavailable reason taxonomy;
- deterministic arithmetic, unit conversion, rounding, and canonicalization;
- evidence required for approval;
- compatibility, migration, deprecation, and owner/review evidence.

Changing limit meaning, stale-data behavior, modification semantics, exposure model, kill-switch interpretation, or arithmetic policy is a semantic policy version change.

### Risk obligation

A `RiskObligation` is the accepted request to evaluate one active `TargetPosition` under one risk scope and policy cut. It contains:

- target identity and target semantic checksum;
- portfolio/account/sleeve and risk-scope identity;
- active risk-policy, limit-set, and exposure-model versions;
- target validity and publication/consumer-acknowledgement state;
- portfolio-state snapshot and account/capital snapshot references;
- market-state/reference/listing lineage needed for valuation and eligibility;
- active reservation/open-order/projected-exposure snapshot references;
- kill-switch and mode-control state at the selected effective position;
- timer cursor and logical-time basis;
- replay class and run identity.

If a target is expired, superseded, unacknowledged, malformed, or outside risk scope before admission, the risk authority records `risk.obligation.admission_rejected` rather than a `RiskDecision`. Once a risk obligation is accepted, it must terminate exactly once.

### Risk decision

A `RiskDecision` is the risk-policy authority's immutable semantic decision for one accepted risk obligation. The allowed semantic dispositions are:

- `approved`: the target may proceed to reservation request exactly as approved;
- `rejected`: the target is not allowed to proceed;
- `modified`: a constrained target may proceed to reservation request only as modified.

Risk could be unable to evaluate because required evidence was missing, stale, recovering, incompatible, or unknown. That is recorded as a fail-closed `risk.obligation.unavailable` disposition, not as a `RiskDecision`. It produces no approved target and no reservation request.

Operational host/runtime interruption is not a semantic decision and is not encoded as rejected, modified, approved, or unavailable. `risk.runtime.interrupted` is a non-terminal recovery/audit fact while recovery remains possible. If recovery succeeds, the original accepted obligation continues under the same identity and eventually terminates as one semantic `RiskDecision` or one `risk.obligation.unavailable` disposition. If recovery cannot complete, Phase 03 marks the affected run scope incomplete or failed, and that unrecoverable disposition is the terminal non-decision outcome for the obligation.

Each accepted risk obligation therefore terminates exactly once as one of:

- a semantic `RiskDecision` with disposition `approved`, `rejected`, or `modified`;
- `risk.obligation.unavailable` with fail-closed reason and lineage;
- an unrecoverable incomplete/failed run-scope disposition under Phase 03, causally linked to one or more `risk.runtime.interrupted` facts.

The unavailable, interrupted, incomplete, or failed paths may not fabricate rejected decisions merely to satisfy decision cardinality. Downstream reservation requests are allowed only from approved or modified semantic `RiskDecision`s.

A `RiskDecision` records:

- requested target and approved/modified target, if any;
- policy, limit-set, exposure-model, arithmetic, and canonicalization versions;
- projected-exposure identity evaluated by risk;
- projected-exposure `risk_sequence` or serialized exposure position evaluated by risk;
- positions, balances, active reservations, pending decisions, open orders, unknown orders, unresolved execution groups, and worst-case exposure effects used by the policy;
- data freshness, kill-switch, mode, and system-health evidence used as domain inputs;
- all triggered rules, including the binding rule;
- decision issue position/time, validity, expiry, and replay class;
- deterministic reason codes and ranked explanations.

A modified decision must prove:

- the original target and modified target share the same risk obligation lineage;
- the modified target remains within the same target key or declared policy-compatible reduced scope;
- every changed amount, unit, leg, or bound is equal-or-less-risky under the policy's partial-order proof;
- no modification increases gross, net, notional, leverage, concentration, liquidity, or execution-group risk unless the policy proves that dimension is irrelevant for the specific target;
- any increase requires a new portfolio target and new risk obligation.

Rejected decisions and unavailable obligation dispositions produce no reservation request. Approved and modified decisions may request reservation, subject to the reservation authority's serialization.

### Projected exposure

`ProjectedExposure` is the reservation authority's serialized view of capacity already consumed or potentially consumed by active decisions and unresolved downstream work.

It includes:

- risk-scope identity;
- `risk_sequence`;
- active reservations;
- approved but not yet reserved decisions where policy treats them as pending;
- executable intents accepted by later phases;
- open orders, partially filled orders, unknown-order states, and unresolved execution groups when later phases introduce them;
- released, expired, consumed, cancelled, and reconciled capacity records;
- semantic checksum and reconstruction lineage.

Projected exposure is not portfolio construction, accounting balance, venue balance, or UI position. It is a safety model for preventing overuse of risk capacity.

### Reservation request

A `ReservationRequest` asks the reservation authority to reserve capacity for one approved or modified `RiskDecision`.

It contains:

- exactly one risk decision identity;
- target identity and modified target identity where applicable;
- risk scope and reservation partition key;
- requested exposure/capacity amount and units;
- risk-decision expiration and reservation TTL policy;
- expected later execution mode;
- execution-group placeholder if relevant;
- projected-exposure precondition, including the exact `risk_sequence` or serialized exposure position recorded by the source `RiskDecision`;
- idempotency key and causation chain.

A reservation request cannot exist without an approved or modified risk decision. It cannot be created for a rejected decision, unavailable obligation disposition, obligation that is still in recovery after interruption, incomplete/failed obligation, expired decision, or superseded decision.

### Reservation outcome

A `ReservationOutcome` is the reservation authority's exact terminal response to one reservation request. It is exactly one of:

- `accepted`: capacity was reserved and an active `Reservation` exists;
- `rejected`: capacity was not available or the request violated reservation policy;
- `stale`: the referenced risk decision, target, projected-exposure precondition, or reservation basis is stale and must return to risk-policy evaluation if work should continue.

Every request receives exactly one outcome. The reservation authority does not emit a `RiskDecision`. A stale outcome does not edit the prior risk decision; it terminates the reservation request and requires a new risk obligation if the target should be reconsidered.

### Reservation

A `Reservation` is the active capacity hold created by an accepted `ReservationOutcome`. It contains:

- reservation identity;
- reservation outcome identity;
- risk decision identity;
- target identity;
- risk scope and partition;
- reserved exposure amount and units;
- `risk_sequence` at acceptance;
- TTL/expiry and release policy;
- downstream consumer eligibility, including mode-specific paper eligibility, human-approved-live awaiting-approval eligibility, or later guarded-automation eligibility;
- lifecycle state and semantic checksum.

Allowed lifecycle states:

```text
active
  -> awaiting_human_approval (human-approved live only)
  -> consumed
  -> released
  -> expired
  -> cancelled_by_control
  -> invalidated_by_run_lifecycle
  -> reconciliation_required
```

The concrete state machine may refine intermediate states, but it must not allow double consumption, release without identity, reuse after terminal state, or executable-live consumption from a human-approved-live reservation before a valid `HumanExecutionApproval` fact is linked.

## Fact taxonomy activated by this leaf

This leaf activates the Phase 03 `risk.*` namespace additively. Reservation outcomes, reservation state, release, and expiry facts are part of `risk.*` as already reserved by Phase 01 and Phase 03. This leaf does not activate a separate `reservation.*` domain namespace and does not activate `order.*`, `fill.*`, `ledger.*`, or live execution semantics.

Minimum risk fact types:

- `risk.scope.registered`;
- `risk.policy.registered`;
- `risk.limit_set.registered`;
- `risk.obligation.admission_rejected`;
- `risk.obligation.accepted`;
- `risk.decision.approved`;
- `risk.decision.rejected`;
- `risk.decision.modified`;
- `risk.obligation.unavailable`;
- `risk.runtime.interrupted`;
- `risk.decision.expired`;
- `risk.decision.superseded`;
- `risk.decision.invalidated_by_control`;
- `risk.decision.terminal`;
- risk publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Minimum reservation-related `risk.*` fact types:

- `risk.reservation_request.accepted`;
- `risk.reservation_outcome.accepted`;
- `risk.reservation_outcome.rejected`;
- `risk.reservation_outcome.stale`;
- `risk.reservation.created`;
- `risk.reservation.awaiting_human_approval`;
- `risk.reservation.consumed`;
- `risk.reservation.released`;
- `risk.reservation.expired`;
- `risk.reservation.cancelled_by_control`;
- `risk.reservation.invalidated_by_run_lifecycle`;
- `risk.reservation.reconciliation_required`;
- risk publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Behavior-changing risk-policy activation, kill-switch toggles, pause/resume, stop, abort, reset, and operator emergency controls remain authoritative only as accepted ordered `ControlOutcome`s under `run.control.*`. Runtime mode is not a behavior-changing control: it is fixed by the run manifest, and changing canonical mode requires a new run or child run with fresh manifest and recovery lineage. Risk and reservation controls may only activate mode-compatible policy inside the existing run mode.

Metrics, traces, logs, query rows, UI cards, comments, notebooks, or imported spreadsheets are not authoritative risk or reservation facts.

## Target eligibility and risk admission

Risk may consume a `TargetPosition` only when all conditions hold:

1. the target is accepted and published by portfolio construction;
2. the risk authority has acknowledged the target publication boundary required by Phase 03;
3. the target is active, unexpired, unsuperseded, and not invalidated at the selected cut;
4. the target's portfolio/account/sleeve/mode is bound to one risk scope;
5. the target has complete recommendation, strategy, feature, state, reference, timer, control, and portfolio-snapshot lineage;
6. the active risk policy supports the target-key schema and target unit;
7. required account/capital/holding/open-order/projected-exposure snapshots are fresh enough under policy;
8. kill-switch and mode-control evidence is present and permits admission;
9. no required upstream authority is recovering, unknown, or degraded beyond policy;
10. the replay class supports the claim being made.

Missing, stale, or unknown evidence fails closed. The risk authority must not infer safety from absent data, UI projections, telemetry health, query-model rows, or prior approvals.

`Live read-only` is excluded from authoritative risk admission. In that mode, proposed targets may be projected for operators, but the risk authority must not accept risk obligations, emit authoritative `RiskDecision`s, create reservation requests, or create `ReservationOutcome`s or `Reservation`s. Any risk-like display in live read-only is a query/report projection and must be labeled non-authoritative.

## Risk evaluation

Risk evaluation is deterministic over the admitted obligation key. It may use only:

- the target and target lineage;
- active risk policy and limit set;
- approved snapshots for portfolio/account/capital/holdings/open orders/projected exposure;
- market-state/reference/listing lineage required to value and validate the target;
- accepted control state, kill-switch state, mode, and timer facts;
- declared deterministic seeds where a policy explicitly requires them.

It may not use:

- host wall time;
- storage iteration order;
- UI state;
- telemetry values as safety evidence;
- unapproved query projections;
- credentials or broker screens;
- future fills/orders not present in the selected snapshot;
- external observations unless a later phase has made them lineage-bearing inputs.

Risk decisions and fail-closed obligation dispositions must carry stable reason codes and deterministic explanations for every binding limit, stale/unavailable input, modification, rejection, approval, or inability to evaluate.

## Exposure serialization and reservation

The reservation authority serializes by risk partition. The partitioning policy declares whether serialization is by:

- portfolio;
- account/sleeve;
- canonical instrument;
- venue/listing;
- opportunity/execution group;
- global run;
- another registered risk-scope key.

For each partition, the reservation authority maintains an authoritative sequence:

```text
risk_sequence_n
  -> reservation request admitted
  -> projected exposure checked
  -> ReservationOutcome emitted
  -> projected exposure advanced to risk_sequence_n+1
```

Equivalent implementations may batch or shard work only if the resulting sequence, outcomes, projected exposure, and audit lineage are semantically identical to the declared partition policy.

Reservation must account conservatively for:

- active reservations;
- pending or accepted executable intents once later phases introduce them;
- open orders;
- partially filled orders;
- unknown order states;
- unresolved cancellation/replacement states;
- multi-leg execution groups;
- delayed fills and late venue evidence;
- reservations whose release has not become authoritative.

When later execution/accounting facts are not yet available in Phase 07, their absence is represented by explicit placeholders or mode restrictions. The reservation authority must not assume capacity was released because an order is missing from a query view.

## Kill switch and mode behavior

Kill switch state is an ordered control/input fact, not a metric or UI toggle. Risk must evaluate the kill-switch state at the selected effective position. Execution phases must later recheck safety because approval can expire or be invalidated between reservation and execution.

Mode rules:

- Runtime mode is inherited from the immutable run manifest. Risk and reservation must not treat a `ControlOutcome`, configuration patch, UI action, or recovery event as an in-place transition between canonical modes; switching modes requires a new run or child run.
- `Replay analysis` may evaluate historical or hypothetical risk checks only when the run manifest explicitly activates that domain path; it creates no executable intent unless paper simulation is separately enabled by `Backtest/paper replay`;
- `Backtest/paper replay` may create deterministic paper risk decisions, reservation outcomes, reservations, and later paper-path evidence, but no live adapter or human-approval workflow;
- `Live read-only` may observe and project through proposed targets as permitted by Phase 01, but cannot admit risk obligations, emit authoritative `RiskDecision`s, create reservation requests, create reservation outcomes/reservations, executable intents, paper/live orders, or fills;
- `Live paper` may create paper risk decisions and accepted reservations for later paper execution without human approval and without live venue submission;
- `Human-approved live` may create live risk decisions and accepted reservations, but an accepted reservation enters an awaiting-human-approval workflow state or equivalent mode-specific eligibility flag. No executable live intent exists until Phase 11 defines and records `HumanExecutionApproval` against that reservation/workflow;
- `Guarded automated live` is deferred and cannot be inferred from human-approved live evidence;
- `paper_candidate` is a Phase 06 research promotion state that may gate portfolio/risk eligibility; it is not a runtime mode.

If mode evidence is missing or incompatible, risk and reservation fail closed.

## Pause, stop, abort, and reset

Risk and reservation lifecycle changes are behavior-changing controls. They take effect only through accepted ordered `ControlOutcome`s and recorded effective positions.

Pause:

- blocks new post-effective risk admissions or reservation requests according to policy;
- does not mutate prior decisions or reservations;
- may allow active reservations to expire or be released under their lifecycle policy;
- preserves pending accepted obligations for recovery or explicit incomplete disposition.

Stop/abort:

- prevents new admissions after the effective position;
- terminally resolves or marks incomplete any accepted obligations under Phase 03 recovery policy;
- invalidates or cancels eligible active decisions/reservations only through explicit lifecycle facts;
- cannot fabricate rejections or releases to hide incomplete work.

Reset:

- creates a new risk/reservation epoch or child run as declared by Phase 03;
- preserves all previous decisions, reservations, releases, and audit history;
- requires fresh risk-scope, policy, snapshot, kill-switch, and projected-exposure lineage before new approvals;
- never reuses active reservations across reset boundaries unless an explicit migration/reconciliation fact proves safety.

## Persistence, recovery, and replay

Risk and reservation authorities must persist or reconstruct:

- risk scopes, policies, limit sets, exposure models, and reservation policies;
- accepted risk obligations and risk-obligation keys;
- terminal risk decisions and lifecycle facts;
- reservation requests, outcomes, reservations, and lifecycle facts;
- projected-exposure sequence history and semantic checksums;
- selected snapshots and their freshness/completeness statuses;
- kill-switch/mode/control effective-position evidence;
- explanations, reason codes, and audit references;
- publication and consumer-acknowledgement lifecycle facts;
- runtime interruption and recovery dispositions.

Same-run recovery must reproduce accepted decision identities, reservation identities, sequence positions, and pending publication obligations exactly where identities were already accepted. Independent equivalent runs compare deterministic identities only if the identity policy declares deterministic derivation; otherwise they compare semantic checksums, lineage, decision disposition, reserved amount, sequence behavior, and explanation equivalence.

Faithful replay must use original risk policies, limit sets, exposure models, reservation policies, controls, timer facts, target facts, snapshots, projected-exposure history, execution feedback facts where applicable, and replay manifest. Current risk limits, current account state, or patched reservation releases cannot be substituted and still called faithful replay.

## Lineage and explanations

Every risk decision and reservation outcome carries causal lineage.

Risk-decision explanations include:

- source target and portfolio snapshot;
- active risk scope, policy, limit set, and exposure model;
- account/capital/holding/open-order snapshots;
- projected-exposure identity and `risk_sequence` evaluated by the decision;
- market/reference/listing lineage used for valuation;
- kill-switch and mode-control state;
- each binding limit and computed value;
- modification proof where applicable;
- rejection reason codes and any unavailable-obligation references that prevented a decision;
- timer basis and effective control position.

Reservation explanations include:

- source risk decision;
- requested and reserved exposure;
- risk partition and `risk_sequence`;
- projected-exposure state before and after;
- competing reservations, open orders, unknown orders, or execution groups considered;
- accepted/rejected/stale reason;
- TTL/expiry/release policy.

Explanation ranking is deterministic and versioned. Equal ranks use stable tie-breakers. Human-readable explanations must not hide influential safety contributors or collapse stale/unknown evidence into generic failure text.

## Observability and performance

Observability is non-authoritative. It may report:

- risk obligations admission-rejected, accepted, approved, rejected, modified, unavailable, interrupted/recovering, incomplete, failed, expired, and superseded;
- reservation requests, accepted/rejected/stale outcomes, active reservations, releases, expiries, and stale-return loops;
- kill-switch state projection and missing-evidence status;
- stale snapshot rates, unavailable input populations, modification rates, and rejection reason distributions;
- queue wait, computation, serialization wait, publication, consumer acknowledgement, and recovery latency;
- projected-exposure sequence gaps, backlog, and replay mismatch alerts.

It may not infer approval or reservation from missing alerts. Missing safety evidence is `unknown` and fails closed at the domain boundary.

Canonical Phase 02 endpoint adoption:

- parent `portfolio_risk` segment remains `trade_recommendation.published.portfolio_authority` to `risk_decision.accepted`;
- companion portfolio-construction subsegment ends at `portfolio_target.accepted`;
- this leaf defines `portfolio_target.published.risk_authority` to `risk_decision.accepted`;
- reservation subsegment runs from `risk_decision.published.reservation_authority` to `reservation_outcome.accepted`;
- publication, recoverability, sequence advancement, and consumer acknowledgement are explicit lifecycle points.

Performance evidence follows Phase 02. Phase 07 must register workloads for:

- concurrent targets in one risk partition;
- many partitions with independent sequences;
- high rejection/unavailable rates;
- stale reservation loops;
- kill-switch toggles under backlog;
- unknown-order conservative exposure;
- multi-leg execution-group placeholder accounting;
- recovery after crash between risk approval and reservation outcome;
- replay throughput and externally paced paper-mode arrival.

## Testing strategy

### Contract and unit tests

Cover:

- risk-scope, policy, limit-set, exposure-model, and reservation-policy schema validation;
- target eligibility and authoritative admission rejection;
- approved, rejected, modified, unavailable-obligation, operationally interrupted/recovered, and unrecoverable incomplete/failed risk obligations;
- equal-or-less-risky modification proofs;
- kill-switch and mode fail-closed behavior;
- projected-exposure sequence advancement;
- accepted, rejected, and stale reservation outcomes;
- reservation lifecycle transitions;
- target/risk/reservation publication and consumer acknowledgement.

### Property and model-based tests

Generate:

- concurrent targets competing for capacity;
- mixed approvals, modifications, rejections, unavailable inputs, and stale snapshots;
- reservation requests racing with release, expiry, and projected-exposure changes;
- kill-switch controls immediately before, at, and after evaluation cuts;
- pause, stop, abort, reset, crash, and replay permutations;
- unknown orders, partial fills, cancelled orders, and execution groups as placeholder facts;
- arithmetic boundary cases and unit conversions.

Properties include:

1. each accepted risk obligation terminates exactly once as either one semantic `RiskDecision`, one unavailable-obligation disposition, or one unrecoverable incomplete/failed run-scope disposition; recoverable `risk.runtime.interrupted` facts do not count as terminal outcomes;
2. rejected decisions and unavailable-obligation dispositions never create reservation requests;
3. modified decisions never increase risk under the policy proof;
4. each reservation request receives exactly one outcome;
5. reservation authority never emits or edits a `RiskDecision`;
6. active capacity is never double-used within a risk partition;
7. stale reservation outcomes require a new risk decision before retry;
8. missing kill-switch, market-state, account, or projected-exposure evidence fails closed;
9. replay reproduces sequence behavior and semantic outcomes under the manifest;
10. telemetry absence cannot prove safety.

### Recovery and replay tests

Cover crash/restart at each boundary:

- after risk obligation accepted before decision;
- after decision before publication;
- after approved/modified decision before reservation request;
- after reservation request before outcome;
- after accepted reservation before publication;
- after release/expiry before projected-exposure publication;
- during kill-switch or reset controls.

Recovery must either reproduce the accepted identity and continue, or mark the affected run scope incomplete/failed under Phase 03. It must not create duplicate decisions, duplicate reservations, phantom releases, or sequence gaps.

## Evidence gates

| Gate | Evidence | Passing condition |
|---|---|---|
| **PR-E01 — Risk terminology matrix** | Target, risk, reservation, execution, account, and accounting terms mapped to canonical Phase 01 vocabulary | No target/recommendation/risk/reservation/order/fill/ledger conflation remains |
| **PR-E02 — Risk obligation lifecycle suite** | Admission-rejected, accepted, approved, rejected, modified, unavailable-obligation, interrupted-and-recovered, unrecoverable-incomplete, unrecoverable-failed, expired, superseded, and invalidated cases | Every accepted obligation terminates exactly once as either one semantic `RiskDecision`, one unavailable-obligation disposition, or one unrecoverable incomplete/failed run-scope disposition; recoverable interruption is non-terminal; non-admitted inputs have authoritative `risk.obligation.admission_rejected` facts |
| **PR-E03 — Modification proof suite** | Generated target modifications across units, sides, instruments, and limits | Modifications are equal-or-less-risky or rejected; increases require new target/risk obligation |
| **PR-E04 — Reservation cardinality suite** | Accepted/rejected/stale outcomes, duplicate requests, retries, idempotency, stale returns | Each request has exactly one outcome; stale never edits the prior decision |
| **PR-E05 — Exposure serialization model check** | Concurrent targets, active reservations, releases, unknown orders, partial fills, open groups, and sequence races | Capacity is never double-used; projected exposure advances deterministically |
| **PR-E06 — Fail-closed safety suite** | Missing/stale kill-switch, market, account, target, projected-exposure, policy, and mode evidence | Risk/reservation refuse unsafe progress with explicit reason codes |
| **PR-E07 — Control effective-position suite** | Policy change, kill switch, pause/resume, stop, abort, reset around selected cuts | Decisions and reservations use exactly the configuration effective at their positions |
| **PR-E08 — Recovery and replay suite** | Crash points, publication failures, sequence recovery, faithful replay, independent equivalent replay | Same-run recovery preserves accepted identities; replay reproduces semantic outcomes |
| **PR-E09 — Mode-boundary suite** | `Replay analysis`, `Backtest/paper replay`, `Live read-only`, `Live paper`, `Human-approved live`, `Guarded automated live`, plus `paper_candidate` as a separate promotion-state gate | No live/paper intent is created here; live read-only creates no authoritative risk/reservation; live paper has no human approval; human-approved live accepted reservations enter awaiting-human-approval workflow/eligibility state and still wait for Phase 11 approval before executable intent |
| **PR-E10 — Observability and performance suite** | Workloads, traces, metrics, alerts, profiles, missing telemetry cases | Measurements are non-authoritative; missing safety evidence is unknown/fail-closed; budgets are evidence-derived |
| **PR-E11 — Audit reconstruction drill** | End-to-end reconstruction from target to risk decision to reservation outcome/reservation | Audit reaches all lineage, versions, controls, snapshots, risk sequence, explanations, actor/system, and lifecycle facts without mutable-current lookups |

## Phase exit criteria

Phase 07 risk/reservation planning is complete when:

- target eligibility, risk obligation, risk decision, reservation request, reservation outcome, reservation, projected exposure, and lifecycle semantics are unambiguous;
- risk-policy and reservation authorities remain separate, with reservation prohibited from issuing or editing risk decisions;
- every accepted risk obligation and reservation request has exact cardinality and terminal disposition rules, without treating recoverable runtime interruptions as terminal or fabricating semantic risk decisions for unrecoverable interruptions;
- fail-closed behavior is explicit for stale, missing, unknown, recovering, degraded, or incompatible inputs;
- exposure serialization and `risk_sequence` behavior can be model checked before execution phases consume it;
- recovery and replay rules preserve identities, sequence, lineage, and semantic outcomes;
- observability/performance requirements are evidence-based and non-authoritative;
- deferred execution, paper broker, ledger, accounting, human approval, and live automation choices cannot weaken the risk/reservation invariants.

## Deferred decisions

Deferred to later implementation or later phases:

- exact numerical risk limits and production capital values;
- initial risk partitioning granularity;
- exact unit conversion providers and valuation marks;
- final reservation TTL defaults;
- first paper execution adapter and fill model;
- order slicing, replacement, cancellation, and execution-group implementation;
- account/ledger reconciliation and mark-to-market accounting;
- human approval UX, operator override matrix, and live execution adapter;
- guarded automated live execution policy.

None of these deferred choices may weaken the cardinality rules, fail-closed behavior, risk/reservation authority split, exposure serialization, or prohibition on executable intents before later execution phases explicitly consume an accepted reservation under the correct mode.
