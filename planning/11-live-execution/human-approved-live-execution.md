# Phase 11B: Human-Approved Live Execution

## Purpose

This document defines Chronos's human-approved live-execution planning contract. It introduces live credentials, live execution adapters, human execution approval, executable live intents, external submission, live acknowledgements, live fills, ambiguous-order handling, live reconciliation, and bounded capital rollout.

Human-approved live execution does not automate final live authorization. A live intent exists only after target construction, risk approval, accepted reservation, and valid `HumanExecutionApproval` are all present and recoverable according to the live-execution fence.

## Objectives

Human-approved live execution must establish that Chronos can:

1. isolate live credentials and live adapter access from replay, research, live-read-only, and live-paper runtimes;
2. accept/reject/revoke/expire human execution approvals through a stronger authenticated approval authority;
3. create executable live intents only after active approved/modified risk decision, accepted reservation, and valid approval;
4. recheck kill-switch, reservation, approval, target, adapter, account, and execution-health state immediately before external submission;
5. submit live orders idempotently and safely through venue adapters;
6. ingest live acknowledgements, rejections, cancellations, replacements, fills, busts, corrections, and ambiguous outcomes;
7. keep unknown external order state worst-case reserved until reconciliation proves otherwise;
8. post live execution/accounting facts only under live-specific ledger/reconciliation policy;
9. operate bounded capital rollout with explicit limits, approvals, runbooks, and rollback;
10. prove write-ahead, recovery, audit, replay, and incident reconstruction sufficient for real-money risk before any guarded automation.

## Scope

### In scope

- human execution approval authority and `execution.approval.*` facts;
- live credential isolation, secret handling, adapter capability checks, and execution-zone boundaries;
- live executable intent eligibility and pre-submit recheck;
- live venue adapter order submission, acknowledgements, cancellations, replacements, fills, corrections, busts, and ambiguous states;
- live reservation transfer, hold, consumption, release, and reconciliation;
- live execution write-ahead, idempotency, recovery, and audit;
- live accounting/reconciliation extension and bounded capital rollout;
- runbooks, incident workflows, and evidence gates for human-approved live execution.

### Out of scope

- external/Polymarket discovery semantics, owned by Phase 11A;
- unconstrained or guarded automated approval substitution, owned by optional Phase 11C;
- broad production brokerage/custody integrations beyond the selected bounded adapter scope;
- tax/regulatory reporting and legal sign-off;
- final implementation ticket breakdown.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Operator identity/approval authority | HumanExecutionApproval, rejection, revocation, expiry, actor auth, anti-replay evidence | Live-paper approval workflow or strategy/risk assessment |
| Execution planning | Executable live intents after all live requirements are satisfied | Risk decisions, reservation outcomes, approval facts |
| Venue execution adapter | Live submission, external IDs, acknowledgements, cancellation/replacement/fill ingestion, ambiguous outcome handling | Strategy, target, risk, approval, reservation, accounting |
| Exposure/reservation authority | Live reservation state, transfer, consumption, release, reconciliation-required state | Live order/fill truth or risk decisions |
| Ledger/accounting/reconciliation | Live ledger postings, positions, P&L, reconciliation with venue/custody evidence under live policy | Live submission or fill invention |
| Credential/secret authority | Secret storage, access policy, rotation, redacted identity/version evidence | Strategy or execution decisions |
| Operations/API/console | Approval command submission, live command status, incident workflows, evidence bundles | Creating approval facts or mutating orders/fills |
| Observability/audit | Live execution health, latency, alerts, audit evidence, redaction | Authoritative execution state |

## Human execution approval

A `HumanExecutionApproval` is an immutable fact required only in `Human-approved live` mode. It contains:

- `execution_approval_id`;
- actor identity, authentication strength, authorization policy, and anti-replay evidence;
- linked accepted reservation, `ReservationOutcome`, risk decision, target, portfolio, account, and prospective action;
- approved quantity/notional/unit bounds, venue/account eligibility, price/slippage/time constraints, and expiry;
- approval reason and optional incident/ticket reference;
- approval status: accepted, rejected, revoked, expired;
- validity interval and replay/audit classification;
- semantic checksum.

Approval is applied after reservation. A pending approval is represented by the reservation/workflow, not by an executable intent. Live paper never uses this authority.

Approval submit, reject, revoke, and expire actions are behavior-changing commands. They enter through the Phase 09 command gateway and must follow the Phase 03 command-admission lifecycle:

```text
approval command received
  -> authenticated and authorized with stronger approval policy
  -> admitted or admission-rejected
  -> exactly one ordered ControlOutcome accepted or rejected
  -> accepted outcome carries the approval behavior change and effective position
  -> execution.approval.* fact is published from the approval authority at that ordered boundary
```

