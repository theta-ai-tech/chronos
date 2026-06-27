# Phase 05: Strategy and Recommendation Runtime

## Purpose

This document defines the planning contract for Chronos's deterministic strategy-evaluation and trade-recommendation runtime. It covers `StrategyEvaluation` outcomes, valid signals versus abstentions, exactly one portfolio-neutral `TradeRecommendation` per valid signal, hold/actionable semantics, strategy declarations, lifecycle, configuration, initial deterministic short-horizon strategy families, ranked explanations, stale/gapped/diagnostic handling, signal validity and supersession, deterministic scheduling, isolation, observability, performance, replay equivalence, conformance evidence, and deferred choices.

This leaf builds on the approved Phase 01 architecture and domain model, Phase 02 observability and performance method, Phase 03 event/persistence/replay/recovery contracts, Phase 04 market-data and market-state contracts, and the Phase 05 feature-runtime leaf. It completes the strategy/recommendation portion of `DM-E10` without redefining market-state ownership, feature semantics, run-input ordering, timer facts, control effective positions, replay classes, lifecycle telemetry, persistence recoverability, portfolio construction, risk, reservation, execution, accounting, or UI read-model ownership.

The upstream boundary is the feature authority's accepted and published valid feature observations and typed non-valid feature dispositions for one exact cut. The downstream boundary is an accepted and published portfolio-neutral `TradeRecommendation`. A recommendation is advisory and non-executable. It is not a target position, risk preview, reservation, executable order intent, paper order, live order, or portfolio-specific decision.

## Objectives

The strategy and recommendation runtime must establish that Chronos can:

1. register immutable, versioned strategy definitions, implementations, instances, parameters, dependency declarations, and recommendation policies;
2. enable, disable, supersede, pause, stop, and reset strategy instances only through ordered behavior-changing controls;
3. evaluate each admitted strategy invocation against one exact feature/state/timer/control cut;
4. emit exactly one terminal `StrategyEvaluation` for every completed admitted invocation, and explicitly preserve or fail any operationally interrupted accepted obligation;
5. encode the outcome as either `signal_emitted` with exactly one valid `StrategySignal`, or `abstained` with no `StrategySignal`;
6. never encode abstention, stale/degraded state, diagnostic observations, or missing dependencies as a neutral or low-confidence signal;
7. produce exactly one portfolio-neutral `TradeRecommendation` for every valid `StrategySignal`;
8. classify each recommendation as either `actionable` with non-zero indicative exposure or `hold` with exactly zero indicative exposure and an explicit strategy-level reason;
9. preserve ranked explanation factors and causal lineage from all influential features, market-state cuts, control/configuration versions, and later external observations;
10. express strategy-indicative size without reading portfolio state or making risk decisions;
11. define deterministic scheduling/cadence from accepted feature publications, state cuts, ordered controls, and recorded run-timer facts;
12. enforce resource, deadline, memory, deterministic-fuel, and overload rules without allowing host timing to alter semantic outputs;
13. isolate strategy code from network, arbitrary persistence, current wall time, mutable global state, credentials, and undeclared inputs;
14. prove replay/live equivalence for equivalent manifests, feature cuts, timer facts, controls, strategy versions, schedules, and deterministic runtime profiles;
15. define initial deterministic short-horizon strategy families without approving concrete trading formulas or alpha claims.

## Scope

### In scope

- strategy definitions, implementations, instances, parameters, schedule policies, dependency declarations, and versioning;
- strategy lifecycle and behavior-changing configuration through ordered `ControlOutcome`s;
- exact-cut strategy admission and evaluation-key construction;
- valid feature input consumption and typed handling of diagnostic, unavailable, feature-runtime-interrupted, stale, gapped, recovering, invalid, unknown, and missing inputs;
- `StrategyEvaluation` cardinality and outcome taxonomy;
- valid `StrategySignal` schema, validity interval, expiration, supersession, score/strength/confidence semantics, reference terms, and ranked explanations;
- abstention reason taxonomy and abstention lineage;
- recommendation authority contract, one-recommendation-per-signal cardinality, hold/actionable semantics, strategy-indicative sizing, and recommendation expiration/supersession;
- scheduling from feature publications, feature-set completion, state-cut eligibility, controls, and recorded run-timer facts;
- deterministic short-horizon strategy-family contracts for order-book imbalance, microprice/spread, and short-horizon momentum;
- deterministic arithmetic, canonicalization, randomization prohibition except approved deterministic seeded randomness, and no-look-ahead;
- resource/deadline isolation, overload behavior, admission, interruption, and recovery;
- observability, health, alerts, profiles, latency endpoints, and performance evidence for strategy and recommendation stages;
- property, model-based, conformance, security, failure, recovery, and live/replay equivalence tests.

### Out of scope

