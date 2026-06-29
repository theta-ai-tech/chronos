# Chronos Architecture

## Purpose

This document defines the structural architecture for Chronos: component boundaries, dependency direction, runtime planes, local deployment model, stable contracts, engineering standards, failure and recovery responsibilities, and the evidence required before implementation of the architecture foundation is considered complete.

The [domain model](domain-model.md) is authoritative for language, identities, lifecycle, invariants, ownership, sequencing, modes, durability semantics, and causal provenance. This document maps those semantics into modules and runtime boundaries. It must not weaken or redefine them.

Chronos begins as a local-first modular system. Logical authorities are separated rigorously in code and contracts, while deployment remains simple. A logical boundary is not automatically a process, network service, repository, database, or queue. Such extraction requires evidence.

This is a planning document. It does not select final libraries, storage products, serialization formats, languages for every module, deployment packaging, cloud services, or ticket-level work.

## Architectural objectives

Chronos must provide:

1. A small, deterministic, measurable event-processing core.
2. Faithful and explicit replay paths that use the same domain transitions as live processing.
3. Strong separation between market facts, observations, decisions, authorization, execution, and accounting.
4. Local operation with one documented command surface and low operational burden.
5. Complete causal and version provenance for every material decision.
6. Failure containment that prioritizes capture, safety, order/fill handling, and ledger integrity.
7. Clean extension points for all eleven planning phases without speculative distributed infrastructure.
8. Technology choices that can be justified by measured requirements and replaced behind stable contracts.
9. Implementation gates based on reproducible evidence, not architectural intent.

### On the role of low latency

Low-latency rigor in Chronos is primarily a **demonstration and credibility goal**, not a source of trading edge. The initial strategies hold positions on a 1–5 minute horizon, at which microsecond-scale hot-path latency has negligible effect on signal validity or realized P&L. The value of the deterministic, allocation-controlled C++ hot path (see the Hot plane section and ADR-0001) is to demonstrate measurement discipline, determinism, and systems engineering to a standard a quantitative trading audience takes seriously — and to keep a clean path open to genuinely latency-sensitive strategies later. This document therefore holds the hot path to production-grade latency, tail, and allocation discipline as a deliberate objective in its own right, while being explicit that, at the current product horizon, that discipline is justified by credibility and future-proofing rather than by present-day microstructure alpha.

## Architecture principles

### Modular before distributed

Chronos starts as a modular monolith plus a local web application where practical. Modules have explicit owners, APIs, dependency rules, and tests. They may share a process only when that does not blur authority or compromise failure isolation.

A process split is a deployment decision. It must not introduce a new domain meaning or change an authority's contract. Conversely, sharing a process does not permit direct mutation of another authority's state.

### Replay and live share domain logic

Replay and live ingestion use different input providers but converge on the same accepted normalized-event, control-event, dispatch, state-transition, strategy, portfolio, risk, execution-simulation, and accounting contracts appropriate to the selected mode.

There must not be a separate simplified "backtest implementation" of market state, strategy logic, risk, or accounting whose semantics drift from the live path. Performance instrumentation may differ by runtime, but domain outcomes must remain governed by the run manifest and domain model.

### Authorities communicate through contracts

Every mutable invariant has one authoritative owner. Other modules:

- submit typed commands;
- consume immutable events or read-only views;
- build disposable projections;
- never mutate another authority's internal state or tables.

Internal function calls may implement these interactions initially. The logical command, event, view, error, version, and idempotency semantics still apply.

### Hot-path policy is explicit

Only work required to transform accepted run inputs into time-sensitive state, observations, evaluations, recommendations, and safety decisions belongs on the hot path. Storage, UI fan-out, reporting, experiment analysis, and optional telemetry must not enter it merely for convenience.

Logical durability does not imply synchronous persistence at every stage. Each later phase must define the handoff at which its accepted facts become recoverable, its permitted loss window, and the consequence when that bound cannot be proven.

### Correctness precedes optimization

Optimized representations may differ from canonical contracts, but they must preserve canonical meaning and pass semantic equivalence tests. A performance optimization that obscures lineage, changes ordering, weakens exact arithmetic, or disables safety invariants is invalid.

### Local-first is a product constraint

The default system must run on one developer/operator machine without requiring Kubernetes, a cloud control plane, a distributed log, or separately administered infrastructure. Local operation must still have production-grade boundaries, observability, recovery drills, and auditability.

### Degradation must be visible

Unavailable, stale, gapped, recovering, invalid, rejected, unknown, and incomplete states are represented explicitly. No adapter or orchestration layer may convert them into plausible success. The console may summarize these states but cannot redefine them.

## System context

Chronos interacts with:

- venue market-data APIs and later execution APIs;
- immutable replay datasets and capture sessions;
- optional external-observation sources;
- a local operator through the console and command API;
- local durable stores for authoritative facts, manifests, snapshots, datasets, and audit evidence;
- local metrics, logs, traces, profiles, and benchmark artifacts;
- optional export destinations that remain outside the authoritative runtime.

At the highest level:

```text
Untrusted venue feeds                     Replay datasets
          |                                      |
          v                                      v
  market-data adapters                    dataset/replay authority
          |                                      |
          v                                      |
    source capture ------------------------------+
          |                                      |
          +----> source-fact persistence         |
          |                                      |
          v                                      |
     normalization <------ reference authority   |
          |                  |                   |
          +----> normalized-fact persistence     |
          |                                      |
          +------------------+-------------------+
                             v
             stream sequencing / run-input dispatch
                    ^                         |
                    |                         v
operator/API -> run/config authority     market-state authority
                    |                         |
                    +-> control events        v
                                         features
                                             |
                                             v
                                         strategies
                                             |
                                             v
                                      recommendations
                                             |
                                             v
                                portfolio -> risk/reservation
                                             |
                                             v
                              paper or future live execution
                                             |
                                             v
                                   fills -> ledger -> projections

Every authority --facts, views, lifecycle notices--> audit/provenance
Every runtime boundary --non-authoritative signals--> observability/telemetry
Persistence ports point from owning authorities to phase-owned durable stores;
recovery reads return through the owning authority, never directly to consumers.
```

The diagram shows logical flow, not mandatory processes or synchronous calls.

## Runtime planes

Chronos separates three runtime planes and one cross-cutting evidence surface. Planes classify work and deployment concerns; they do not replace the single authoritative owners defined below.

### Hot plane

The hot plane contains bounded, time-sensitive processing:

- accepted normalized market/reference/control input consumption;
- deterministic run-input dispatch;
- market-state transition and immutable view publication;
- feature calculation needed by active strategies;
- strategy evaluation and recommendation production;
- mode-appropriate portfolio, risk, and reservation decisions;
- execution-health rechecks and order submission where enabled;
- fill acceptance and safety-critical exposure updates where enabled;
- latency-point capture with bounded measurement overhead.

Hot-plane code must:

- avoid hidden network or storage reads during deterministic decisions;
- avoid unbounded queues, retries, collection growth, or logging;
- use explicit memory, scheduling, deadline, and backpressure policies;
- consume immutable configuration epochs and reference versions;
- publish typed results without handing out mutable internal state;
- remain benchmarkable without the UI or control plane.

The hot plane may initially run in one process. Modules within it remain separately testable and obey dependency rules.

The hot plane is implemented in optimized C++. This is a primary product goal, not a deferred optimization: demonstrable, measured low-latency C++ on the time-sensitive path is a defining deliverable of Chronos, and strategy evaluation runs inside this C++ core through the strategy SDK rather than in a higher-level runtime. The choice of C++, the placement of the C++/control-plane boundary, and hot-path arithmetic representation are fixed by ADR (see `docs/adr/0001-hot-path-language-and-boundary.md` and `docs/adr/0003-hot-path-arithmetic-representation.md`) rather than left open. Implementation language for a hot-path module may be substituted only through an ADR that supersedes ADR-0001 with measured justification; "optimize to C++ later" is not an accepted path for hot-plane modules.

### Control plane

The control plane hosts operator and run orchestration:

- authenticated command intake;
- run creation, initialization, pause, resume, stop, failure, abort, and reset workflows;
- configuration validation, versioning, activation, and control-event creation;
- strategy instance and mode configuration;
- dataset and replay selection;
- risk-limit and kill-switch command handling;
- human execution approval in the later live-execution phase;
- health aggregation and operational readiness;
- queries and read-model APIs for the console.

The control plane does not mutate hot-plane state directly. Behavior-changing commands become accepted or rejected control events and take effect only at their recorded effective positions.

The control plane is implemented in Python (the higher-level runtime relative to the C++ hot plane). This is permitted only through stable contracts and must not place runtime-specific objects inside canonical domain messages. The Python control/research surface stays off the hot path by construction and communicates with the C++ core over the compact, bounded, versioned boundary defined in ADR-0001.

