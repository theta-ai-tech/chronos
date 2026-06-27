# Chronos Telemetry Contracts

## Purpose

This document defines the planning-level observability contract for Chronos: what operational signals mean, how they are named and correlated, how clocks and latency are represented, which measurements each later phase must publish, how health and degradation are derived, how telemetry behaves under pressure, and what evidence is required before the observability foundation is considered implemented.

The approved [domain model](../01-architecture/domain-model.md) remains authoritative for domain language, identities, lifecycle, ordering, modes, causal provenance, auditability, and clock semantics. The approved [architecture](../01-architecture/architecture.md) remains authoritative for component boundaries, runtime planes, ownership, dependency direction, local-first topology, hot-path constraints, failure priorities, and the performance-budget process. This document specializes those contracts without redefining them.

This is not a product or implementation selection document. It does not choose a telemetry library, collector, database, dashboard product, tracing protocol, file format, hosted service, or fixed numeric service-level objective.

## Observability objectives

Chronos observability must make it possible to:

1. Determine whether each runtime scope is alive, ready for its permitted mode, degraded, recovering, overloaded, or unsafe.
2. Locate where time and capacity are consumed along a specific causal path without presenting incompatible clocks as precise durations.
3. Detect gaps, stale state, queue pressure, dropped optional telemetry, deadline misses, invariant failures, and recovery progress before they become plausible trading results.
4. Explain the operational context around a material domain decision while preserving the authoritative causal graph in domain facts and audit records.
5. Compare replay, benchmark, live read-only, live paper, and later live-execution runs under versioned workloads and declared instrumentation profiles.
6. Quantify the cost of instrumentation and prove that telemetry cannot alter domain outcomes, ordering, or safety.
7. Support local incident investigation and performance work without requiring remote infrastructure.
8. Provide stable schemas that later phases extend rather than replacing with component-local naming.
9. Produce machine-readable evidence for phase gates, regression review, capacity planning, recovery drills, and audit reconstruction.
10. Make telemetry incompleteness visible; absence of evidence must never be rendered as evidence of health.

## Non-goals

The observability foundation does not:

- become authoritative for market state, strategy outcomes, risk decisions, reservations, orders, fills, ledger entries, positions, P&L, or run lifecycle;
- replace audit and provenance records with logs or traces;
- guarantee lossless retention of every optional diagnostic signal;
- put a blocking network exporter, remote query, or synchronous telemetry-store write on a latency-critical path;
- assign profitability, strategy quality, risk appetite, execution quality, or accounting policy;
- define dashboards or console layout;
- select numeric latency, throughput, availability, retention, recovery, or overhead targets before reference workloads and environments exist;
- claim cross-host or source-to-Chronos latency is precise when clock comparability is unproven;
- require a distributed collector, service mesh, cloud account, or high-availability monitoring stack;
- make high-cardinality domain identifiers metric dimensions;
- treat replay logical time as processing duration;
- require verbose per-event logs in production;
- retain secret values, raw credentials, private keys, authentication tokens, or unsafe payloads for diagnostic convenience.

## Authority and responsibility

The **observability authority** owns:

- telemetry semantic conventions and schema registry;
- metric, log, trace, latency-point, and health-projection definitions;
- clock-domain registration and comparability rules for measurements;
- instrumentation profiles and measurement-overhead evidence;
- telemetry-profile epochs, transitions, and comparison-validity rules;
- telemetry pipeline status, loss accounting, and exporter health;
- local collection and evidence-export contracts;
- observability artifact manifests and schema compatibility tests.

The observability authority does not own:

- the domain condition being observed;
- authoritative run, market, strategy, risk, execution, or accounting transitions;
- the causal graph and actor/configuration attribution retained by audit and provenance;
- recovery decisions for another authority;
- safety policy, readiness prerequisites, or validity deadlines defined by later functional phases.

Each domain authority owns the meaning of its states and emits typed operational observations through the observability contract. The observability authority validates, aggregates, stores, and exports those observations. It may derive health projections from declared inputs, but it cannot silently reinterpret an `unknown`, `stale`, `gapped`, `rejected`, `recovering`, or `unavailable` domain state as healthy.

For execution fencing, the relevant risk, reservation, execution-planning, and adapter authorities own their boundary acknowledgements. Observability only projects those authoritative acknowledgements and their age/completeness. It cannot infer `execution_fenced` from absence of submissions, a kill-switch control event, a metric value, or a successful command response. If any required boundary acknowledgement is missing, stale, contradictory, or from the wrong control epoch, the projection is `unknown`, never `execution_fenced`.

## Signal taxonomy

Chronos uses five distinct evidence classes. A fact may lead to more than one signal, but each signal has one declared class and purpose.

| Class | Purpose | Typical lifetime | Authority and reliability | Prohibited use |
|---|---|---|---|---|
| **Structured log** | Bounded diagnostic narrative about an operational occurrence, with machine-queryable context | Short to medium; policy-driven | Non-authoritative telemetry; may be sampled or shed according to profile | Proving that a domain transition occurred; reconstructing economic state |
| **Metric** | Aggregated count, level, rate, distribution, or bounded-state measurement | Aggregated and retained by policy | Non-authoritative telemetry; individual updates may be lossy only under declared policy | Carrying per-event causation, arbitrary IDs, payloads, or exact audit history |
| **Trace** | Bounded causal/runtime work graph for sampled or selected operations | Short to medium; sampling-driven | Non-authoritative telemetry; spans may be partial and must report sampling state | Replacing domain lineage, asserting total order, or proving durable acceptance |
| **Health signal** | Typed declaration or projection of liveness, readiness, degradation, recovery, overload, and dependency state | Current state plus transition history | Source state belongs to the emitting authority; aggregation belongs to observability/control/query projections | Guessing readiness from traffic volume or absence of errors |
| **Audit/provenance record** | Immutable evidence of material commands, decisions, actors, configuration, causes, and outcomes | Long enough to satisfy reconstruction policy | Authoritative evidence surface governed by audit/provenance and domain owners | Being sampled, silently dropped, or treated as ordinary telemetry |

### Structured logs

Logs answer “what operational condition occurred here?” They use a versioned event name and typed fields rather than free-form text as the primary contract. A human-readable message is optional and must not be the only carrier of reason, outcome, scope, or identity.

Logs are appropriate for:

- startup and shutdown stages;
- dependency and adapter transitions;
- validation, parsing, quarantine, and retry diagnostics;
- queue saturation and shedding notices;
- health transitions;
- recovery steps;
- unexpected exceptions and invariant failures;
- operator-facing troubleshooting context already safe to expose.

Per-event success logging on the hot path is prohibited by default. Where event-level diagnostics are temporarily enabled, they require an explicit instrumentation profile, bounded rate, expiry, redaction policy, and overhead measurement.

### Metrics

Metrics answer “how much, how often, how long, or what bounded state?” Metric types are:

- **counter:** monotonic count of occurrences;
- **up/down counter:** bounded concurrent work or resource ownership where negative changes are valid;
- **gauge:** current sampled level or state;
- **histogram/distribution:** observed values such as duration, size, age, or depth;
- **state set:** one current state from a bounded enum, represented without free-form labels.

Metric names and dimensions must remain stable enough for longitudinal comparison. A metric is not a compact log record. Domain IDs, error text, symbols with unbounded discovery, URLs, payload fragments, stack traces, and actor identities do not belong in metric dimensions.

### Traces

Traces answer “which runtime operations contributed to this observed path?” They connect process work across declared boundaries, including queue wait and asynchronous handoff. Trace topology may mirror causal work, but trace parentage is not the authoritative domain causation graph.

Every trace or span declares:

- instrumentation schema/version;
- trace and span identity;
- parent or link relationships;
- operation name;
- component/authority and runtime instance;
- start/end latency points or duration validity;
- status and typed outcome;
- sampling decision and sampling policy version;
- relevant bounded scope fields;
- domain causal references when safe and useful.

Fan-in, fan-out, batching, replay, and asynchronous work use span links where a single parent would erase the real relationship. A batch span may summarize many events, but it must not pretend they share one domain cause.

### Health signals

Health signals answer “may this scope continue its declared responsibilities?” They are typed state declarations with evidence, not log severity or HTTP status alone. Health is addressed separately below.

### Audit and provenance