- portfolio construction, portfolio-specific target positions, capital allocation, exposure aggregation, or portfolio-state reads;
- risk policy, risk previews, approvals, modifications, rejections, reservations, projected exposure, or execution authorization;
- paper broker simulation, order generation, fills, ledger, P&L, slippage, fees, or accounting;
- proving profitability, statistical alpha, research experiment comparison, parameter search, or strategy promotion workflow;
- exact formula, threshold, weight, optimization objective, or market-specific trading rule for any initial strategy family;
- ML strategies, online learning, opaque model serving, or external network feature lookup;
- multi-leg opportunity construction, opportunity resolution, candidate-market ranking, or Polymarket discovery beyond lineage-preserving extension seams;
- programming language, plugin ABI, process topology, IPC mechanism, storage product, or ticket-level implementation plan.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Run/configuration authority | Run manifest, active strategy/recommendation configuration, instance enablement, parameter activation, schedule selection, timer policy reference, and ordered behavior-changing controls | Strategy values, signals, recommendations, feature values, stream sequencing |
| Stream/run-input authority | Run-input ordering, control effective positions, timer-stream epochs/sequences, and dispatch order | Strategy meaning, strategy outputs, recommendation outputs |
| Market-state authority | Consumable state views and state-quality lineage already inherited through feature observations | Strategy evaluation, explanations, recommendations |
| Feature authority | Valid feature observations, diagnostic observations, unavailable/feature-runtime-interrupted outcomes, feature lineage, feature publication and consumer acknowledgement | Reclassifying feature dispositions as strategy results; recommendation construction |
| Strategy runtime | Strategy definitions, dependency contract, invocation admission, evaluation outcomes, abstentions, valid signals, signal explanations, signal validity/expiration/supersession | Recommendations, portfolio sizing, target positions, risk decisions, orders |
| Recommendation authority | Exactly one portfolio-neutral actionable or hold `TradeRecommendation` per valid `StrategySignal`, recommendation explanations, recommendation validity/expiration/supersession | Strategy evaluation, abstention, portfolio-specific sizing, risk approval, reservation, execution |
| Opportunity/resolution authority | Later composition of active actionable recommendations into opportunities and listing/market resolution | Creating additional recommendations for a signal, strategy evaluation, risk/execution authorization |
| Dataset/replay authority | Replay manifests, replay class, ordered reconstruction, fixture identity, and fidelity classification | A second strategy implementation or alternate recommendation semantics |
| Observability | Non-authoritative measurements, health/alert projections, and evidence export | Creating signals/recommendations, proving missing validity, or supplying missing lineage |
| Query-model authority | Disposable operator/research projections of evaluations, signals, recommendations, and explanations | Becoming the source of strategy or recommendation truth |

The strategy runtime and recommendation authority are separate semantic owners even when implemented in one process or package. A valid signal cannot exist without one owning `StrategyEvaluation`. A recommendation cannot exist without exactly one valid signal. No component may create a recommendation for an abstention or create two recommendations for the same signal.

## Canonical strategy and recommendation concepts

### Strategy definition

A `StrategyDefinition` is an immutable semantic contract. It contains at minimum:

- `strategy_id`;
- stable name and bounded description;
- semantic version;
- declared strategy family;
- supported evaluation scope: one listing, declared multi-listing bundle, or later extension scope;
- required valid feature definitions and versions or compatibility ranges;
- accepted feature freshness, quality, fidelity, and window requirements;
- required market-state and timer lineage inherited through features;
- optional later external-observation dependency declarations;
- schedule/cadence policy;
- warmup/readiness and reset behavior;
- signal output schema, score/strength/confidence semantics, direction domain, horizon domain, and reference-term policy;
- abstention reason taxonomy;
- explanation-factor schema and ranking policy;
- recommendation policy reference, including hold/actionable rule and indicative sizing basis;
- deterministic arithmetic/canonicalization policy;
- resource, deterministic-fuel, memory, and deadline budget class;
- implementation capability and isolation requirements;
- compatibility, migration, deprecation, and owner/review evidence.

A definition fixes strategy meaning. Changing required features, feature interpretation, cut selection, stale-data handling, schedule, threshold semantics, scoring semantics, explanation ranking, hold/actionable rule, indicative sizing policy, horizon interpretation, arithmetic policy, or external-observation use is a semantic version change.

### Strategy implementation

A `StrategyImplementation` is one concrete executable realization of a compatible definition. It identifies:

- implementation identity and version;
- compatible definition IDs/versions;
- build artifact and dependency identities;
- deterministic runtime profile;
- approved numeric/arithmetic profile;
- supported feature-output schema versions;
- supported schedule and timer policy versions;
- resource profile class;
- conformance corpus and semantic checksum set.

Two implementations may claim the same definition only after a conformance corpus proves semantic equivalence over all required fixtures, generated cases, and boundary conditions. Performance equivalence, matching dashboards, or similar live behavior is not semantic equivalence.

### Strategy instance

A `StrategyInstance` binds a definition and implementation to one run:

- `strategy_instance_id`;
- definition and implementation versions;
- resolved immutable parameters with units and canonical values;
- declared listing/bundle/economic scope;
- exact feature-dependency set;
- schedule policy and schedule epoch;
- recommendation policy version;
- explanation policy version;
- active configuration/control epoch and effective position;
- resource/deadline budget reference;
- activation, disablement, reset, and supersession relationship.

Instances are never mutated in place. Behavior-changing changes create a new immutable instance/configuration version or a superseding activation record effective at one ordered position. A rejected control cannot change strategy scheduling, dependencies, parameters, recommendations, or validity.

### Strategy evaluation

A `StrategyEvaluation` is one immutable terminal outcome for one completed strategy invocation at one exact evaluation key. Its outcome is exactly one of:

- `signal_emitted`, referencing exactly one valid `StrategySignal`;
- `abstained`, carrying one or more typed abstention reasons and referencing no `StrategySignal`.

Operational host/runtime interruption is not a `StrategyEvaluation` outcome. If the runtime cannot complete the invocation after accepting the evaluation key, it records a typed `strategy.runtime.interrupted` operational fact, preserves the accepted obligation for recovery where policy permits, and marks the affected run scope incomplete or failed under the Phase 03 recovery policy. Ordinary semantic dependency failure produces `abstained`, not a runtime interruption.

A completed strategy invocation must terminate exactly once. It may not produce both a signal and an abstention, multiple signals, a signal without an evaluation, or an evaluation without a terminal outcome.

### Valid strategy signal

A `StrategySignal` is the strategy runtime's immutable valid assessment for one signal-emitted evaluation. It identifies:

- exactly one `StrategyEvaluation`;
- strategy definition, implementation, instance, parameter, schedule, arithmetic, explanation, and recommendation-policy versions;
- declared evaluation scope and listing/canonical-instrument references;
- exact input cut: feature observations, feature windows where referenced, state lineage inherited from features, control/configuration epoch, and timer cursor/logical-clock position;
- direction, allowing only the domain-approved signal-direction values and never using direction to encode abstention;
- score or strength, with unit/domain and interpretation;
- confidence only when calibrated by declared evidence; otherwise the quantity must be named as a score, rank, margin, or strength rather than probability;
- expected horizon or holding-window hint;
- reference price, mark, spread, or other interpretation basis where defined;
- ranked explanation factors;
- issue position/time under the declared logical basis;
- validity interval, expiration policy, supersession policy, and current terminal status.

