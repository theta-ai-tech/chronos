# Phase 05: Feature Runtime

## Purpose

This document defines the planning contract for Chronos's deterministic feature runtime. It covers feature identity and versioning, exact market-state cuts, declared history windows, valid/diagnostic/unavailable outcomes, no-look-ahead enforcement, feature dependency graphs, scheduling, deterministic arithmetic, bounded window storage, materialization, recovery, resource isolation, observability, performance, replay equivalence, and implementation evidence.

This leaf builds on the approved Phase 01 architecture, Phase 02 observability and performance method, Phase 03 event/persistence/replay/recovery contracts, and Phase 04 market-data and market-state contracts. It specializes the Phase 01 feature authority without redefining market-state ownership, run-input order, control effective positions, logical timers, publication lifecycle, replay classes, clock semantics, or feature-consumability.

The Phase 04 market-state boundary ends when an immutable `ListingStateView` or `StateViewBundle` is published to and accepted by the feature authority. This leaf begins at that exact publication boundary. The feature runtime never reads mutable market-state structures, adapter or normalization buffers, another authority's persistence store, query projections, telemetry, or an independently assembled set of “latest” views.

## Objectives

The feature runtime must establish that Chronos can:

1. register immutable, versioned feature definitions and behavior-changing configurations;
2. declare every market-state, history, timing, quality, and upstream-feature dependency before activation;
3. evaluate each feature against one exact run-wide state cut and exact historical coverage;
4. prevent current or future facts outside that cut from influencing an output;
5. represent valid, diagnostic, and unavailable outcomes without zero-fill, stale carry-forward, or hidden fallback;
6. construct and validate an acyclic, versioned feature dependency graph;
7. schedule evaluations deterministically from published state cuts, ordered controls, and recorded run-timer facts;
8. maintain bounded feature-owned windows that can be rebuilt from authoritative inputs;
9. produce immutable feature outcomes with complete lineage and reproducible semantic checksums;
10. preserve deterministic arithmetic across live processing, replay, rebuild, and supported runtimes;
11. bound computation, memory, retained history, queueing, publication, and deadline behavior;
12. isolate feature code from network, wall clock, credentials, arbitrary storage, and mutable global state;
13. expose exact feature-stage telemetry and empirical performance evidence under the Phase 02 method;
14. prove live/replay semantic equivalence for equivalent manifests, input cuts, timer facts, and schedules;
15. provide initial feature-family contracts without prematurely fixing trading formulas.

## Scope

### In scope

- feature-definition, implementation, instance, parameter, dependency, arithmetic, and schedule versions;
- activation and supersession at ordered configuration effective positions;
- exact `ListingStateView` and `StateViewBundle` input-cut contracts;
- market-state, historical-window, logical-time, and upstream-feature dependencies;
- valid feature observations, diagnostic feature observations, and unavailable outcomes;
- no-look-ahead and future-input exclusion;
- count-, run-input-, logical-time-, source-time-, and feature-observation windows;
- bounded history retention, warmup, invalidation, rebuild, and checkpoint acceleration;
- feature DAG construction, cycle rejection, topological evaluation, and exact-cut consistency;
- state-change, event-class, interval, and run-timer scheduling policies;
- deterministic arithmetic, canonicalization, numeric validity, and output units;
- result memoization, cache invalidation, accepted materialization, and publication;
- resource/deadline budgets, overload behavior, isolation, and failure containment;
- feature-runtime health, alerts, logs, metrics, traces, profiles, and latency points;
- live/replay equivalence and deterministic recovery;
- property, model-based, contract, failure, security, and performance tests;
- initial feature families: order-book imbalance, microprice/spread, returns, trade imbalance, and volatility.

### Out of scope

- strategy evaluation, signal/abstention logic, ranked explanation policy, or recommendation construction;
- portfolio construction, risk, execution, accounting, or P&L;
- research experiment comparison, parameter search, statistical strategy validation, or strategy promotion;
- defining profitable thresholds, weights, combinations, or market-specific trading rules;
- ML feature learning, online model training, or opaque model-serving semantics;
- mutable notebook state as production feature truth;
- production multi-venue synchronization or L3 feature formulas;
- exact programming language, plugin ABI, process topology, IPC, storage engine, cache product, or decimal library;
- ticket-level implementation decomposition and estimates.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Run/configuration authority | Run manifest, feature activation, instance parameters, schedule selection, timer-generation policy, and ordered behavior-changing controls | Feature values, market state, timer-stream sequence allocation |
| Stream/run-input authority | Run-input selection, control effective positions, timer-stream epochs/sequences, and deterministic merge order | Feature meaning, feature scheduling beyond registered trigger interpretation, feature values |
| Market-state authority | Immutable listing views, run-wide bundles, complete state lineage, state quality, feature-consumability decisions, and state checkpoints | Feature formulas, feature history, feature DAG, or feature output validity |
| Feature authority | Feature definitions, dependency graph, feature-owned windows, scheduling from accepted cuts, evaluation outcomes, output lineage, feature checkpoints, and feature publication | Mutable market state, strategy outcomes, recommendations, portfolio/risk decisions |
| Strategy runtime | Consumption of declared valid feature observations and later `StrategyEvaluation` outcomes | Reclassifying diagnostic/unavailable feature outcomes as valid |
| Dataset/replay authority | Input manifests, replay class, ordered reconstruction, dataset integrity, and fidelity classification | Feature semantics or a second feature implementation |
| Observability | Non-authoritative measurements, health/alert projections, and evidence export | Creating feature outcomes, proving validity, or supplying missing lineage |
| Query-model authority | Disposable operator/research projections of accepted feature outcomes | Becoming the source of feature truth or strategy input |

The feature authority is the sole owner of feature-definition semantics and accepted feature outcomes. Market state proves whether one exact cut satisfies declared state dependencies. The feature authority proves whether the declared computation over that cut and its exact history produced a valid, diagnostic, or unavailable result.

### DM-E10 adoption boundary

This leaf adopts only the feature-runtime and stale-data consumability prerequisites of `DM-E10`:

- fresh/consumable state may produce valid feature observations;
- stale, gapped, recovering, invalid, unavailable, diagnostic-only, or unknown required state cannot produce a valid trade-capable feature observation;
- diagnostic observations remain explicitly non-tradeable;
- missing or invalid required feature inputs remain explicit for the strategy boundary.

The companion Phase 05 strategy/recommendation leaf owns `StrategyEvaluation`, signal versus abstention behavior, exactly one actionable/hold recommendation per valid signal, zero-delta handling, explanations, and completion of `DM-E10`. This leaf does not claim that the full `DM-E10` artifact passes and does not make strategy or recommendation evidence a prerequisite for feature-runtime closure.

## Canonical feature concepts

### Feature definition

A `FeatureDefinition` is an immutable semantic contract. It contains at minimum:

- `feature_definition_id`;
- stable human-readable name and bounded description;
- semantic version;
- output shape, value type, unit, scale, and allowed domain;
- required evaluation scope: one listing, multiple listings in one bundle, or future declared scope;
- market-state dependency declaration;
- history-window declarations;
- upstream-feature dependency declarations;
- schedule/cadence declaration;
- warmup and invalidation policy;
- quality and fidelity policy;
- arithmetic, canonicalization, and rounding policy references;
- missing, stale, gapped, recovering, invalid, and unavailable behavior;
- deadline and resource-budget class;
- diagnostic-output permission;
- implementation capability requirements;
- compatibility and migration classification;
- owner and review evidence.

A definition fixes meaning, not merely a function name. Changing a window boundary, sample basis, required depth, price basis, normalization, sign, unit, quality tolerance, missing-data treatment, schedule, or arithmetic policy is a semantic version change.

### Feature implementation

A `FeatureImplementation` is one concrete executable realization of a compatible definition. It identifies:

- `feature_implementation_id` and version;
- compatible definition IDs/versions;
- build artifact and dependency identities;
- deterministic-runtime capability declaration;
- supported numeric/arithmetic policy versions;
- supported input/view schema versions;
- supported platform/runtime set;
- resource profile class;
- conformance evidence and semantic checksum set.

Two implementations may claim the same definition only after the conformance corpus proves semantic equivalence over required fixtures and generated cases. Performance equivalence is not semantic equivalence.

### Feature instance

A `FeatureInstance` binds a definition and implementation to one run:

- instance identity;
- definition and implementation versions;
- resolved parameters with units and canonical values;
- listing or bundle scope;
- dependency-graph node identity;
- schedule policy and schedule epoch;
- resource/deadline budget references;
- active configuration/control epoch and effective position;
- activation and optional supersession relationship.

Behavior-changing instance changes occur only through an accepted ordered `ControlOutcome` at its recorded effective position. An instance is never mutated in place. A changed parameter set creates a new immutable instance/configuration version or a superseding activation record.

### Feature evaluation

A `FeatureEvaluation` is one immutable record of a completed or terminally interrupted attempt for one exact evaluation key. Every admitted evaluation produces exactly one terminal disposition:

- `valid_observation`;
- `diagnostic_observation`;
- `unavailable`;
- `runtime_interrupted`.

The terminal evaluation record references exactly one valid observation, exactly one diagnostic observation, exactly one unavailable outcome, or exactly one runtime interruption fact. It never references more than one. `runtime_interrupted` contains no feature observation/value and references an operational `feature.runtime.timeout` or equivalent runtime-fault fact. Unavailable is never encoded as a numeric zero, `NaN`, prior value, neutral value, or absent row.

