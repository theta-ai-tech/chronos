# Phase 09: Control API and Read Models

## Purpose

This document defines Chronos's local operations API, command gateway, and read-model planning contract. The control/API layer lets operators inspect authoritative state through disposable projections and submit typed commands that become ordered domain control outcomes. It does not own trading semantics, market state, strategy logic, risk decisions, reservations, execution facts, ledger entries, or approval facts.

This leaf builds on approved Phases 01-08. It makes the infrastructure usable without weakening the engine: GUI/API actions must pass through the same command, authorization, audit, ordering, recovery, and replay boundaries as non-UI callers.

## Objectives

The control API and read-model layer must establish that Chronos can:

1. expose local-first query, subscription, and command interfaces without coupling domain authorities to a UI framework;
2. separate commands that may change authoritative state from queries over read models;
3. convert accepted behavior-changing commands into ordered `ControlOutcome`s with effective positions before domain behavior changes;
4. make read models disposable, freshness-labelled, source-position-labelled, and rebuildable from authoritative facts;
5. prevent query projections from becoming risk, execution, accounting, or market truth;
6. authenticate and authorize operators for command and data scopes;
7. record immutable audit facts for operator-relevant command attempts, outcomes, approvals where later applicable, and security events;
8. provide bounded subscriptions and backpressure behavior that cannot stall safety, capture, execution, fill, or accounting paths;
9. support incident investigation through links to authoritative facts, traces, metrics, logs, snapshots, and replay manifests;
10. define conformance gates for API contracts, command idempotency, projection freshness, access control, recovery, and replay.

## Scope

### In scope

- local API boundary shape: commands, queries, subscriptions, exports, and administrative endpoints;
- command gateway semantics, validation, idempotency, authorization, audit, and ordered control-outcome handoff;
- read-model ownership, projection schemas, freshness, source positions, completeness, rebuild, and invalidation;
- subscription delivery semantics, rate limits, backpressure, and stale/unknown markers;
- operator identity, access control, data classification, redaction, and secret handling;
- API versioning, compatibility, schema fixtures, and contract tests;
- local-first operation for one operator/developer machine and later multi-process topology;
- evidence gates for reliability, security, and projection correctness.

### Out of scope

- console visual layout and human workflow design, owned by the companion Phase 09 leaf;
- trading strategy formulas, portfolio/risk policy values, paper broker models, ledger posting rules, or live execution behavior;
- human-approved-live approval fact semantics, except reserving a command submission path for Phase 11;
- external hosted service selection, production identity provider choice, multi-user organization policy, or ticket-level implementation plan.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Command gateway | Request validation, authentication/authorization check, idempotency, command audit, command-to-domain submission | Domain behavior, control effective-position assignment, risk/execution/accounting values |
| Run/configuration authority | Accepted/rejected `ControlOutcome`s, effective positions, run lifecycle/configuration control facts | UI/API request lifecycle or read-model materialization |
| Query-model authority | Disposable projections, source positions, freshness, completeness, rebuild state, subscription payloads | Authoritative domain facts or direct mutation of any domain state |
| Domain authorities | Market, strategy, portfolio, risk, reservation, execution, ledger, reconciliation facts consumed by projections | API transport concerns or console layout |
| Operator identity/audit authority | Actor identity, authorization decisions, audit facts, security-relevant events | Strategy assessment, live-paper approval workflow, hidden execution authorization |
| Observability authority | Metrics, traces, logs, health projections, alert projections, measurement semantics | Domain truth or command acceptance |
| Console application | Presentation and command submission | Owning read models, approval facts, or direct mutation of engine state |

The API may be in-process, local HTTP, WebSocket, local socket, or CLI-backed. Transport choice is implementation detail. Contract semantics are not.

## Canonical concepts

### Command

A `Command` is a typed request from an operator, scheduler, CLI, test harness, or internal control component that may change behavior or lifecycle if accepted by the owning domain authority.

Command examples:

- start, pause, resume, stop, abort, or reset a run;
- activate a configuration or strategy assignment;
- enable/disable strategy, portfolio, risk, execution, or accounting policy within the existing run mode;
- activate/deactivate kill switch;
- request paper execution pause/cancel according to Phase 08 semantics;
- request dataset/replay administrative work when it changes an authoritative dataset, run, manifest, or registry record;
- later submit, reject, revoke, or expire human-approved-live approval commands for Phase 11.

A command contains:

- command identity and idempotency key;
- actor identity and authorization scope;
- target authority and target resource;
- typed payload and schema version;
- requested effective timing semantics, such as next legal effective position or scheduled logical/event time;
- reason, ticket/reference, and optional incident/correlation reference;
- dry-run/validation-only marker where supported;
- replay/audit classification.

A command response is not itself a behavior change. Behavior changes only when the owning authority accepts a `ControlOutcome` and assigns an effective position. Query-model refresh, projection rebuild, projection invalidation, and export preparation are read-model/admin lifecycle operations unless they change an authoritative dataset, run, manifest, registry, or domain record; they must not be routed through the behavior-changing `ControlOutcome` path merely because an operator requested them.

### Control outcome projection

The API exposes command outcomes through projections over authoritative `ControlOutcome`s. A projection must show:

- request lifecycle status such as received, validation rejected, unauthorized, duplicate, submitted, timeout, pending, stale, superseded, failed, or unknown;
- command acknowledgement stage for behavior-changing controls: `admitted`, `accepted`, `rejected`, `effective`, and, for execution-capable scopes, `execution_fenced`;
- authoritative control fact identity when accepted;
- effective position when assigned;
- effective application evidence when the stream/run-input authority has applied the accepted control;
- execution-fence evidence, boundary acknowledgements, aggregate fence acknowledgement, epoch, and freshness for execution-capable scopes;
- source stream cursor and run-input position;
- actor and authorization summary;
- validation and rejection reason codes;
- freshness and projection lag.

If the projection has not observed the authoritative outcome, it reports pending or unknown. It must not infer admission from request receipt, acceptance from an HTTP 200, effective application from accepted command status, or `execution_fenced` from absence of new submissions, process liveness, metric silence, or a kill-switch control event alone. For execution-capable scopes, an accepted command is not proof that execution has stopped; only the authoritative current-epoch fence acknowledgement may project `execution_fenced`.

### Read-model administrative operation

A `ReadModelAdminOperation` is a query-model lifecycle request, not a domain command, when it only rebuilds, invalidates, exports, refreshes, or checks a disposable projection.

It contains:

- operation identity and idempotency key;
- actor identity and authorization scope;
- projection identity and schema version;
- requested source cursor/range or export scope;
- rebuild/export reason and optional incident reference;
- redaction/export classification;
- status: accepted, rejected, running, completed, failed, unavailable, or partial.

Read-model administrative operations may emit `api.read_model.*` facts and audit facts. They must not create `ControlOutcome`s, assign domain effective positions, or mutate source domain facts. If a request changes an authoritative dataset, run, manifest, or registry record, it is no longer a read-model operation and must use the owning authority's command path.

### Query

A `Query` reads a read model or reconstructable evidence artifact. It must declare:

- projection identity and schema version;
- source authorities and source cursors;
- freshness/lag/completeness fields;
- mode and run scope;
- authorization and redaction level;
- unavailable/stale/degraded markers;
- rebuild status and semantic checksum where applicable.

Queries are not authority. A query result cannot approve risk, release reservations, cancel orders, post ledger entries, clear alerts, or mutate any domain fact.

### Read model

A `ReadModel` is a disposable projection optimized for operator/API consumption. It may materialize:

- run lifecycle, configuration, and effective controls;
- market-data health, source cursor, state freshness, and lineage summaries;
- strategy evaluations, signals, recommendations, explanations, and paper-candidate status;
- portfolio targets, risk decisions, reservations, and execution fence state;
- paper intents, orders, fills, ledger positions, P&L, and reconciliation cases;
- observability health, alerts, incidents, and evidence links.

Every read model has:

- owner and schema version;
- source fact set and cursor coverage;
- build/rebuild policy;
- freshness and completeness policy;
- redaction/classification policy;
- invalidation rules;
- monotonic publication or snapshot identity where needed;
- tests proving it is rebuildable or safely disposable.