A valid signal is immutable. Later evaluations may expire or supersede it through explicit lifecycle facts; they do not edit it. A signal does not authorize a trade, reserve capacity, create a target, or imply an order.

### Abstention

An `abstained` `StrategyEvaluation` records that a valid signal was not emitted for one completed invocation. Abstention is the required semantic result for trade-capable evaluation when required inputs are stale beyond policy, gapped, recovering, invalid, diagnostic-only, unavailable, feature-runtime-interrupted, unknown, insufficiently warm, out of scope, or otherwise unable to satisfy the strategy contract.

Abstention identifies:

- the same strategy instance, input cut, schedule key, and configuration context as a signal-emitted evaluation would have used;
- one or more stable reason codes;
- all causally relevant invalid, stale, gapped, diagnostic, unavailable, or missing inputs;
- whether the outcome is expected/warmup, data-quality driven, configuration driven, lifecycle driven, or runtime/resource driven;
- diagnostic calculations, if any, as non-tradeable references only.

Abstention is never encoded as:

- a `StrategySignal`;
- direction `neutral`;
- zero score;
- low confidence;
- zero-size recommendation;
- hold recommendation;
- unavailable recommendation;
- absence of a row.

Abstentions produce no `TradeRecommendation`.

### Trade recommendation

A `TradeRecommendation` is the recommendation authority's single immutable, portfolio-neutral interpretation of one valid `StrategySignal`. For each valid signal in a complete strategy/recommendation pipeline, exactly one recommendation must be accepted. The recommendation is exactly one of:

- `actionable`: non-zero indicative exposure under the strategy's recommendation policy;
- `hold`: exactly zero indicative exposure with an explicit strategy-level reason.

A recommendation identifies:

- exactly one valid `StrategySignal`;
- recommendation identity, policy version, schema version, and authority version;
- `actionable` or `hold`;
- proposed direction and non-risk-approved indicative size or exposure;
- size unit, basis, bounds, rounding/canonicalization policy, and whether size is absolute, notional, quantity, normalized score-to-exposure, or another declared representation;
- reference entry or interpretation term where defined;
- expected holding window and exit/stop hints where defined;
- ranked explanation factors copied or derived from the signal without hiding causal contributors;
- validity interval, expiration, supersession, and lifecycle status;
- issue position/time under the same declared logical basis as the signal or an explicitly declared recommendation basis;
- complete causation chain back to the signal, evaluation, features, state lineage, controls, timers, and later external observations where applicable.

Recommendation sizing is strategy-indicative only. It must not inspect portfolio state, available cash, current holdings, open orders, projected exposure, risk limits, kill-switch state, venue account balances, or execution liquidity. Portfolio-specific target construction may later consume active actionable recommendations and produce zero or one portfolio-scoped target per policy, but that is outside this leaf.

A `hold` recommendation is not an abstention. It is produced only after a valid signal exists and the strategy's recommendation policy converts that signal to a zero-delta advisory outcome for a strategy-level reason. Portfolio-specific reasons such as already-held exposure, insufficient buying power, concentration limits, minimum trade size, or risk rejection are not hold reasons in this leaf.

## Fact taxonomy activated by this leaf

This leaf activates the Phase 03 `strategy.*` and `recommendation.*` namespaces additively without changing their envelope, lifecycle, ordering, identity, or recoverability contracts.

Behavior-changing strategy lifecycle and configuration facts are not owned by the `strategy.*` namespace. Strategy activation, disablement, supersession, pause, reset, implementation changes, schedule changes, explanation-policy changes, and recommendation-policy changes are authoritative only when expressed as accepted ordered `ControlOutcome`s under the existing `run.control.*` contract. The strategy registry and query models may expose derived metadata such as “active instance,” “disabled instance,” or “superseded instance,” but those are non-authoritative projections over `run.control.*` history and cannot be replayed as behavior-changing facts.

Minimum strategy fact types:

- `strategy.evaluation.accepted`;
- `strategy.evaluation.signal_emitted`;
- `strategy.evaluation.abstained`;
- `strategy.runtime.interrupted`;
- `strategy.signal.valid`;
- `strategy.signal.expired`;
- `strategy.signal.superseded`;
- `strategy.evaluation.terminal`;
- strategy publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Minimum recommendation fact types:

- `recommendation.obligation.accepted`;
- `recommendation.actionable`;
- `recommendation.hold`;
- `recommendation.expired`;
- `recommendation.superseded`;
- `recommendation.obligation.terminal`;
- recommendation publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Concrete encoded names are fixed in the registry. Metrics, traces, query rows, UI cards, logs, cache entries, or comments are not authoritative strategy or recommendation facts.

## Evaluation key and exact-cut contract

Every admitted strategy invocation has one canonical evaluation key. It includes:

- run identity and replay class;
- strategy instance and configuration epoch;
- strategy definition, implementation, parameter, arithmetic, schedule, explanation, and recommendation-policy versions;
- declared evaluation scope;
- selected feature-observation identities and semantic checksums;
- selected non-valid feature dispositions when they causally produce abstention;
- inherited `StateViewBundle`/`ListingStateView` lineage through those features;
- run-input sequence or schedule position that triggered the evaluation;
- run-timer cursor/logical-clock position when timer-triggered or freshness-relevant;
- control effective position and active configuration epoch;
- external-observation inputs only when a later phase has explicitly activated them;
- deterministic-fuel/deadline budget class;
- identity/checksum policy.

The key excludes host thread, process ID, scheduler wake-up time, queue order, cache location, telemetry profile, current wall time, storage row order, random-device output, operator display state, and portfolio state.