An admission rejection before a valid evaluation key can be established is limited to malformed, unauthorized, unknown-version, or contract-invalid input that cannot identify a legitimate scheduled evaluation. It is a typed scheduler/admission result, not a `FeatureEvaluation`. Once the pinned schedule selects a legitimate cut, the runtime must establish the evaluation key. Dependency failure or a deterministic input, fuel, memory, or logical-deadline limit then produces a terminal unavailable or permitted diagnostic outcome. Host-specific queue, process, or containment failure must preserve/recover that obligation or fail the affected run explicitly; it cannot invent an alternate semantic result.

### Activated feature fact taxonomy

This leaf activates the Phase 03 `feature.*` namespace additively without changing its envelope, identity, causation, lifecycle, or compatibility rules. The minimum logical fact types are:

- `feature.evaluation.accepted`, identifying one accepted evaluation key;
- `feature.observation.valid`, containing one valid observation;
- `feature.observation.diagnostic`, containing one diagnostic-only observation;
- `feature.unavailable`, containing one no-value terminal outcome;
- `feature.runtime.timeout`, containing one operational host-time preemption disposition with no feature value;
- `feature.evaluation.terminal`, binding the evaluation to exactly one semantic outcome or one runtime interruption fact;
- feature publication, supersession, invalidation, and recovery facts where required by the lifecycle.

Concrete encoded names are fixed in the registry. A transport message, cache entry, metric, trace span, or query row does not become a feature fact merely by using similar fields.

## Definition registry and compatibility

### Registry responsibilities

The feature registry records:

- all definitions, implementations, instances, parameter schemas, units, and value domains;
- exact dependency declarations and graph edges;
- schedule, warmup, quality, arithmetic, and resource policies;
- compatibility ranges for state-view, bundle, timer, event, and feature-output schemas;
- deterministic identity and checksum algorithms;
- correction, supersession, and deprecation rules;
- evidence identities and activation status.

Unknown required feature definitions, implementations, parameter fields, units, dependency types, schedule semantics, or arithmetic versions fail activation. They do not fall back to a locally convenient default.

### Compatibility classes

Changes are classified as:

- **representation-compatible:** encoding or storage changes with proven canonical semantic equivalence;
- **implementation-compatible:** a new implementation passes the definition's complete semantic conformance corpus;
- **definition-compatible extension:** additive metadata or optional capability that does not change existing outputs;
- **semantic definition change:** any behavior-changing meaning, requiring a new definition version and explicit activation;
- **incompatible:** cannot share identity, cache entries, materialized history, or equivalence claims.

Historical observations retain their original definition, implementation, parameter, arithmetic, dependency, and schedule versions. A newer implementation does not reinterpret them in place.

## Dependency declarations

### Market-state dependencies

Every feature instance resolves a complete market-state dependency contract before activation. It declares:

- required listing identities;
- whether evaluation is single-listing or multi-listing;
- exact required `ListingStateView` and `StateViewBundle` schema/capability versions;
- required book depth and bounded-depth closure;
- required top-of-book, spread, side, and structural validity;
- whether empty, one-sided, locked, crossed, or truncated state is permitted;
- required public-trade history type, horizon/count, completeness, and fidelity;
- required source-time quality where source time determines membership;
- required book/trade/reference/market-control/run-control/run-timer continuity;
- required reference/listing-definition versions or compatibility relation;
- required market status;
- maximum authoritative state age and publication lag;
- tolerated replay/fidelity classes;
- whether diagnostic evaluation is permitted when each requirement fails.

The declaration uses the stable Phase 04 reason families. A feature may strengthen Phase 04 requirements but cannot weaken a state result from `diagnostic_only`, `not_consumable`, or `unknown` to `consumable`.

### Exact current input cut

Every admitted evaluation is anchored to exactly one accepted `StateViewBundle`, called the **evaluation cut**.

For a single-listing feature:

- the evaluation cut identifies one exact bundle;
- the feature consumes exactly one declared member `ListingStateView` from that bundle;
- the observation lineage records both the bundle identity and the member view identity;
- other member views are context only and cannot affect the result unless declared.

For a multi-listing feature:

- the evaluation consumes the exact declared member views referenced by one bundle;
- every required member identity is fixed before admission;
- independently queried “latest” views, dynamically resolved aliases, or members from different bundles are prohibited.

Using a bundle for every evaluation preserves the run-wide control, reference, timer, configuration, and effective-position context. It does not make undeclared bundle members feature inputs.

### Upstream-feature dependencies

A feature may depend on another feature only through a declared `FeatureDependency` containing:

- upstream feature definition/instance and compatible output version;
- required outcome class, normally valid only;
- exact same-cut, lagged-cut, or windowed relation;
- scope/listing/bundle compatibility;
- unit/value-domain requirement;
- history and warmup requirement;
- missing/diagnostic/unavailable policy;
- graph edge identity and version.

An upstream diagnostic observation cannot satisfy a valid dependency. An unavailable upstream result propagates according to the downstream definition's explicit unavailable/diagnostic policy.

No feature may discover dependencies dynamically during evaluation. Dynamic dispatch based on mutable registries, network responses, query results, or unrecorded runtime state is prohibited.

## Evaluation key and exact-cut protocol

### Evaluation key

One logical evaluation is identified by a canonical `FeatureEvaluationKey` containing:

- run ID and mode;
- feature instance identity and graph version;
- evaluation-cut bundle identity;
- exact required listing-view identities;
- trigger identity and trigger type;
- trigger `run_input_sequence`, run-timer position, or declared schedule position;
- active configuration/control epoch and effective position;
- canonical dependency-window definitions and resolution descriptors;
- declared upstream-feature slots and exact observation identities already resolved at key acceptance, where available;
- definition, implementation, parameter, arithmetic, canonicalization, schedule, and registry versions.

The key excludes host thread, process ID, memory address, queue order, telemetry profile, wall time, and cache location.

Duplicate admission of the same key is idempotent. A conflicting attempt with the same key but different input identities, versions, or descriptors stops the affected feature scope as contradictory.

Resolved window identities, final upstream-observation identities, and failed dependency evidence are bound into the terminal disposition. They cannot alter the accepted trigger/cut/configuration identity represented by the key.

### Admission protocol

For one candidate evaluation:

1. accept the exact published bundle and required member-view identities;
2. resolve the active feature graph/configuration at that bundle's effective position;
3. verify that the trigger is legal under the pinned schedule policy;
4. construct and atomically accept one evaluation key and initial evaluation state;
5. evaluate Phase 04 feature-consumability for all declared state dependencies;
6. resolve upstream-feature dependencies at their exact declared cut relation;
7. construct and validate every required history window;
8. prove that no dependency lies after the evaluation cut or outside the declared window;
9. execute under the declared deadline/resource budget;
10. validate output shape, unit, numeric domain, lineage, and semantic checksum;
11. atomically accept exactly one terminal evaluation outcome and its exact identity;
12. publish the outcome to declared consumers under the append-only publication lifecycle.

No partial numeric result becomes visible before terminal acceptance. Failure at any post-admission step produces diagnostic or unavailable according to the definition and failure class; it does not leave an invocation indefinitely ambiguous.

## Historical windows

### Window declaration

Every historical dependency is a versioned `WindowDefinition`. It declares:

- window identity/version;
- input kind: listing views, bundle cuts, market-state fields, public trades, or upstream feature observations;
- scope and partition key;
- ordering basis;
- end-boundary relation to the evaluation cut;
- start-boundary rule;
- inclusive/exclusive boundary semantics;
- count, logical-time duration, source-time duration, run-input distance, or compound bound;
- sampling/resampling rule, if any;
- duplicate and equal-time tie-break policy;
- minimum coverage and warmup rule;
- continuity, quality, fidelity, and reference-version requirements;
- behavior across gaps, epoch changes, controls, listing-definition changes, and resets;
- retention and rebuild requirements;
- maximum members, bytes, and age;
- canonical window checksum.

A generic “last N” or “last X minutes” without ordering basis, boundary semantics, time authority, and completeness rule is invalid.

### Window identity

One resolved `FeatureWindow` identifies:

- definition/version and feature instance;
- evaluation-cut bundle identity;
- exact start and end boundaries;
- ordered member identities or a content-addressed member-set reference;
- member count and coverage;
- complete relevant cursor and logical-time bounds;
- continuity/gap/reference/control/fidelity status;
- warmup status;
- truncation/capacity status;
- semantic checksum.

The full ordered membership remains reconstructable. A digest may be carried in hot-path records only when the complete membership can be recovered and verified.

Window identity and semantic checksum include only semantic provenance: definition/version, feature instance, evaluation cut, exact boundaries, ordered membership, cursor/logical-time bounds, quality/fidelity status, and canonicalization/arithmetic versions. They exclude checkpoint identity, cache hit/miss, process/runtime identity, rebuild attempt, recovery path, storage location, wall-clock reconstruction time, and operator/tool identity.

Operational construction and recovery provenance is recorded separately in immutable `FeatureWindowBuildAttempt` evidence containing the resulting window identity/checksum, attempt identity, source path, checkpoint or origin-replay references, process/runtime, start/end processing points, validation result, and failure reason. Multiple valid build attempts for the same semantic window must produce the same identity/checksum.

### Time bases

- **Run-input windows** use `run_input_sequence` and exact bundle order.
- **Logical-time windows** use recorded run-timer/state logical-clock positions.
- **Source-time windows** use source-asserted event times only when the dependency declares acceptable source-time quality and deterministic equal-time ordering.
- **Count windows** use an exact eligible-member count after filtering under a versioned rule.
- **Feature-observation windows** use accepted upstream observations ordered by their declared evaluation-cut relation, never publication wall time.

Host wall time, query time, scheduler wake-up time, or completion time cannot determine authoritative membership.

### Observation time