### Data plane

The data plane provides persistence and reconstruction mechanisms for facts owned by domain authorities:

- raw source capture;
- accepted normalized-event journals or datasets;
- ordered control-event history;
- run initialization manifests and terminal attestations;
- snapshots and checkpoint metadata;
- authoritative execution, fill, reservation, approval, ledger, and reconciliation facts as phases introduce them;
- dataset catalog metadata and replay manifests;
- material audit and provenance records;
- rebuildable projections where useful.

The data plane is not one universal database. Storage may be separated by record semantics, access pattern, durability need, and recovery owner. A module may not use shared storage as an undocumented integration API.

### Evidence surface

Observability and audit consume facts from every plane:

- structured logs describe operational context;
- metrics quantify rates, levels, errors, and latency distributions;
- traces connect bounded runtime work;
- audit records preserve actor, cause, configuration, and outcome;
- benchmark and profile artifacts provide reproducible performance evidence.

Telemetry is not an authority for market state, risk, orders, fills, positions, or P&L. Loss of optional telemetry may be tolerated only under a declared policy and cannot remove the authoritative causal chain.

## Fact publication and recoverability lifecycle

For architecture and durability discussions, every material result is tracked through distinct lifecycle states. These states describe handling of a fact; they do not replace the domain event's own business lifecycle.

| State | Meaning |
|---|---|
| **Computed** | A module has produced a candidate result in memory. No authority has yet committed to its validity or uniqueness. It cannot be treated as a domain fact. |
| **Accepted** | The owning authority has validated the candidate, serialized it against competing transitions, assigned required identity/order, and accepted it as an immutable domain fact. |
| **Published** | The accepted fact is available to declared downstream consumers through the authority's publication contract. Publication does not by itself prove crash recovery. |
| **Recoverability-accepted** | The phase-owned persistence boundary has acknowledged responsibility for preserving or reconstructing the accepted fact under a declared loss policy. |
| **Recoverable** | The fact can be reconstructed after the tested failure class from retained authoritative inputs, durable records, or an atomic external reconciliation point, with identity, ordering, causation, and version intact. |

Rules:

- `Computed` results may be discarded without correction because they were never accepted.
- An authority may combine `Accepted` and `Published` atomically in process, but the meanings remain distinct for failure analysis.
- A rebuildable projection may become recoverable through retained inputs rather than direct persistence.
- A fact cannot be described as durable merely because publication succeeded or an asynchronous write was requested.
- Each phase defines which transition is synchronous, asynchronous, batched, reconstructed, or allowed to lag, plus the permitted loss window.
- If recoverability cannot be proven within policy, the run or affected scope becomes incomplete, failed, or non-faithful as required by the domain model.

### Authorization fence

Execution-capable authorization facts have a stronger fence:

1. The approving `RiskDecision` is accepted and unexpired.
2. The `ReservationOutcome` and linked reservation are accepted at the authoritative `risk_sequence`.
3. In human-approved live mode, `HumanExecutionApproval` is accepted and valid.
4. These required authorization facts are `Recoverable` before acceptance of the executable order intent, or one atomic protocol makes the authorization facts recoverable in the same commit that accepts the intent. `Recoverability-accepted` alone is insufficient.
5. Execution planning revalidates mode, expiry, reservation, kill-switch, and execution-health state before accepting the intent.
6. The adapter performs the same safety-class recheck before external submission.

An executable intent may be computed while a fence is pending, but it cannot be accepted, published as executable, or routed. A crash cannot convert computed or merely published authorization into executable authority. Phase-specific durability plans must prove that restart cannot duplicate capacity, bypass approval, or lose an ambiguous submission.

## Logical components and boundaries

Each component below is a logical module. Initial process placement is addressed separately.

### Reference authority

Owns canonical instruments, venue listings, mappings, venue rules, effective versions, and validation of reference compatibility.

Provides:

- immutable reference views by explicit version and effective position;
- validation of price, quantity, symbol, and listing status;
- reference events for ordered run consumption;
- migration and compatibility checks.

It does not infer strategy equivalence or consume market events to mutate reference facts.

### Adapter SDK

Defines contracts shared by market-data, external-source, paper, and future live-execution adapters:

- lifecycle and health states;
- source framing and capture handoff;
- capability declaration;
- version and venue metadata;
- bounded retry and reconnection hooks;
- test harnesses and conformance fixtures;
- opaque venue extensions that preserve source meaning.

The SDK contains no concrete venue policy and no strategy logic.

### Source adapters and capture

Own connectivity, framing, receive timestamps, capture sequence, raw payload integrity, and source-session health.

They emit immutable source events or explicit capture failures. They do not normalize venue meaning, infer gaps beyond the source contract, or update market state.

Capture must remain able to retain malformed and unsupported input. Under pressure it follows the declared priority and loss policy rather than silently discarding evidence.

### Normalization

Owns source decoding, validation, canonical mapping, normalized stream assignment, and venue-extension preservation.

Normalization is deterministic for a pinned source event, normalizer version, reference version, and declared policy. It emits normalized events or typed failures; it does not mutate market state or produce strategy observations.

### External-observation authority

Deferred for initial V1 but reserved as a distinct extension boundary. It owns normalization, quality, correction, timing, and provenance of non-venue observations such as news, social data, or prediction-market state.

External observations do not enter the market-book authority or masquerade as normalized venue events. Consumers receive typed, versioned observations with explicit source and quality. Every influential observation must remain visible in downstream causal lineage and explanation factors.

### Stream and run-input authority

Owns:

- stream identity, epoch, and cursor progression;
- duplicate and ordering acceptance;
- definition, versioning, and deterministic execution of the merge policy for market, reference, and control inputs;
- monotonic `run_input_sequence`;
- application of control-event effective positions;
- dispatched run-input facts and their ordered positions.

This authority does not own the run initialization manifest or replay reconstruction. It publishes the selected merge-policy identity/version and ordered dispatch facts so the run/configuration authority can reference the policy and the dataset/replay authority can reconstruct and validate the same order. It does not interpret book, strategy, risk, or accounting semantics. It determines which accepted input is next, not what the input means.

### Market-state authority

Owns sequenced state transitions, complete `StateLineage`, L2 state, future additive L3 state, recent trade windows, freshness, synchronization, and immutable views.

The authority:

- accepts only validated, sequenced inputs;
- applies one run input atomically;
- publishes complete immutable cuts;
- exposes explicit non-tradeable states;
- restores only from validated snapshots plus complete tail input.

Consumers cannot access mutable order-book structures or bypass lineage.

### Feature runtime

Owns deterministic, versioned feature definitions, bounded history/window access, valid feature observations, and diagnostic observations.

It consumes immutable market-state views and declared external observations only through explicit contracts. A cache is an implementation detail and may not become an unversioned source of truth.

### Strategy runtime

Owns strategy-instance lifecycle and `StrategyEvaluation` outcomes. It emits exactly one signal or one abstention per completed invocation as defined by the domain model.

It provides:

- a versioned strategy interface;
- declared input, state, and history dependencies;
- deterministic clock and optional seeded-randomness access;
- resource and deadline controls;
- explanation-factor output;
- isolation hooks that can evolve without changing strategy semantics.

The runtime does not own recommendations, portfolio sizing, risk, or execution. Strategy code cannot access network, mutable global configuration, current wall time, credentials, or arbitrary persistence during deterministic evaluation.

### Recommendation authority

Owns exactly one actionable or hold recommendation for each valid signal. It is portfolio-neutral and preserves signal and external-observation explanation lineage.

This boundary prevents product-facing "suggested trade" semantics from being conflated with target positions, risk previews, or executable intents.

### Opportunity and resolution modules

These are extension modules for multi-venue and external-signal phases. They own:

- opportunities and legs;
- candidate eligibility;
- candidate-market ranking;
- mapping economic intent to listings;
- provenance of market and external observations.

They consume actionable TradeRecommendations, compose them into an economic opportunity where needed, and resolve opportunity legs to eligible listings before target construction as specified by the domain model. They cannot perform strategy evaluation, create a second TradeRecommendation for a signal, or authorize risk or execution.

### Portfolio construction

Owns portfolio-scoped aggregation, sizing, absolute target positions, no-change outcomes, and target supersession.

It consumes active actionable recommendations and an explicit portfolio-state version. It neither reserves capacity nor assumes a recommendation is executable.

### Risk-policy authority

Owns approved, modified, and rejected risk decisions under versioned policies and authoritative projected-exposure inputs.

It fails closed when inputs are stale, unavailable, or inconsistent. It does not submit orders or allocate concurrent capacity.

### Exposure and reservation authority