An invocation may be rejected before evaluation-key acceptance only for malformed, unauthorized, unknown-version, disabled-instance, impossible-scope, or contract-invalid requests that cannot correspond to a legitimate scheduled strategy invocation. Once a legitimate schedule selection and input cut establish the key, semantic dependency failure must produce an abstention; operational runtime failure must preserve or explicitly fail the accepted obligation. It cannot disappear.

## Input dependency contract

### Valid feature inputs

Trade-capable strategy evaluation may consume only accepted, published, valid feature observations that satisfy the strategy's declared dependency contract. The strategy runtime must verify:

- feature definition/implementation/version compatibility;
- feature instance identity and scope;
- output unit, domain, quality, and semantic checksum;
- observation time and window basis;
- state lineage and timer/control cursor completeness inherited from the feature;
- publication/consumer-acknowledgement state;
- freshness and validity relative to the strategy's schedule and policy;
- absence of diagnostic-only, unavailable, feature-runtime-interrupted, stale, gapped, recovering, invalid, unknown, or superseded status unless the strategy explicitly declares those as causes for abstention.

The strategy runtime cannot reclassify a diagnostic observation as valid, substitute a previous valid observation, fill missing values with zero, infer latest state from a query model, or read feature caches directly to bypass accepted feature facts.

### Diagnostic and unavailable inputs

Diagnostic observations, unavailable feature outcomes, feature-runtime-interrupted outcomes, and non-consumable state qualities may be referenced only as causation for an abstention or as diagnostic explanation for operator investigation. They cannot contribute to valid signal score, direction, confidence, horizon, indicative size, recommendation, target, or risk.

### External observations

External observations are not a V1 input to the initial strategy families unless a later phase activates their source, normalization, quality, lineage, and replay contracts. When activated, a strategy definition must declare external-observation dependencies exactly like feature dependencies:

- source and schema version;
- effective-time and quality policy;
- correction/supersession handling;
- staleness and gap behavior;
- contribution/explanation requirements;
- replay-manifest inclusion.

Every influential external observation must appear in causal provenance and ranked explanation factors for the affected evaluation, signal, and recommendation. A downstream explanation cannot collapse it into an unattributed generic score.

## Valid signal versus abstention rules

A trade-capable strategy emits a valid signal only when all required conditions hold:

1. the strategy instance is enabled and active at the evaluation cut;
2. its schedule selects the cut;
3. every required feature dependency is valid, compatible, fresh, complete, and within declared window and quality bounds;
4. every required inherited state lineage is consumable under Phase 04;
5. every required timer/control/reference cursor is present and continuous under the declared policy;
6. every required external observation is present and valid, when such dependencies are activated;
7. the strategy implementation completes within deterministic semantic limits;
8. output validation accepts the direction, score/strength, horizon, confidence/score interpretation, reference terms, and explanation factors.

If any required semantic condition fails after key acceptance, the completed outcome is `abstained`. The abstention must name stable reasons and causally reference the failing dependencies. If an operational runtime interruption prevents semantic completion, no `StrategyEvaluation` is invented; the accepted obligation remains recoverable or the run scope is marked incomplete/failed under the registered recovery policy.

Valid signal emission and abstention are both successful domain outcomes. Missing a signal because the runtime omitted a selected invocation is not a valid abstention; it is a scheduling/recovery failure.

## Recommendation rules

For every valid `StrategySignal`, the recommendation authority must accept exactly one recommendation obligation, and that obligation must terminate as exactly one `TradeRecommendation`. If operational failure prevents that terminal recommendation, the affected run scope is incomplete/non-faithful until recovery either publishes the missing recommendation or fails the run; the signal cannot be treated as a complete downstream-available result without its one recommendation.

Pause, disablement, stop, abort, reset, and policy changes do not retroactively cancel the one-recommendation obligation for a valid signal whose signal-emitting evaluation was accepted before the control's effective position. A pause blocks only new post-effective strategy-evaluation admissions and post-effective valid-signal emissions. Every accepted pre-effective valid signal still creates exactly one recommendation obligation, and that obligation must complete with exactly one actionable or hold `TradeRecommendation` unless the run/scope is explicitly marked incomplete/non-faithful under the recovery policy. Host timing, queue delay, scheduler delay, or publication lag cannot leave a valid signal without its one recommendation.

The recommendation policy is part of the strategy definition or an explicitly versioned companion policy. It defines:

- action/hold decision rule;
- indicative size basis;
- sign and direction mapping;
- minimum and maximum strategy-indicative exposure;
- unit, scale, rounding, and canonicalization;
- horizon and exit/stop hint derivation;
- handling of weak, ambiguous, conflicting, or below-threshold valid signals;
- explanation transformation from signal factors to recommendation factors;
- expiration and supersession policy;
- compatibility with opportunity/resolution modules where later applicable.

Actionable recommendations:

- propose non-zero indicative exposure;
- carry no portfolio identity;
- carry no risk approval;
- carry no reservation;
- carry no executable route;
- may later be consumed by portfolio construction if still active.

Hold recommendations:

- propose exactly zero indicative exposure;
- carry a strategy-level reason such as below-action threshold, conflicting strategy-internal evidence, unstable reference terms, or explicitly neutral action policy after a valid assessment;
- terminate the lifecycle before target construction;
- cannot be converted into a target, risk request, reservation, or order.

An abstention cannot be converted into hold to satisfy the one-recommendation-per-signal rule. That rule applies only after a valid signal exists.

## Strategy families for initial implementation

Planning approval of this leaf approves the contracts below, not formulas, thresholds, weights, or profitability claims. Each concrete strategy definition must later provide formulas, parameter schemas, fixtures, oracles, performance budgets, and evidence before activation.

### Order-book imbalance family

This family evaluates declared L2 book structure over one listing and one exact state/feature cut. Its definition must declare:

- required book depth and closure;
- side and level inclusion policy;
- quantity unit and aggregation policy;
- spread/locked/crossed/one-sided handling;
- stale/gap/recovery behavior;
- feature dependencies such as imbalance observations or raw feature components;
- score/strength domain and sign convention;
- horizon interpretation;
- ranked explanation factors such as bid-side pressure, ask-side pressure, top-of-book context, spread state, depth closure, and data-quality constraints.

