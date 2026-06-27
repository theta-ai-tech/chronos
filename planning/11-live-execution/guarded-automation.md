# Phase 11C: Optional Guarded Automation

## Purpose

This document defines the optional guarded-automation planning contract. Guarded automation may be considered only after human-approved live execution evidence is satisfactory. It replaces or narrows the per-action human approval requirement only under an explicit automation policy, constrained authority, bounded scope, stronger guardrails, and rollback evidence.

Guarded automation is not unconstrained trading. It cannot escape the established target, risk, reservation, execution, adapter, reconciliation, ledger, control, audit, and incident boundaries.

## Objectives

Guarded automation must establish that Chronos can:

1. define an explicit automation policy that states which approval requirement is substituted and under what constraints;
2. restrict automation by mode, account, venue, instrument, strategy, opportunity type, risk scope, order size, frequency, time, and capital;
3. require all existing target, risk, reservation, execution-health, adapter, credential, and reconciliation gates before live submission;
4. prove kill-switch and rollback behavior under load and ambiguous-order conditions;
5. detect and stop drift, stale data, degraded quality, unknown order state, reconciliation blockers, policy breaches, and performance breaches;
6. preserve human override, pause, stop, abort, and emergency disable paths with non-conflated acknowledgement/fence semantics;
7. provide stronger observability, audit, replay, and post-incident evidence than human-approved live execution;
8. prevent discovery/opportunity evidence from becoming direct execution authority;
9. phase rollout through tiny bounded capital and explicit promotion evidence;
10. fail closed when any automation precondition is missing, stale, unknown, or degraded beyond policy.

## Scope

### In scope

- automation policy identity, scope, limits, guardrails, and promotion lifecycle;
- approval-substitution semantics for narrowly defined live actions;
- automated intent eligibility and pre-submit checks building on Phase 11B;
- automated kill-switch, rollback, emergency disable, and incident workflows;
- drift/degradation detection and policy-breach handling;
- bounded capital rollout and evidence renewal;
- audit/replay requirements for automated live actions.

### Out of scope

- unconstrained automation;
- removing target/risk/reservation/execution/ledger/reconciliation gates;
- self-modifying strategies or runtime policy changes without review;
- increasing capital/venue/instrument scope automatically;
- legal/compliance approval itself;
- implementation ticket breakdown.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Automation-policy authority | Automation scope, approval-substitution rules, guardrails, promotion/retirement evidence | Strategy alpha, risk decisions, reservation, live order truth |
| Risk/reservation authorities | Risk approval, projected exposure, reservations, capacity serialization | Automation permission or live adapter behavior |
| Execution planning/adapter | Live intent/submission after automation policy and all live gates pass | Policy approval, risk approval, credential grants |
| Operator identity/control | Emergency disable, override, audit, policy activation commands | Silent automated expansion |
| Observability/incident authority | Breach detection, guardrail telemetry, incident evidence | Domain truth or approval substitution |
| Discovery/opportunity authority | Candidate/opportunity facts and explanations | Direct automation authorization |

Automation adds a policy gate; it does not remove existing gates.

## Automation policy

An `AutomationPolicy` contains:

- policy identity and semantic version;
- eligible runtime mode: `Guarded automated live`;
- eligible accounts, portfolios, risk scopes, venues, instruments, strategies, and opportunity types;
- maximum per-order, per-run, per-day, and aggregate exposure;
- allowed order types and execution constraints;
- required risk/reservation/adapter/account/market/reconciliation health;
- freshness and quality thresholds;
- blocked conditions and fail-closed reasons;
- kill-switch, emergency disable, rollback, and incident triggers;
- promotion, renewal, expiry, and retirement evidence;
- actor/reviewer approvals for policy activation;
- audit and replay requirements.

Policy activation is a behavior-changing control with effective position. Runtime mode remains immutable; a human-approved live run cannot become guarded automated live in place.

## Approval substitution

Guarded automation substitutes only the per-action `HumanExecutionApproval` requirement for actions inside policy scope. It does not substitute:

- target construction;
- risk decision;
- reservation outcome/reservation;
- execution-health and pre-submit recheck;
- adapter idempotency;
- credential isolation;
- reconciliation;
- accounting;
- kill-switch/fence acknowledgement;
- audit.

If an action falls outside automation scope, it must revert to human-approved-live workflow or fail closed.