The approval command response is not approval truth. Request receipt, admission, accepted/rejected `ControlOutcome`, effective application, duplicate retry, timeout, and unknown outcome remain distinct. A deduplicated retry references the original `ControlOutcome` and cannot create a second approval/revocation/expiry. Rejected or failed-before-admission commands do not create valid approvals. Replay consumes the ordered `ControlOutcome` and linked `execution.approval.*` facts, not the original imperative request or a console/API projection.

## Executable live intent eligibility

Execution planning may create a live intent only when:

1. run mode is `Human-approved live`;
2. source risk decision is approved or modified, active, unexpired, and unsuperseded;
3. source reservation outcome is accepted and reservation is active, unexpired, live-eligible, and not consumed/released/cancelled/invalidated;
4. valid `HumanExecutionApproval` is linked to the reservation/workflow and covers the prospective action;
5. adapter/account/venue/instrument are supported and credentials are available through approved secret boundary;
6. kill-switch and execution controls permit intent creation at the selected effective position;
7. required market/account/reference/reconciliation evidence is fresh enough;
8. idempotency proves no equivalent live intent/order already exists;
9. approval, reservation, and intent readiness are recoverable before publication as executable.

If any evidence is missing, stale, revoked, expired, unknown, or incompatible, no executable live intent exists.

## Pre-submit fence

Immediately before external submission, the live adapter/execution boundary must recheck:

- active kill-switch/control epoch and execution-fenced status;
- reservation active state and remaining capacity;
- approval active state and expiry;
- intent validity and supersession;
- adapter/account capability and credential availability;
- market/reference/account freshness;
- reconciliation blockers and unknown order state;
- idempotency/submission attempt state.

The recheck is authoritative at the execution boundary. A live intent that was previously ready may be held, expired, cancelled, or reconciliation-required if conditions changed.

## Live venue adapter behavior

A live adapter owns:

- local send commitment;
- external client/order IDs;
- venue acknowledgement/rejection;
- cancellation and replacement requests/responses;
- live fills, partial fills, busts, corrections, fees/allocations where venue-provided;
- ambiguous submission/ack/fill state;
- venue-specific clock/timestamp quality;
- adapter health and capability status.

The adapter cannot synthesize authorization, ignore approval/reservation state, or retry an ambiguous economic order unsafely. If external state is unknown, Chronos stops unsafe retry and keeps worst-case reservation/exposure until reconciliation.

## Live fact taxonomy

This leaf extends existing execution namespaces and activates approval facts.

Minimum approval facts:

- `execution.approval.requested`;
- `execution.approval.accepted`;
- `execution.approval.rejected`;
- `execution.approval.revoked`;
- `execution.approval.expired`;
- `execution.approval.superseded`;

Each accepted/rejected/revoked/expired approval fact must reference the command identity, actor, anti-replay evidence, and the single ordered `ControlOutcome` that authorized or rejected the behavior-changing approval action. The fact records approval-domain semantics; the `ControlOutcome` records command ordering, effective position, idempotency, and replay barrier. No approval fact may appear as valid live-execution authority without that linkage.

Minimum live execution fact extensions:

- `execution.intent.live_accepted`;
- `execution.intent.live_rejected`;
- `execution.adapter.pre_submit_check_accepted`;
- `execution.adapter.pre_submit_check_rejected`;
- `execution.order.live_submit_committed`;
- `execution.order.live_acknowledged`;
- `execution.order.live_rejected`;
- `execution.order.live_cancel_requested`;
- `execution.order.live_cancelled`;
- `execution.order.live_replace_requested`;
- `execution.order.live_replaced`;
- `execution.order.live_unknown`;
- `execution.fill.live_accepted`;
- `execution.fill.live_corrected`;
- `execution.fill.live_busted`;

Live reconciliation uses `reconciliation.*`. Live ledger postings use `ledger.*` under live accounting policy. Reservation facts remain `risk.*`.

## Credential and secret isolation

Rules:

- live credentials are unavailable to replay, research, live-read-only, live-paper, paper broker, strategy, portfolio, and query/console-only processes;
- credentials are accessed only through narrow adapter ports with least privilege;
- secret values never appear in commands, events, telemetry, logs, fixtures, audit exports, screenshots, or evidence bundles;
- credential identity/version/access policy may be recorded in redacted form;
- credential rotation, revocation, missing secret, and access denial are explicit states;
- local developer-shell inheritance cannot silently grant live adapter access.

Credential availability is necessary but not sufficient for execution.

## Reservation, unknown state, and reconciliation

Live reservation handling follows the worst-case rule:

- submitted/open/acknowledged/unknown orders hold reservation capacity;
- partial fills consume filled capacity and hold residual/worst-case capacity;
- rejected orders release only when rejection is authoritative;
- cancellation releases only after authoritative cancellation and fill-race reconciliation;
- ambiguous submission/ack/fill state keeps worst-case exposure until reconciled;
- live fills drive ledger postings under live accounting policy;
- reconciliation cases freeze unsafe retry or release where policy requires.

No UI, query projection, metric, or absence of venue events can release live reservation capacity.