The family must not assume that imbalance itself implies tradability. Stale, gapped, truncated-with-unknown-best, recovering, or closure-exhausted state must abstain or use diagnostic-only output according to policy.

### Microprice/spread family

This family evaluates short-horizon pressure or reference movement derived from declared microprice, spread, top-of-book, or related feature observations. Its definition must declare:

- exact reference-price basis;
- spread unit and interpretation;
- whether locked/crossed/one-sided books are invalid, diagnostic-only, or family-specific abstention causes;
- required feature freshness and timer alignment;
- score/strength sign convention;
- horizon and reference-entry interpretation;
- ranked explanation factors such as microprice displacement, spread widening/narrowing, top-of-book liquidity, reference movement, and quality/fidelity status.

No formula is approved by naming this family. Any concrete use must pin the exact calculation, bounds, arithmetic, and conformance fixtures.

### Short-horizon momentum family

This family evaluates declared short-window movement using valid features such as returns, trade imbalance, volatility, or price/reference changes. Its definition must declare:

- source basis: trades, mid, microprice, mark, or another registered feature;
- exact window basis: count, run-input, logical-time, source-time, or feature-observation window;
- minimum history and warmup policy;
- late/corrected input handling;
- volatility or noise treatment where used;
- score/strength domain and sign convention;
- horizon and decay/expiration policy;
- ranked explanation factors such as recent return, trade pressure, volatility context, window completeness, and data-quality constraints.

Momentum definitions must prove no-look-ahead for every window. Host wall time, query time, and latest available display state cannot determine membership.

## Explanation factors

Every signal and recommendation must carry ranked explanation factors. A factor identifies:

- factor identity and type;
- rank and ranking policy version;
- human-readable label;
- observed value and unit where applicable;
- normalized contribution, qualitative role, or signed influence where supported;
- source class: feature, market-state lineage, control/configuration, timer, external observation, strategy parameter, recommendation policy, or diagnostic status;
- causal references to exact feature observations, non-valid dispositions, external observations, control facts, or policy versions;
- quality/freshness/fidelity status;
- whether the factor supports action, supports hold, or explains abstention.

The ranking algorithm must be deterministic and versioned. Equal contributions use a declared stable tie-breaker. Explanation order cannot depend on hash-map iteration, thread completion, serialization order, UI sorting, current wall time, or telemetry state.

For abstentions, explanation factors are permitted only as abstention diagnostics and cannot be presented as signal factors. For recommendations, explanation factors must preserve all influential signal factors and add recommendation-policy factors without dropping required causal references.

## Signal and recommendation validity lifecycle

### Signal validity

A valid signal has:

- issue position/time under the declared logical basis;
- validity start and expiry policy;
- maximum age or terminal run-input/timer position;
- supersession scope;
- stale-input invalidation policy;
- lifecycle status: active, expired, superseded, invalidated-by-control, invalidated-by-run-lifecycle, or withdrawn-by-correction where later permitted.

Signals are immutable. Expiration and supersession are separate facts. A later signal in the same supersession scope may supersede an earlier active signal. The scope must be declared by strategy instance, listing/bundle/economic scope, signal family, and policy version.

### Recommendation validity

A recommendation inherits the signal's validity constraints unless the recommendation policy declares a stricter interval. It has:

- issue position/time;
- active/expired/superseded status;
- link to exactly one signal;
- lifecycle termination for hold recommendations;
- downstream eligibility flag for actionable recommendations.

Recommendation expiration or supersession does not mutate the signal. Signal expiration must cause its recommendation to become ineligible for new downstream target construction. Already-created downstream artifacts remain governed by their own later-phase validity rules.

### Corrections and replay classes

Faithful replay reproduces the original accepted evaluation, signal, recommendation, expiration, and supersession facts under the original run manifest and replay class. Corrected event-time research may produce different outcomes, but must label the run and cannot be presented as faithful reproduction of the original live decision history.

## Scheduling and run-timer interaction

Each strategy instance declares a deterministic `StrategySchedulePolicy`. Legal trigger classes include:

- every eligible feature-set completion at a declared exact cut;
- every eligible `StateViewBundle` where all required features are valid or typed non-valid dispositions are available for abstention;
- selected feature-observation class or value-change class under deterministic thresholds;
- every N eligible run-input cuts;
- recorded logical-time interval through selected `run.timer.*` facts;
- explicit ordered control-triggered evaluation;
- startup, reset, or warmup-complete evaluation where declared.

The policy defines:

- origin and first eligible cut;
- cadence and phase alignment;
- tie behavior when multiple triggers refer to one cut;
- deduplication;
- whether one cut may create multiple strategy evaluations;
- pause, disable, stop, abort, and reset behavior;
- deadline derivation;
- terminal and supersession behavior.

Timer-triggered strategy evaluation uses recorded `run.timer.*` facts already selected through the run-input stream and represented in the feature/state lineage. Replay consumes the recorded timer facts and never regenerates timers from host wall time. Timer gaps, duplicate timers, degraded timers, new timer epochs, and missed intervals follow the pinned run/configuration and stream policies.

Resource pressure may not silently coalesce, skip, reorder, or reschedule trade-capable evaluations. If the declared schedule selects a cut, the runtime must accept the evaluation key and complete it as signal or abstention, or record an operational runtime-interruption fact that preserves/fails the obligation explicitly. A strategy may deliberately define sparse cadence or latest-sampled semantics only as a versioned deterministic schedule rule, never as an opportunistic queue optimization.

The strategy runtime does not acknowledge a required feature publication until it has either proved that the cut selects no strategy evaluation or retained every selected evaluation obligation. If capacity prevents that handoff, the feature publication remains backpressured/unaccepted and the run becomes unready; the cut is not acknowledged and forgotten.

## Controls and lifecycle

