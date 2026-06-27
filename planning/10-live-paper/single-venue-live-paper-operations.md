# Phase 10A: Single-Venue Live Paper Operations

## Purpose

This document defines Chronos's production-like single-venue live-paper operating contract. Phase 08 proves the paper execution/accounting lifecycle. Phase 10A proves that the full live-input-to-paper-output system can run continuously against live market data with bounded local infrastructure, supervised restart, recovery drills, soak evidence, operational runbooks, and empirically validated budgets.

Live paper remains paper. It uses live capture and normalization, but all execution outcomes are simulated by the paper broker and all accounting is paper accounting. No live credentials, live submission, live venue acknowledgement, live fill, or human-approved-live approval workflow is introduced here.

## Objectives

Single-venue live-paper operation must establish that Chronos can:

1. run the full single-venue pipeline from live source capture through paper accounting under the immutable `Live paper` mode;
2. operate within measured CPU, memory, storage, queue, telemetry, and latency envelopes on the intended local-first topology;
3. survive reconnects, source gaps, process crashes, restarts, storage pressure, projection rebuilds, and operator controls without unsafe paper execution;
4. prove kill-switch/pause/stop/abort/reset paths under overload with non-conflated admission, acceptance, effective, and execution-fence acknowledgement stages;
5. run sustained soak campaigns that include market bursts, strategy activity, portfolio/risk/reservation, paper orders/fills, ledger posting, and reconciliation;
6. keep alerting, incident workflow, runbooks, evidence bundles, and replay/reconstruction handoff usable during faults;
7. validate readiness endpoints, supervised restart behavior, checkpoint/replay recovery, and child-run policy;
8. keep operator burden low enough for local operation without hiding unknown or degraded states;
9. preserve paper/live isolation and prohibit live execution claims;
10. define evidence gates that must pass before adding multi-venue/L3 or live execution complexity.

## Scope

### In scope

- single selected venue/source family, bounded watchlist, and live input mode;
- end-to-end live-paper topology and operational envelopes;
- readiness/liveness/degradation contracts for live paper;
- supervised startup, shutdown, restart, recovery, and child-run behavior;
- kill-switch and safety-control drills under normal, burst, degraded, and overload conditions;
- soak, capacity, stress, and fault-injection campaigns;
- incident/runbook workflows using Phase 09 console/API;
- evidence bundle, audit reconstruction, and replay handoff for live-paper runs;
- local-first resource sizing, retention, storage pressure, and export policy.

### Out of scope

- second venue, cross-venue synchronization, L3 state, or opportunity routing, owned by Phase 10B;
- external/Polymarket discovery, owned by Phase 11A;
- live venue execution adapters, live credentials, human approvals, guarded automation, or real-money accounting, owned by Phase 11 or later;
- exact production hosting/provider choices, on-call organization, or implementation ticket breakdown.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Run/orchestration authority | Live-paper run manifest, topology activation, supervised lifecycle, mode-compatible component activation | Domain state mutation outside owning authorities or mode changes in place |
| Source/capture/normalization authorities | Live source connectivity, capture, gap/reconnect facts, normalized events | Market-state decisions beyond their contracts |
| Domain authorities | Market state, features, strategy, recommendations, portfolio, risk, reservation, paper execution, accounting facts | Operational dashboards or topology policy |
| Operations/API/console | Command submission, read models, incident workflow, runbooks, evidence bundles | Domain truth or direct mutation |
| Observability authority | Health, metrics, traces, alerts, profiles, budgets, measurement uncertainty | Safety decisions or execution fences |
| Persistence/replay authority | Retention, recovery, replay class, valid-prefix/manifest evidence | Inventing missing live source continuity |
| Operator identity/audit authority | Actor identity, authorization, command and incident audit | Silent execution authorization |

Phase 10A may refine deployment topology and operational gates, but it must not redefine domain semantics from earlier phases.

## Live-paper topology

The initial live-paper topology may be a modular monolith or a small supervised local process group. It must define:

