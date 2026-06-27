# Phase 09: Operator Console and Incident Workflows

## Purpose

This document defines Chronos's operator-console planning contract: the local-first user interface and workflow layer for monitoring runs, understanding decisions, submitting typed commands, investigating incidents, and coordinating recovery without owning domain state.

The console is a consumer and control surface. It renders read models, evidence links, alerts, runbooks, and command forms. It never directly edits market state, recommendations, targets, risk decisions, reservations, orders, fills, ledger entries, reconciliation cases, or approval facts.

## Objectives

The operator console and incident workflows must establish that Chronos can:

1. present the full trading lifecycle from source capture through accounting with mode, freshness, quality, and authority labels;
2. make infrastructure health, safety fences, stale data, unknown states, and degraded projections visible before they become hidden trading risk;
3. guide operators through typed command submission while preserving command gateway validation, authorization, idempotency, audit, and effective-position rules;
4. support incident triage, runbook execution, evidence capture, replay handoff, and post-incident analysis;
5. expose strategy/recommendation/target/risk/execution/accounting explanations without converting explanations into authority;
6. make kill-switch, pause, stop, abort, reset, reconciliation-required, and degraded-state workflows explicit and auditable;
7. display paper execution and accounting state without implying live execution or real-money custody truth;
8. reserve a clearly separated future workflow area for human-approved-live approvals without introducing it into live paper;
9. enforce role-appropriate visibility and redaction for sensitive data;
10. define usability, safety, and evidence gates that prove the console cannot bypass the engine contracts.

## Scope

### In scope

- console information architecture and workflow contracts, not pixel-perfect design;
- run overview, lifecycle timeline, health/safety dashboard, lifecycle trace, and evidence links;
- command forms and command-status views;
- alert and incident views, operator acknowledgements, notes, runbook steps, and evidence bundles;
- replay/reproduction handoff from incidents;
- paper execution/accounting/reconciliation views;
- stale/unknown/degraded state rendering rules;
- role/access/redaction expectations for local-first operation;
- evidence gates for safe control workflows and incident handling.

### Out of scope

- API transport implementation, endpoint schema details, or projection storage, owned by the companion Phase 09 leaf;
- domain authority semantics for market, strategy, portfolio, risk, reservation, execution, accounting, reconciliation, or live approvals;
- exact visual design system, frontend framework, hosted deployment, or ticket-level implementation plan;
- human-approved-live execution approvals beyond reserving future workflow separation for Phase 11.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Console application | Presentation, workflow composition, command form submission, operator notes, evidence navigation | Authoritative domain state, read-model ownership, approval facts |
| Command gateway | Validation, authorization, idempotency, command status | Console layout or domain behavior |
| Query-model authority | Projection payloads, freshness, completeness, rebuild state | Console rendering choices or source facts |
| Domain authorities | Source facts and lifecycle semantics rendered by the console | UI workflow state except consumed projections |
| Observability/alert authority | Health projections, alert instances, notification state | Domain controls, command acceptance, incident resolution truth |
| Audit/provenance authority | Immutable operator/security-relevant audit facts and evidence bundles | Mutating domain facts |
| Reconciliation authority | Discrepancy cases and resolution facts | Console-only clearing of discrepancies |
| Operator identity authority | Actor identity and allowed capabilities | Strategy evaluation or silent execution authorization |

Operator notes, acknowledgements, and runbook progress are audit/workflow facts. They may improve investigation context; they do not clear the underlying condition unless the owning domain authority emits a resolving fact.

## Console surfaces

### Run overview

The run overview must show:

- run identity, mode, replay class/input mode, manifest, parent/child lineage, and lifecycle state;
- active configuration epoch and effective controls;
- capture/source health, market-state freshness, strategy activity, portfolio/risk status, execution/accounting status, and reconciliation state;
- safety fence status with `healthy`, `degraded`, `unknown`, or `failed` semantics;
- current paper account/portfolio summaries with source cursor and freshness labels;
- warnings when a view is partial, stale, rebuilding, unavailable, or non-authoritative.

Mode must be prominent. `Live read-only`, `Live paper`, `Backtest/paper replay`, and future live modes must not share ambiguous action labels.

### Lifecycle trace

The lifecycle trace lets an operator follow one decision path:

```text
source/capture
  -> normalized event
  -> market-state view
  -> feature observation
  -> strategy evaluation
  -> signal / abstention
  -> recommendation
  -> target
  -> risk decision
  -> reservation outcome / reservation
  -> executable paper intent
  -> paper order
  -> fill
  -> ledger/P&L/reconciliation
```

Every step must display source identity, status, quality/freshness, lineage links, and whether the step is absent because the lifecycle stopped legitimately or because evidence is missing/unknown. A hold recommendation, risk rejection, stale reservation, rejected order, no fill, or unavailable P&L must appear as an explicit outcome, not as an empty table.

### Command center

The command center presents typed controls for:

- run start/pause/resume/stop/abort/reset;
- configuration and strategy activation;
- kill-switch activation/deactivation;
- paper execution pause/cancel where allowed;
- projection rebuild/export as read-model administrative operations, not behavior-changing domain commands;
- reconciliation workflow actions;
- future human-approved-live approval commands in a separate Phase 11 area.

Every command form must show:

- target authority and resource;
- current mode and whether the command is mode-compatible;
- expected authoritative outcome type;
- requested effective timing semantics;
- risk/safety warning where applicable;
- required actor capability;
- idempotency key/request identity;
- audit reason field when required;
- post-submit status that distinguishes request receipt, authoritative acceptance, effective position assignment, rejection, duplicate, timeout, and unknown outcome.
- for safety-critical controls, non-conflated acknowledgement stages: admitted, accepted/rejected, effective, and, where execution-capable boundaries apply, execution-fenced.

A console button must not directly mutate domain state or bypass command gateway validation.

### Health, safety, and alert views

The console renders:

- capability readiness and liveness;
- market data gaps, stale books, reference-data issues, and recovery status;
- strategy/portfolio/risk/execution/accounting degraded states;
- kill-switch command progress and execution-fence acknowledgements;
- unknown order/reservation/reconciliation-required state;
- alert definitions, alert instances, severity, source facts, and operator acknowledgement.

Alert acknowledgement records operator awareness only. It does not clear the source condition. Clearing requires the owning domain/observability authority to publish a resolving fact.

### Incident workspace

An incident workspace groups:

- incident identity, severity, status, affected run/scope, and owner;
- source alerts and domain facts;
- operator notes and timeline;
- runbook checklist and command links;
- evidence bundle: manifests, traces, metrics, logs, read-model snapshots, replay dataset references, checksums;
- reconstruction/replay instructions;
- post-incident findings and follow-up references.

Incident status is workflow state. It does not override domain health, reconciliation, risk, or execution status.

### Paper execution and accounting views

Paper views must show:

- executable paper intents and their source reservation/risk/target lineage;
- paper orders with branching lifecycle, unknown states, cancellation/replacement races, and terminal status;
- fills and accounting posting status;
- ledger transactions, positions, cash, realized/unrealized P&L, valuation freshness, and cost-basis policy;
- reconciliation cases and unresolved severe blockers;
- paper account/epoch/reset lineage.

These views must be labelled as paper. They cannot imply live submission, live custody, or real-money P&L.

## Human-approved-live separation

Phase 09 may reserve UI space and command gateway compatibility for future human-approved-live workflows, but it must not implement or simulate approval authority in live paper.

Rules:

- live paper has no human approval workflow;
- human-approved-live approval/rejection/revocation/expiry facts are Phase 11 `execution.approval.*` facts;
- pending approval appears only for human-approved live reservations/workflows, not executable intents;
- no executable live intent exists until Phase 11 records valid approval;
- paper controls and future live approval controls must be visually and contractually distinct.

## Stale, unknown, degraded, and unavailable rendering

The console must use explicit state labels:

- `current`: source cursor and freshness satisfy policy;
- `stale`: data is known but older than policy allows;
- `partial`: projection is missing some declared source coverage;
- `unknown`: source truth cannot be established;
- `rebuilding`: projection is being reconstructed;
- `degraded`: authority is operating under declared reduced capability;
- `failed`: scope cannot continue safely without recovery/restart/operator action;
- `non_authoritative`: view is explanatory or hypothetical.

Unknown must never be rendered as healthy, filled, cancelled, approved, reconciled, or fenced. Empty UI widgets must distinguish “legitimately no facts” from “facts not loaded.”