Audit records answer “what material action, decision, configuration, actor, and result formed the retained history?” Audit follows the domain causal chain and minimum stable message contract. It is never sampled. If an audit handoff or retention obligation cannot be met, the owning phase applies its fail-closed, incomplete, or non-faithful policy.

Observability may emit a metric or log about audit-pipeline lag or failure. That telemetry does not make the missing audit evidence acceptable.

## Common telemetry envelope

Every telemetry record preserves a minimum logical envelope directly or through an enclosing batch/resource manifest.

### Required fields

| Field | Meaning |
|---|---|
| `telemetry_schema_version` | Version of this signal class and semantic convention. |
| `signal_class` | `log`, `metric`, `trace`, or `health`; audit uses the domain/audit contract rather than this envelope. |
| `signal_name` | Stable namespaced semantic name. |
| `observed_time` | When the instrumentation observed or emitted the signal, as a named `TimePoint`. |
| `component` | Stable logical component/module name. |
| `authority` | Domain or infrastructure authority being observed. |
| `runtime_instance_id` | One process/runtime incarnation; changes on restart. |
| `deployment_id` | Identity of the local deployment or later declared topology. |
| `environment_class` | Bounded classification such as `development`, `test`, `benchmark`, or `production-like`; it is not a free-form environment name. |
| `instrumentation_profile` | Identity/version of enabled instrumentation, sampling, aggregation, and diagnostic settings. |
| `instrumentation_profile_epoch` | Monotonic run- or runtime-scoped epoch identifying the exact interval during which that profile was effective. |
| `build_version` | Executable/build identity sufficient to correlate with run provenance. |
| `outcome` | Bounded result such as `success`, `rejected`, `invalid`, `unavailable`, `timeout`, `cancelled`, `unknown`, or `error`, when applicable. |
| `quality` | Completeness, sampling, truncation, loss, and uncertainty status relevant to interpretation. |

Fields that do not apply are absent. They are never populated with placeholder identities or invented times.

### Conditional scope fields

The following fields are mandatory when the signal concerns that scope:

- `run_id` and `mode` for run-bound processing;
- `capture_session_id` for capture work reusable across runs;
- `stream_id` and `stream_epoch` for stream-bound work;
- `run_input_sequence` for a specific dispatched input;
- `configuration_epoch` and relevant `VersionRef` identities for behavior-dependent work;
- `canonical_instrument_id` and/or `listing_id` for instrument/listing-scoped work;
- `strategy_instance_id` for strategy work;
- `portfolio_id`, `account_id`, and `risk_scope_id` for portfolio/risk/execution/accounting work;
- bounded `venue_id`, `adapter_id`, or `dependency_id` for adapter/dependency work;
- `replay_class` for replay and backtest runs;
- `workload_manifest_id`, `reference_environment_id`, and `benchmark_run_id` for performance evidence.

### Telemetry profile epochs and transitions

The initial telemetry profile identity, configuration digest, and epoch are recorded in process-start evidence and every applicable run initialization manifest. A profile epoch contains:

- profile identity/version and canonical configuration digest;
- run/runtime scope;
- monotonic `instrumentation_profile_epoch`;
- accepted transition identity and actor/policy cause;
- requested and actual effective boundary;
- start and end evidence positions;
- sampling, aggregation, diagnostic, redaction, and export settings;
- expected resource envelope and comparison class.

A profile change is a typed, authenticated command accepted or rejected by the observability authority. An accepted transition fact exists before the new profile is used, creates a new epoch, and records the exact local processing boundary and, for run-bound work where available, the first affected `run_input_sequence`. The run/configuration evidence references the accepted transition; the profile change does not become an unrecorded alternative source of domain behavior. It never mutates an earlier epoch. Benchmark profiles are immutable for one benchmark run; changing one terminates that comparison interval and requires a new benchmark run or explicitly separate result partition.

Every metric interval, span, log, health observation, evidence artifact, and loss record identifies its profile epoch directly or through an enclosing resource manifest. Metric aggregation intervals cannot cross epochs. Long-running traces that cross an epoch record the transition through a span event/link or are partitioned according to the trace schema.

An undeclared, unrecorded, partially applied, or unverifiable profile change:

- marks affected telemetry completeness as `unknown`;
- invalidates performance and longitudinal comparisons covering that interval;
- prevents the interval from satisfying a phase or regression gate;
- cannot be repaired by relabeling records after collection.

### Correlation and lineage fields

Telemetry correlation supplements, but never replaces, domain causation.

| Field | Contract |
|---|---|
| `trace_id`, `span_id`, `parent_span_id` | Runtime tracing identity only. Optional when tracing is disabled or unsampled. |
| `trace_links` | Links for batching, fan-in, fan-out, queue handoff, or related work that cannot be represented by one parent. |
| `causation_refs` | Typed references to the directly relevant domain commands, events, state cuts, or decisions. Preserve multiple refs where the domain relationship is many-to-one. |
| `correlation_refs` | Optional workflow grouping such as command, incident, recovery, or benchmark correlation. Never substitutes for `causation_refs`. |
| `subject_refs` | Typed domain subjects following the minimum stable message contract. |
| `state_lineage_digest` | Stable digest/reference to a complete `StateLineage` where full vectors are too large for telemetry. The full lineage remains in authoritative facts/evidence. |
| `effective_position` | Required for telemetry about behavior-changing control application. |
| `risk_sequence` | Required for telemetry tied to projected exposure/reservation serialization. |
| `order_intent_id`, `order_id`, `fill_id`, `ledger_transaction_id` | Allowed in logs/traces and exemplars under restricted access; prohibited as general metric dimensions. |

A single global correlation ID is insufficient for Chronos. Instrumentation must preserve typed many-to-many links where the underlying domain path branches or joins.

## Naming and semantic conventions

### Namespaces

Telemetry names use lowercase dot-separated namespaces:

```text
chronos.<area>.<subject>.<measurement_or_event>
```

Examples of semantic shape, not fixed implementation names:

```text
chronos.runtime.process.start
chronos.pipeline.queue.depth
chronos.market.stream.quality
chronos.strategy.evaluation.duration
chronos.risk.decision.outcome
chronos.telemetry.records.dropped
```

The `<area>` uses stable architecture/domain areas such as:

- `runtime`
- `telemetry`
- `pipeline`
- `source`
- `reference`
- `market`
- `feature`
- `strategy`
- `recommendation`
- `portfolio`
- `risk`
- `execution`
- `ledger`
- `reconciliation`
- `run`
- `dataset`
- `experiment`
- `console`
- `security`

Names describe one meaning. Reusing a name after changing units, population, boundaries, clock treatment, or label semantics is prohibited; such a change requires a new semantic version and, where coexistence is necessary, a new name.

### Units and types

- Durations use a declared base unit and are exported consistently; field and schema metadata carry the unit.
- Byte, event, order, fill, quantity, and money measurements are never conflated.
- Money metrics include a bounded currency/asset dimension only where the population is controlled; otherwise they are exposed through query/audit artifacts rather than general metrics.
- Ratios define numerator, denominator, inclusion rules, and zero-denominator behavior.
- Histogram boundaries, aggregation algorithm, and temporality are versioned as part of the metric schema.
- Counts distinguish attempted, accepted, rejected, completed, failed, dropped, retried, and deduplicated populations.
- Current values distinguish observed gauges from authoritative domain state.
- Boolean labels are avoided where a bounded state enum communicates the lifecycle more accurately.

### Metric aggregation and temporality

Every metric data point or enclosing metric batch declares:

| Field | Contract |
|---|---|
| `aggregation_temporality` | `delta`, `cumulative`, or `instantaneous`; the meaning is fixed by the metric schema. |
| `interval_start` | Required named `TimePoint` at which the aggregation population begins. For an instantaneous value it equals `interval_end`. |
| `interval_end` | Required named `TimePoint` at which the delta/cumulative population ends exclusively, or at which an instantaneous value was observed. |
| `aggregation_reset_id` | Identity of the accumulation epoch within producer, metric schema, and dimension-set scope. It changes on process restart, collector state loss, explicit reset, incompatible schema/profile change, or any discontinuity that prevents continuation. |
| `instrumentation_profile_epoch` | Profile epoch under which the complete interval was collected. An interval may not span profile epochs. |
| `population` | Versioned definition of included/excluded observations and sampling treatment. |

Additional rules:

- A cumulative value is meaningful only with its interval start and reset identity.
- A delta value covers exactly its declared half-open interval and cannot be merged across gaps, resets, or incompatible clock domains.
- An instantaneous gauge has equal start/end points and does not imply continuity between samples.
- Histograms include count, sum only where semantically valid, versioned boundaries or aggregation parameters, temporality, interval, reset identity, profile epoch, and sampled/lost population status.
- Histogram sums are omitted when overflow, unit semantics, sampling, or numeric representation makes them misleading.
- Downsampling preserves source intervals, reset boundaries, profile epochs, loss status, and the aggregation function used.
- Collector restart or state loss never continues a prior cumulative series by assumption; it creates a new reset identity.
- Query and export layers must not merge intervals across reset identities or profile epochs without presenting them as distinct partitions.

### Reasons and outcomes

Every rejection, failure, degradation, shed, retry, or health transition uses:

- a stable machine-readable `reason_code`;
- a bounded `outcome`;
- optional safe human-readable context;
- the responsible scope and authority;
- the originating exception/error class when safe;
- retryability and operator-action classification where applicable.

Free-form exception messages are never metric labels. Stack traces belong only in restricted logs or crash artifacts and must pass redaction.

### Severity

Log severity communicates operational urgency, not business desirability:

- `debug`: temporary or profile-gated diagnostic detail;
- `info`: expected lifecycle transition or bounded summary;
- `warn`: degraded but contained condition requiring attention or policy action;
- `error`: operation or scope failed, with no implied process death;
- `fatal`: process/runtime cannot safely continue.

A rejected trade, hold recommendation, strategy abstention, venue order rejection, or risk rejection is not automatically an error. Severity follows whether the software behaved according to contract.

## Cardinality policy

### Principles

Metric dimensions must be finite, reviewed, and justified by an operational question. Every metric schema declares:

- permitted dimensions and their bounded value source;
- estimated worst-case series count under the reference workload;
- aggregation scope;
- aggregation temporality, interval clock, and reset behavior;
- reset-identity and instrumentation-profile-epoch treatment;
- whether dimensions are available in all modes;
- retention and downsampling implications;
- an owner and compatibility policy.

### Permitted metric dimensions

Typical bounded dimensions include:

- component and authority;
- environment class and mode;
- outcome and stable reason-code family;
- replay class;
- queue or boundary name;
- health state and degradation class;
- bounded venue/adapter/dependency identifier;
- strategy definition/class when the configured set is bounded by the run manifest;
- order type or lifecycle state from a bounded enum;
- data-quality state;
- telemetry profile and exporter class.

### Prohibited metric dimensions

The following are prohibited from general metric labels:

- event, command, trace, span, run, capture-session, order, fill, ledger, or audit IDs;
- account, actor, portfolio, or user identity unless an explicit single-tenant bounded operational schema is reviewed and access-controlled;
- arbitrary instrument symbols or externally discovered markets without a bounded registry and series-budget proof;
- raw URLs, topic/channel names, file paths, exception text, stack traces, payload content, or timestamps;
- version-control commit IDs on every time series when a bounded deployment/build resource attribute is sufficient;
- unbounded strategy parameters, external-observation sources, reason text, or model outputs.

High-cardinality identifiers belong in logs, traces, audit records, exemplars, or queryable evidence artifacts. Exemplars link an aggregate observation to a small bounded sample of traces; they do not turn IDs into labels.

### Cardinality enforcement

The implementation must support:

- schema-time allowlists for dimensions;
- runtime limits on unknown label values and series creation;
- counters for rejected, collapsed, or overflowed dimensions;
- an explicit overflow bucket that preserves the aggregate without inventing identity;
- tests that generate worst-case configured dimensions;
- local inspection of current series count and memory cost;
- change review when a new dimension increases the declared series budget.

## Clock domains and timestamp contract

### Clock-domain registry

Every `TimePoint` used by telemetry references a registered clock domain containing:

- `clock_domain_id`;
- clock class: monotonic, UTC wall, source asserted, replay logical, simulated, or synchronized external;
- runtime/process/host scope;
- source and implementation;
- resolution and precision;
- synchronization method where applicable;
- measured or declared uncertainty bound;
- adjustment behavior, including step, slew, reset, wrap, or suspend effects;
- initialization and invalidation time;
- comparability relationships with other domains.

Clock registration is part of process-start evidence and, where relevant, the run manifest. A process restart creates a new monotonic clock-domain identity even on the same machine.

### Timestamp rules

- Canonical named times from Phase 01 remain unchanged; a generic `timestamp` or `latency` field is prohibited.
- Wall-clock values are UTC and include precision, but wall time is not used for elapsed processing duration unless comparability is proven.
- Monotonic time is used for elapsed work inside its valid runtime scope.
- Source times remain source assertions and carry source precision/quality.
- Replay logical time orders domain decisions; it is not host processing time.
- Benchmark processing time uses monotonic clocks and cannot influence replay outcomes.
- Clock steps, suspension, invalid samples, or uncertainty changes produce explicit quality/status signals.
- Missing endpoints produce an incomplete segment, not a zero duration.

### Comparability classes

Every attempted segment is classified:

| Class | Meaning | Permitted output |
|---|---|---|
| **Same-domain** | Both points use the same valid monotonic clock domain | Precise duration bounded by resolution |
| **Bounded cross-domain** | Domains have a measured synchronization relationship and uncertainty bound | Apparent duration plus uncertainty and method |
| **Incomparable** | No valid comparability proof exists | Endpoint times may be retained separately; scalar duration is rejected |
| **Logical-time only** | One or both points are replay logical/simulated domain time | Logical progression may be reported; never processing latency |

Negative or physically impossible results do not get clamped to zero. They invalidate the measurement and increment a typed clock/segment error signal.

## Latency points and segments

### Point contract

Each `LatencyPoint` contains:

- point name and schema version;
- `TimePoint` and clock-domain identity;
- runtime instance and component/authority;
- run/mode and relevant scope;
- causation references or batch relation;
- measurement method;
- resolution, uncertainty, and quality;
- whether the point is on the hot path, control path, external boundary, or asynchronous evidence path.

Instrumentation captures a point at the exact named boundary. Implementations may not substitute “available,” “ready,” “handled,” or another locally convenient point for an accepted, published, recoverability-accepted, or recoverable lifecycle boundary.

### Fact-lifecycle point vocabulary

Latency point names preserve the Phase 01 fact-publication lifecycle:

| Suffix | Exact boundary |
|---|---|
| `<fact>.accepted` | The owning authority has validated and serialized the fact, assigned required identity/order, and accepted it as immutable. |
| `<fact>.published.<consumer>` | The accepted fact has crossed the authority's declared publication boundary and is available to the named consumer. Publication does not imply recoverability. |
| `<fact>.recoverability_accepted` | The phase-owned persistence boundary has acknowledged responsibility under the declared loss policy. |
| `<fact>.recoverable` | The fact has satisfied the phase-owned reconstruction/recovery proof for the measured failure class. |
| `<boundary>.local_received` | Bytes or a local message crossed a named Chronos ingress boundary; no domain fact has yet been accepted. |
| `<boundary>.local_send_committed` | The local adapter completed the declared send handoff to the operating-system, paper, or transport boundary; it does not prove external receipt. |

Lifecycle points are not interchangeable. If acceptance and publication are atomic in one implementation, both logical points are emitted with that declared relationship rather than collapsed into an ambiguous “available” point. Recoverability latency is measured separately from hot-path publication latency unless an authorization fence requires recoverability before publication.

### Baseline processing points

The following point families support the Phase 01 segment vocabulary. Exact encoded names are fixed in the schema registry:

| Segment | Start point | End point | Contract note |
|---|---|---|---|
| `ingress` | `source.ingress.local_received` | `source_event.accepted` | Excludes source network time; acceptance is owned by capture |
| `normalization` | `source_event.accepted` | `normalized_event.published.stream_authority` | Covers capture acceptance through normalized-event availability; source-event publication, queue wait, normalization acceptance, and normalized-event publication are named subsegments |
| `dispatch_wait` | `normalized_or_control_event.published.stream_authority` | `run_input.published.market_state` | Covers availability through selected run-input dispatch; run-input acceptance and publication are named subsegments tied to `run_input_sequence` |
| `state_apply` | `run_input.published.market_state` | `market_state_view.published.feature_authority` | End is publication of the immutable view with complete lineage to the next named pipeline consumer, not merely mutation completion |
| `feature` | `market_state_view.published.feature_authority` | `feature_observation.published.strategy_runtime` | Diagnostic observations use a distinct end-point type |
| `strategy` | `required_feature_set.published.strategy_runtime` | `strategy_evaluation.accepted` | Outcome is signal-emitted or abstained; publication may be measured separately |
| `recommendation` | `strategy_signal.published.recommendation_authority` | `trade_recommendation.accepted` | Not emitted for abstention |
| `portfolio_risk` | `trade_recommendation.published.portfolio_authority` | `risk_decision.accepted` | Portfolio-target and risk-publication subsegments remain explicit |
| `intent_ready` | `risk_decision.published.reservation_authority` | `order_intent.accepted` | ReservationOutcome, reservation, mode requirements, and human approval where applicable are named intermediate points |
| `submit` | `order_intent.published.execution_adapter` | `adapter.local_send_committed` | Requires successful execution-gate recheck; local send does not prove venue receipt |
| `acknowledgement` | `adapter.local_send_committed` | `order_acknowledgement.accepted` | Canonical parent segment from local adapter send commitment through accepted external/paper acknowledgement |
| `acknowledgement_local_round_trip` | `adapter.local_send_committed` | `adapter.ack.local_received` | Refinement of `acknowledgement`; valid elapsed round trip when both points share one local monotonic domain or a bounded local cross-process relation |
| `acknowledgement_ingest` | `adapter.ack.local_received` | `order_acknowledgement.accepted` | Venue-reported timestamps are not endpoints for this local processing segment |
| `fill_ingest` | `adapter.fill.local_received` | `fill.accepted` | Unmatched/reconciliation paths use typed alternative accepted outcomes |
| `accounting` | `fill.accepted` | `accounting_projection.published.query_model` | Covers accepted fill through ledger posting and projection update; fill publication, ledger acceptance/publication, valuation, and projection publication are named subsegments, while recoverability remains separate |
| `recoverability_acceptance` | `<fact>.accepted` | `<fact>.recoverability_accepted` | Persistence/reconstruction responsibility accepted under the fact class's loss policy |
| `recoverability_proof` | `<fact>.recoverability_accepted` | `<fact>.recoverable` | Defined per fact class and tested failure model; never inferred from publication |

Later phases may add subsegments but cannot redefine these endpoints. A subsegment declares whether it partitions or overlaps its parent.

### Local round trip versus venue timestamp breakdown

`acknowledgement_local_round_trip` is a valid elapsed measurement when the local send-commit and local receive points use the same monotonic clock domain. It includes transport and venue processing time as one opaque round trip; it does not partition that duration or prove when the venue received, accepted, or emitted the acknowledgement.

Venue-provided receive, transact, acknowledgement, and execution timestamps remain source assertions in venue/source clock domains. They may support a breakdown only when a measured synchronization relationship provides an uncertainty bound. Otherwise Chronos reports:

- the valid local send-to-local-receipt round trip;
- the venue timestamp values and their declared quality separately;
- no scalar local-to-venue or venue-to-local sub-duration;
- no claim that the round trip can be decomposed into network and venue-processing components.

Paper execution may define same-process or same-host simulated acknowledgement points, but it must identify them as paper-model time rather than venue behavior.

### Queue and asynchronous handoff points

Every asynchronous boundary captures, subject to instrumentation profile:

- enqueue attempted;
- enqueue accepted/rejected/shed;
- dequeue started;
- item expired/stale before work;
- processing completed/failed;
- acknowledgement or persistence handoff where applicable.

Queue wait is measured separately from service time. Batching records batch size, oldest/newest item age, and fan-in links without assigning one event's latency to every item unless measured.

### Kill-switch control points

The safety-critical kill-switch path has mandatory measurement points:

1. `command_received`: authenticated endpoint first receives the command.
2. `command_admitted`: reserved-capacity admission accepts it, or a typed rejection point is emitted.
3. `control_outcome_accepted`: run/configuration authority accepts or rejects the command.
4. `effective_position_assigned`: accepted control event has a concrete effective position.
5. `effective_applied`: stream/run-input processing activates the switch epoch.
6. `execution_boundary_ack.accepted`: each capable risk/reservation/intent/adapter authority accepts its authoritative acknowledgement that the active epoch is observed and new work is rejected for scope.
7. `execution_fence_ack.accepted`: the phase-owned control/execution authority accepts the authoritative aggregate acknowledgement only after all required, current-epoch boundary acknowledgements are present.
8. `execution_fence_ack.published.observability`: the authoritative acknowledgement is published to observability/query projection.
9. `operator_acknowledgement.published.command_caller_read_model`: the typed authoritative command/fence state crosses its declared publication boundary.

Observability does not create steps 6 or 7. It projects their accepted/published facts. Missing, stale, contradictory, or wrong-epoch boundary evidence makes the projected fence state `unknown`; absence of new submissions is not fence evidence. Phase 02 defines a representative synthetic command-to-fence authority harness, measures it under the Phase 02 reference load/environment, and accepts empirical budgets for admission, acceptance, effective application, boundary acknowledgement, aggregate fence acknowledgement, and projection. Portfolio/risk and execution phases replace synthetic authorities with real owners, adopt or tighten the budgets, and define breach behavior; they may not defer the first empirical budget setting.

The contract reports each same-clock or bounded cross-clock segment independently. It does not claim `command_received` to `execution_fence_ack.accepted` is precise when the points cross incomparable domains.

### Aggregation rules

- End-to-end measurements are computed from matched endpoints on the same causal instance or a declared bounded relationship.
- Percentiles from separate segment histograms are never added to claim an end-to-end percentile.
- Parallel spans are represented as a graph; critical-path analysis uses instance-level evidence, not aggregate arithmetic.
- Missing or sampled child spans do not imply zero work.
- Retry, queue, approval-wait, reconciliation, and external-wait time remain distinguishable.
- Source timestamp to local receive and venue timestamp breakdowns are `apparent_external` unless uncertainty is bounded; same-local-clock send-to-local-receipt round trips remain valid opaque elapsed measurements.
- Histograms record population, clock comparability class, aggregation temporality, interval start/end, reset identity, and instrumentation profile epoch.

## Alert contract

An alert is a non-authoritative operator-notification projection derived from typed health, metric, trace, or authoritative safety facts. It never creates or substitutes for a domain command, kill-switch state, risk decision, execution fence, or acknowledgement.

Every alert definition and instance records:

- versioned alert definition, owner, severity, affected scope, and source facts;
- trigger condition, evaluation window, missing-data behavior, and clear condition;
- deduplication/fingerprint key and grouping policy;
- first observed, last observed, fired, acknowledged, cleared, and expired times;
- current state: `pending`, `firing`, `acknowledged`, `cleared`, `expired`, or `unknown`;
- source-position, freshness, completeness, instrumentation profile epoch, and clock comparability;
- operator acknowledgement identity and note where present;
- routing class and notification outcome without embedding credentials or destination secrets.

Rules:

- missing or stale required evidence yields `unknown`, never an inferred healthy or cleared state;
- deduplication cannot merge different safety scopes, runs, epochs, or root causes;
- acknowledgement records operator awareness only and does not clear the source condition;
- clearing requires the declared clear condition over authoritative or sufficiently complete projected evidence;
- notification delivery failure is observable and does not alter alert truth;
- replay may evaluate alerts deterministically from recorded inputs, but replay alerts are mode-labelled and cannot page as live incidents by default;
- alert evaluation, routing, and notification are bounded and cannot block the hot path.

Phase 02 defines and conformance-tests this schema. Phase 09 owns console interaction, and Phase 10 activates operational routing and runbooks.

## Phased metric and trace schemas

Phase 02 establishes schema families and conformance rules. A later phase activates a family when it introduces the relevant authority. An unimplemented family is not falsely emitted with zeros.

### Phase 02: observability foundation

Required schemas:

- telemetry records attempted, accepted, batched, exported, dropped, rejected, and redacted;
- exporter/collector queue depth, capacity, oldest-item age, flush duration, failure, retry, and shutdown drain outcome;
- instrumentation profile, sampling decisions, series count, cardinality overflow, and schema rejection;
- profile-epoch transition, incomplete/undeclared transition, comparison-validity, metric temporality, interval, and aggregation-reset signals;
- process start/stop, runtime instance, build/configuration identity, clock registration, resource usage, and uncaught failure;
- health-state transitions and health-projection age/completeness;
- latency-point validity, incomplete segment, incomparable clock, and uncertainty distributions;
- instrumentation CPU, memory, allocation, queue, storage, and latency overhead.

### Phase 03: event and persistence infrastructure

Activate:

- capture, normalization, sequencing, control-event, publication, persistence-handoff, snapshot, replay-input, and recovery rates/outcomes;
- per-boundary bounded queue depth, capacity, age, wait, service time, overflow, stale-work, retry, quarantine, and drain;
- computed/accepted/published/recoverability-accepted/recoverable transition lag where meaningful;
- journal/snapshot/checkpoint progress, tail distance, recovery duration, integrity failure, and fidelity/incomplete state;
- dispatch progression and control effective-position lag;
- trace links across capture, normalization, persistence, dispatch, and recovery.

### Phase 04: market-data pipeline

Activate:

- source connectivity/session/epoch transitions;
- message and byte rates, framing/decode outcomes, duplicate/gap/out-of-order/late/quarantine counts;
- book/trade/reference/control stream cursor progression;
- snapshot/delta synchronization state and recovery duration;
- market-state freshness, state-lineage completeness, state publication rate, and checksum/invariant failures;
- data-quality states `valid`, `stale`, `gapped`, `recovering`, `invalid`, and `unavailable`;
- instrument/listing scope only under the approved bounded-cardinality policy.

### Phase 05: deterministic strategy runtime

Activate:

- feature computation attempted/accepted/diagnostic/invalid/unavailable;
- feature window age and required-input readiness;
- StrategyEvaluation outcomes `signal_emitted` and `abstained`, with stable abstention reasons;
- signal and actionable/hold recommendation counts and lifecycle outcomes;
- strategy deadline misses, resource-budget breaches, sandbox/plugin failures, and disabled state;
- feature, strategy, and recommendation segment distributions;
- explanation/provenance completeness checks without exporting explanation content as metric labels.

### Phase 06: research and backtesting

Activate:

- dataset validation and replay-class outcomes;
- replay throughput, processing-time factor, logical-time progress, deterministic checksum match, and repeated-run variance;
- experiment lifecycle and result completeness;
- look-ahead, leakage, comparison-validity, and manifest-validation failures;
- workload/reference-environment identity on benchmark artifacts rather than high-cardinality time-series labels;
- trace/metric profile explicitly separated from production-like hot-path profiles.

### Phase 07: portfolio, risk, and control

Activate:

- target construction and aggregation outcomes;
- RiskDecision approved/modified/rejected/unavailable outcomes and stable rule families;
- projected-exposure/risk-sequence progression;
- reservation request accepted/rejected/stale outcomes, active reservation level, expiry/release/consumption;
- risk freshness and health-gate failures;
- kill-switch admission through authoritative boundary and aggregate execution-fence acknowledgement publication, including unknown/incomplete/wrong-epoch projection and breach status;
- control-path queue reservation, saturation, and deadline evidence;
- no metric implies authorization merely because evaluation succeeded.

### Phase 08: paper execution and accounting

Activate:

- executable paper-intent lifecycle;
- paper order submit/ack/reject/cancel/partial-fill/fill/unknown outcomes;
- fill-model latency/cost/slippage assumptions by bounded model version;
- fill deduplication/correction and unmatched/reconciliation outcomes;
- ledger posting success/idempotent replay/balance invariant and projection lag;
- position/P&L projection freshness and mark quality without making telemetry authoritative;
- reservation transfer/release and accounting recovery signals.

### Phase 09: operations console

Activate:

- query/read-model freshness, source position, completeness, subscription lag, and rebuild state;
- command request/admission/outcome/acknowledgement latency and typed failures;
- console/API availability, authentication/authorization outcomes, and rate/resource protection;
- health view staleness and missing-source indicators;
- no strategy, risk, execution, or accounting meaning derived solely from UI telemetry.

### Phase 10: live paper operation

Activate:

- sustained live capture/strategy/paper/accounting capacity and deadline compliance;
- reconnect/recovery/soak/fault-injection outcomes;
- local resource saturation, process restart, checkpoint recovery, and run fidelity;
- alert/detection delay, operator acknowledgement, and runbook exercise evidence;
- empirically adopted budgets and regression comparisons for the production-like workload.

### Phase 10B and Phase 11 extensions

When introduced, activate:

- per-venue and cross-venue synchronization quality under bounded venue cardinality;
- L3 depth/order-event resource and quality measurements without changing L2 meanings;
- external-observation freshness, quality, correction, isolation, and contribution completeness;
- opportunity/resolution/ranking pipeline outcomes;
- live credential/access boundary health without secret values;
- human approval wait/expiry/revocation and execution-fence paths;
- adapter submission, acknowledgement, fill, unknown-order, retry suppression, and reconciliation;
- execution-group residual-exposure and partial-completion status;
- external/source apparent-latency measurements with explicit uncertainty.

## Health, readiness, and degradation

### State model

Each health scope exposes one typed state:

| State | Meaning |
|---|---|
| `starting` | Initialization or validation is in progress; no readiness claim. |
| `healthy` | The scope meets all declared obligations for the requested capability and mode. |
| `degraded` | The scope can continue a declared subset, with explicit impairment and containment. |
| `recovering` | Recovery is active from a known evidence position; readiness depends on recovery policy. |
| `unready` | Process may be alive, but the scope must not accept the requested responsibility. |
| `failed` | The scope cannot continue safely without restart, recovery, reconfiguration, or operator action. |
| `stopping` | Supervised shutdown/drain is in progress. |
| `unknown` | Evidence is missing, stale, contradictory, or the projection cannot be trusted. |

`unknown` is never treated as `healthy`. Health state includes:

- scope and capability;
- state and stable reason codes;
- observed time, age, and expiry;
- source authority and evidence references;
- affected and unaffected responsibilities;
- dependencies and their states;
- configuration/build/runtime identity;
- recovery progress and operator action where applicable.

An execution-fence projection is a specialized health/query projection over authoritative, current-epoch fence acknowledgements. It may show `execution_fenced` only when the phase-owned aggregate `execution_fence_ack.accepted` fact is present, current, and published to the projection. If expected boundary acknowledgements or the aggregate fact are absent, stale, contradictory, incomplete, or lost, the projection is `unknown`. Observability never substitutes traffic silence, metric absence, process liveness, or a control-event timestamp for that authoritative evidence.

### Liveness

Liveness answers whether the runtime can make progress or respond to supervision. It is intentionally weak. A live process may still be unready, stale, gapped, unsafe, or unable to execute.

Liveness probes must not:

- perform expensive domain queries;
- block on remote exporters;
- imply market, risk, execution, or accounting readiness;
- reset failure counters or mutate domain state;
- declare success solely because an HTTP endpoint responds.

### Readiness

Readiness is capability- and mode-specific. Examples include:

- `capture_ready`
- `replay_ready`
- `strategy_ready`
- `paper_execution_ready`
- `live_read_only_ready`
- `human_approved_live_ready`
- `query_ready`

A runtime may be ready for capture while unready for strategy or execution. Readiness is derived from phase-owned prerequisites such as:

- validated configuration and secret references;
- registered valid clocks;
- required dependency and adapter state;
- proven stream continuity and freshness;
- recovered authority state and complete tail;
- active run/configuration epoch;
- risk/reservation/execution reconciliation;
- telemetry conditions that are declared safety-critical;
- audit/recoverability fences where required.

The observability authority hosts the projection contract; the functional phase owns its readiness prerequisites and safe-state behavior.

### Degradation

Every degradation declares:

- reason and first-observed time;
- affected scope and capability;
- remaining permitted work;
- prohibited work and fail-closed boundary;
- whether fidelity, completeness, latency validity, or evidence retention is affected;
- escalation and recovery criteria.

Examples:

- telemetry exporter failure may allow domain work while local buffers and loss accounting remain within policy;
- loss of optional trace spans may degrade diagnostic completeness but not domain fidelity;
- stale market data makes trade-capable strategy evaluation abstain and prevents downstream authorization;
- audit or authorization-recoverability failure may make execution unready even if metrics remain healthy;
- accounting discrepancy may freeze affected execution while capture and monitoring continue.