- process/runtime identities and restart boundaries;
- component activation order and readiness dependencies;
- bounded queues and priority/reserve classes;
- source capture and reconnection policy;
- persistence generation and local storage policy;
- telemetry/profile activation;
- API/console availability;
- paper broker/accounting activation;
- evidence export location and retention.

The topology must be minimal enough for local operation and explicit enough for recovery drills. Splitting a component into a process is allowed only when it improves isolation, restart, or dependency containment without weakening contracts.

## Runtime readiness and degradation

Readiness is capability-specific:

- `capture_ready`;
- `normalization_ready`;
- `market_state_ready`;
- `strategy_ready`;
- `portfolio_ready`;
- `risk_ready`;
- `reservation_ready`;
- `paper_execution_ready`;
- `accounting_ready`;
- `query_ready`;
- `control_ready`;
- `telemetry_ready`;
- `replay_recovery_ready`.

A global green status is insufficient. Each capability must expose healthy, degraded, unknown, failed, and recovering states with source evidence. Missing readiness evidence blocks execution-capable paper progress where policy requires it.

## Operating envelope and budgets

Phase 10A accepts no arbitrary latency/resource numbers. Budgets are derived from measured workloads:

- normal live source rate;
- burst source rate;
- reconnect/gap recovery;
- strategy activity burst;
- portfolio/risk/reservation contention;
- paper order/fill burst;
- ledger posting and snapshot publication;
- projection rebuild during live paper;
- telemetry profile changes;
- storage pressure and retention/export.

Budgets must include CPU, memory, disk write/read rate, storage growth, queue depth/age, telemetry overhead, control-path latency, source lag, market-state freshness, recommendation-to-target latency, risk/reservation serialization latency, intent/order/fill/accounting latency, query freshness, and restart/recovery times.

## Kill switch and safety controls

Kill-switch, pause, stop, abort, and reset drills must prove:

- reserved-capacity command admission remains available under overload;
- accepted/rejected outcomes are serialized by the owning authority;
- effective positions are assigned and applied before domain behavior changes;
- risk/reservation/execution/accounting boundaries acknowledge the active control epoch where applicable;
- execution-fenced projection appears only after authoritative aggregate fence acknowledgement;
- accepted command is not presented as execution stopped;
- unknown or stale fence evidence is rendered as unknown;
- active paper orders, unknown orders, fills, reservations, and ledger postings follow Phase 08 safety rules.

The kill switch prevents new unsafe work; it does not fabricate cancellation, fill, reservation release, ledger correction, or reconciliation outcomes.

## Soak and fault campaigns

Required campaigns:

- baseline live-paper soak through a full configured market session or declared representative duration;
- burst soak with source and strategy activity above baseline;
- reconnect/gap campaign;
- process crash/restart at each implemented boundary;
- storage pressure and telemetry backpressure campaign;
- kill-switch and pause/stop/abort/reset under load;
- paper broker unknown/cancel/fill race drill;
- accounting/reconciliation fault drill;
- projection rebuild and console/API recovery drill;
- replay reconstruction of selected live-paper incidents.

Each campaign records workload manifest, environment, software versions, run manifest, random seeds where applicable, telemetry profile, observed budgets, failures, operator actions, and replay/reconstruction evidence.

## Runbooks and incident workflow

Phase 10A activates runbooks for:

- source disconnect/gap;
- stale market-state view;
- strategy degraded/unavailable;
- risk/reservation unavailable;
- paper execution unknown order;
- accounting posting failure;
- reconciliation case severe;
- storage pressure/quarantine exhaustion;
- telemetry/export failure;
- kill-switch/fence unknown;
- restart/child-run decision.

Runbooks must define detection, operator action, expected command path, evidence to capture, escalation/fail-closed condition, recovery verification, and post-incident replay handoff.

## Reset, restart, and child-run policy

Restart is allowed only with proven recovery source:

- retained source/control facts or explicit source gap/incomplete status;
- durable accepted controls and effective positions;
- recoverable risk/reservation/paper/accounting facts;
- projection rebuild plan;
- evidence of unknown external/paper effects where applicable.

If the same run cannot be resumed safely, recovery creates a child run with parent lineage, initial-state policy, gap/incomplete markers, and fresh mode-compatible manifest. Run mode cannot change through restart or reset-in-place.