Every valid or diagnostic observation carries a structured `observation_time` containing the value/position, clock-domain identity, time-basis enum, policy/version, precision, and quality/uncertainty where applicable.

By default, `observation_time` is the logical-clock position/time recorded by the selected evaluation-cut `StateViewBundle` under the run's pinned timer policy. A feature definition may instead declare one explicit event or source boundary as its observation basis, such as the source time of the exact terminal trade member in a complete window. That alternative is legal only when the definition names the boundary, exact member-selection rule, source clock domain, required time quality, tie behavior, and fallback/unavailable policy.

`observation_time` is never evaluation start/completion time, scheduler wake-up time, publication time, host wall time, or a cache/checkpoint/rebuild timestamp. Processing times remain Phase 02 telemetry points and cannot enter feature value, identity, semantic checksum, window membership, or no-look-ahead decisions.

### No look-ahead

For every evaluation:

- the current bundle is the maximum authoritative cut;
- every state/member cursor must be less than or equal to the evaluation cut under the approved lineage relation;
- every upstream same-cut observation must reference the same bundle;
- every lagged observation must reference a strictly earlier compatible bundle;
- a time-window end is resolved from recorded logical/source-time evidence available at the cut;
- late facts accepted after the cut cannot be inserted into that historical evaluation;
- corrected research replay creates a distinct run/lineage and may yield different windows without claiming faithful equivalence;
- processing completion order cannot select members;
- prefetch may load only identities already proven eligible; it cannot reveal future values to feature code.

The runtime supplies feature code with a bounded read-only evaluation context. There is no API for asking “current,” “latest,” “next,” future cursor positions, unpublished state, or host time.

### Window continuity and invalidation

A window is not valid merely because it contains enough values. It must satisfy the definition's continuity contract.

Typed invalidation causes include:

- insufficient warmup coverage;
- missing required member or view;
- stale/gapped/recovering/invalid/unavailable state;
- crossed trade-continuity boundary without complete post-boundary coverage;
- unresolved book epoch or bounded-depth closure exhaustion;
- incompatible listing/reference definition;
- control/configuration epoch transition prohibited by the definition;
- timer gap or unknown logical-clock progression;
- source-time quality below requirement;
- fidelity class below requirement;
- retention/capacity loss;
- upstream diagnostic/unavailable result;
- checksum or lineage contradiction.

Definitions may declare a legal segmentation or reset at a compatible control/reference/epoch boundary. Such behavior is explicit and versioned. The runtime never stitches incompatible segments or silently carries a prior value across a reset.

## Window storage, retention, and rebuild

### Feature-owned history

The feature authority may maintain bounded indexes and windows over accepted immutable inputs. It owns:

- feature-specific membership indexes;
- exact resolved windows;
- upstream-observation history;
- warmup and coverage frontiers;
- feature checkpoint accelerators;
- deterministic eviction eligibility.

It does not own or duplicate mutable market state. Retained state-view handles always resolve to exact immutable identities. If Phase 04 retention expires a required view, the feature runtime must already possess an approved immutable retained representation/reference sufficient for reconstruction, or the feature becomes unavailable.

### Bounds

Every instance declares and enforces:

- maximum retained cuts/members;
- maximum logical/source-time horizon;
- maximum bytes;
- maximum upstream-feature members;
- maximum concurrently open windows;
- maximum checkpoint/tail size;
- maximum queue depth and age.

Bounds are derived from the activated definitions and supported workload, not guessed from current traffic. Unbounded maps keyed by event, timestamp, listing, parameter, or feature identity are prohibited.

### Eviction

An item is evictable only when:

- no active or recoverable evaluation can require it under declared windows;
- checkpoint/rebuild policy can reproduce it if needed;
- no publication/retry obligation references it;
- retention and audit obligations permit removal;
- eviction does not change an already accepted observation.

Pressure does not permit silent early eviction. If required history cannot be retained, affected instances transition to explicit degraded/unready status and subsequent evaluations become unavailable or diagnostic according to policy.

### Checkpoints and rebuild

A feature checkpoint is an accelerator containing or referencing:

- run and graph/configuration epoch;
- exact market-state bundle frontier;
- exact active feature instances and versions;
- window definitions and resolved coverage frontiers;
- retained member identities/checksums;
- accepted upstream feature frontiers;
- accepted evaluation/output/publication frontiers;
- arithmetic/canonicalization/registry versions;
- semantic checksum and consistency-cut proof.

Recovery with a trusted compatible checkpoint:

1. validates run, graph, definitions, implementations, parameters, schedules, arithmetic, and registry versions;
2. reconstructs the Phase 03 run-input and Phase 04 market-state publication frontiers;
3. selects a trusted compatible feature checkpoint;
4. validates exact cut, member references, output frontier, integrity, and semantic checksum;
5. replays the complete required market-state and upstream-feature tail in canonical cut order;
6. rebuilds windows, warmup state, evaluation idempotency, and publication obligations;
7. reproduces exact same-run accepted evaluation/outcome identities under the registered identity policy;
8. republishes only original pending outcomes idempotently;
9. enables strategy consumption only after required instances are caught up and ready.

### Origin rebuild without a feature checkpoint

When no trusted compatible feature checkpoint exists, the feature authority performs an origin rebuild:

1. validate the run manifest, active/superseded graph epochs, feature definitions/implementations/instances, parameters, schedules, arithmetic, registry, timer policy, and target feature frontier;
2. derive the exact recursive input closure from each feature instance's activation/reset origin through the target frontier, including every required `StateViewBundle`, declared member `ListingStateView`, control/timer position, upstream feature dependency, and historical-window member;
3. obtain that closure either from retained immutable Phase 04 listing-view/bundle history or by requesting Phase 04 origin replay to reproduce the exact accepted listing views/bundles, identities, lineage, quality, and order;
4. verify closure completeness, canonical order, checksums, graph/configuration epochs, and absence of mixed “latest” substitution;
5. traverse bundles in `run_input_sequence` order and feature nodes in canonical topological order, rebuilding windows, warmup/invalidation state, evaluations, outcomes, idempotency, and publication obligations;
6. compare every previously accepted outcome identity/checksum and frontier under the registered identity policy;
7. publish only original pending outcomes idempotently and enable downstream consumption only after exact catch-up.

The input closure is semantic and independent of the chosen reconstruction path. Retained-history and Phase 04 origin-replay paths must produce identical window and outcome semantics. Their checkpoint IDs, replay attempts, processes, durations, and storage paths are operational provenance and are excluded from feature/window identity and semantic checksums.

If the exact recursive closure is unavailable, contradictory, or cannot be reproduced by Phase 04 origin replay, origin rebuild fails. The runtime does not begin from an arbitrary later view, truncate a required window, infer warmup, substitute a query projection, or claim same-run faithful recovery.

If required history or accepted output identity cannot be reconstructed, same-run recovery stops as incomplete/non-faithful. It does not fabricate warmup completion, use a newer “latest” state, or allocate replacement identities for accepted outcomes.

## Warmup

Warmup is an explicit state per feature instance and dependency:

- `not_started`;
- `warming`;
- `ready`;
- `invalidated`;
- `rebuilding`;
- `failed`.

Warmup progress is measured by exact declared coverage, not elapsed host time or number of scheduler iterations. `ready` requires:

- current-cut state dependencies are consumable;
- every required window meets coverage and continuity;
- required upstream features are ready at the required relation;
- graph/configuration versions match the active cut;
- no unresolved capacity, checksum, or recovery condition exists.

Before readiness, an admitted evaluation produces `unavailable` with a stable warmup reason unless the definition explicitly permits a diagnostic observation. Warmup never emits a valid placeholder value.

A gap, incompatible reference change, listing removal/re-addition, reset, graph change, or required-history loss may invalidate readiness. Recovery to `ready` follows the definition's recorded post-boundary coverage rule.

## Feature outcome contract

### Valid feature observation

A `ValidFeatureObservation` means all required dependencies were proven consumable and the computation completed within the semantic contract. It contains:

- observation identity;
- evaluation identity/key;
- run, mode, feature definition/implementation/instance, and graph versions;
- exact evaluation-cut bundle identity;
- exact required listing-view identities;
- exact upstream-feature observation identities;
- resolved window identities/checksums and coverage;
- the complete `StateLineage` of every required listing view, the containing bundle lineage/context, and the logical-clock position;
- structured `observation_time`, its clock-domain identity/time basis/policy/quality, and exact window boundaries;
- active configuration/control epoch and effective position;
- numeric or structured value;
- unit, scale, value domain, and arithmetic policy;
- validity `valid`;
- semantic checksum;
- causation and supersession references;
- acceptance, publication, and recoverability lifecycle evidence.

Only a valid observation may satisfy a trade-capable strategy feature dependency.

### Diagnostic feature observation

A `DiagnosticFeatureObservation` is an explicitly non-tradeable computation allowed by the definition when one or more requirements fail. It contains the same provenance fields as a valid observation plus:

- validity `diagnostic_only`;
- stable failed-requirement reason codes;
- exact quality/continuity/coverage evidence;
- whether a numeric result exists;
- an explicit prohibition on strategy-input eligibility.

A diagnostic numeric value is not “degraded valid.” It cannot enter a valid required feature set, StrategySignal, recommendation, target, or risk path.

### Unavailable outcome

A `FeatureUnavailable` outcome contains no feature value. It identifies:

- evaluation identity/key;
- exact cut, instance, trigger, versions, and dependency descriptors;
- stable unavailable reason codes;
- failed dependency, admission, deadline, isolation, arithmetic, capacity, or recovery evidence;
- retry/supersession eligibility;
- semantic checksum over the outcome;
- lifecycle and causation references.