Owns serialized projected exposure, `risk_sequence`, reservation outcomes, reservations, transfer, consumption, expiry, and release.

This boundary must be single-writer for each declared risk scope, even if multiple scopes are processed concurrently. It cannot issue or alter a risk decision. Its state must account for open and unknown orders and execution-group worst cases.

### Execution planning and group coordination

Owns translation of approved targets, accepted reservations, mode requirements, and later human approvals into mode-bound executable order intents.

Execution-group coordination is additive for sliced or multi-leg actions. It declares partial-completion and unwind policy without claiming venue atomicity.

This component cannot convert a paper intent into a live intent or create an intent that awaits approval.

### Paper broker

Owns model-versioned simulated order acknowledgements, rejections, fills, partial fills, cancellation races, and latency/cost behavior.

The paper broker uses the same order and fill lifecycle contracts as live execution where semantics match, while retaining explicit simulation provenance. It cannot present modeled outcomes as venue facts.

### Live execution adapters

Deferred until the live-execution phase. They own venue submission, acknowledgement, cancellation, external identifiers, fill ingestion, ambiguous-outcome handling, and reconciliation hooks.

They accept only executable live intents valid for the adapter, account, venue, mode, approval, reservation, and execution-health state. They cannot synthesize authorization or retry an ambiguous economic order unsafely.

### Ledger and accounting

Owns immutable balanced postings, idempotent economic effects, cash and asset balances, positions, cost basis, fees, realized P&L, and accounting projections.

Positions and P&L are derived. The ledger never consumes a UI edit as economic truth.

### Mark and valuation

Owns mark policy, mark quality, accounting-currency conversion where introduced, and unrealized valuation projections. It is separate from the ledger so valuation cannot rewrite economic facts.

### Reconciliation

Owns comparisons between Chronos and authoritative paper/venue evidence, discrepancy lifecycle, freezes, correction workflows, and resolution evidence.

Reconciliation produces linked correction facts. It does not mutate source, order, fill, or ledger history.

### Research, backtest, and experiment registry

Owns authoritative experiment identity and lifecycle:

- immutable experiment specification and parent/comparison relationships;
- selected run, dataset, replay class, strategy, feature, portfolio, cost, risk, paper, accounting, and mark versions;
- declared hypotheses, metrics, acceptance criteria, and controlled variables;
- status, result-artifact references, semantic checksums, and invalidation/supersession;
- review and strategy-promotion evidence.

The registry references run manifests, datasets, and result artifacts owned elsewhere. It does not execute a second market-state, strategy, risk, paper, or accounting engine; research and backtesting invoke the same production domain paths. Reports and notebooks are consumers and cannot rewrite registered inputs or results.

### Operator identity and approval authority

Owns authenticated operator identity and, in human-approved live mode only, immutable execution approval, rejection, revocation, and expiry facts.

It validates actor, reservation, approving risk decision, prospective action, authorization policy, freshness, and anti-replay conditions. The console and API submit commands to this authority but do not create approval facts themselves. Live paper never invokes this authority for execution approval.

### Run and configuration authority

Owns:

- run lifecycle and parent/reset/recovery relationships;
- configuration schemas, resolved configuration identity, epochs, and effective control events;
- the immutable initialization manifest and terminal attestation;
- selection of mode, replay class/live input mode, dataset/capture identity, initial snapshots, component/policy versions, and merge-policy version.

The initialization manifest references the merge-policy identity/version defined by the stream sequencing authority. Referencing a policy does not transfer ownership of its definition. The authority does not reconstruct dataset order or validate that a dataset satisfies its manifest.

### Dataset and replay authority

Owns:

- source-capture, normalized-dataset, corrected-research-dataset, and replay manifests;
- replay-class and correction-policy declarations;
- partition integrity, ordered-input reconstruction, snapshot/tail selection, and replay-input validation;
- validation that reconstructed inputs conform to the run manifest's selected merge-policy version and initial cursors;
- content identity, lineage, retention references, and faithful/non-faithful classification.

It consumes the stream authority's versioned merge-policy contract to reconstruct and validate run inputs. It cannot define a competing merge policy, mutate the run initialization manifest, or claim corrected research replay is faithful capture.

The stream, run/configuration, and dataset/replay authorities may share a process or storage technology initially, but their records and ownership are singular and non-overlapping.

### Observability

Owns measurement definitions, metric and trace schemas, log conventions, health projections, latency-point comparability, and evidence export.

Instrumentation may observe a boundary but cannot cause a domain decision to succeed, fail, or change ordering. Blocking telemetry exporters are prohibited on the hot path.

### Audit and provenance

Owns retention and traversal of the causal graph and actor/configuration attribution not already fully represented by domain facts.

Audit is not a shadow source of truth. It references authoritative records and must identify any missing or incomplete evidence.

### Query models and operations API

Owns disposable read models and typed query/command endpoints for the console. Read models may be eventually consistent and must expose source position, age, completeness, and mode.

Commands are routed to the owning authority and return command outcomes. API handlers do not update domain stores directly.

### Operations console

Owns presentation, interaction flow, local session handling, and operator command submission.

It does not contain strategy, risk, execution, or accounting rules. UI validation improves usability but never replaces authority-side validation.

## Dependency direction

Dependencies point inward toward stable semantic contracts and outward toward replaceable infrastructure.

```text
                         applications
       console   CLI   local API   replay/benchmark runners
                              |
                              v
                  orchestration and use cases
                              |
                              v
        domain authorities and deterministic transition logic
                              |
                              v
           canonical contracts, value objects, and policies

Infrastructure implementations point inward:

venue adapters  storage  clocks  telemetry  web frameworks  serializers
       \           |       |        |             |             /
        +----------+-------+--------+-------------+------------+
                              |
                              v
                    declared ports/contracts
```

Normative rules:

1. Canonical contracts and value objects depend on no adapter, database, web, UI, telemetry, or operating-system implementation.
2. Domain authorities may depend on canonical contracts and narrow ports for clocks, persistence handoff, event publication, and external capabilities.
3. Infrastructure implements ports; domain modules do not import infrastructure implementations.
4. Orchestration coordinates authorities through public contracts and cannot reach into internal state.
5. The UI depends on the operations API schema, never engine internals.
6. Adapters depend on the adapter SDK and canonical contracts, never strategies or portfolio rules.
7. Strategy implementations depend only on the strategy SDK and approved deterministic utilities.
8. Research tooling invokes production domain paths; production modules do not depend on notebooks or experiment presentation.
9. Observability hooks use stable measurement interfaces; domain code does not depend on a vendor exporter.
10. Cross-module persistence queries are prohibited. Consumers use events, commands, or declared read interfaces.
11. Circular dependencies are prohibited. Shared semantics move into an explicitly owned lower-level contract module rather than a generic utility dumping ground.

These rules must be enforced by build targets, package visibility, import rules, and architecture tests.

## Stable contracts

The architecture stabilizes logical contracts before choosing transport.

### Contract categories

Every boundary uses one or more of:

- **Command contract:** intended transition, target authority, actor, precondition, idempotency key, and requested effective position where applicable.
- **Event contract:** immutable accepted/observed fact using the minimum envelope and namespace taxonomy.
- **View contract:** immutable snapshot with identity, version, source position, freshness, completeness, and reconstruction lineage.
- **Query contract:** side-effect-free request for a view or projection.
- **Capability contract:** adapter/runtime declaration of supported features and constraints.
- **Health contract:** typed readiness, liveness, degradation, dependency, and recovery status.
- **Snapshot contract:** state plus complete cursor/configuration/version metadata and validation checksum.
- **Manifest contract:** immutable versions, inputs, policies, clocks, modes, and initial state.

### Encoding and transport independence

Canonical meaning must survive:

- in-process typed calls;
- bounded in-memory queues;
- durable journal records;
- local inter-process communication;
- future network transport where justified;
- batched or columnar dataset representations.

No transport-specific field may become the sole carrier of identity, ordering, causation, actor, mode, or version. Serialization adapters must prove round-trip semantic equivalence.

### Compatibility

Each contract declares:

- semantic owner;
- schema and policy versions;
- required and optional fields;
- unknown-field and unknown-type behavior;
- ordering and idempotency scope;
- compatibility window;
- migration, rebuild, or rejection path;
- test fixtures and conformance owner.

Semantic changes require a new version even when the wire shape remains compatible. During planning and early implementation, Chronos may support only one active major version, but retained data must remain interpretable or have an explicit migration/re-normalization path.

### API boundaries

The local operations API separates:

- commands that may change authoritative state;
- queries over read models;
- streaming subscriptions to projections;
- administrative dataset/run operations;
- later execution approvals with stronger identity and authorization.