Read-model loss may degrade UI/API usability but must not change domain state. Rebuild must use authoritative facts and declared snapshots, not another stale projection as truth.

### Subscription

A `Subscription` streams projection updates to a console, CLI, or local consumer. It declares:

- projection topic and schema version;
- delivery ordering scope;
- replay-from-cursor support, if any;
- loss, coalescing, and backpressure policy;
- heartbeat/stale markers;
- authorization and redaction scope.

Subscription delivery is best-effort unless a topic explicitly declares stronger guarantees. Losing a UI subscription update cannot lose domain facts.

## Fact taxonomy activated by this leaf

This leaf activates API/query/audit/control-plane support facts without redefining domain fact namespaces.

Minimum API/control support fact types:

- `api.command.received`;
- `api.command.rejected`;
- `api.command.duplicate`;
- `api.command.submitted_to_authority`;
- `api.command.outcome_observed`;
- `api.subscription.opened`;
- `api.subscription.closed`;
- `api.subscription.backpressured`;
- `api.query.served`;
- `api.query.rejected`;
- `api.read_model.rebuild_requested`;
- `api.read_model.rebuild_started`;
- `api.read_model.rebuild_completed`;
- `api.read_model.invalidated`;
- `api.read_model.unavailable`;

Behavior-changing results remain owned by their domain namespace, usually `run.control.*` through accepted `ControlOutcome`s. Security- and operator-relevant facts not already fully expressed by domain events are `audit.*`. Human-approved-live approval facts remain reserved for Phase 11 under `execution.approval.*`.

API support facts do not replace authoritative domain events. They explain request handling and projection behavior.

## Command handling

The command path is:

```text
request received
  -> schema validation
  -> authentication and authorization
  -> reserved-capacity admission for safety-critical controls where applicable
  -> idempotency lookup
  -> target authority submission
  -> authoritative outcome accepted/rejected by target authority
  -> effective position assigned and later applied for accepted behavior-changing controls
  -> execution-capable boundaries acknowledge fence where applicable
  -> outcome projection published
```

Rules:

- validation failure rejects before target authority submission;
- unauthorized commands produce audit facts without leaking sensitive target state;
- duplicate commands return the prior outcome and never repeat state transitions;
- accepted behavior-changing commands must become ordered control facts before behavior changes;
- requested wall-clock “now” is resolved by the owning authority into the next legal effective position;
- safety-critical controls expose non-conflated `admitted`, `accepted/rejected`, `effective`, and `execution_fenced` stages where applicable;
- dry-run validation must be labelled non-authoritative and cannot reserve future capacity or bypass later checks;
- command timeout means outcome unknown to the caller, not rejected or accepted unless the authoritative outcome is observed.

## Read-model freshness and correctness

Every read model must expose:

- source authority identities;
- source cursor or ledger/risk/execution sequence coverage;
- update time and logical source position;
- projection build identity;
- freshness/lag estimate;
- completeness state: complete, partial, stale, rebuilding, unavailable, or unknown;
- mode and run identity;
- quality flags from source authorities.

Projection correctness must be tested against authoritative facts. If a projection contradicts authoritative facts, the projection is invalidated or rebuilt; domain facts are not changed to match the projection.

## API and schema versioning

API contracts must define:

- endpoint/topic/command name;
- request/response/event schema;
- semantic version;
- compatibility window;
- unknown-field and unknown-type handling;
- idempotency semantics;
- authorization requirements;
- pagination/cursor behavior;
- error/retry taxonomy;
- test fixtures and conformance owner.

Breaking semantic changes require a new version or explicit migration window. Internal language-native objects and mutable pointers must not appear in API schemas.

## Security and redaction

The API must classify and protect:

- secrets, tokens, credentials, and private keys;
- actor identity and audit metadata;
- account, portfolio, strategy, and incident-sensitive fields;
- raw payloads and logs that may contain sensitive external data;
- evidence bundles and exports.

Rules:

- default local operation may be single-user, but command authorization and data classification still exist as contracts;
- secrets must never appear in API responses, telemetry, logs, audit notes, errors, or exported fixtures;
- command write scopes and read scopes are separate;
- read-only access cannot submit commands;
- projection aggregation cannot expose restricted raw traces/logs without authorization;
- dangerous controls require explicit capability and audit reason.

## Recovery and replay

The API/read-model layer must recover:

- submitted command records and idempotency results;
- observed authoritative outcomes;
- non-conflated acknowledgement-stage observations: admitted, accepted/rejected, effective, and execution-fenced where applicable;
- audit/security records;
- projection build manifests;
- subscription cursors where declared;
- read-model admin operation requests and completion/failure facts.

Recovery rules:

- if command submission status is unknown, the API must query/await authoritative outcome or report unknown; it cannot resubmit without idempotency proof;
- if read models are lost, they rebuild from authoritative facts and snapshots;
- if a rebuild cannot prove cursor completeness, the projection is unavailable or degraded;
- replay may reconstruct command handling and projections for investigation, but replayed commands cannot affect a live run by default.

## Observability and performance

Observability must include:

- command validation, authorization, submission, outcome-observation, and effective-position latency;
- query latency, projection freshness, rebuild duration, subscription lag, and backpressure;
- command rejection, duplicate, timeout, and unknown-outcome counts;
- authorization failure and redaction-denial counts;
- projection invalidation and contradiction counts;
- API availability, local resource use, and rate-limit behavior.

Telemetry is not command truth. The authoritative outcome is the target authority's fact.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **API-E01 — Command contract suite** | Valid/invalid commands for run, config, strategy, risk, execution, accounting controls | Accepted behavior-changing commands produce domain outcomes; invalid commands do not |
| **API-E02 — Idempotency and timeout suite** | Duplicate requests, retry after timeout, crash before/after submission, unknown outcome | No duplicate state transition; unknown remains unknown until authoritative outcome is observed |
| **API-E03 — Effective-position and fence acknowledgement suite** | “Now,” scheduled logical/event time, pause/resume/kill-switch controls around market inputs and execution-capable boundaries | Behavior changes only at recorded effective positions; admitted, accepted/rejected, effective, and execution-fenced are distinguishable; only authoritative current-epoch fence evidence projects `execution_fenced` |
| **API-E04 — Read-model correctness suite** | Projection rebuilds from market/strategy/risk/execution/accounting facts, stale and partial inputs | Projections expose source cursors/freshness and never contradict authoritative facts silently |
| **API-E04a — Read-model admin boundary suite** | Rebuild, invalidation, export, and authoritative dataset/run mutations | Disposable projection operations emit `api.read_model.*`/audit facts only; authoritative dataset/run changes use owning command path |
| **API-E05 — Subscription backpressure suite** | Slow client, dropped connection, burst updates, replay-from-cursor | Domain paths are not stalled; delivery loss is visible and recoverable where declared |
| **API-E06 — Security/redaction suite** | Read/write scopes, restricted logs/traces, secret canaries, dangerous controls | Unauthorized access fails safely; no secrets leak |
| **API-E07 — Recovery/rebuild suite** | Lost projections, API crash, rebuild failure, command-store recovery | API state recovers or reports unavailable without changing domain facts |
| **API-E08 — Observability/performance suite** | Representative local workloads and overload cases | Measurements are non-authoritative and budgets are evidence-derived |

## Phase exit criteria

Control API/read-model planning is complete when:

- command/query/subscription boundaries are unambiguous;
- behavior-changing commands cannot bypass ordered `ControlOutcome`s;
- safety-critical control status distinguishes admission, acceptance/rejection, effective application, and execution fencing;
- read models are disposable, freshness-labelled, source-position-labelled, and rebuildable;
- API schemas, idempotency, error handling, versioning, and authorization are contractually defined;
- security/redaction rules protect secrets and restricted evidence;
- recovery handles unknown outcomes and lost projections safely;
- the companion console leaf can build workflows without owning domain authority.

## Deferred decisions

Deferred to later implementation or phases:

- exact transport choice and UI framework;
- detailed endpoint list and schema syntax;
- multi-user hosted deployment and organization policy;
- external identity provider integration;
- Phase 11 human-approved-live approval endpoint details.