Unavailable is not zero activity, neutral direction, unchanged value, `NaN`, infinity, null-as-value, or “use previous.”

Accepted feature outcomes are immutable. A later cut may produce a superseding outcome, but late input, a corrected dataset, a changed formula, or an implementation defect never edits an accepted observation in place. Corrected research replay uses a new run/lineage. A discovered implementation defect is represented by linked invalidation/correction evidence and a new definition/implementation result; consumers can still reconstruct what the original run observed.

### Stable reason families

Reason families include:

- `warmup_insufficient`;
- `state_not_consumable`;
- `state_diagnostic_only`;
- `state_evidence_unknown`;
- `listing_missing_or_inactive`;
- `bundle_member_mismatch`;
- `history_insufficient`;
- `history_gap_or_epoch_boundary`;
- `history_stale_or_too_old`;
- `history_capacity_lost`;
- `source_time_quality_insufficient`;
- `reference_or_control_incompatible`;
- `upstream_feature_unavailable`;
- `upstream_feature_diagnostic`;
- `graph_or_version_mismatch`;
- `deadline_expired`;
- `resource_budget_exceeded`;
- `arithmetic_invalid_or_overflow`;
- `implementation_fault`;
- `isolation_violation`;
- `publication_or_recovery_unavailable`;
- `unsupported_capability`;
- `run_paused_stopped_or_aborted`.

Free-form diagnostics may supplement but never replace stable codes.

## Feature DAG

### Graph definition

The active feature graph is an immutable `FeatureGraphVersion` containing:

- graph identity/version;
- exact node/instance identities;
- directed dependency edges;
- scope and exact-cut relation per edge;
- topological order or canonical topological tie-break rule;
- root trigger mapping;
- activation configuration epoch/effective position;
- graph checksum;
- resource/deadline aggregation policy.

### Cycle rules

The graph must be acyclic after expanding aliases, shared subfeatures, lagged dependencies, and parameterized instances.

- direct and indirect same-evaluation cycles are prohibited;
- a lagged dependency is legal only when it references an already accepted observation from a strictly earlier compatible cut under a declared window;
- using a previous value does not turn a cycle into a valid lag unless the lag relation and initial condition are explicit;
- dynamic runtime edge creation is prohibited;
- graph activation fails atomically if cycle, unresolved dependency, unit mismatch, scope mismatch, or incompatible schedule is found.

Cycle detection uses canonical node identities and produces a deterministic cycle witness.

### Topological evaluation

For one evaluation cut:

- eligible nodes are selected by the pinned schedule and active graph;
- same-cut dependencies execute in canonical topological order, with parallel execution allowed only between independent nodes;
- parallel completion order cannot affect output identity, cache selection, window membership, publication order where semantic order matters, or downstream admission;
- shared upstream results are evaluated once per exact evaluation key and referenced by all dependants;
- a terminal upstream outcome deterministically propagates under each downstream policy.

## Scheduling and cadence

### Schedule policy

Every instance has one immutable `FeatureSchedulePolicy` declaring one or more triggers:

- every eligible market-state bundle;
- change to a declared listing member;
- selected market input/event class;
- every N eligible run-input cuts;
- recorded logical-time interval;
- specific recorded run-timer class;
- explicit ordered control-triggered evaluation;
- upstream-feature completion at the same cut.

Market-input/event-class triggers are evaluated from the exact selected-fact identity/type and run-input metadata carried by the accepted bundle. The feature runtime does not subscribe to a second direct market-event stream or infer the cause from changed state.

The policy defines:

- trigger eligibility;
- exact first trigger/origin;
- cadence and phase alignment;
- tie behavior when multiple triggers refer to one cut;
- deduplication;
- whether one cut may create multiple distinct evaluations;
- paused/stopped/aborted behavior;
- deadline derivation;
- terminal conditions.

### Run-timer triggers

Timer-triggered evaluation uses the recorded `run.timer.*` fact already selected and applied through market state:

- the evaluation cut is the exact bundle produced for that timer's run-input position;
- the bundle and required listing views contain the applied run-timer cursor and logical-clock position;
- replay consumes the recorded timer fact and does not regenerate a timer from host time;
- duplicate, gap, missed, degraded, or new-epoch timer conditions follow the pinned policy;
- a timer gap cannot be hidden by later wall-clock scheduling;
- scheduler wake-up latency affects performance evidence, not logical trigger identity.

### Controls and lifecycle

Enable, disable, parameter, graph, and schedule changes take effect only at accepted control effective positions.

- a cut before the position uses the prior graph/instance;
- the cut at or after the position uses the new graph/instance as declared;
- in-flight evaluation treatment is pinned: complete under the old cut, cancel to unavailable, or supersede under a new evaluation key;
- host arrival time of the command does not decide effect;
- rejected controls do not change scheduling.

Pause stops new trade-capable feature progression at its effective position. Capture, market state, recovery, and explicitly approved diagnostic work may continue according to run policy. Stop/abort prevents new evaluations and resolves admitted work under the terminal policy.

### No pressure-driven semantic skipping

Resource pressure may not silently coalesce, skip, reorder, or reschedule trade-capable feature evaluations.

If the declared schedule selects a cut, the runtime must:

- accept its canonical evaluation key;
- complete it with a valid or permitted diagnostic observation; or
- complete it with a terminal unavailable outcome; or
- terminate it as `runtime_interrupted` through an explicit runtime fault fact and fail/classify the affected run scope under the timeout replay rules.

Malformed or unauthorized requests that do not correspond to a legitimate schedule selection may be rejected before key acceptance. Queue, capacity, resource, or deadline pressure is not such a case.

Bundle consumer acceptance and schedule-obligation acceptance form one lossless semantic handoff: the feature authority does not acknowledge a required published cut until it has either proved that the cut selects no evaluation or retained every selected evaluation key/obligation. If capacity prevents that handoff, Phase 04's feature publication remains backpressured/unaccepted and the run becomes unready; the cut is not acknowledged and forgotten.

A definition may deliberately specify a sparse cadence or latest-sampled semantic schedule, but that is a versioned deterministic rule based on ordered inputs, never an opportunistic queue optimization. Any omitted cuts are derivable from the schedule manifest.

## Deterministic arithmetic

### Arithmetic contract

Every definition pins:

- numeric representation class;
- input unit conversions;
- scale and range;
- operation order where non-associativity matters;
- accumulation and reduction order;
- rounding mode and rounding points;
- overflow, underflow, divide-by-zero, and invalid-domain behavior;
- missing-value prohibition or explicit optional-field semantics;
- canonical encoding and hashing;
- platform/runtime compatibility requirements.

Unqualified binary floating point is not accepted for authoritative semantics. A floating implementation may be approved only with a complete deterministic profile specifying format, operation order, compiler/runtime constraints, exceptional-value handling, and cross-platform evidence. `NaN` payloads, signed zero ambiguity, implicit fused operations, locale-sensitive parsing, and unordered reductions cannot affect canonical output.

### Reductions and ordering

Book levels, trades, window members, and upstream observations use canonical iteration order. Parallel reductions must prove the same result as the canonical order or use an explicitly deterministic algorithm. Hash-map iteration, thread completion, SIMD lane grouping, or database row order cannot determine a result.

### Units and value domains

Every output has an explicit unit and semantic domain, such as:

- dimensionless bounded ratio;
- exact price;
- price difference;
- relative return;
- signed quantity or notional;
- rate per declared time basis;
- volatility/statistical dispersion under a named estimator contract.

“Score” is not an acceptable feature unit unless its normalization and range are part of the definition. Display rounding is separate from authoritative feature value.

## Caching and materialization

### Cache role

A cache is a bounded accelerator keyed by the complete evaluation key or a proven canonical subset that cannot alias different semantics. It may store:

- resolved market-state dependency results;
- window memberships;
- intermediate deterministic subfeature values;
- accepted feature outcomes;
- canonical encodings/checksums.

A cache may not:

- become an unversioned source of truth;
- key only by listing and wall time;
- omit definition, parameter, cut, graph, arithmetic, or dependency versions;
- return a prior value for an unavailable current cut;
- survive an incompatible version/control/reference transition without invalidation;
- change output based on hit/miss status.

Cache cold and warm execution must be semantically identical.

Incremental accumulators, rolling statistics, shared subexpressions, and partial reductions are caches under this rule. Each has a versioned update order, exact covered-member frontier, checksum, invalidation policy, and full-recomputation oracle. A gap, correction, incompatible version, missing member, or checksum mismatch invalidates the accumulator; it cannot continue from an assumed state.

### Accepted materialization

An accepted feature outcome is an immutable `feature.*` domain fact under the Phase 03 event envelope. It is not merely a cache row.

Every accepted semantic evaluation/outcome identity follows one registered policy:

1. deterministic derivation from the canonical evaluation key, semantic terminal outcome class, canonical payload, and semantic checksum; or
2. atomic acceptance with a recovery source that preserves the exact opaque identity before publication can affect a downstream consumer.

Under policy 1, every derivation input and algorithm version is retained so same-run recovery reproduces the exact identity. Under policy 2, publication is fenced until exact-identity recovery is established for the declared failure class.

Across independent equivalent runs, opaque IDs may differ; equivalence uses evaluation keys with run-specific fields normalized as declared, lineage, outcome content, and semantic checksums. Same-run retry/recovery always preserves exact accepted identities.

### Publication lifecycle

Each valid, diagnostic, or unavailable semantic outcome has a consumer-specific append-only publication state:

```text
not_published
  -> publication_in_progress
  -> published_to_strategy_runtime
  -> strategy_consumer_accepted
  | publication_failed_retryable -> publication_in_progress
  | publication_failed_terminal
```