An HTTP, WebSocket, local socket, or in-process API is an implementation choice. API schemas must not expose mutable internal pointers, language-native object serialization, or secret values.

## Execution modes and architecture enforcement

Mode is immutable per run and is enforced at multiple boundaries:

| Mode | Input architecture | Decision path | Execution boundary |
|---|---|---|---|
| Replay analysis | Dataset/replay provider through normal dispatch | State through targets as permitted by the domain model | No executable intent unless paper simulation is explicitly enabled |
| Backtest/paper replay | Replay provider plus paper policies | Full deterministic paper path | Paper broker only |
| Live read-only | Live capture and normalization | State through recommendations and proposed targets | No authoritative risk, reservation, intent, order, or fill |
| Live paper | Live capture plus paper risk/execution | Full paper path | Paper broker only; no human-approval workflow |
| Human-approved live | Live capture, live risk/reservation, approval authority | Full path with approval before intent creation | Eligible live adapter only |
| Guarded automated live | Live capture and explicit automation policy | Full path under declared guardrails | Eligible live adapter only |

Mode enforcement is defense in depth:

1. The run manifest fixes the mode.
2. Orchestration constructs only mode-allowed components.
3. Domain state machines reject prohibited transitions.
4. Executable intents carry mode and eligible adapter scope.
5. Adapter capability checks reject mismatched intents.
6. Credentials are unavailable to processes and modules that do not need them.
7. Tests prove prohibited paths are unreachable.

A configuration change cannot convert an existing run or intent to another mode.

## Repository and module structure

The intended repository is a monorepo with language-neutral top-level ownership. Exact build-system syntax and language package names are deferred.

```text
/
├── contracts/
│   ├── domain/                 # canonical envelopes, IDs, value semantics
│   ├── commands/
│   ├── events/
│   ├── views/
│   └── conformance/            # fixtures and compatibility tests
├── core/
│   ├── dispatch/
│   ├── market_state/
│   ├── features/
│   ├── strategy_runtime/
│   ├── recommendation/
│   ├── portfolio/
│   ├── risk/
│   └── execution_planning/
├── accounting/
│   ├── ledger/
│   ├── valuation/
│   └── reconciliation/
├── adapters/
│   ├── sdk/
│   ├── market_data/
│   ├── external/
│   ├── paper/
│   └── execution/
├── runtime/
│   ├── run_control/
│   ├── configuration/
│   ├── datasets/
│   ├── persistence/
│   ├── observability/
│   └── audit/
├── applications/
│   ├── local_api/
│   ├── console/
│   ├── cli/
│   ├── replay_runner/
│   └── benchmark_runner/
├── strategies/
│   ├── sdk/
│   ├── reference/
│   └── fixtures/
├── research/
│   ├── registry/
│   ├── experiments/
│   ├── reports/
│   └── dataset_tools/
├── tests/
│   ├── contract/
│   ├── integration/
│   ├── replay/
│   ├── recovery/
│   ├── performance/
│   └── end_to_end/
├── tools/
│   ├── development/
│   ├── schema/
│   ├── profiling/
│   └── release/
├── config/
│   ├── schemas/
│   ├── defaults/
│   └── examples/
├── docs/
│   ├── adr/
│   ├── runbooks/
│   └── prds/
└── planning/
```

Structural rules:

- Generated contract code, if used, is generated from one canonical source and never hand-edited in multiple languages.
- `core/` contains no concrete venue, storage, web, UI, or telemetry-exporter implementation.
- Concrete strategies live outside the strategy runtime.
- Research artifacts cannot be imported by production runtime targets.
- The experiment registry stores immutable specifications and result references; notebooks and reports are non-authoritative consumers.
- Tests may use privileged inspection helpers that production code cannot import.
- Fixtures and small redistributable datasets are versioned; large or sensitive captures are referenced by content-addressed manifests rather than committed casually.
- Secret-bearing files are excluded by construction and validated in CI.
- Every top-level module documents its owner, public API, accepted dependencies, failure semantics, and reconstruction source.

The structure may be refined before implementation, but changes must preserve ownership and dependency direction rather than grouping code solely by language or framework.

## Process and deployment evolution

### Initial topology

The default initial logical topology is:

1. **Ingress boundary:** concrete adapters, receive timestamps, source framing, and source-health reporting.
2. **Capture boundary:** immutable source-event acceptance and handoff to source persistence.
3. **Normalization/reference boundary:** pinned decoding and mapping against explicit reference-data versions.
4. **Engine runtime:** stream sequencing and merge-policy execution, run-input dispatch, market state, required features, strategy runtime, recommendations, and later mode-appropriate portfolio/risk/paper modules.
5. **Run/control/API runtime:** run and static configuration, immutable initialization manifests, control commands/events, operator identity, query models, and console API.
6. **Dataset/replay runtime:** dataset validation, ordered reconstruction, replay providers, experiment-registry integration, and result manifests.
7. **Persistence adapters:** narrow ports from owning authorities to source, event, manifest, snapshot, experiment, execution, ledger, and audit stores selected by later phases.
8. **Audit/provenance and telemetry sinks:** asynchronous consumers of authoritative facts and non-authoritative measurements respectively.
9. **Web console:** static/local web application served by or alongside the local API.

Direction is explicit: adapters feed capture; capture feeds persistence and normalization; reference data feeds normalization and dispatch; dataset/replay feeds dispatch; owning authorities publish to persistence/audit ports; telemetry observes boundaries; recovery data returns to the owning authority for validation before publication. Consumers never read another authority's store as an integration shortcut.

Where language and performance choices allow, the engine and control/API may begin in one operating-system process while retaining module boundaries. If separate runtimes are chosen from the start, their IPC contract must be bounded, versioned, observable, and replayable.

### Permitted extraction triggers

A module may move to its own process when evidence shows at least one of:

- fault isolation is required to keep safety-critical work available;
- independent resource control prevents latency or memory interference;
- language/runtime isolation is necessary;
- security policy requires credential or permission separation;
- durability or lifecycle differs materially;
- independent scaling is measured and cannot be addressed locally;
- upgrade/restart isolation materially improves recovery;
- an external integration has blocking or unstable behavior that must be contained.

Extraction requires:

- a measured baseline and stated problem;
- a versioned contract already valid in process;
- bounded queues and explicit backpressure;
- compatible clock/latency treatment;
- idempotent restart and replay behavior;
- failure injection and recovery evidence;
- updated threat model and operational burden assessment.

### Distributed deployment

Remote or multi-host deployment is not prohibited, but it is deferred until a product requirement and measurements justify it. Moving to multiple hosts introduces clock uncertainty, partial failure, network partitions, deployment coordination, and a larger security surface. It must be treated as a new architecture decision, not a packaging toggle.

### No infrastructure assumptions

The architecture does not require:

- microservices;
- Kubernetes;
- Kafka or another distributed log;
- a service mesh;
- cloud-managed databases;
- containers for local development;
- separate databases per logical module;
- distributed consensus;
- remote object storage.

Later phases may select a simple local implementation first and preserve migration through ports, manifests, and contract tests.

## Configuration and secrets

### Configuration classes

Configuration is classified as:

- **Build configuration:** compiler, feature flags, target platform, generated-code versions.
- **Static runtime configuration:** process ports, local paths, resource ceilings, telemetry destinations.
- **Run configuration:** mode, datasets, instruments, strategy instances, policy versions, clocks, initial state.
- **Behavior-changing configuration:** strategy parameters, thresholds, risk limits, pause state, kill switch, and allowed execution behavior.
- **Secret references:** credential identity/version and access policy, never secret values.

Every configuration class, including static runtime configuration, must be schema-validated, versioned, attributable to a source, resolved under explicit precedence, and represented by a redacted canonical identity. A running domain path reads immutable configuration snapshots. Behavior-changing changes take effect only through ordered control events and configuration epochs.

Environment variables and command-line arguments may locate configuration or secret references, but they may not become an unrecorded alternative source of domain behavior.

### Precedence

Configuration precedence must be deterministic and inspectable. Each resolved value records its winning source category without recording secret values. The precedence order itself is versioned; changing it is a configuration-schema change.

The resolved static and run configuration is materialized in canonical redacted form, hashed or otherwise identified, and referenced by process-start evidence and, where relevant, the initialization manifest. Unknown keys, type mismatches, invalid units, incompatible versions, unresolved substitutions, unsafe paths, and forbidden secret interpolation fail validation.

Defaults are explicit and versioned. Safety-relevant values must not acquire permissive defaults merely because a field is omitted.

### Static-configuration restart semantics

Static configuration is immutable for one process incarnation. A change to ports, paths, resource ceilings, persistence endpoints, telemetry destinations, parser limits, or security settings requires a supervised restart unless a later contract explicitly reclassifies that field as behavior-changing run configuration.