### Aggregation

Health aggregation follows dependency and capability graphs, not a universal “worst color wins” rule. Aggregation must preserve:

- the failing source scopes;
- containment boundaries;
- capability-specific consequences;
- stale or missing evidence;
- projection age and completeness.

An aggregate cannot be healthier than a mandatory dependency, but an isolated optional dependency must not make unrelated capabilities falsely unavailable.

## Telemetry pipeline, loss, and backpressure

### Priority classes

Telemetry is assigned one of:

1. **Safety-operational:** bounded health and loss indicators required to determine readiness or containment.
2. **Baseline operational:** core rates, errors, queue pressure, and latency distributions required for normal operation.
3. **Diagnostic:** traces, debug logs, profiles, and event-level detail enabled by sampling or incident profile.
4. **Export copy:** asynchronous replication to optional external destinations.

Audit/provenance is not a telemetry priority class. It follows its authoritative persistence and failure contract.

### Hot-path rules

- Instrumentation performs bounded in-memory work only.
- Remote I/O, DNS, authentication, exporter retries, compression requiring unbounded work, and telemetry-store queries are prohibited on the hot path.
- Telemetry enqueue uses a declared bounded queue or lock-free/bounded handoff whose contention and failure behavior are measured.
- Optional telemetry failure cannot change a domain result, ordering decision, random draw, timeout, or effective position.
- Safety-operational point capture may be mandatory, but its implementation must still be bounded and its failure converted into explicit health/readiness state according to the owning phase.
- Allocation, formatting, stack capture, and payload serialization on the hot path are profile-controlled and benchmarked.

### Overflow behavior

Each telemetry buffer declares:

- capacity and derivation method;
- priority classes admitted;
- partitioning and ordering requirements;
- overflow and shedding policy;
- whether sampling occurs before or after enqueue;
- oldest-item and staleness policy;
- shutdown drain deadline;
- local fallback and export retry policy.

Allowed responses include aggregation, sampling, coalescing of equivalent health transitions, dropping lower-priority diagnostic records, or stopping optional export. Silent overwrite is prohibited.

Every loss or suppression interval records, through a separately protected mechanism where possible:

- signal class and schema family;
- count or bounded estimate;
- first/last observed times;
- affected component/runtime/run scope;
- reason: overflow, sampling, rate limit, schema rejection, redaction rejection, storage full, exporter failure, or shutdown deadline;
- whether safety-operational visibility or evidence completeness was affected.

If even the protected loss counter cannot be recorded, health becomes `unknown` or `degraded`; the system must not claim complete telemetry.

### Backpressure interaction

Telemetry backpressure must not propagate into market, control, risk, execution, fill, or accounting queues. The telemetry pipeline may reduce fidelity of optional signals before it consumes resources reserved for domain safety and capture.

If telemetry resource use breaches its declared envelope:

- diagnostic collection is reduced first;
- optional export is paused;
- local retention may shorten according to policy;
- baseline and safety-operational signals retain reserved capacity;
- readiness changes only when the phase-specific contract says observability loss prevents safe operation;
- the breach and resulting profile change are visible and attributable.

## Redaction and security

### Data classification

Telemetry fields are classified:

- **public operational:** safe component names, bounded states, schema versions;
- **internal operational:** local paths after sanitization, build/config identities, detailed reason codes;
- **sensitive metadata:** account, portfolio, actor, order, fill, external market, network, or strategy identities;
- **secret:** credentials, tokens, keys, passwords, signed requests, cookies, raw authorization headers, private endpoint material;
- **payload-sensitive:** raw source payloads, order requests, user-supplied text, stack locals, dataset rows.

Secret values are prohibited in all telemetry, audit, manifests, error messages, crash dumps, and test fixtures. Sensitive metadata is permitted only where operationally necessary, access-controlled, and excluded from general metric dimensions.

### Redaction contract

- Redaction occurs before a record enters shared queues, storage, or exporters.
- Field allowlists are preferred over denylist-only redaction.
- Unknown structured fields default to reject, remove, or quarantine according to schema policy.
- Hashing is not automatically anonymization; stable hashes of secrets or identities remain sensitive.
- Raw payload logging is disabled by default and requires an explicit bounded diagnostic contract that proves no secret or prohibited data can enter.
- Exceptions and stack traces are scrubbed before persistence/export.
- Query parameters, headers, local usernames, home paths, and environment-variable values receive explicit handling.
- Redaction failure increments a protected counter and rejects the unsafe record; it never exports the original.

### Access and integrity

- Local telemetry listeners bind according to the approved local exposure policy, defaulting to loopback or local IPC.
- Write and query interfaces are distinct and independently authorized.
- Producer identity is authenticated or bound to a trusted local runtime capability; a producer cannot claim another authority, component, runtime instance, deployment, or profile epoch.
- Write authorization is limited to registered signal schemas and producer scopes. A telemetry writer receives no query, profile-control, or export authority by implication.
- Query authorization is limited by data classification and scope. Read access to aggregate metrics does not imply access to sensitive logs, traces, exemplars, or evidence bundles.
- Evidence artifacts record checksums and manifests sufficient to detect truncation or substitution where used for phase approval.
- Export destinations use explicit destination and data-class allowlists, authentication references, encryption where applicable, and bounded egress policy. Redirects or destination substitution cannot bypass the allowlist.
- Diagnostic profile activation is separately authorized, time-bounded, attributable, audited, and accepted as a profile-epoch transition before it changes runtime behavior or data exposure.
- Telemetry data is never used as an implicit command channel.

## Retention, storage, and export

### Retention classes

Retention policy is defined by signal class and purpose, not one global duration:

- safety-operational transition history;
- baseline metrics and aggregates;
- detailed logs;
- sampled traces;
- profiles and crash artifacts;
- benchmark and phase-gate evidence;
- telemetry-loss manifests;
- exported copies.

Exact periods are deferred. Each implemented policy must define:

- local capacity budget and eviction order;
- raw versus aggregated retention;
- downsampling and aggregation semantics;
- run/incident pinning;
- deletion and compaction behavior;
- evidence manifests and checksums;
- privacy/security constraints;
- behavior when storage is unavailable or full.

Eviction of optional telemetry cannot erase audit/provenance. A phase-gate artifact referenced by an approved result is immutable or content-addressed under the evidence policy; ordinary rolling telemetry may expire.

### Export

Export is asynchronous and optional for initial local operation. An export bundle includes:

- schema registry/version references;
- initial instrumentation profile plus complete profile-epoch and transition history;
- runtime/build/configuration identities;
- clock-domain registry and uncertainty;
- run/workload/reference-environment references where applicable;
- sampling/loss/redaction status partitioned by profile epoch and aggregation reset identity;
- selected metrics, logs, traces, health transitions, and profiles;
- content checksums and creation metadata.

Export failure does not delete the local copy. Retry is bounded and cannot starve local collection or domain work. Re-export is idempotent by artifact identity.

## Local-first topology

The initial logical topology is:

```text
domain/runtime modules
       |
       | bounded instrumentation API
       v
per-process in-memory buffers/aggregators
       |
       v
local collector and schema/redaction validation
       |
       +--> local metric/log/trace/profile storage
       |
       +--> health/query projection for operations API
       |
       +--> evidence bundle writer
       |
       +--> optional asynchronous external exporter
```

This diagram is logical, not a mandate for separate processes. The first implementation may embed collection and storage if it preserves:

- bounded hot-path handoff;
- failure isolation from domain authorities;
- schema and redaction enforcement;
- independent telemetry health;
- restart identity and clock-domain registration;
- local inspection without a cloud dependency;
- replaceable storage/export ports.

If the engine and control plane use separate runtimes, telemetry crosses that boundary through a versioned bounded contract. Cross-process traces use links and clock uncertainty rather than assuming shared monotonic time. A collector crash cannot crash the engine; an engine crash leaves enough local evidence to identify the runtime incarnation and declared loss interval where possible.

## Deterministic replay and observability

### Semantic separation

Replay domain outcomes must remain semantically identical regardless of optional telemetry profile. Logs, metric samples, trace IDs, host processing timestamps, and profile artifacts are excluded from domain semantic checksums.

Instrumentation must not:

- read host wall time inside replay-domain decisions;
- introduce race-dependent ordering;
- consume strategy randomness;
- mutate configuration epochs;
- change batching/dispatch policy unless the benchmark explicitly declares a different workload;
- make hidden network or storage reads;
- alter exception handling or retry semantics.

### Replay observability modes

At minimum:

- **semantic-minimal:** mandatory health, loss, and correctness evidence with minimum performance interference;
- **baseline:** standard operational metrics and bounded tracing used for comparison;
- **diagnostic:** increased trace/log/profile detail for investigation;
- **benchmark:** fixed profile used for reference performance measurement.

Profiles and every profile-epoch transition are versioned run evidence. The initialization manifest identifies the initial epoch; the terminal attestation/evidence bundle identifies the complete epoch history and any incomplete transition. Comparisons across runs or intervals require compatible profile identities, configurations, epoch boundaries, and declared transitions, or must explicitly treat them as experimental variables.

An undeclared profile change invalidates the affected comparison interval even when domain semantic checksums still match. Post-hoc discovery of a profile change may classify the domain run as semantically valid, but it cannot restore performance-comparison validity.

### Replay time

Telemetry distinguishes:

- replay logical/event-time progression;
- host monotonic processing duration;
- processing throughput or replay-speed factor;
- pauses caused by pacing, debugging, operator control, or resource contention.

A corrected event-time replay and faithful capture-order replay may have different logical progression but use the same processing measurement contract. Neither may be labeled “live latency.”

### Determinism tests

Given the same run manifest and domain inputs:

- domain semantic checksums match across allowed telemetry profiles;
- accepted domain fact order and causal references match;
- mandatory point names and segment validity classifications are stable;
- telemetry record counts may differ only where sampling/profile policy permits;
- every profile transition produces a new declared epoch and partitions metric intervals/comparisons;
- an undeclared transition causes the comparison-validity check to fail;
- trace/log identities may differ and are never part of domain identity;
- instrumentation failure injection does not change domain outcomes.

## Testing and overhead measurement

### Contract tests

The observability implementation must provide:

- schema validation and compatibility tests for each signal class;
- naming, unit, type, required-field, aggregation-temporality, interval, reset-identity, profile-epoch, and enum tests;
- cardinality allowlist and worst-case series tests;
- unknown field/type/version behavior;
- batch reconstruction and signal-envelope round trips;
- redaction and secret-canary tests;
- negative producer-identity tests proving one producer cannot claim another authority/component/runtime/profile epoch;
- negative write-authorization tests proving a writer cannot publish unregistered schemas or exceed its allowed producer scope;
- negative query-authorization tests proving aggregate access cannot read restricted logs, traces, exemplars, or evidence;
- negative profile-activation tests proving unauthenticated, unauthorized, expired, replayed, or undeclared transitions are rejected;
- negative export-allowlist tests covering disallowed destinations, redirects, data classes, and credential references;
- health aggregation and stale-evidence tests;
- clock-domain and latency-segment validity tests;
- telemetry-loss and overflow accounting tests;
- exporter idempotency and evidence-manifest tests.

### Clock and latency fixtures

The Phase 01 `DM-E13` fixture is implemented here and covers:

- same-monotonic-domain duration;
- bounded cross-domain duration with retained uncertainty;
- incomparable clocks rejecting scalar duration;
- clock reset/step/suspend and invalid sample;
- missing endpoint and incomplete segment;
- parallel branches and fan-in;
- queue wait separated from service time;
- sampled/missing child spans;
- replay logical time separated from processing time;
- valid same-local-monotonic-clock send-to-local-receipt round trip;
- venue timestamp values retained separately while incomparable local-to-venue/venue-to-local breakdowns are rejected;
- bounded external-clock breakdown retaining uncertainty where synchronization is proven;
- proof that segment percentiles are not summed as end-to-end percentiles.

### Failure and pressure tests

Fault injection covers:

- collector unavailable or restarting;
- exporter timeout, authentication failure, and destination rejection;
- local telemetry storage full or read-only;
- buffer saturation and diagnostic shedding;
- schema/cardinality/redaction rejection storms;
- corrupted batch or evidence artifact;
- process crash before flush;
- clock invalidation;
- health source stale or contradictory;
- inability to record protected loss counters.

Tests verify that domain work follows its own safe policy, optional telemetry cannot backpressure authoritative queues, and visibility degrades explicitly.

### Overhead methodology

No numeric overhead target is invented in planning. The implementation establishes budgets through the Phase 01 performance-budget process:

1. Define versioned correctness workloads for each activated phase, including normal, burst, and overload shapes.
2. Record a reference environment, runtime configuration, build profile, power settings, and competing load.
3. Run a correctness-preserving minimally instrumented baseline.
4. Run each candidate instrumentation profile on the same workload.
5. Measure distributions and confidence/variance for:
   - end-to-end and segment latency;
   - throughput and deadline misses;
   - CPU time and scheduling;
   - allocations and resident memory;
   - queue depth, contention, and drops;
   - disk/network bytes and storage growth;
   - shutdown drain and recovery time.
6. Verify domain semantic checksums are unchanged.
7. Attribute overhead by signal class and instrumentation point through profiles or controlled removal.
8. Establish proposed budgets only after the workload baseline and product validity deadlines are known.
9. Propose the Phase 02 baseline observability profile, its resource envelope, regression thresholds, and exception policy from those measurements.
10. Review and accept or reject that complete profile as a Phase 02 gate; recording measurements without an accepted operating profile is insufficient.

Results report absolute measurements and relative change with statistical context. One fast sample or an idle workload is insufficient. Shared noisy CI may detect gross regressions but cannot be the sole approval environment.

### Empirical profile acceptance contract

Phase 02 implementation must finish with at least one accepted **baseline observability profile** for the Phase 02 reference workload and environment. The accepted artifact fixes measured numeric values; this planning document fixes how those values are derived and governed.

The profile contains:

- profile identity/version, canonical configuration, and permitted transition classes;
- reference workload and environment manifests;
- enabled signal schemas, sampling, aggregation, histogram, redaction, retention, and export settings;
- accepted resource envelope covering CPU, scheduling, allocations, resident memory, telemetry queue capacity/use, series count, local write rate, storage growth, export bandwidth, shutdown drain, and recovery behavior;
- accepted latency/throughput overhead distributions and deadline impact relative to the correctness-preserving baseline;
- telemetry loss and cardinality limits;
- regression thresholds, comparison method, sample requirements, confidence/variance treatment, and controlled reference environment;
- owner, approvers, review date, and evidence identities.

Regression thresholds are empirical and metric-specific. They must state whether comparison is absolute, relative, distributional, or model-based; which direction is worse; how noise is handled; and what constitutes inconclusive evidence. A threshold cannot be selected merely to make the current implementation pass.

The exception policy requires:

- named owner and approver independent from the measurement producer where practical;
- affected profile, workload, environment, metrics, and scope;
- evidence-backed rationale and risk;
- explicit temporary replacement threshold or containment;
- issue/reference, review date, and non-renewing expiry;
- rollback or remediation plan;
- proof that domain correctness, safety, audit, redaction, and bounded-backpressure invariants remain satisfied.

Expired, unowned, repeatedly renewed without new evidence, or safety-weakening exceptions fail the gate. A workload, environment, profile, or profile-epoch change invalidates direct comparison unless compatibility is proven or the change is treated as an experimental variable.

## Implementation evidence and exit gates