Diagnostic/query consumers have independent publication states. Retry republishes the same accepted identity and payload. Terminal publication failure makes the required feature path unready; it does not permit strategy evaluation from a cache or query projection.

`runtime_interrupted` and `feature.runtime.timeout` use a separate runtime-fault evidence/publication lifecycle and are never published as valid, diagnostic, or unavailable feature observations to satisfy strategy dependencies.

## Resource and deadline budgets

### Budget declaration

Every feature instance and graph declares:

- maximum input members and bytes;
- maximum retained window members and bytes;
- maximum intermediate/output size;
- maximum allocations or allocation class;
- CPU/service-time budget;
- queue-wait and end-to-end feature-stage deadline;
- maximum concurrent evaluations;
- maximum graph fan-in/fan-out;
- maximum publication backlog;
- cancellation/supersession policy;
- overload and recovery behavior.

Numeric budgets are established empirically under Phase 02's workload and statistical method. Planning fixes the required dimensions and breach semantics, not arbitrary values.

Semantic resource limits are deterministic functions of declared inputs and configuration, such as maximum members, bytes, graph nodes, output size, checked arithmetic range, or versioned execution-fuel units. CPU duration, scheduler delay, and host memory pressure are operational measurements and containment signals; they cannot silently choose a different valid feature value.

### Deadline semantics

Every deadline identifies:

- start boundary;
- end boundary;
- clock domain;
- derivation from trigger/cut and validity policy;
- whether it is an ordered logical validity deadline, a deterministic execution-fuel/resource limit, or an operational processing objective;
- breach outcome.

Ordered logical validity deadlines are derived from accepted state/timer/control inputs and may deterministically make an evaluation unavailable. Deterministic execution-fuel/resource limits may also produce a reproducible unavailable outcome.

Host monotonic time measures queueing, processing SLOs, containment timeouts, and operational lateness. A normal semantic-equivalence run may not use host timing to choose a different feature value or terminal validity class. If an implementation exceeds an operational deadline, the runtime marks health/readiness degraded and prevents unsafe downstream use according to the strategy/control contract; it may finish the same semantic computation, pause through an ordered control, or fail the affected run. It does not silently turn a host-specific delay into a replay-stable domain result.

A result whose ordered logical validity deadline or deterministic resource limit has expired cannot be reclassified valid because its arithmetic later completes. Its accepted terminal outcome is unavailable or diagnostic according to policy, and later work cannot overwrite it. A host-time containment fault is retained as operational/failure evidence and is compared only in a declared fault campaign, not passed off as ordinary deterministic replay equivalence.

### Overload

Under pressure:

1. preserve run controls, market-state correctness, accepted evaluation identities, and publication/recovery obligations;
2. stop admitting lower-priority diagnostic or query requests that are outside the required schedule;
3. apply bounded backpressure at the feature publication/acceptance boundary before required cuts are lost;
4. accept keys for required scheduled evaluations only when their exact-cut identity and eventual terminal obligation can be retained;
5. use deterministic declared input/resource limits to produce typed unavailable outcomes where applicable;
6. stop or pause the affected progression rather than converting host-specific queue timing into a semantic result;
7. allow admitted work to reach one terminal disposition or fail the run under the declared containment policy;
8. mark required feature/strategy readiness degraded or false;
9. recover only after backlog, history, and exact-cut continuity are proven.

Dropping an accepted cut, overwriting a queue slot, shrinking a window silently, reusing stale cache content, or emitting a plausible value is prohibited.

## Isolation and security

### Deterministic evaluation environment

Feature code receives only:

- immutable typed state-view/bundle handles for exact declared members;
- bounded read-only resolved windows;
- accepted upstream feature observations;
- immutable parameters and version metadata;
- deterministic arithmetic/canonicalization utilities;
- the configured deterministic fuel/operation budget and deterministic budget-exhaustion semantics;
- bounded scratch allocation;

Feature code cannot access:

- network or venue adapters;
- filesystem or arbitrary persistence;
- process environment;
- credentials or secret material;
- host wall time or unrestricted monotonic time;
- runtime cancellation, timeout, deadline, scheduler, queue-pressure, or remaining-host-time state;
- random devices or unpinned randomness;
- mutable global/static state;
- query models, telemetry stores, or another authority's database;
- threads/processes outside the approved runtime capability;
- undeclared state fields or feature instances.

The runtime owns cancellation and preemption externally. It may stop execution at a deterministic fuel/operation limit or at an operational host-time containment deadline, but feature code cannot poll, branch on, catch, or otherwise observe a cancellation/deadline token. A deterministic fuel exhaustion follows the declared semantic outcome contract. Host-time preemption follows the runtime-timeout contract below.

### Isolation model

In-process, process-isolated, sandboxed, WebAssembly, or another execution boundary is deferred. Whatever mechanism is selected must prove:

- capability restriction;
- memory and CPU enforcement;
- fault containment;
- deterministic input/output serialization;
- versioned ABI/SDK compatibility;
- no privilege expansion;
- bounded startup/cold-load behavior;
- identical semantic output across approved isolation modes.

A crash, panic, exception, timeout, invalid memory access, forbidden syscall, or malformed output is contained to the smallest declared scope and produces health/audit evidence. Accepted evaluation obligations are resumed with the same key and semantic inputs when exact recovery is possible; otherwise the evaluation terminates as `runtime_interrupted` and the affected run/scope fails or is classified explicitly. A terminal unavailable feature outcome is emitted only when the pinned deterministic fuel/operation contract defines it; an operational runtime-fault disposition never masquerades as unavailable. No fault can corrupt market state, other feature instances, run configuration, or accepted outcomes.

### Runtime timeout fact and replay rules

Operational host-time preemption emits an immutable `feature.runtime.timeout` fact owned by the feature runtime and terminates the evaluation as `runtime_interrupted` unless exact transparent continuation completes before terminal acceptance. The fact records the evaluation key, runtime/implementation/profile identity, monotonic start/deadline/preemption evidence, clock domain, resource observations, preemption disposition, and causal health/audit references. It contains no feature value and is excluded from feature/window semantic identity and checksums.

Timeout handling preserves replay claims as follows:

- normal deterministic live/replay semantics never derive a feature outcome from host elapsed time;
- if exact recovery can resume the same accepted evaluation before a terminal interruption is accepted, the timeout attempt remains operational history and the eventual semantic outcome is computed from the original logical inputs and deterministic fuel budget;
- if host-time preemption prevents completion and recovery, the affected original run/scope fails or is explicitly classified operationally incomplete;
- a fault-reproduction replay may inject the recorded timeout disposition only through an explicit immutable fault manifest that names the original timeout fact and evaluation key;
- a replay using that manifest is labeled runtime-fault reproduction and is compared with the original operational disposition, not presented as an ordinary normalized-fact semantic-equivalence run;
- a normalized-fact or research replay without the fault manifest recomputes from logical inputs and deterministic budgets in a new run; any successful output does not rewrite or claim exact reproduction of the original timeout-faulted run.

Thus host timeout remains observable and reproducible as an operational fault without allowing feature code to observe wall time or making ordinary semantic replay hardware-dependent.

## Initial feature families

The initial families are conformance targets, not committed trading formulas. Each family must produce one or more concrete versioned definitions before implementation.

### Order-book imbalance

The definition must later specify:

- listing and side/depth scope;
- required depth closure and completeness;
- quantity or notional basis;
- level weighting, if any;
- treatment of empty, one-sided, locked, crossed, or truncated books;
- output unit/range and sign convention;
- exact arithmetic and zero-denominator behavior;
- state-change or timer cadence;
- whether any historical smoothing/window is used.

No formula, number of levels, weighting curve, smoothing constant, or threshold is fixed here.

### Microprice and spread family

Possible definitions include exact spread, relative spread, or a microprice-like derived price. Each concrete definition must specify:

- required best bid/ask and size fields;
- locked/crossed/one-sided policy;
- price and quantity units;
- reference price/denominator;
- output unit and range;
- rounding and tick/reference-definition behavior;
- cadence and optional history.

“Microprice” cannot be used as an unversioned name for different weighting formulas.

### Return family

A concrete return definition must specify:

- price basis, such as mid, microprice-like output, trade price, or another declared feature;
- exact current and prior cut relation;
- logical/source/run-input/count horizon;
- sampling and equal-time behavior;
- arithmetic form and unit;
- behavior across gaps, stale cuts, controls, reference changes, or missing prior values;
- warmup and minimum coverage.

No horizon, price basis, log/simple arithmetic, resampling method, or fallback is fixed here.

### Trade-imbalance family

A concrete definition must specify:

- trade-side/aggressor classification source and confidence;
- count or time window;
- quantity, notional, or count basis;
- source-time quality;
- trade-gap and lossy-epoch policy;
- corrections and duplicate treatment;
- output sign, unit, and range;
- minimum activity/coverage and zero-activity semantics when a complete window genuinely contains no trades.

Missing trades or incomplete continuity are never represented as balanced flow.

### Volatility family

A concrete definition must specify:

- input price/return feature;
- estimator semantics;
- window and sampling;
- minimum observations;
- time scaling and unit;
- mean/centering treatment;
- arithmetic precision;
- gap, stale, outlier, correction, and zero-variance behavior;
- cadence and warmup.

No estimator, annualization, horizon, sampling interval, or outlier policy is fixed here.

## Failure model