On restart:

- the complete redacted static configuration identity and provenance are recorded before readiness;
- incompatible changes fail startup rather than partially applying;
- persistence and dataset locations are validated before authorities recover state;
- the process remains unready until its recovery owners prove the required continuity;
- the prior and new configuration identities remain linked in operational audit;
- rolling or partial restart behavior, if later supported, must prove cross-version compatibility.

Static configuration cannot silently change domain behavior. If a static change affects ordering, arithmetic, durability, security, or another semantic contract, it also requires an ADR and appropriate version change.

### Secrets

Secrets are accessed through narrow runtime ports and provided only to the process/module that needs them. Requirements include:

- no secret values in manifests, events, datasets, logs, traces, errors, crash reports, UI payloads, or test fixtures;
- separate credentials for market data and execution when supported;
- no live execution credentials in replay, research, live-read-only, or paper runtimes;
- least privilege and venue-side restrictions where available;
- explicit rotation and revocation behavior;
- startup validation that records secret identity/version and availability without exposing value;
- redaction tests and repository scanning;
- no strategy-plugin access to secrets.

The initial local implementation may use operating-system facilities or an encrypted local mechanism, but plaintext committed credentials and implicit developer-shell inheritance are prohibited.

## Build, toolchain, and CI standards

### Reproducible toolchain

The repository must pin or constrain:

- compiler/interpreter/runtime versions;
- package and system dependencies;
- schema/code generators;
- formatting and linting tools;
- benchmark and profiling tools;
- supported local platforms;
- generated artifacts and regeneration commands.

One documented bootstrap path must prepare a supported machine without manual mutation of source files. Builds must work without undeclared network access after dependencies and declared test datasets are available.

### Build graph

The build graph must mirror dependency direction. It must support:

- isolated module compilation/type checking;
- contract-code generation and drift detection;
- unit, contract, integration, replay, recovery, and performance targets;
- debug/sanitized and release/benchmark profiles;
- architecture/import-boundary checks;
- deterministic test seeds and artifact capture;
- a minimal engine build independent of UI and research tooling.

Warnings and static-analysis failures are governed centrally. Local suppression requires a reason and the narrowest possible scope.

### CI gates

Every change must run gates proportional to its affected boundaries. The baseline includes:

- formatting and linting;
- compilation or type checking for supported profiles;
- unit and contract tests;
- schema compatibility and generated-code drift checks;
- architecture dependency tests;
- secret and credential scanning;
- software-dependency and license policy checks;
- deterministic replay fixtures affected by the change;
- sanitizer and undefined-behavior checks for applicable native code;
- integration and end-to-end smoke paths;
- benchmark correctness validation.

Scheduled or release gates add:

- broader replay corpora;
- repeated scheduling/host determinism checks;
- property and model-based tests;
- crash/restart and fault-injection suites;
- soak and capacity tests;
- full dependency and supply-chain review;
- baseline performance comparison;
- recovery and audit reconstruction drills.

CI must publish machine-readable results and retain the manifests, seeds, versions, checksums, and relevant artifacts needed to reproduce a failure.

### Performance CI

Noisy shared CI is not the sole source of performance truth. Performance testing uses:

- a correctness-preserving benchmark profile;
- versioned workloads and manifests;
- controlled reference hardware or statistically characterized environments;
- repeated samples and tail distributions;
- explicit comparison methodology;
- recorded instrumentation state;
- triage rather than automatic acceptance of unexplained regressions.

## Security boundaries

Chronos is local-first, not trust-free. Local processes, datasets, adapters, browser surfaces, and dependencies remain attack surfaces.

### Trust zones

The architecture distinguishes:

1. **Untrusted external input:** venue payloads, external observations, imported datasets, browser requests.
2. **Validated domain input:** decoded and contract-checked commands/events that are not yet necessarily accepted by an authority.
3. **Authoritative runtime state:** accepted facts and internal state owned by domain authorities.
4. **Execution-capable zone:** live credentials, approvals, reservations, intent routing, and venue submission.
5. **Presentation/research zone:** console, exports, notebooks, reports, and user-authored analysis.

Data crosses a zone only through validation and typed contracts.

### Required controls

- Parsers treat source payloads and imported datasets as hostile and enforce framing, size, depth, range, and resource limits.
- The local API binds and authenticates according to its exposure; "localhost" alone is not an authorization model for execution.
- Browser-origin and command authorization controls prevent cross-site or local malicious command submission.
- Human execution approval requires stronger authenticated identity and anti-replay controls than ordinary queries.
- Execution credentials and adapter access are absent from non-live modes.
- Strategy and plugin-like code has no ambient network, filesystem, process, or secret access unless a later ADR explicitly grants a bounded capability.
- Durable artifacts use safe path handling, integrity checks, and explicit import/export locations.
- Logs and errors are structured and redacted.
- Dependency provenance, checksums/locks, review, and update policy are part of release evidence.
- Safety-critical operator commands require explicit reason, actor, command identity, and immutable outcome.
- Non-overridable controls are identified in the later risk and execution plans.

Security incidents that threaten execution integrity activate the relevant kill switch or execution freeze and preserve evidence for recovery.

## Failure containment and backpressure

### Containment scopes

Failures are contained at the narrowest safe scope:

- one malformed source event;
- one source stream or epoch;
- one listing or instrument;
- one strategy instance;
- one run;
- one portfolio/risk scope;
- one account or execution adapter;
- one projection or UI subscription;
- one process.

A contained failure may escalate when invariants cross scopes. For example, uncertain account exposure may freeze all execution sharing that account, while unrelated market capture continues.

### Priority order under pressure

When resources are constrained, the architecture prioritizes:

1. execution safety, fill ingestion, unknown-order handling, reservation integrity, and ledger correctness;
2. accepted control events, kill switch, and authoritative audit;
3. source capture and continuity evidence;
4. sequencing, state correctness, and freshness status;
5. time-sensitive strategy and risk work that can still meet validity deadlines;
6. required operational health;
7. UI projection updates, optional analytics, verbose logs, and exports.

Work may be shed only under an explicit policy that records what was lost or skipped and how run fidelity is affected.

### Queue policy

Every asynchronous boundary declares:

- producer and consumer;
- bounded capacity and sizing method;
- ordering and partition key;
- overflow behavior;
- deadline and stale-work policy;
- retry and poison-message policy;
- instrumentation;
- shutdown and drain behavior;
- recovery source.

Unbounded queues are prohibited. Silent overwrite is prohibited for authoritative facts. Blocking is allowed only when its safety and latency effect is explicitly designed.

### Kill-switch command path

The kill switch is a safety-critical behavior-changing command, not a best-effort UI flag. Its path is bounded and independently observable:

1. **Admission:** an authenticated command enters a reserved-capacity, priority admission path that cannot be starved by ordinary control traffic. Invalid identity, scope, mode, or replayed command identity yields an explicit rejection.
2. **Acceptance:** the run/configuration authority serializes the command and emits exactly one accepted or rejected control outcome. Acceptance records actor, scope, reason, requested time, chosen effective position, and prior switch state.
3. **Effective application:** the stream/run-input authority applies the accepted control event before the first input at its recorded `effective_position`. Risk and execution gates consume the resulting active configuration epoch.
4. **Acknowledgement:** acknowledgements are typed and non-conflated: `admitted`, `accepted` or `rejected`, `effective`, and, for execution-capable scopes, `execution_fenced`. `Execution_fenced` means every local boundary capable of accepting or externally submitting new work for the switch scope has observed the active switch epoch and rejects that work; it does not claim that previously submitted or unknown venue orders are cancelled. A receipt or accepted command is not proof that execution has stopped.
5. **Recheck:** risk decision, reservation-to-intent conversion, intent release, adapter submission, retry, replacement, and cancellation workflows recheck the active kill-switch state at their safety boundary. Already submitted or unknown orders follow the declared cancel/reconcile policy rather than being reported as stopped.
6. **Recovery:** restart restores the last recoverable switch state and remains execution-unready until control position and execution scope are reconciled. Uncertainty fails closed.

The observability phase establishes the measurement points, representative load, reference environment, and empirical budgets for command admission, acceptance, effective application, acknowledgement, and execution fencing. The portfolio/risk and execution phases adopt those measured budgets, define breach behavior, and prove the command path remains available under overload. No arbitrary latency number is fixed in Phase 01.

### Failure matrix

Each later phase supplies a matrix covering:

- failure source;
- detection signal;
- affected scope;
- immediate safe state;
- work permitted to continue;
- authority responsible for recovery;
- required operator action;
- reconstructability and fidelity status;
- evidence test.

The console displays this state but does not own it.

## Recovery ownership