## Automated live intent eligibility

An automated live intent may exist only when:

1. run mode is `Guarded automated live`;
2. automation policy is active, unexpired, and covers the exact action;
3. target, risk decision, reservation, account, venue, adapter, credential, market, and reconciliation evidence satisfy Phase 11B requirements;
4. policy exposure/frequency/time limits have capacity;
5. no guardrail, incident, unknown order, unresolved reconciliation, stale data, or degraded quality blocks automation;
6. kill-switch and emergency-disable controls are inactive at the selected effective position;
7. idempotency and recovery fences are satisfied.

Missing or unknown evidence blocks automation.

## Guardrails and rollback

Guardrails include:

- hard exposure/frequency/order-size limits;
- stale market/external/account/reconciliation blockers;
- source quality and synchronization blockers;
- abnormal rejection/fill/slippage/latency thresholds;
- repeated cancellation/replacement or ambiguous-order thresholds;
- ledger/reconciliation mismatch blockers;
- drawdown or loss-proxy thresholds where policy defines them;
- dependency health blockers;
- manual emergency disable.

Rollback stops new automated intents, preserves existing live order/reservation/accounting handling, opens incidents where required, and may route future actions to human-approved-live mode only through a new run/manifest if mode changes are required.

## Promotion and renewal

Automation promotion requires:

- Phase 11B human-approved live evidence in the same or narrower scope;
- replay/backtest/paper/live-paper evidence for the candidate policy;
- bounded live shadow or dry-run evidence where applicable;
- incident-free or incident-reviewed trial period;
- explicit reviewer approval;
- renewal deadline and retirement criteria.

Renewal requires fresh evidence. Repeated manual renewal without meeting evidence is an unmet engineering requirement, not permanent approval.

## Mode behavior

- `Guarded automated live` is optional and separate from `Human-approved live`.
- `Human-approved live` cannot silently automate per-action approvals.
- `Live paper` and `Backtest/paper replay` can test automation logic only as simulation/shadow evidence.
- `Live read-only` cannot create automated live intents.
- Mode changes require new or child runs.

## Observability and audit

Automation requires:

- policy-scope and eligibility decision logs;
- guardrail state and breach facts;
- pre-submit check evidence;
- limit utilization and exhaustion;
- kill-switch/emergency-disable acknowledgement stages;
- unknown-order/reconciliation blocker duration;
- automated action audit trail from signal/opportunity through ledger;
- evidence renewal status.

Metrics cannot approve automation. Audit must reconstruct why each automated action was allowed.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **GA-E01 — Policy-scope suite** | In-scope/out-of-scope accounts, venues, instruments, strategies, opportunities | Only exact in-scope actions can use automation |
| **GA-E02 — Approval-substitution suite** | Missing risk, missing reservation, stale account, no policy, expired policy | Automation substitutes only human approval and never other gates |
| **GA-E03 — Guardrail suite** | Stale data, unknown order, reconciliation blocker, limit breach, latency breach, quality degradation | Automation fails closed and opens required incident/control paths |
| **GA-E04 — Kill-switch/rollback drill** | Overload, ambiguous order, emergency disable, run abort, child run | New automated work stops with non-conflated acknowledgement/fence evidence |
| **GA-E05 — Promotion/renewal suite** | Insufficient evidence, expired renewal, scope increase, successful narrow rollout | Automation cannot activate or expand without reviewed evidence |
| **GA-E06 — Audit/replay drill** | Automated action from signal/opportunity to ledger with faults | Reconstruction proves policy, gate evidence, adapter state, and accounting outcome |
| **GA-E07 — Discovery negative suite** | High-ranked external opportunity without policy/risk/reservation | Discovery never becomes direct execution authority |

## Phase exit criteria

Guarded automation planning is complete when:

- automation policy scope and approval substitution are exact;
- all existing risk/reservation/execution/accounting/reconciliation gates remain required;
- guardrails, rollback, promotion, renewal, and audit are evidence-based;
- mode separation prevents accidental automation in human-approved live or paper modes;
- optional automation can be declined without weakening Phase 11B.

## Deferred decisions

Deferred unless this optional phase is explicitly pursued:

- whether to implement guarded automation at all;
- first automation scope;
- exact capital limits and renewal cadence;
- legal/compliance requirements;
- organizational approval process.