| Failure | Required classification | Feature effect | Recovery |
|---|---|---|---|
| Missing/incompatible view or bundle | Dependency unavailable/contract-invalid | No valid output | Wait for compatible ordered cut or fail instance |
| Market state diagnostic/not consumable/unknown | Typed state reason | Diagnostic only if permitted; otherwise unavailable | Later eligible cut under same policy |
| History insufficient | Warmup | Unavailable or permitted diagnostic | Accumulate exact post-boundary coverage |
| History gap/capacity loss | Invalidated | No valid output | Rebuild from complete inputs or re-warm |
| Upstream feature unavailable | Dependency unavailable | Propagate under declared policy | Recompute/restore upstream first |
| Graph/version contradiction | Invariant failure | Stop affected graph/run | Restore compatible manifest or new run |
| Arithmetic overflow/invalid domain | Computation invalid | Unavailable or diagnostic; never clamped silently | Correct definition/input in new lineage |
| Ordered logical deadline or deterministic input/fuel limit breach | Reproducible budget failure | Terminal unavailable/diagnostic | Later eligible cut or changed version/configuration |
| Host processing SLO/containment timeout | Operational implementation fault | `runtime_interrupted`; no feature value or host-timing-derived unavailable/valid observation | Resume exact evaluation or fail/classify affected run; fault campaign remains distinct |
| Feature implementation crash | Isolated implementation fault | Degraded/unready; accepted keys remain obligated or run fails explicitly | Restart/reload under same version, resume exact obligations, or disable through control |
| Cache corruption/miss | Accelerator failure | Recompute; no semantic change | Evict/rebuild cache |
| Accepted outcome identity unrecoverable | Recovery failure | No same-run continuation past affected frontier | Stop or child/new run with explicit fidelity |
| Publication retryable failure | Delivery degraded | Preserve original outcome and retry | Idempotent re-publication |
| Publication terminal failure | Required path unavailable | Strategy path unready | Operator/system recovery; no bypass |
| Timer gap/unknown logical time | Scheduling/freshness unknown | No valid timer-triggered output | Restore recorded timer continuity or new run |
| Pause/stop/abort boundary | Ordered lifecycle transition | Apply pinned in-flight policy | Resume only through approved run lifecycle |

No failure is converted into a valid observation because a prior value exists or a query screen can display one.

## Observability and alerts

### Mandatory telemetry

Use Phase 02 contracts and bounded dimensions for:

- feature definition/implementation/instance/graph activation;
- evaluation candidate, admission, start, terminal disposition, semantic outcome/runtime-interruption class, and reason family;
- exact cut and dependency readiness without high-cardinality IDs as metric labels;
- valid, diagnostic, and unavailable counts/rates;
- state-consumability and upstream-feature dependency outcomes;
- window coverage, age, members, bytes, warmup, invalidation, and rebuild;
- DAG node/edge count, depth, fan-in/fan-out, ready queue, and critical path;
- schedule trigger type, deduplication, missed/degraded timer, and control epoch;
- queue wait, service time, publication time, deadline, and breach;
- cache hit/miss/eviction/corruption with profile epoch;
- arithmetic overflow/invalid-domain and checksum mismatch;
- isolation startup, fault, timeout, forbidden capability, and restart;
- accepted/published/recoverability lifecycle;
- retained history, resident memory, allocations, CPU, and publication backlog;
- replay/live semantic checksum and equivalence result;
- health/readiness per required feature set without unbounded instance labels.

Explanation content, full window membership, event IDs, bundle IDs, and arbitrary parameter strings remain in restricted logs/traces/evidence, not general metric dimensions.

### Canonical latency

The Phase 02 canonical `feature` segment remains:

- start: `market_state_view.published.feature_authority`;
- valid end: `feature_observation.published.strategy_runtime`.

The start refers to the exact listing-view/bundle publication required by the active dependency contract. Registered subsegments may include:

- feature-boundary consumer acceptance;
- scheduler admission;
- dependency validation;
- window resolution;
- upstream wait;
- queue wait;
- computation;
- output validation/checksum;
- semantic outcome or runtime-interruption acceptance;
- publication to strategy runtime;
- strategy-consumer acceptance.

Diagnostic observations use `feature_diagnostic_observation.published.<consumer>`. Unavailable outcomes use `feature_unavailable.published.strategy_runtime`. These are distinct typed endpoints and are not mislabeled as successful valid feature availability. A required-feature readiness duration may span mixed terminal outcomes but must identify its population and outcome; it cannot redefine the canonical valid `feature` segment.

Cache-hit and cache-miss populations are reported separately when materially different. Recoverability and query projection are separate from the hot-path segment unless an explicit publication fence applies.

### Health and readiness

Health distinguishes:

- process/runtime liveness;
- registry/graph/configuration readiness;
- per-instance warmup/readiness;
- required state-boundary publication health;
- window continuity/capacity;
- timer/schedule continuity;
- isolation capability;
- evaluation and publication backlog;
- recovery/checkpoint readiness;
- strategy-consumer publication health.

This leaf supplies only the feature-side prerequisites projected into `strategy_ready`: every required active feature instance is warmed, current declared market-state dependencies are consumable, required windows/upstream features are complete, and the valid-observation publication path is healthy within deadline/capacity policy. A current required unavailable/diagnostic/runtime-interrupted disposition makes the feature prerequisite false. The companion strategy leaf owns the final strategy-readiness and StrategyEvaluation behavior. Liveness or recent cache hits do not imply readiness.

### Alerts

Use the Phase 02 alert schema for:

- repeated unavailable or diagnostic outcomes for required features;
- warmup/rebuild exceeding accepted objective;
- state/window/upstream dependency contradiction;
- timer gap or schedule drift;
- feature queue/deadline/capacity breach;
- arithmetic or semantic-checksum mismatch;
- graph cycle/version/activation failure;
- isolation violation or repeated implementation crash;
- publication retry exhaustion or terminal failure;
- cache corruption storm;
- live/replay semantic mismatch;
- feature-stage performance regression.

Alerts remain non-authoritative projections and do not activate/disable features. Behavior changes require the run/configuration control path.

## Performance method

Phase 05 adopts all Phase 02 workload, arrival, coordinated-omission, statistical, instrumentation-profile, reference-environment, saturation, replication, waiver, and artifact rules.

### Workload dimensions

Campaigns vary:

- listing count and bundle size;
- active feature definitions/instances;
- graph nodes, edges, depth, fan-in, fan-out, and shared subfeatures;
- book depth and trade-window size;
- state-update and run-timer rates;
- schedule mix and control-change frequency;
- window horizon, member count, and source-time quality;
- cold/warm cache and cold/warm implementation load;
- valid, diagnostic, unavailable, gap, recovery, and warmup populations;
- exact arithmetic widths and worst-case values;
- publication subscriber count;
- normal, burst, sustained, overload, adversarial, and recovery workloads;
- checkpoint/origin rebuild and tail catch-up;
- supported runtime/isolation modes.

### Required measurements

Evidence establishes:

- canonical feature latency distributions by family, outcome, schedule, and cache state;
- queue wait versus service time;
- end-to-end required-feature readiness and deadline-miss rate;
- offered, admitted, completed, valid, diagnostic, unavailable, rejected, retried, and superseded rates;
- sustainable cut and evaluation rates;
- first saturation boundary and overload disposition;
- CPU, allocations, resident memory, retained-window bytes, and cache footprint;
- graph critical-path and parallelism behavior;
- timer-trigger and control-effective-position lag;
- warmup and rebuild throughput/duration;
- checkpoint creation and recovery overhead;
- publication and consumer-acknowledgement backlog;
- isolation startup/fault cost;
- telemetry-profile overhead;
- semantic checksum equality at every supported load below and beyond saturation.

Phase 05 implementation must accept empirical supported envelopes, stage budgets, regression thresholds, and exception policy. A fast result with changed outcomes, omitted cuts, reduced windows, or disabled invariant checks fails.

## Live and replay equivalence

Given:

- equivalent accepted Phase 04 bundle/view cuts;
- the same run-input order and control effective positions;
- the same recorded run-timer facts;
- the same feature definitions, implementations, instances, parameters, graph, schedules, arithmetic, canonicalization, registry, and initial feature state;
- the same history and upstream-feature inputs;

live processing and normalized-fact replay produce semantically identical:

- admitted evaluation keys and deterministic schedule omissions;
- window boundaries, membership, coverage, and checksums;
- warmup/readiness/invalidation transitions;
- graph node eligibility and topological dependency relations;
- valid/diagnostic/unavailable terminal outcome class and stable reasons;
- feature values, units, lineages, and semantic checksums;
- supersession and publication obligations.

Exact IDs match across independent runs only when the registered identity policy is deterministic across those runs. Opaque policies compare canonical evaluation semantics, lineage, outcomes, and checksums. Same-run retry/recovery always preserves exact IDs.

Host scheduling, thread count within supported semantics, cache hit/miss, memory address, process boundary, telemetry profile, benchmark speed, and wall-clock completion time do not change semantic output. Intentional resource/deadline failure campaigns are separately manifested and cannot be compared as ordinary semantic-equivalence runs.

Checkpoint selection, absence of a checkpoint, retained-history rebuild, Phase 04 origin replay, and the number or identity of rebuild attempts also cannot change feature/window semantics. They are operational reconstruction paths. Equivalent exact input closures must produce identical windows, observation times, outcomes, and semantic checksums.

Host-time timeout/preemption is not a logical feature input. Ordinary semantic-equivalence replay excludes it from computation. Runtime-fault reproduction requires the explicit recorded fault manifest defined above and is labeled separately; without that manifest a new replay recomputes and cannot claim to reproduce the original operational timeout disposition.

Replay-class distinctions remain:

- faithful capture-order replay preserves original accepted source/control/timer behavior and pinned versions;
- normalized-fact replay consumes original normalized facts and recorded timers;
- raw re-normalization creates new market-state and feature lineage;
- corrected event-time research replay may create different cuts/windows and cannot claim faithful live identity.