Recovery follows authority ownership. A generic supervisor may restart processes, but it cannot decide that domain continuity is proven.

| Authority | Recovery responsibility |
|---|---|
| Capture | Restore source connection/session, preserve epoch transition, declare any unprovable loss |
| Normalization | Resume from retained source cursor with pinned versions; quarantine poison input |
| Stream/run-input | Validate cursor and dispatch state against its owned merge-policy version; resume deterministic sequencing from reconstructed accepted inputs |
| Market state | Validate snapshot with complete lineage, replay complete tail, compare semantic checksum |
| Features/strategy/recommendation | Recompute from authoritative views and pinned versions; never restore hidden mutable caches as truth |
| Portfolio/risk/reservation | Restore serialized exposure and active reservations before accepting new work |
| Paper/live execution | Reconcile in-flight, submitted, and unknown orders before retry or release |
| Ledger/accounting | Replay idempotent economic facts and verify balanced projections |
| Run/configuration | Restore the immutable initialization manifest, lifecycle, active epoch, and selected merge-policy reference; create a child recovery run when same-run resume cannot be proven |
| Dataset/replay | Validate dataset/replay manifests and integrity; reconstruct ordered inputs under the referenced merge-policy version; prove replay class and faithful/non-faithful status |
| Experiment registry | Restore immutable experiment specifications and result references; mark incomplete results without inventing completion |
| Query/console | Rebuild projections and expose lag/completeness |
| Observability/audit | Restore evidence where authoritative; declare optional telemetry gaps explicitly |

### Recovery contracts

Every authority with state defines:

- authoritative records;
- snapshot contents and validation;
- permitted loss window;
- checkpoint and tail semantics;
- startup reconciliation;
- idempotency boundary;
- incomplete/non-faithful conditions;
- fail-closed behavior;
- recovery time measurement;
- recovery drill.

Snapshots are accelerators, not independent truth. A snapshot without complete cursor, control, configuration, schema, and policy lineage is unusable.

Clean shutdown is not assumed. Crash points must be tested between acceptance, publication, persistence handoff, external submission, acknowledgement, fill acceptance, reservation transfer, and ledger posting as applicable.

## Performance-budget process

This architecture intentionally does not invent latency, throughput, memory, recovery, or telemetry-overhead numbers before workloads and hardware are defined.

### Budget hierarchy

Budgets are established in this order:

1. **Product scenario:** mode, instruments, venue streams, strategies, portfolios, operator actions, and expected duration.
2. **Correctness workload:** versioned event mix, burst shape, ordering anomalies, book depth, feature windows, and failure cases.
3. **Reference environment:** hardware, operating system, runtime versions, power/performance settings, and competing load.
4. **Causal path:** named latency points and segments from the domain model.
5. **Validity deadline:** latest time at which each decision remains useful and safe.
6. **Resource envelope:** CPU, memory, allocation, queue, disk, and network constraints.
7. **Stage budgets:** apportioned only after an end-to-end baseline and profile identify actual costs.
8. **Overload policy:** admission, shedding, pausing, and stale-work behavior beyond capacity.
9. **Regression policy:** statistical comparison, review threshold, owner, and exception expiry.

### Required budget artifacts

Each performance-sensitive phase must produce:

- a scenario/workload manifest;
- a reference-environment manifest;
- named same-clock and cross-clock latency semantics;
- throughput and burst requirements;
- latency distribution and tail objectives;
- queue-capacity derivation;
- allocation and memory-residency objectives;
- instrumentation-overhead measurement;
- overload and recovery objectives;
- current baseline with confidence/variance;
- stage-level profile and proposed budget;
- correctness result for the same workload;
- decision record for any accepted tradeoff.

### Budget rules

- End-to-end objectives are set before local stage optimization.
- Percentiles from separate stages are not added to claim an end-to-end percentile.
- Replay logical time is never reported as processing latency.
- External source/venue time is separated from Chronos processing unless clock uncertainty is bounded.
- Tail latency and deadline miss rate matter alongside medians.
- A faster result that changes semantic output fails.
- A budget exception is temporary, owned, evidenced, and cannot silently redefine the target.
- Hardware upgrades do not substitute for understanding an algorithmic or allocation regression.
- Optimization work begins with profiles and representative workloads.

The observability phase defines measurement mechanics. The event, strategy, risk, paper, and live phases set scenario-specific budgets through this process.

## Testing strategy

Chronos uses a layered testing pyramid with replay and recovery as first-class layers rather than treating all confidence as end-to-end UI testing.

### Level 1: Pure unit and invariant tests

Fast tests cover:

- exact arithmetic, units, rounding, and boundaries;
- value objects and identities;
- parsers and validators;
- deterministic transition functions;
- state machines;
- policy rules;
- explanation and provenance construction;
- invalid and unavailable states.

### Level 2: Property and model-based tests

Generated sequences cover:

- book updates and recovery;
- stream cursor and state-lineage vectors;
- control effective positions;
- strategy outcome/recommendation cardinality;
- reservation concurrency;
- order lifecycles and cancel/fill races;
- ledger balance and corrections;
- mode-prohibited transitions.

### Level 3: Contract and conformance tests

Every boundary has producer/consumer fixtures covering:

- supported versions;
- unknown fields and types;
- malformed and oversized input;
- batching/serialization round trips;
- idempotency;
- ordering;
- capabilities and health;
- redaction.

All adapter implementations run a shared SDK conformance suite.

### Level 4: Module integration tests

Tests combine a small number of real modules with controlled ports, such as:

- capture plus normalization;
- dispatch plus market state;
- market state plus feature/strategy/recommendation;
- portfolio plus risk/reservation;
- paper broker plus ledger;
- control authority plus effective-position dispatch.

### Level 5: Deterministic replay and golden tests

Versioned fixtures cover all replay classes and full semantic checksums. Identical manifests must produce identical domain results across repeated scheduling and supported environments, subject only to explicit exclusions.

Golden fixtures are reviewed artifacts. Updating expected output requires explanation of the semantic change, not a blind regeneration.

### Level 6: Failure, crash, and recovery tests

Fault injection covers:

- disconnects, gaps, duplicates, late and poison events;
- queue saturation and deadline breaches;
- process termination at durability boundaries;
- snapshot corruption and incomplete tails;
- unavailable dependencies;
- unknown external outcomes;
- duplicate fills and corrections;
- stale approvals and reservations;
- UI/control disconnection.

Recovery tests prove either semantic continuity or the correct incomplete/failed/non-faithful outcome.

### Level 7: Performance and soak tests

Benchmarks use representative manifests, retain correctness checks, measure overhead, and produce profiles. Soak tests examine leaks, queue drift, clock behavior, dataset growth, reconnects, and recovery over sustained runs.

### Level 8: End-to-end operator scenarios

Local scenarios exercise:

- create and run replay;
- inspect state, signals, explanations, and latency;
- apply ordered configuration changes;
- pause, resume, stop, and reset;
- run live read-only and live paper;
- later approve or reject live execution;
- investigate and recover a failure.

End-to-end tests verify integration and usability; they do not replace lower-level semantic evidence.

## Architecture decision governance

### ADR scope

An architecture decision record is required for decisions that affect:

- domain meaning or authority;
- module or process boundaries;
- dependency direction;
- contract/schema compatibility;
- ordering, clocks, determinism, or replay;
- persistence and durability;
- security or trust zones;
- execution safety;
- language/runtime selection;
- storage, IPC, queue, or deployment technology;
- performance-budget tradeoffs;
- supported platform or operational burden.

Minor local implementation choices need not become ADRs unless they establish a precedent or constrain later phases.

### ADR contents

Each ADR records:

- status and date;
- context and problem;
- decision drivers;
- considered alternatives;
- chosen decision;
- consequences and risks;
- affected contracts, phases, and evidence;
- migration and rollback/reversal path;
- expiry or review trigger where uncertainty remains;
- links to measurements, prototypes, threat models, or tests.

Status values are `proposed`, `accepted`, `superseded`, or `rejected`. Accepted ADRs are immutable; changes supersede them.

### Review rules

- A decision that changes an approved semantic baseline requires re-review of the domain model and all affected later planning leaves.
- A decision affecting an earlier phase triggers cumulative compatibility review.
- Temporary exceptions identify owner, scope, reason, evidence, and expiry.
- ADRs cannot waive domain invariants merely because a technology makes them inconvenient.
- Experimental spikes are permitted before an ADR, but their code and data are not production precedent until the decision is accepted.

## Technology decision criteria

Technologies are evaluated against weighted project needs rather than familiarity alone.

### Required criteria