Strategy enablement, disablement, supersession, pause, reset, parameter changes, implementation upgrades, schedule changes, explanation-policy changes, and recommendation-policy changes are behavior-changing controls. They take effect only through accepted ordered `ControlOutcome`s under `run.control.*` at recorded effective positions. This leaf may consume those control outcomes but cannot replace them with `strategy.instance.*` activation, disablement, or supersession events.

- A cut before the effective position uses the prior active instance/policy.
- A cut at or after the effective position uses the new instance/policy as declared.
- In-flight evaluation treatment is pinned: complete under the old key, interrupt under an explicit runtime fact, or supersede under a new key according to the control policy.
- Host command receipt time does not determine effect.
- Rejected controls do not affect scheduling, evaluation, signal validity, or recommendation validity.

Pause stops new trade-capable strategy-evaluation admissions and new post-effective valid-signal emissions at its effective position. It does not cancel or weaken recommendation cardinality for valid signals accepted before that position. Capture, market state, feature recovery, and explicitly approved diagnostic work may continue according to run policy. Stop/abort prevents new evaluations and resolves admitted work under the terminal policy; any already accepted valid signal must still receive exactly one recommendation or the run/scope is incomplete/non-faithful.

Reset does not delete prior evaluation, signal, recommendation, or explanation history. It creates a new declared run/accounting/configuration epoch as applicable and starts new strategy/recommendation lifecycles from the reset effective position.

## Deterministic runtime and isolation

Strategy code receives only:

- declared valid feature observations and typed non-valid dispositions selected for the evaluation key;
- declared static parameters and versioned policies;
- deterministic arithmetic utilities;
- deterministic seeded randomness only when the strategy definition declares it and the seed is part of the evaluation key;
- logical timer/control/configuration context carried by the input cut;
- deterministic fuel/operation counters where exposed as semantic limits.

Strategy code must not access:

- network;
- current wall time or host monotonic time for semantic decisions;
- filesystem, arbitrary persistence, database, cache, or query model;
- environment variables, credentials, secrets, process state, or user profile;
- mutable global registries;
- random devices or nondeterministic RNG;
- live portfolio state, account state, balances, open orders, fills, risk limits, kill-switch state, or execution adapter state;
- telemetry, logs, metrics, traces, or UI state as input.

Host deadlines and cancellation are externally enforced. Strategy code may observe deterministic fuel/logical-budget exhaustion only if the definition declares it as a semantic limit. It may not branch on remaining host time, scheduler delay, cancellation token state, thread identity, process identity, or queue position.

## Deterministic arithmetic and output validation

Every strategy definition and recommendation policy pins:

- numeric representation;
- input unit conversions;
- operation order;
- rounding mode and rounding points;
- overflow/underflow/divide-by-zero behavior;
- exceptional-value policy;
- output canonical encoding and semantic checksum;
- sign conventions and value domains;
- deterministic tie-breakers.

Unqualified binary floating point is not accepted for authoritative semantics. A floating implementation may be approved only with a complete deterministic profile specifying format, operation order, compiler/runtime constraints, exceptional-value handling, and cross-platform evidence.

Output validation rejects:

- direction outside the declared domain;
- abstention encoded as direction or score;
- `NaN`, infinity, signed-zero ambiguity, locale-sensitive parsing, or unordered-reduction artifacts;
- score/strength outside declared domain;
- confidence without declared interpretation;
- horizon outside declared bounds;
- explanation factors missing required causal references;
- recommendation size that violates the declared strategy-indicative policy;
- actionable recommendation with zero indicative exposure;
- hold recommendation with non-zero indicative exposure;
- recommendation referencing a portfolio or risk state.

## Persistence, recovery, and replay

The strategy runtime and recommendation authority publish durability matrices during implementation. At planning level, the required authoritative records are:

- strategy definitions, implementations, instances, parameter schemas, policy versions, and activation controls;
- accepted strategy evaluation keys/obligations;
- terminal `StrategyEvaluation` outcomes;
- valid signals and abstention facts;
- runtime-interruption facts;
- accepted recommendation obligations;
- actionable/hold recommendations;
- expiration and supersession facts;
- publication and consumer-acknowledgement lifecycle facts;
- evidence needed to reconstruct explanation factors and semantic checksums.

Same-run recovery must reproduce accepted identities and pending publication obligations exactly where identities are accepted facts. Independent equivalent runs may compare deterministic identities only when the identity policy declares deterministic derivation; otherwise they compare semantic lineage, checksums, scopes, outcomes, explanations, and recommendation cardinality.

Faithful replay must use the original pinned strategy/recommendation definitions, implementations, parameters, schedule policies, feature outcomes, timer facts, controls, and replay manifest. Current-code normalization, current feature implementations, current strategy formulas, or current recommendation policies cannot be substituted and still called faithful replay.

## Observability and performance

Observability is non-authoritative. It may report strategy and recommendation health, latency, volume, cardinality, deadline misses, abstention populations, expiration/supersession rates, explanation completeness, and publication status. It cannot create, validate, repair, or infer missing strategy or recommendation facts.

Canonical latency segments adopt Phase 01 and Phase 02 endpoints without redefining them:

- `strategy`: `required_feature_set.published.strategy_runtime` to `strategy_evaluation.accepted`;
- `recommendation`: `strategy_signal.published.recommendation_authority` to `trade_recommendation.accepted`;
- subsegments for admission wait, dependency resolution, computation, output validation, explanation construction, recommendation construction, publication, and consumer acknowledgement. Publication/availability and consumer acknowledgement are measured as subsegments after acceptance, not by changing the canonical parent endpoint.

Metrics and traces must distinguish:

- signal-emitted, abstained, operational runtime-interrupted, and rejected-before-key-admission cases;
- actionable and hold recommendations;
- no-recommendation-obligation-yet, recommendation-obligation-pending, recommendation-terminal, and operationally incomplete states;
- stale, gapped, recovering, invalid, diagnostic-only, unavailable, warmup, disabled, paused, and resource/deadline reason classes;
- strategy family, definition version, implementation version, policy version, and bounded scope dimensions.