## Testing strategy

### Unit and contract tests

Cover:

- definition/implementation/instance schemas and compatibility;
- parameter units, defaults, bounds, and unknown fields;
- single-listing member selection from one exact bundle;
- multi-listing exact-bundle enforcement;
- Phase 04 consumability and stable-reason propagation;
- evaluation-key canonicalization and duplicate/conflict behavior;
- every window basis and inclusive/exclusive boundary;
- `observation_time` default bundle-logical-clock semantics, explicit event/source-boundary alternatives, clock-domain payload, and processing-wall-time rejection;
- warmup, invalidation, re-warm, and rebuild;
- every terminal disposition and semantic-outcome/runtime-interruption cardinality;
- arithmetic range, rounding, exceptional values, and canonical encoding;
- cache key completeness and invalidation;
- control effective positions and timer triggers;
- publication lifecycle and consumer acknowledgement;
- registry and graph activation.

### Property and model-based tests

Generate:

- arbitrary compatible/incompatible listing views and bundles;
- ordered bundle sequences with controls and timer facts;
- count, time, run-input, and feature-observation windows;
- gaps, late facts, epoch transitions, reference changes, listing add/remove, and resets;
- DAGs with shared nodes, valid lagged edges, direct/indirect cycles, and scope/unit mismatches;
- scheduling permutations and parallel completion orders;
- valid/diagnostic/unavailable dependency combinations;
- numeric boundary and overflow cases;
- cache cold/warm/eviction/corruption sequences;
- checkpoint, crash, replay, and publication-retry boundaries;
- bounded pressure and deadline breaches.

Properties include:

1. no resolved member lies after the evaluation cut;
2. window membership equals an independent canonical reference model;
3. equivalent schedules admit the same logical evaluation keys;
4. graph activation accepts only acyclic compatible graphs;
5. topological parallelism does not change outputs;
6. every admitted evaluation has exactly one terminal disposition; valid/diagnostic/unavailable are semantic feature outcomes, while runtime interruption references exactly one operational timeout fact and no feature value;
7. diagnostic/unavailable outcomes never satisfy valid downstream dependencies;
8. cache state never changes semantic output;
9. missing or gapped data is never converted to zero or prior value;
10. same manifest and cuts produce identical values/checksums;
11. bounds are never exceeded silently;
12. same-run recovery reproduces exact accepted identities and pending publication;
13. checkpoint, retained-history, and Phase 04 origin-replay rebuild paths produce identical semantic window identities/checksums while operational attempt provenance differs;
14. independently equivalent runs follow their declared opaque/deterministic identity comparison policy.

### No-look-ahead tests

The corpus must include:

- one future bundle differing only after the evaluation cut;
- late-arriving corrections;
- equal source times with different accepted order;
- prefetched but ineligible later state;
- future upstream-feature observations;
- timer facts after the trigger;
- corrected-event-time research runs;
- randomized storage iteration and scheduler completion.

Changing, withholding, or corrupting any fact strictly after the cut must not change the original evaluation. Any API or code path capable of reading a future/unpublished value fails conformance even when current fixtures happen not to exploit it.

### Initial-family conformance tests

Before a concrete formula activates, each initial family supplies:

- exact definition and parameter schema;
- independent reference implementation or executable oracle;
- canonical market-state and window fixtures;
- unit/range/sign tests;
- quality/gap/warmup tests;
- arithmetic edge cases;
- live/replay golden checksums;
- resource profile under representative and adversarial inputs.

Planning approval of this leaf does not approve a formula.

### Failure, recovery, and security tests

Inject failure:

- before/after evaluation-key acceptance;
- during dependency/window resolution;
- during computation and output validation;
- before/after semantic outcome or runtime-interruption acceptance;
- before/after recoverability fence where applicable;
- during each publication state;
- after strategy consumer application with lost acknowledgement;
- during checkpoint, retained-history origin rebuild, Phase 04 origin replay, and tail rebuild;
- under cache corruption, history loss, queue saturation, and timer gaps;
- on process/runtime/feature implementation crash;
- through external deterministic-fuel exhaustion and host-time preemption while proving feature code cannot observe cancellation/deadline state.

Security tests attempt:

- network, filesystem, environment, secret, clock, random-device, and arbitrary-store access;
- cross-run/listing/feature scope escape;
- malformed or oversized parameter/output payloads;
- graph/registry downgrade or signature/identity spoofing;
- cache poisoning and checksum substitution;
- high-cardinality telemetry injection;
- denial through pathological windows or graph structure.

The runtime must contain the fault, preserve accepted authoritative inputs/outcomes, and produce typed evidence without leaking secrets.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of this leaf and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **FR-E01 — Feature authority and schema registry** | Definitions, implementations, instances, evaluations, valid/diagnostic/unavailable outcomes, runtime-timeout facts, units, versions, owners, compatibility, reason codes | One feature authority; semantic outcomes and operational timeout facts remain distinct; every semantic change is versioned; unknown required semantics fail closed |
| **FR-E02 — Exact-cut dependency suite** | Single-listing feature from exact bundle member; multi-listing bundle; controls, timer cursor, reference positions; latest/mixed-cut negative cases | Every evaluation names one exact bundle and members; undeclared members cannot influence output; latest/mixed cuts fail |
| **FR-E03 — Feature/stale-data consumability prerequisites (`DM-E10` partial, `MS-E11`)** | Consumable, diagnostic-only, not-consumable, unknown, stale, gapped, recovering, invalid, unavailable, closure-exhausted feature cases; explicit ownership map to companion strategy/recommendation leaf | Only consumable state yields valid feature output; diagnostics remain non-tradeable; unavailable is explicit; this artifact does not claim signal/abstention/recommendation portions of `DM-E10` pass |
| **FR-E04 — Window-definition, provenance, observation-time, and membership oracle** | Count, run-input, logical-time, source-time, and upstream-feature windows; exact boundaries, ties, gaps, versions, semantic checksums; separate build attempts; default/alternative observation-time bases and clock domains | Runtime membership equals independent oracle; rebuild/checkpoint attempts do not affect window identity/checksum; observation time follows declared logical/event/source basis and never processing wall time |
| **FR-E05 — No-look-ahead proof** | Future bundles, future upstream outputs, late corrections, prefetched state, equal-time inputs, timer boundaries, storage/scheduler permutations | No data after the exact cut can affect the observation; violations are mechanically detected |
| **FR-E06 — Warmup/invalidation/rebuild state machine** | Initial warmup, post-gap, reference/control transition, listing add/remove, reset, capacity loss, checkpoint rebuild, no-checkpoint origin rebuild from retained views or Phase 04 origin replay | Valid output begins only at proven coverage; invalidation is explicit; exact closure is rebuilt in order; missing closure fails without truncation/substitution |
| **FR-E07 — Feature DAG conformance** | Valid graphs, shared nodes, deterministic topological order, lagged edges, cycles, dynamic-edge attempts, unit/scope mismatches | Only acyclic compatible graphs activate; cycle witness and ordering are deterministic |
| **FR-E08 — Scheduler/control/timer fixture** | Every schedule class; duplicate triggers; controls before/at/after effective positions; recorded timer gaps/epochs; pause/stop/abort | Same ordered inputs admit same keys; replay never regenerates timers; pressure cannot silently alter cadence |
| **FR-E09 — Evaluation/disposition cardinality and identity crash matrix** | Crash/retry/conflict at key acceptance, computation, semantic outcome acceptance, runtime interruption, identity/recovery, publication, acknowledgement | Every admitted key has one terminal disposition; runtime interruption has one runtime fact and no feature value; same-run identity/effect is never duplicated or substituted |
| **FR-E10 — Deterministic arithmetic suite** | Units, scales, operation/reduction order, rounding, overflow, zero/division, exceptional values, canonical encoding across supported platforms | Exact semantic checksums match; unsupported nondeterministic arithmetic fails activation |
| **FR-E11 — Initial feature-family definition pack** | Concrete versioned definitions and oracles for imbalance, microprice/spread, returns, trade imbalance, volatility | Each formula is explicit, independently testable, and approved separately; family name alone is insufficient |
| **FR-E12 — Cache/materialization equivalence** | Cold/warm cache, eviction, corruption, incompatible versions, key alias attacks, rebuild | Cache state never changes output; accepted facts retain complete identity/lineage; corruption causes recompute or explicit failure |
| **FR-E13 — Window capacity and retention campaign** | Declared member/byte/age bounds, worst-case windows, active evaluations, eviction, pressure, retained-view expiry | No unbounded growth or silent early loss; affected readiness/outcomes fail explicitly |
| **FR-E14 — Checkpoint/origin rebuild and exact recovery** | Trusted/corrupt/absent checkpoints; exact recursive closure from retained listing/bundle history or Phase 04 origin replay; complete/missing tails; graph/version mismatch; accepted outcomes/publication obligations; operational attempt records | Checkpoint and both origin paths produce identical semantic windows/outcomes; operational provenance is excluded from identity/checksum; same-run exact identities recover; missing closure stops safely |
| **FR-E15 — Isolation and security suite** | Forbidden capabilities, cancellation/deadline-token access, scope escape, malformed/oversized data, registry downgrade, cache poisoning, pathological graph/window | Feature code sees only logical inputs and deterministic fuel/operation budget; runtime cancellation/preemption is externally enforced and unobservable; failures are contained and typed |
| **FR-E16 — Resource/deadline/timeout/overload campaign** | Normal, burst, sustained, saturation, hostile inputs, admission, external preemption, backlog, deterministic fuel/logical deadline, host timeout, recorded timeout fact, optional fault manifest | Deterministic limits yield reproducible typed outcomes; host timeout remains an operational runtime fact; ordinary replay stays semantic and fault-reproduction replay is explicitly manifested/labeled |
| **FR-E17 — Telemetry, health, alerts, and latency extension** | Phase 02 schemas, exact feature endpoints, outcome populations, bounded dimensions, profile epochs, loss/failure cases | Canonical endpoint is not redefined; diagnostics/unavailable are distinct; telemetry cannot create validity |
| **FR-E18 — Performance/SLO adoption (`PS-E01`–`PS-E17`)** | Versioned workload/environment, scheduled arrivals, tails, saturation, coordinated-omission controls, resources, cache profiles, statistical record | Accepted feature-stage budgets and regression thresholds are evidence-derived; correctness checksums pass throughout |
| **FR-E19 — Live/replay equivalence proof** | Recorded timers, controls, cuts, graphs, windows, observation times, cache/isolation/scheduling permutations, checkpoint/origin paths, replay classes, timeout-fault manifest distinction | Equivalent semantic manifests produce identical schedules, windows, observation times, outcomes, lineage, and checksums; timeout-fault reproduction remains separately labeled and cannot claim ordinary semantic equivalence |
| **FR-E20 — Property/model-based corpus** | Retained seeds/shrinks for cuts, windows, DAGs, quality, arithmetic, pressure, crashes | Failures reproduce; implementation equals independent models; all stated properties hold |
| **FR-E21 — Strategy-boundary prerequisite contract** | Valid, diagnostic, unavailable, and runtime-interrupted publication/status; consumer acknowledgement; prohibited cache/query bypass; explicit companion-leaf ownership | Only accepted published valid observations can satisfy valid feature dependencies; all other dispositions remain typed non-valid boundary inputs/status, while final readiness, abstention, signal, and recommendation behavior remains owned by the companion leaf |
| **FR-E22 — Schema/version migration and historical interpretation** | Representation changes, compatible implementation swap, semantic definition change, graph/parameter supersession, historical reads | No historical outcome is reinterpreted; incompatible caches/windows are not reused; new semantics create new lineage |
| **FR-E23 — Multi-listing/L3/external extension seam** | Mock second listing/venue bundle, future L3 capability, future external dependency declaration attempt | Existing exact-cut, authority, quality, DAG, and lineage meanings extend without redefinition; unsupported dependencies fail closed |
| **FR-E24 — Independent critique and cumulative Phase 01–05 review** | Critique disposition and comparison with all approved prior leaves and PRDs | Zero unresolved material findings; no authority, lifecycle, ordering, lineage, clock, replay, recovery, telemetry, or evidence contract is weakened |