1. Semantic correctness and ability to enforce domain invariants.
2. Determinism and control over clocks, ordering, randomness, and arithmetic.
3. Measurable latency, tail behavior, allocation, and resource use.
4. Local operational simplicity and debuggability.
5. Crash behavior, durability, backup, and recovery support.
6. Contract/versioning and migration support.
7. Security posture, dependency provenance, and maintenance health.
8. Interoperability across likely engine, control, and UI runtimes.
9. Testability, fault injection, profiling, and observability.
10. Ecosystem maturity for the exact problem, not general popularity.
11. Portability across supported development environments.
12. Reversibility and data export without semantic loss.
13. Total maintenance burden for a small team.
14. Licensing and distribution constraints.

### Decision method

For material choices:

- define workload and non-functional requirements first;
- identify at least one simpler option;
- prototype the highest-risk behavior;
- test failure and recovery, not only happy-path throughput;
- measure with production-like contracts and instrumentation;
- document operational tasks and upgrade path;
- select the least complex option that meets proven needs;
- retain a replacement seam where uncertainty remains material.

Examples of choices governed this way include engine language, Python/native boundary, serializer, embedded store, journal format, snapshot format, IPC, UI framework, and telemetry stack.

## Phase dependencies and extension points

The planning tree has eleven phases. Each phase extends the architecture without changing established meanings.

### 01 — Architecture and engineering standards

Establishes:

- domain semantics and authorities;
- module and dependency boundaries;
- runtime planes;
- repository/build/CI standards;
- mode and security boundaries;
- decision governance;
- evidence contracts.

No later phase may invent an alternative lifecycle or bypass these boundaries.

### 02 — Observability foundation

Extends:

- typed health contracts;
- latency-point and segment instrumentation;
- structured log, metric, trace, profile, and alert schemas;
- measurement overhead and clock-comparability controls;
- local evidence collection and dashboard inputs.

It observes canonical boundaries through ports and cannot become required for domain correctness except where a safety policy explicitly consumes an authoritative health fact.

### 03 — Event and persistence infrastructure

Selects:

- source capture and accepted-event journal formats;
- manifest, snapshot, dataset-catalog, and retention mechanisms;
- durability matrices and recovery-point policies;
- stream/run-input implementation;
- schema evolution and rebuild tooling;
- local storage boundaries.

It implements the data-plane ports without centralizing all module state into an unowned shared database.

### 04 — Market-data pipeline

Adds:

- first venue market-data adapter;
- reference mappings;
- normalization leaf schemas;
- source sequence/gap/reconnect policy;
- L2 state and later-compatible L3 extension seams;
- live read-only capture and replay parity.

Venue-specific fields remain in adapters/extensions; the core consumes canonical contracts.

### 05 — Deterministic strategy runtime

Adds:

- feature and strategy SDKs;
- deterministic invocation and resource controls;
- strategy instances and versioning;
- evaluation/signal/abstention contracts;
- recommendation authority and explanations;
- reference strategies as conformance examples.

Strategy isolation may begin in process and later move behind the same SDK contract.

### 06 — Research and backtesting platform

Adds:

- the authoritative research/backtest/experiment registry;
- experiment manifests;
- dataset selection and replay-class controls;
- result comparison and reporting;
- cost/latency assumptions;
- look-ahead protection;
- strategy promotion evidence.

Research calls the production replay and decision path. It may add analysis stores but cannot create a second semantic engine.

### 07 — Portfolio, risk, and control plane

Adds:

- portfolio state and construction;
- risk policies and limit hierarchy;
- serialized projected exposure;
- reservation lifecycle;
- kill switch and safety health inputs;
- control commands and operator audit.

It preserves the signal/recommendation distinction and establishes single-writer risk scopes without requiring a distributed risk service.

### 08 — Paper execution and accounting

Adds:

- executable paper intents;
- paper broker and fill models;
- order lifecycle;
- immutable ledger;
- positions, cost basis, marks, P&L, and reconciliation;
- execution/accounting durability boundaries.

Paper and live contracts share only valid semantics. Simulation provenance remains explicit.

### 09 — Operations console

Adds:

- local API and read models;
- replay/live controls;
- state, signal, recommendation, position, P&L, health, and latency views;
- investigation and audit navigation;
- explicit lag, freshness, quality, and mode display.

The console remains replaceable and non-authoritative.

### 10 — Live paper operation

This phase has two ordered substages.

**10A — Single-venue live-paper operation** integrates prior modules into a production-like local deployment with:

- continuous capture and live paper processing;
- supervised lifecycle and restart;
- alerting, runbooks, capacity, soak, and recovery evidence;
- operational configuration and safe degradation;
- end-to-end audit reconstruction.

**10B — Multi-venue and optional L3 live-paper expansion** owns the original product PRD Phase 5 scope after 10A is stable:

- additional venue-adapter conformance;
- multi-venue reference mappings, streams, complete state-lineage cuts, and synchronization policies;
- normalized venue comparison and listing resolution;
- additive L3 event/state capability without breaking L2 consumers;
- cross-venue opportunity evaluation in replay and live paper;
- execution-group and residual-exposure simulation where multi-leg recommendations are tested.

Phase 10 therefore proves the architecture under sustained live-paper conditions first for one venue and then for the original multi-venue/L3 scope before live credentials are introduced. Passing 10A does not imply 10B is complete.

### 11 — External discovery and human-approved live execution

This phase also has ordered substages.

**11A — External-signal and Polymarket discovery** owns the original product PRD Phase 6 scope before live execution is enabled:

- external-observation adapter and authority implementation;
- source quality, correction, timing, and provenance;
- opportunity and leg creation;
- candidate-market discovery, eligibility, ranking, and opportunity resolution;
- Polymarket discovery and human-facing recommendation;
- explanations that preserve every influential external observation.

Discovery and recommendation do not require live Polymarket execution. External sources cannot distort or block the low-latency market-data core when disabled or degraded.

**11B — Human-approved live execution foundation** adds:

- authenticated approval authority;
- live execution adapters and credentials;
- pre-submit health and mode enforcement;
- external order/fill durability and reconciliation;
- unknown-order containment;
- tightly bounded capital rollout.

The execution zone is additive. It cannot change upstream facts or treat recommendations as authorization.

If a discovered opportunity is later made executable, it must enter the established target, risk, reservation, human-approval, intent, adapter, reconciliation, and ledger path. Completion of 11A never authorizes execution.

**11C — Guarded automation, optional** may follow only after human-approved execution evidence is satisfactory. It adds explicit automation policy and constrained approval substitution; unconstrained automation remains out of scope.

### Cross-phase extension obligations

The seams for original PRD Phases 5 and 6 are established earlier but activated in 10B and 11A:

- Phases 01–04 preserve adapter, listing, stream, lineage, L2/L3, external-event, and schema extension contracts.
- Phases 05–06 preserve multi-input strategy, opportunity, explanation, experiment, and replay contracts.
- Phases 07–09 preserve multi-leg target/risk/reservation, execution-group, audit, and operator-review contracts.
- Phase 10B validates multi-venue/L3 behavior without live-capital risk.
- Phase 11A validates external/Polymarket discovery before live execution, without granting external observations execution authority.

These obligations prevent orphaning the original roadmap while avoiding unused implementation on the initial hot path.

## Explicit non-goals

This architecture does not:

- operate an exchange or matching engine;
- define profitable strategies or claim trading alpha;
- choose the first venue;
- choose exact languages for every non-hot-path component;
- require a specific language at a non-hot-path component boundary before measurement and prototyping (the optimized-C++ hot path and the Python control plane are fixed product-goal constraints by ADR-0001, and are deliberately exempt from the measure-first selection rule; all other component languages remain evidence-driven);
- define concrete event leaf schemas beyond the domain-model taxonomy;
- choose storage, queue, IPC, web, telemetry, or deployment products;
- require microservices, containers, Kubernetes, Kafka, or cloud hosting;
- provide high availability across machines;
- support multi-user hosted operation in the initial product;
- provide unconstrained automation;
- implement fund administration, tax, investor reporting, regulatory reporting, or compliance case management;
- make the console, telemetry store, notebook, report, or ad hoc research database authoritative; only the defined experiment registry owns experiment identity and lifecycle;
- permit strategy code to submit orders;
- permit manual mutation of fills, balances, positions, or P&L;
- define implementation tickets, staffing, estimates, or release dates.

## Deferred decisions

The following decisions belong to later planning or evidence-driven ADRs:

1. UI language and framework, and the concrete FFI/IPC mechanism of the Python/C++ boundary. (The hot-path language is fixed to optimized C++ and the control plane to Python by ADR-0001; only the concrete boundary mechanism and the UI runtime remain open.)
2. Whether the first engine and control plane share a process.
3. Canonical schema-definition and generated-code mechanism.
4. In-memory queue and dispatch implementation.
5. Capture journal, normalized journal, snapshot, manifest, and catalog storage formats.
6. Embedded database or file-store choices and transaction boundaries.
7. Exact ID generation and deterministic derived-ID policy.
8. Concrete fixed-point widths, per-listing scale exponents, and the exact rounding-mode policy per operation. (Fixed-point integer representation on the hot path is fixed by ADR-0003; the remaining choices are the specific integer widths, scales, and rounding modes.)
9. First venue, source protocol, resumption, and sequencing rules.
10. Feature window storage and cache design.
11. Strategy loading, ABI/API, sandboxing, and upgrade mechanism.
12. Exact risk-scope partitioning, reservation persistence, and limit policy.
13. Paper model and accounting policies.
14. Operations API transport, console framework, and local authentication implementation.
15. Telemetry libraries, local collection store, and visualization tools.
16. Packaging, installers, containers, and supported operating-system matrix.
17. Concrete performance budgets, after scenarios and reference hardware exist.
18. Live credential mechanism, approval policy, execution venue, and capital rollout.
19. Multi-host deployment or distributed infrastructure.

Deferral does not permit a module to create an incompatible local convention. An unresolved decision must remain behind a contract, configuration seam, or unimplemented boundary.

## Implementation-phase evidence and exit contract

Evidence is gated by when the capability exists. Phase 01 closes only on architecture scaffolding and baseline contracts that can be implemented in Phase 01. Evidence requiring market feeds, strategy execution, persistence recovery, paper orders, or live execution belongs to the phase that introduces that capability.

### Phase 01 closure evidence

| Evidence ID and artifact | Required contents | Phase 01 pass condition |
|---|---|---|
| **AR1-E01 — Repository/module scaffold** | Initial directories or build targets for contracts, core, adapters, runtime, applications, research, tests, tools, configuration, and docs; owner metadata for each created module | Scaffold matches dependency direction; no module claims an unimplemented authority or imports a prohibited layer |
| **AR1-E02 — Enforced dependency graph** | Machine-generated dependency graph and allow/deny rules over the Phase 01 scaffold | No cycle, UI-to-core-internal edge, research-to-production reverse dependency, adapter-to-strategy dependency, or direct cross-authority persistence dependency |
| **AR1-E03 — Baseline contract package** | Minimum event envelope, command/event distinction, identity/value-object stubs, view/capability/health interfaces, version fields, and representative fixtures available at this phase | Contracts compile or validate, preserve domain terminology, reject unsafe unknown semantics, and do not commit to deferred transport/storage technology |
| **AR1-E04 — Ownership and topology registry** | Every Phase 01 module mapped to one authority, public port, plane, initial process placement, allowed dependencies, and future recovery owner | Stream merge policy, run manifest, and replay reconstruction have the singular ownership defined in this document; no Phase 01 state has ambiguous ownership |
| **AR1-E05 — Configuration foundation** | Versioned schemas and resolver for build, static, run, behavior-changing, and secret-reference configuration; provenance/precedence model; canonical redaction; static restart classification | Invalid/unknown values fail; precedence is deterministic; redacted identities are stable; secret values cannot enter resolved artifacts; static changes are classified as restart-required |
| **AR1-E06 — Local bootstrap/build proof** | Pinned toolchain, dependency locks, one documented bootstrap, debug/test build, representative contract fixture, and clean shutdown | A clean supported machine can build and run the Phase 01 fixture without undocumented services, live credentials, or source edits |
| **AR1-E07 — CI and supply-chain baseline** | Formatting, linting, compile/type checks, unit/contract tests, architecture rules, generated-code drift check where applicable, secret scan, dependency/license policy, and artifact retention | A representative violation of each configured gate fails CI and yields reproducible evidence |
| **AR1-E08 — Security and secret-boundary baseline** | Initial trust-zone diagram, local API exposure policy, secret-port interfaces, redaction tests, repository scan, and rule that live credentials are absent | Phase 01 processes have no live credential path; untrusted inputs cross validation ports; redaction and scanning tests pass |
| **AR1-E09 — Lifecycle and authorization-fence contracts** | Typed definitions/tests for `computed`, `accepted`, `published`, `recoverability-accepted`, and `recoverable`, plus an unimplemented execution authorization-fence interface | States are not conflated; an executable-intent acceptance port requires typed proof that authorization is recoverable or participates in the same atomic recoverability commit |
| **AR1-E10 — ADR and technology-decision governance** | ADR template/register, decision criteria, supersession rules, and initial topology/toolchain decisions | Every material Phase 01 technology or boundary choice is recorded with alternatives and does not contradict the domain model |
| **AR1-E11 — Phase compatibility obligation register** | Each obligation below assigned to its implementation phase, contract owner, expected evidence, and cumulative-review trigger | No later requirement is falsely claimed as Phase 01 evidence or left without an owning phase |
| **AR1-E12 — Independent critique and planning consistency** | Critique disposition plus comparison with `domain-model.md` and current planning index | Zero unresolved material findings; all deferrals retain a contract seam and named owning phase |

Phase 01 does not fail because a later adapter, queue, durable store, strategy, risk engine, paper broker, console, or live execution path has not yet been implemented. It fails if its scaffold prevents those capabilities, assigns them ambiguously, or claims unsupported evidence.

### Cumulative compatibility obligations

The following are architecture obligations, not Phase 01 closure artifacts. Each becomes testable when its owning phase introduces the relevant implementation. At that phase gate, the evidence is added and the full architecture compatibility review is rerun.

| Owning phase | Required architecture evidence when capability exists |
|---|---|
| **02 — Observability** | Named clock/latency contracts; measurement overhead; kill-switch admission, acceptance, effective, acknowledgement, and execution-fence measurement points; empirical budget-setting method and reference workload |
| **03 — Event/persistence** | Concrete computed-to-recoverable transitions; durability matrix; bounded queues; source/event/control persistence; manifest/snapshot contracts; dataset reconstruction using the stream-owned merge-policy version; crash/restart evidence for implemented authorities |
| **04 — Market data** | Adapter conformance; capture/normalization/reference topology; complete lineage; gap/reconnect containment; replay/live input equivalence for accepted normalized/control facts |
| **05 — Strategy runtime** | Deterministic feature/strategy/recommendation path; resource containment; explanation provenance; no separate research engine semantics |
| **06 — Research/backtesting** | Authoritative experiment registry; replay-class and dataset controls; immutable experiment/result references; look-ahead protection; repeated-run semantic equivalence and comparison validity |
| **07 — Portfolio/risk/control** | Projected-exposure serialization; reservation outcomes; operational kill-switch command admission/effective/ack path through risk and reservation; overload and stale-state fail-closed behavior |
| **08 — Paper execution/accounting** | Authorization recoverability fence for paper intents; paper order/fill lifecycle; idempotent ledger; accounting recovery; mode isolation; queue and failure-containment evidence |
| **09 — Operations console** | Typed command acknowledgement states; source-position/freshness display; kill-switch status through `execution_fenced`; non-authoritative read-model behavior and secure local command submission |
| **10A — Single-venue live paper** | Production-like topology, soak/capacity profile, supervised restart, recovery drills, audit reconstruction, empirically validated budgets, and low-burden local operation |
| **10B — Multi-venue/L3 live paper** | Original PRD Phase 5 evidence: second venue conformance, multi-stream lineage/synchronization, L2 compatibility under additive L3, cross-venue opportunity replay/live-paper validation, and grouped residual-exposure simulation |
| **11A — External/Polymarket discovery** | Original PRD Phase 6 evidence: external-observation quality/provenance, optional isolation from the hot core, opportunity resolution, candidate ranking, Polymarket discovery, and explanation lineage; discovery itself grants no execution authority |
| **11B — Human-approved live execution** | Live credential isolation; accepted/recoverable reservation and approval fence before intent; pre-submit kill-switch recheck; ambiguous-order reconciliation; external submission/fill durability; bounded capital rollout; any executable discovered opportunity uses these gates |
| **11C — Optional guarded automation** | Explicit automation policy, constrained authority substitution, guardrail and kill-switch evidence, rollback, and proof that automation cannot escape established risk/execution scopes |

For every phase gate:

1. New evidence must be evaluated against the current domain model and this architecture.
2. Earlier evidence affected by new implementation must be rerun, not assumed valid.
3. Ownership, dependency, mode, configuration, recoverability, security, and topology maps must be updated.
4. A semantic or structural conflict requires an ADR and re-review of every affected earlier and later planning leaf.
5. Passing one substage does not imply a later substage has passed.

Planning approval of this document fixes the structural baseline. Later phases may refine implementation details and add modules, but any change to authority, dependency direction, runtime-plane responsibility, mode enforcement, recoverability fence, or stable contract requires an ADR and cumulative review.