Telemetry dimensions must be bounded. Explanation labels, symbols, raw parameter values, unbounded reason text, and source payloads cannot become high-cardinality metric labels.

Performance evidence follows the Phase 02 method:

- registered workload, environment, profile, arrival model, and statistical decision rules;
- scheduled-versus-actual arrival accounting for cadence tests;
- overhead bounds for strategy/recommendation telemetry;
- representative and adversarial feature cuts;
- burst, sustained, saturation, and recovery profiles;
- correctness checksums throughout;
- regression thresholds and exception policy.

## Testing strategy

### Unit and contract tests

Cover:

- definition/implementation/instance schemas and compatibility;
- parameter units, bounds, defaults, and unknown fields;
- strategy schedule policies and effective-position behavior;
- valid feature input acceptance and non-valid feature rejection/abstention;
- evaluation-key canonicalization and duplicate/conflict behavior;
- outcome cardinality: exactly one signal or one abstention per completed invocation;
- one signal per signal-emitted evaluation;
- exactly one recommendation per valid signal;
- no recommendation for abstention;
- actionable/hold zero/non-zero rules;
- recommendation portfolio-neutrality;
- explanation factor ranking and causal references;
- signal/recommendation expiration and supersession;
- output validation and arithmetic canonicalization;
- no forbidden capability access;
- publication lifecycle and consumer acknowledgement.

### Property and model-based tests

Generate:

- ordered feature publications, timer facts, controls, and state-quality changes;
- valid, stale, gapped, recovering, invalid, unavailable, diagnostic-only, unknown, feature-runtime-interrupted, operational runtime-interrupted, and warmup input combinations;
- strategy enable/disable/parameter/schedule changes around exact effective positions;
- duplicate triggers, tied triggers, and multiple trigger classes selecting one cut;
- arbitrary strategy-family parameters within declared domains;
- score, direction, confidence, horizon, explanation, and size boundary cases;
- expiration and supersession sequences;
- crash/retry/publication states;
- live/replay schedule permutations.

Properties include:

1. every completed invocation terminates exactly once, and every accepted-but-interrupted invocation has an explicit operational interruption/recovery fact;
2. signal-emitted evaluations reference exactly one valid signal;
3. abstained evaluations reference no signal and produce no recommendation;
4. every valid signal produces exactly one recommendation;
5. actionable recommendations always have non-zero indicative exposure;
6. hold recommendations always have zero indicative exposure and stop before target/risk;
7. diagnostic/unavailable inputs cannot become valid signal inputs;
8. no output depends on facts after the evaluation cut;
9. explanation ranking is deterministic and complete;
10. strategy/recommendation outputs never reference portfolio/risk/execution state;
11. equivalent manifests produce equivalent outcomes under the declared identity policy;
12. pressure cannot silently skip selected evaluations or recommendation obligations.

### No-look-ahead tests

The corpus must include:

- future feature observations after the selected cut;
- future timer facts;
- late and corrected market data;
- future control changes;
- prefetched but ineligible state or feature cache entries;
- equal observation times with different accepted order;
- storage iteration and scheduler completion permutations;
- current wall-time perturbations;
- replay classes that intentionally recompute corrected history.

Changing, withholding, or corrupting any fact strictly after the selected cut must not change the original strategy evaluation, signal, recommendation, explanation, validity, or semantic checksum.

### Failure, recovery, and security tests

Inject failure:

- before/after evaluation-key acceptance;
- during dependency resolution;
- during strategy computation;
- during output validation;
- before/after signal or abstention acceptance;
- before/after recommendation obligation acceptance;
- during recommendation construction;
- before/after recommendation acceptance;
- during publication and acknowledgement;
- during expiration/supersession;
- during same-run recovery and faithful replay.

Security tests attempt:

- network, filesystem, environment, secret, clock, random-device, arbitrary-store, telemetry, and query-model access;
- portfolio/risk/execution state access;
- registry downgrade or policy spoofing;
- malformed or oversized inputs/outputs;
- explanation high-cardinality injection;
- cache poisoning and checksum substitution;
- denial through pathological parameters or trigger floods.