## Operator notes, acknowledgements, and audit

Operator workflow facts may include:

- `audit.operator.note_added`;
- `audit.operator.alert_acknowledged`;
- `audit.operator.runbook_step_recorded`;
- `audit.operator.evidence_bundle_created`;
- `audit.operator.incident_status_changed`;
- `audit.security.authorization_denied`;
- `audit.security.sensitive_export_denied`.

These facts explain operator activity. They do not mutate domain outcomes. If a note references an order, fill, ledger entry, risk decision, reservation, or reconciliation case, it stores a reference and cannot edit the referenced fact.

## Recovery and replay

The console must tolerate:

- API restart;
- projection rebuild;
- lost subscription updates;
- unavailable telemetry;
- command outcome unknown;
- evidence-bundle creation failure;
- local browser/app refresh.

Recovery rules:

- reload views from read models with source cursors and freshness markers;
- replay missed subscription updates only where the subscription declares cursor support;
- show unknown command outcome until the authoritative outcome is observed;
- preserve operator notes/audit facts or mark them failed/unavailable explicitly;
- never resubmit dangerous commands automatically after UI retry without idempotency proof and operator-visible status.

Incident replay handoff must identify replay class. Replayed alerts or incidents cannot page live operators by default and must be mode-labelled.

## Observability and performance

Console observability must include:

- page/view load and projection freshness latency;
- command form validation and submit-to-outcome-observed latency;
- subscription lag and dropped/coalesced update counts;
- incident/evidence bundle creation duration;
- stale/unknown/degraded widget counts;
- authorization/redaction denials;
- local resource usage.

Console performance budgets are evidence-derived. A slow console must not slow domain safety paths.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **OC-E01 — Authority-boundary UI suite** | UI attempts for risk edit, order edit, ledger edit, direct state mutation | Console can only submit typed commands or notes; no direct mutation path exists |
| **OC-E02 — Mode-label suite** | Replay, live read-only, live paper, human-approved live placeholder, guarded automation placeholder | Actions and labels do not imply prohibited execution/approval paths |
| **OC-E03 — Command workflow suite** | Submit, duplicate, reject, timeout, accepted/effective, unknown outcome | UI distinguishes request status from authoritative outcome and effective position |
| **OC-E04 — Stale/unknown rendering suite** | Stale projections, missing facts, empty legitimate results, rebuild, unavailable telemetry | Unknown/stale/degraded are explicit and never rendered as safe/current |
| **OC-E05 — Incident workflow drill** | Alert to incident, runbook note, evidence bundle, replay handoff, post-incident summary | Incident artifacts link authoritative facts and do not clear source conditions by acknowledgement alone |
| **OC-E06 — Paper/accounting view suite** | Intents, orders, fills, ledger, P&L, reconciliation cases, reset epoch | Paper evidence is labelled paper and does not imply live custody/execution |
| **OC-E07 — Security/redaction UX suite** | Restricted views, denied commands, secret canaries, export attempts | Unauthorized data/actions are blocked and audited without leaking secrets |
| **OC-E08 — Recovery/resubscription suite** | App refresh, API restart, lost subscription, projection rebuild, command unknown | Console recovers with correct freshness/status and no automatic dangerous resubmit |
| **OC-E09 — Observability/performance suite** | Local workload, high update rate, slow client, incident mode | Console remains bounded and cannot backpressure domain queues |

## Phase exit criteria

Operator-console planning is complete when:

- the console is clearly a presentation and command-submission surface;
- lifecycle, health, safety, incident, paper execution, accounting, and reconciliation views preserve authority labels;
- command workflows cannot bypass API, authorization, idempotency, audit, or effective-position rules;
- stale/unknown/degraded/unavailable states are explicit;
- incident workflows produce useful evidence without clearing domain conditions incorrectly;
- human-approved-live approval workflow remains separated for Phase 11;
- security/redaction and recovery behavior are testable.

## Deferred decisions

Deferred to later implementation or phases:

- exact UI framework and visual design system;
- hosted or multi-user deployment;
- detailed page layout and component library;
- external notification integrations and on-call routing, activated in Phase 10 operations work;
- full human-approved-live approval UX, handled in Phase 11.