Planning approval fixes the contracts below. The artifacts become mandatory when implementing Phase 02. Later phase schemas become mandatory only when those capabilities exist and are rerun cumulatively.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **OT-E01 — Telemetry semantic registry** | Versioned log, metric, trace, health, point, and segment schemas; names, types, units, temporality, interval, reset identity, profile epoch, required fields, outcomes, owners, compatibility rules | Schemas validate; metric/histogram intervals cannot cross reset/profile epochs; no signal class conflates telemetry with audit or authoritative domain state |
| **OT-E02 — Common-envelope and profile-epoch conformance suite** | Valid/invalid fixtures for required and conditional fields, causal refs, scopes, batches, unknown versions, initial profiles, declared transitions, epoch boundaries, and quality | Every record is attributable to component/runtime/profile epoch; undeclared or incomplete transitions invalidate comparison; missing applicable run/scope/version/epoch fields fail |
| **OT-E03 — Cardinality model** | Dimension allowlists, prohibited fields, series-count estimator, worst-case configured fixture, overflow behavior | No unbounded IDs/text enter metric labels; estimator and runtime enforcement expose bounded series cost |
| **OT-E04 — Clock-domain registry and DM-E13 fixture** | Registered clock classes; same-local-clock round trip; bounded cross-clock uncertainty; incomparable venue timestamp breakdown; logical-time and parallel-path fixtures | Precise duration only for valid comparable clocks; local send-to-local-receipt is valid and opaque; unsupported venue sub-durations are rejected; uncertainty retained; percentiles not summed |
| **OT-E05 — Lifecycle-exact latency-point implementation** | Phase-appropriate accepted, published-to-consumer, recoverability-accepted, recoverable, local-receive, and local-send-commit points; endpoint placement, queue/service separation, and causal matching tests | No ambiguous available/ready/posted endpoint remains; missing/incompatible endpoints become incomplete/invalid rather than zero or fabricated latency; recoverability is never inferred from publication |
| **OT-E06 — Kill-switch measurement/projection fixture** | Synthetic authoritative command, boundary-acknowledgement, aggregate execution-fence-acknowledgement, publication, and projection facts, including missing/stale/wrong-epoch cases | Observability only projects accepted authoritative acknowledgements; absent or invalid evidence yields `unknown`; measurement works under representative control backlog without unsupported cross-clock precision |
| **OT-E07 — Health model conformance** | Liveness/readiness/degradation/recovery schemas, capability dependency fixtures, stale/unknown evidence, and authoritative execution-fence projection cases | `unknown` never becomes healthy or fenced; capture, query, strategy, paper, and future live readiness remain capability-specific |
| **OT-E08 — Loss and backpressure fault suite** | Bounded buffers, priority classes, overflow, sampling, exporter/storage failure, protected loss accounting, shutdown drain | Optional telemetry cannot block/change domain outcomes; all tested loss is visible; inability to account for loss changes health to unknown/degraded |
| **OT-E09 — Security, authorization, and redaction suite** | Field classification, secret canaries, exception/stack/payload tests, producer identity, write/query authorization, profile activation, destination/data-class export allowlists, redirect handling, and local listener policy | Zero secret canaries escape; forged producer identity, unauthorized write/query/profile activation, and disallowed/redirected export attempts fail; unsafe records are rejected before shared queues/storage/export and counted safely |
| **OT-E10 — Local-first collection proof** | One-machine topology, process isolation behavior, local query, restart identity, local storage/export bundle, no remote dependency | System operates and can be investigated locally; collector/export failure does not crash or stall domain runtime |
| **OT-E11 — Replay non-interference and profile-transition suite** | Same replay under semantic-minimal, baseline, diagnostic, benchmark, telemetry-failure, declared-transition, and undeclared-transition profiles | Domain facts/order/checksums are identical; declared epochs partition telemetry; undeclared changes invalidate comparisons; logical time is never reported as processing latency |
| **OT-E12 — Accepted empirical observability profile** | Versioned workload/reference-environment manifests, repeated baseline/profile results, accepted numeric resource envelope, overhead distributions, regression thresholds/method, loss/cardinality limits, approvers, and exception policy | One Phase 02 baseline profile is formally accepted from reproducible evidence; thresholds are defensible; no unresolved or expired exception exists; recording measurements without acceptance fails |
| **OT-E13 — Retention/export evidence** | Retention classes, capacity/eviction policy, evidence pinning, loss manifests, complete profile-epoch/reset history, export authorization, bundle checksums, and idempotency | Optional expiry cannot erase audit; full/storage/export failures are explicit; profile/reset partitions survive export; approved evidence bundles are reproducible |
| **OT-E14 — Phase schema activation matrix** | Every schema family mapped to introducing phase, authority, dimensions, points, health dependencies, and cumulative test trigger | No later capability is falsely emitted in Phase 02 or left without an activation owner |
| **OT-E15 — Independent critique and cumulative compatibility review** | Critique disposition and comparison against Phase 01 plus all Phase 02 leaves | Zero unresolved material findings; no authority, lifecycle, clock, mode, causal, or recoverability contract is weakened |

### Phase 02 exit conditions

Phase 02 implementation is complete only when:

- the common contracts and registries exist independently of any vendor exporter;
- mandatory Phase 02 telemetry can be collected and inspected locally;
- clocks and latency pass `DM-E13`;
- cardinality, redaction, loss, and health behavior fail visibly and safely;
- replay outcomes are invariant across approved instrumentation profiles;
- profile epochs/transitions are complete run evidence and undeclared changes invalidate comparisons;
- Phase 02 has formally accepted an empirically derived baseline observability profile, numeric resource envelope, regression thresholds, and exception policy for its versioned reference workload/environment;
- kill-switch measurement semantics are proven synthetically without claiming the later risk/execution behavior is implemented, and observability projects rather than creates authoritative execution-fence acknowledgements;
- later metric families are registered as activation obligations, not emitted as misleading zeros;
- all evidence artifacts are reproducible and machine-readable;
- independent critique and cumulative Phase 01 compatibility review have no unresolved material finding.

Phase 02 does not fail because market feeds, strategies, risk, paper execution, the console, or live execution are not implemented. It fails if the observability foundation cannot instrument those future paths without semantic redefinition, unsafe cardinality, clock ambiguity, unbounded overhead, or telemetry/audit conflation.

## Deferred product and technology choices

The following remain deferred to implementation planning, later phases, or evidence-driven architecture decisions:

1. Telemetry SDK, protocol, collector, embedded agent, and schema-generation mechanism.
2. Local metric, log, trace, profile, and evidence storage products/formats.
3. Whether collection is embedded, sidecar-like, or a separate local process.
4. Exact histogram boundaries, sampling rates, queue capacities, flush cadence, batch sizes, and compression.
5. Numeric objectives for later functional workloads and environments. Phase 02 must derive and accept its own baseline observability profile, resource envelope, regression thresholds, and exception policy during implementation rather than deferring them further.
6. Reference hardware and supported benchmark environment.
7. Dashboard layout, notification products/destinations, and operations-console presentation; the alert schema and lifecycle are fixed in this phase.
8. External export destinations and hosted observability support.
9. Exact policy for instrument-, strategy-, venue-, account-, and portfolio-scoped metric dimensions after real cardinality is measured.
10. Profilers, crash-dump mechanism, and production diagnostic activation workflow.
11. Concrete audit storage and retention technology, which remains separate from optional telemetry.
12. Multi-host trace propagation and clock synchronization until a multi-host topology is justified.
13. Venue/source-specific timestamp quality and external-latency interpretation.
14. Phase-specific readiness prerequisites, validity deadlines, and breach actions owned by later functional phases.
15. Regulatory, legal, or contractual record-retention requirements if Chronos later handles live capital.

A deferred choice may not introduce a local convention that contradicts this contract. Until selected, implementation remains behind versioned ports, profiles, schemas, and evidence manifests.

## Cumulative compatibility obligations

At every later phase gate:

1. Activate only the schema families for capabilities introduced by that phase.
2. Re-run Phase 02 contract, cardinality, clock, redaction, loss, replay, and overhead tests for affected paths.
3. Add the phase's reference workloads and empirically accept or refine the applicable observability profile, resource envelope, regression thresholds, and exception policy.
4. Confirm new health prerequisites remain capability-specific and preserve containment.
5. Confirm new logs/traces retain typed domain causal references without becoming the authoritative causal graph.
6. Confirm new identifiers do not create unbounded metric dimensions.
7. Confirm new clocks or processes have explicit domains and comparability policy.
8. Confirm telemetry pressure cannot consume resources reserved for safety, control, capture, execution facts, or accounting.
9. Confirm retention/export changes do not weaken audit reconstruction or evidence reproducibility.
10. Confirm metric intervals retain temporality, start/end, reset identity, and profile epoch through collection, query, downsampling, and export.
11. Treat any change to an established signal meaning, latency endpoint, authority, readiness rule, profile policy, or loss policy as a compatibility decision requiring cumulative review.

Approval of this document fixes the observability semantic baseline. Later phases may add measurements and stricter requirements, but they may not redefine established names or use telemetry as a substitute for domain authority, auditability, clock correctness, or safe failure behavior.