The runtime must contain the fault, preserve accepted authoritative obligations, and produce typed evidence without leaking secrets or silently changing semantics.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of this leaf and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **SR-E01 — Strategy/recommendation authority registry** | Strategy definitions, implementations, instances, parameters, schedules, explanation policies, recommendation policies, fact types, owners, versions, compatibility, reason codes, and derived registry/query metadata | One owner for evaluations/signals/abstentions and one owner for recommendations; semantic changes are versioned; unknown required semantics fail closed; strategy instance active/disabled/superseded status is non-authoritative metadata derived only from `run.control.*` history |
| **SR-E02 — StrategyEvaluation cardinality suite (`DM-E10`)** | Fresh, stale, gapped, recovering, invalid, diagnostic-only, unavailable, warmup, disabled, paused, feature-runtime-interrupted, and operationally interrupted cases | Every completed invocation terminates once as signal-emitted or abstained; signal-emitted has exactly one valid signal; abstained has no signal; abstention is never encoded as signal; operational interruption is not a StrategyEvaluation outcome |
| **SR-E03 — Recommendation cardinality suite (`DM-E10`)** | Actionable, hold, below-threshold, ambiguous-but-valid, expired, superseded, duplicate-signal, abstention, pause-before-signal, pause-after-signal, queue-delay, and host-timing cases | Every valid signal has exactly one recommendation; abstentions have none; duplicate recommendations are rejected or deduplicated to the prior accepted outcome; pause blocks only new post-effective admissions and cannot leave an accepted/pre-effective valid signal without exactly one actionable/hold recommendation |
| **SR-E04 — Hold/actionable semantics suite** | Zero/non-zero indicative exposure, strategy-level hold reasons, invalid hold reasons, actionable size bounds, downstream eligibility | Hold has exactly zero indicative exposure and stops before target/risk; actionable has non-zero indicative exposure; portfolio/risk reasons cannot appear as hold reasons |
| **SR-E05 — Exact-cut dependency and no-look-ahead proof** | Feature cuts, inherited state lineage, timer/control cursors, future inputs, late corrections, cache/query bypass attempts | Outputs depend only on declared inputs at or before the cut; latest/mixed-cut reads fail |
| **SR-E06 — Feature-boundary adoption (`FR-E21`)** | Valid/diagnostic/unavailable/feature-runtime-interrupted feature publications and consumer acknowledgement | Only accepted valid feature observations satisfy valid dependencies; non-valid feature outcomes cause abstention under strategy policy |
| **SR-E07 — Strategy lifecycle/control fixture** | Enable, disable, supersede, parameter, implementation, schedule, policy, pause, stop, abort, reset, rejected controls around exact effective positions; attempts to replay `strategy.instance.*` lifecycle facts | Host command timing has no semantic effect; rejected controls do not change outputs; reset creates new lifecycle epoch without deleting history; all behavior-changing lifecycle/configuration authority comes solely from accepted ordered `run.control.*` `ControlOutcome`s |
| **SR-E08 — Scheduling and run-timer fixture** | Feature completion, every-cut, N-cut, logical interval, explicit control, warmup-complete triggers; duplicate/tied/missed/degraded timer cases | Same ordered inputs admit same strategy keys; replay consumes recorded timers and never regenerates them from wall time |
| **SR-E09 — Explanation completeness and ranking corpus** | Signal, abstention, actionable, hold, and later external-observation cases; equal-rank tie cases | Ranked factors are deterministic, bounded, causally complete, and preserve every influential feature/external contributor |
| **SR-E10 — Initial strategy-family definition packs** | Order-book imbalance, microprice/spread, and short-horizon momentum contracts with schemas, fixtures, oracles, quality behavior, and no approved formulas until separately registered | Family activation requires concrete formula evidence; family names alone cannot emit signals |
| **SR-E11 — Signal/recommendation validity lifecycle suite** | Active, expired, superseded, invalidated-by-control, invalidated-by-run-lifecycle, corrected/replay-class cases | Lifecycle facts are monotonic; immutable signals/recommendations are never edited; expired recommendations cannot create new downstream work |
| **SR-E12 — Strategy-indicative sizing conformance** | Size units, basis, bounds, rounding, zero/non-zero behavior, forbidden portfolio/risk/account inputs | Recommendation size is portfolio-neutral and strategy-indicative only; no portfolio/risk/execution state is read |
| **SR-E13 — Deterministic arithmetic and output validation suite** | Score, confidence, direction, horizon, explanation contribution, indicative size, rounding, overflow, exceptional values, canonical encoding | Semantic checksums match across supported runtimes; invalid outputs fail closed |
| **SR-E14 — Isolation and security suite** | Forbidden network/filesystem/wall-time/random/persistence/telemetry/query/portfolio/risk access; malformed inputs; downgrade/spoof attempts | Strategy code sees only declared deterministic inputs and utilities; failures are contained and typed |
| **SR-E15 — Resource/deadline/overload campaign** | Normal, burst, sustained, saturation, hostile parameters, deterministic fuel, host timeout, backlog, publication pressure, pause under backlog, and host-timing permutations | Selected evaluations/recommendation obligations are never silently skipped; deterministic limits and operational interruptions are typed and reproducible under declared replay class; host timing cannot prevent a pre-effective valid signal from receiving its one recommendation |
| **SR-E16 — Persistence and recovery matrix** | Accepted keys, outcomes, signals, recommendation obligations, recommendations, expiration/supersession, publication/acknowledgement, crash points, pause effective-position crashes | Same-run recovery reproduces accepted identities and pending obligations; unrecoverable loss marks the run incomplete/non-faithful; every accepted/pre-effective valid signal either recovers exactly one recommendation or the affected run/scope is marked incomplete/non-faithful |
| **SR-E17 — Faithful replay and corrected-replay suite** | Original pinned versions, feature outcomes, timers, controls, schedules, replay classes, corrected-history runs | Faithful replay reproduces original evaluations/signals/abstentions/recommendations; corrected replays are labeled and cannot masquerade as faithful |
| **SR-E18 — Observability and latency extension** | Phase 02 schemas, strategy/recommendation endpoints, bounded dimensions, profiles, alerts, loss cases | Telemetry does not redefine endpoints or create validity; populations and cardinality states are distinguishable |
| **SR-E19 — Performance/SLO adoption (`PS-E01`–`PS-E17`)** | Registered workloads, arrival model, statistical rules, tails, saturation, resource use, correctness checksums | Accepted budgets and regression thresholds are evidence-derived and preserve correctness under load |
| **SR-E20 — Property/model-based corpus** | Retained seeds/shrinks for inputs, controls, timers, schedules, lifecycle, recommendations, explanations, failures | Implementation equals independent models; all stated properties hold |
| **SR-E21 — Cross-phase compatibility review** | Approved Phases 01-04 plus feature-runtime compatibility matrix; ownership, lifecycle, timer, telemetry, replay, stale-data, and identity checks | No later leaf redefines approved upstream terms; `DM-E10` strategy/recommendation portion is complete without requiring portfolio/risk evidence |

## Deferred choices

The following choices are intentionally deferred and must be resolved before implementation or activation of affected strategies:

- exact formulas, thresholds, weights, scores, confidence calibration, and expected-horizon rules for each strategy family;
- exact recommendation sizing formulas and units per strategy definition;
- deterministic identity policy for independent equivalent runs versus opaque same-run identities;
- programming language, SDK, plugin ABI, process boundary, sandbox technology, and IPC;
- concrete persistence engine and batching/flush strategy;
- concrete metric names, dashboards, and alert thresholds beyond Phase 02 schema obligations;
- strategy research, parameter search, statistical validation, and promotion workflow;
- opportunity/resolution integration for multi-leg, multi-venue, and external/Polymarket phases;
- portfolio construction and risk policy contracts.

None of these deferred choices may weaken the cardinality, isolation, replay, stale-data, or portfolio-neutral recommendation rules in this document.