## Leaf exit conditions

This leaf is implementation-complete only when:

- immutable versioned feature definitions, implementations, instances, parameters, schedules, and graph versions are registered;
- every evaluation is tied to one exact `StateViewBundle` and exact declared member views;
- all current-state, history, timer, quality, and upstream-feature dependencies are explicit and machine-validated;
- no-look-ahead tests prove facts after the cut cannot influence output;
- every admitted evaluation has exactly one valid, diagnostic, unavailable, or runtime-interrupted terminal disposition; runtime interruption is operational and carries no feature value;
- only valid observations can reach trade-capable strategy dependencies;
- all windows have exact membership, continuity, warmup, bounds, checksums, and rebuild behavior;
- window identity/checksum contains only semantic provenance, while checkpoint/recovery/build attempts remain separate operational evidence;
- absence of a feature checkpoint triggers exact-closure origin rebuild from retained Phase 04 view/bundle history or Phase 04 origin replay, and unavailable closure fails safely;
- every valid/diagnostic observation uses the selected bundle logical-clock observation time or an explicitly declared event/source boundary with clock domain, never processing wall time;
- DAG activation rejects cycles, unresolved dependencies, unit/scope mismatches, and dynamic edges;
- schedules are deterministic from ordered cuts, controls, and recorded timers, with no pressure-driven semantic skipping;
- arithmetic is deterministic and canonical across supported environments;
- caches are optional accelerators and materialized accepted outcomes remain immutable/recoverable;
- same-run recovery reproduces exact accepted identities and publication obligations or stops safely;
- resource, deadline, isolation, overload, and failure behavior is bounded and evidenced;
- feature code has no cancellation/deadline token or host-time visibility; deterministic fuel is its only execution budget input and runtime preemption remains externally enforced;
- initial feature families have concrete separately approved definitions and independent oracles before activation;
- live/replay semantic equivalence passes while replay classes remain distinct;
- Phase 05 empirical latency, capacity, memory, window, rebuild, and telemetry-overhead envelopes are accepted;
- `FR-E01` through `FR-E24` pass;
- feature-runtime prerequisites of `DM-E10`, affected other `DM-E*`, and affected `AR-E*`, `OT-E*`, `PS-E*`, `EC-E*`, `PRR-E*`, `MD-E*`, and `MS-E*` evidence pass cumulatively; companion strategy/recommendation evidence remains outside this leaf;
- independent critique and cumulative review of all approved prior leaves have no unresolved material finding.

This leaf does not fail because strategy formulas, signals, recommendations, research experiments, portfolio/risk, or execution are not yet implemented. It fails if any later capability would require reading mutable/latest state, weakening exact-cut lineage, treating unavailable as a value, using host time for semantic scheduling, bypassing recorded timers, creating a second research feature engine, or accepting nondeterministic outputs.

## Deferred choices

The following remain for implementation planning or later approved leaves:

1. Concrete programming language, module boundaries, SDK shape, plugin ABI, and isolation technology.
2. Concrete exact numeric/decimal types, provided the selected arithmetic profile passes `FR-E10`.
3. Concrete window indexes, immutable-handle representation, cache, checkpoint, journal, and materialization products.
4. Exact numeric CPU, latency, deadline, memory, queue, history, checkpoint, and publication budgets; these are evidence-derived.
5. Exact formula, parameter, horizon, weighting, normalization, smoothing, and estimator choices for each initial feature family.
6. Whether all initial features are built-ins or some use a plugin boundary.
7. Exact deterministic/opaque identity format and semantic hash algorithm.
8. Optimization choices such as SIMD, vectorization, incremental formulas, shared subexpressions, memory pools, or code generation.
9. Production multi-venue synchronization and cross-venue feature definitions.
10. L3-specific features and L3-to-L2 derived-feature relationships.
11. External-observation feature dependencies, activated only by their owning later phase.
12. Research-only exploratory feature APIs and notebook integration, which must still call the production feature semantics.

Deferral does not permit local implementations to invent incompatible semantics. Every choice remains behind the exact-cut, version, window, no-look-ahead, outcome, determinism, recovery, isolation, telemetry, and evidence contracts defined here.

## Cumulative review checklist

At this and every later phase gate:

1. Confirm feature code consumes only accepted immutable Phase 04 views/bundles and declared upstream observations.
2. Confirm every evaluation references one exact bundle and exact required member views.
3. Confirm undeclared bundle members, mutable latest state, query projections, telemetry, and cross-authority stores cannot influence output.
4. Confirm market state remains owner of state quality/consumability and feature runtime cannot upgrade it.
5. Confirm feature definition, implementation, instance, parameter, schedule, graph, arithmetic, and registry versions remain distinct and explicit.
6. Confirm all windows have exact time/order basis, boundaries, membership, continuity, warmup, bounds, and checksum.
7. Confirm window identity/checksum excludes checkpoint, recovery, process, storage, and build-attempt provenance, which remains separate operational evidence.
8. Confirm checkpoint absence rebuilds the exact recursive closure from retained Phase 04 history or Phase 04 origin replay and fails if closure is unavailable.
9. Confirm no-look-ahead holds for live, replay, rebuild, prefetch, cache, and parallel scheduling.
10. Confirm recorded run timers, not host wall time, drive logical cadence and timer windows.
11. Confirm `observation_time` uses the bundle logical clock or an explicitly declared event/source boundary and includes its clock domain.
12. Confirm controls take effect at the Phase 03 effective position and rejected controls have no effect.
13. Confirm the graph is acyclic, static for its version, scope/unit compatible, and deterministically ordered.
14. Confirm every admitted evaluation has exactly one valid, diagnostic, unavailable, or runtime-interrupted terminal disposition, and runtime interruption carries no feature value.
15. Confirm unavailable/diagnostic never becomes zero, prior value, neutral valid output, or trade-capable input.
16. Confirm cache cold/warm/evicted/corrupt paths are semantically identical or fail explicitly.
17. Confirm accepted identities and publication obligations are idempotent and recoverable under the registered policy.
18. Confirm window/history/resource bounds cannot be exceeded silently.
19. Confirm deterministic arithmetic and canonical reductions remain stable across supported runtimes.
20. Confirm feature isolation prohibits network, arbitrary persistence, secrets, wall time, unpinned randomness, mutable global state, and cancellation/deadline-token visibility.
21. Confirm host timeout is a runtime fact, ordinary replay does not derive semantics from it, and fault reproduction requires an explicit labeled manifest.
22. Confirm the Phase 02 canonical `feature` segment remains unchanged and diagnostic/unavailable endpoints remain distinct.
23. Confirm live/replay comparisons use equivalent manifests and preserve replay-class distinctions.
24. Confirm this leaf adopts only the feature/stale-data prerequisite subset of `DM-E10`; companion strategy/recommendation evidence owns completion.
25. Confirm research/backtesting reuses this runtime rather than creating a second feature implementation.
26. Confirm later multi-venue, L3, and external-observation features extend dependency types without redefining existing semantics.
27. Confirm all prior-phase evidence affected by feature-runtime activation is rerun cumulatively.