Paper reset creates a new paper account/ledger epoch or explicit opening-balance transaction; it never deletes prior paper orders, fills, ledger entries, P&L, or incident evidence.

## Evidence bundles and audit reconstruction

Every Phase 10A gate must produce an evidence bundle containing:

- run manifest, topology manifest, configuration epochs, and mode;
- source/capture manifests and cursor coverage;
- control outcomes and acknowledgement stages;
- domain facts and snapshots needed for reconstruction;
- telemetry profiles, metrics, traces, logs, and alert instances;
- incident/runbook records and operator commands;
- paper broker model and accounting policy versions;
- replay class and replay/reconstruction result;
- checksums and export manifest.

Audit reconstruction must answer what happened, what the system knew, what the operator did, which controls were effective, what was paper-executed, what was posted to ledger, and what remains unresolved.

## Mode behavior

- `Live paper` is the primary mode for this leaf and must stay immutable for the run.
- `Backtest/paper replay` may replay Phase 10A evidence but does not prove live operational readiness by itself.
- `Live read-only` may share capture/console infrastructure but cannot create risk decisions, reservations, intents, orders, fills, or ledger facts.
- `Human-approved live` and `Guarded automated live` are not enabled by Phase 10A evidence.
- `paper_candidate` remains a research promotion state and is not a runtime mode.

## Observability and performance

Observability must include all Phase 02 and phase-specific lifecycle points, plus:

- sustained source lag and market-state freshness;
- end-to-end lifecycle latency and backlog;
- control admission/acceptance/effective/fence latencies;
- reconnect, recovery, restart, and child-run timing;
- local resource saturation;
- queue pressure by priority class;
- paper execution/accounting throughput and failure rates;
- operator action timing and incident workflow duration.

Metrics are evidence, not authority. Missing telemetry does not clear safety conditions.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **LP10A-E01 — Topology readiness suite** | Startup/shutdown/restart manifests, readiness endpoints, dependency failures | Capability readiness is specific, mode-correct, and safely degraded/failed when evidence is missing |
| **LP10A-E02 — Sustained soak campaign** | Full live-paper pipeline under baseline and burst workloads | Budgets are measured, breaches classified, no silent data/state loss |
| **LP10A-E03 — Safety-control drill** | Kill-switch/pause/stop/abort/reset under load | Stages admitted/accepted/effective/execution-fenced are non-conflated and source-backed |
| **LP10A-E04 — Recovery/restart campaign** | Crash/restart at capture, dispatch, risk, reservation, execution, accounting, API/projection boundaries | Recovery resumes safely or creates child run with explicit incomplete/gap lineage |
| **LP10A-E05 — Storage/backpressure campaign** | Full queues, storage pressure, telemetry/export failure | Safety/control/economic facts are protected; optional telemetry/projections degrade first |
| **LP10A-E06 — Incident/runbook drill** | Representative incidents and operator actions | Runbooks capture evidence, commands use API path, source conditions are not cleared by acknowledgement alone |
| **LP10A-E07 — Audit reconstruction drill** | Selected live-paper run and incident bundle | Reconstruction proves lifecycle, controls, paper execution, accounting, and unresolved states |
| **LP10A-E08 — Paper/live isolation suite** | Attempts to route live, use live credentials, trigger human approval | Live-paper evidence remains paper-only and no live path is reachable |

## Phase exit criteria

Phase 10A planning is complete when:

- live-paper topology, readiness, operational envelopes, and resource budgets are evidence-based;
- soak, restart, recovery, control, storage, and incident campaigns are specified;
- kill-switch/fence semantics remain non-conflated and authoritative;
- paper/live isolation is preserved;
- runbooks and evidence bundles can support local operation and audit reconstruction;
- Phase 10B can add multi-venue/L3 without changing single-venue meaning.

## Deferred decisions

Deferred to implementation or later phases:

- exact selected first venue/source and watchlist;
- exact local supervisor/process manager;
- numeric budget thresholds until measured;
- hosted deployment and on-call routing;
- multi-venue/L3, external discovery, and live execution.