## Bounded capital rollout

Human-approved live execution starts with bounded capital. Rollout policy declares:

- maximum account/portfolio/risk-scope exposure;
- allowed venues/instruments/markets;
- approval authority and dual-control requirements where applicable;
- per-order/per-day/per-run limits;
- kill-switch and rollback thresholds;
- reconciliation blockers;
- evidence required to increase limits.

Increasing capital or venue scope requires new reviewed evidence and configuration activation. It is not inferred from successful paper or discovery performance.

## Recovery and replay

Live execution must persist or reconstruct:

- approval facts and anti-replay evidence;
- linked approval command identities, deduplicated retry state, and exactly-one `ControlOutcome` references/effective positions;
- live intent eligibility and publication;
- adapter pre-submit checks;
- local send commitments and idempotency keys;
- external identifiers and acknowledgement/fill/cancel/replace facts;
- ambiguous/unknown states;
- reservation transition requests/outcomes;
- ledger/reconciliation facts;
- credential access identity/version evidence.

Recovery rules:

- if send may have occurred but acknowledgement is unknown, do not resubmit until reconciliation proves safe;
- if acknowledgement/fill arrived but ledger did not post, replay fills to accounting with original identities;
- if approval expired/revoked during recovery, no new intent/submission may occur;
- if live state cannot be reconciled, affected scope remains failed/degraded with worst-case reservation/accounting disposition;
- replay reproduces Chronos-observed live behavior but cannot be used to claim external venue chronology unless clock uncertainty supports it.

## Mode behavior

- `Human-approved live` is the only live-execution mode activated here.
- `Live paper` has no approval workflow and no live adapter.
- `Live read-only` has no authoritative risk/reservation/intent/order/fill.
- `Backtest/paper replay` may simulate live-like scenarios but cannot access live credentials.
- `Guarded automated live` is not enabled by this leaf.

Run mode is immutable. A paper or read-only run cannot become human-approved live in place.

## Observability and operations

Observability must include:

- approval wait, rejection, revocation, expiry, and anti-replay outcomes;
- pre-submit check latency and rejection reasons;
- send-commit, acknowledgement, cancellation, replacement, fill, and reconciliation latencies;
- ambiguous/unknown order counts and duration;
- credential boundary health without secret values;
- bounded-capital limit utilization;
- kill-switch/fence path under live execution;
- incident/runbook/evidence bundle coverage.

Metrics are not execution truth.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **HL-E01 — Approval command/control suite** | Accepted/rejected/revoked/expired approval commands, actor auth, anti-replay attempts, duplicate retries, timeouts, effective positions | Only approval facts linked to exactly one ordered `ControlOutcome` can satisfy live intent eligibility; request receipt/API success is never approval truth; live paper never invokes approval |
| **HL-E02 — Live intent eligibility suite** | Missing approval, stale reservation, expired risk, disabled adapter, revoked credential | No live intent exists unless all requirements are active and recoverable |
| **HL-E03 — Pre-submit fence suite** | Kill-switch race, reservation expiry, approval revocation, stale market/account, reconciliation blocker | Adapter submits only after current boundary recheck passes |
| **HL-E04 — Adapter idempotency/ambiguity suite** | Crash before/after send, lost ack, duplicate client ID, cancellation/fill race | No unsafe retry; unknown keeps worst-case reservation until reconciled |
| **HL-E05 — Credential isolation suite** | Non-live modes, logs/telemetry/evidence, denied secret access, rotation | Live credentials are inaccessible outside approved adapter boundary and never leak |
| **HL-E06 — Live accounting/reconciliation suite** | Fills, busts, corrections, fees, unknown orders, statement mismatch | Ledger/reconciliation facts are explicit and immutable |
| **HL-E07 — Bounded capital rollout suite** | Limit breaches, scope increase requests, rollback, kill switch | Capital scope cannot expand without evidence and reviewed activation |
| **HL-E08 — Recovery/audit drill** | End-to-end approval to order/fill/ledger with crash points | Audit reconstructs authority, actor, reservation, submission, venue evidence, and unresolved risk |

## Phase exit criteria

Phase 11B planning is complete when:

- approval, live intent, adapter, credential, reservation, reconciliation, and ledger boundaries are unambiguous;
- approval submit/reject/revoke/expire actions are bound to command gateway, exactly-one `ControlOutcome`, effective-position, idempotency, and replay semantics;
- no live intent can exist before valid approval and accepted reservation;
- pre-submit recheck and ambiguous-order handling are explicit;
- credential isolation and redaction are testable;
- bounded capital rollout and rollback are governed by evidence;
- guarded automation remains disabled unless Phase 11C is separately approved.

## Deferred decisions

Deferred to implementation or later phases:

- first live venue/adapter/account;
- exact approval UX and dual-control policy;
- exact live accounting chart and statement reconciliation sources;
- legal/compliance constraints;
- guarded automation.
