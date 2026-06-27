# Performance Engineering and SLO Method

## Purpose

This document defines how Chronos turns performance measurements into reproducible engineering evidence, capacity limits, stage budgets, and service-level objectives. It covers workload design, benchmark execution, statistical comparison, profiling, overload characterization, telemetry-overhead analysis, result governance, and cumulative phase gates.

The approved [domain model](../01-architecture/domain-model.md) remains authoritative for domain semantics, deterministic replay, named latency segments, clock validity, causal matching, failure behavior, and correctness evidence. The approved [architecture](../01-architecture/architecture.md) remains authoritative for runtime boundaries, local-first deployment, bounded queues, performance-budget hierarchy, engineering gates, and phase ownership. [Telemetry contracts](telemetry-contracts.md) remains authoritative for latency-point schemas, clock comparability, instrumentation profiles and epochs, metric aggregation, telemetry loss, and observability evidence.

This document specializes those contracts without redefining their endpoints, authorities, lifecycle states, or phase order.

## Objectives

The performance method must:

1. Make every accepted performance claim reproducible from versioned inputs, environment evidence, and machine-readable results.
2. Measure speed and capacity without weakening deterministic behavior, domain invariants, safety controls, auditability, or recovery.
3. Distinguish product-facing SLOs, internal budgets, measured baselines, capacity boundaries, and temporary exceptions.
4. Establish representative normal, burst, overload, recovery, and sustained workloads before setting numeric targets.
5. Measure latency distributions and deadline misses rather than relying on averages or isolated fast samples.
6. Identify the sustainable throughput region, onset of saturation, overload behavior, and recovery envelope.
7. Derive bounded queue capacities and admission policies from measured service behavior and declared burst assumptions.
8. Quantify observability overhead through controlled comparisons while preserving identical domain inputs and semantic checksums.
9. Separate fast shared-CI regression signals from controlled-host acceptance evidence.
10. Treat noisy, drifted, incomplete, or semantically changed benchmark runs as invalid or inconclusive rather than favorable evidence.
11. Produce profiles and counters that explain regressions instead of responding to them with blind optimization or hardware escalation.
12. Let each later phase adopt evidence-backed budgets without inventing values for capabilities that do not yet exist.

## Non-goals

This planning leaf does not:

- choose CPUs, memory sizes, storage devices, operating systems, cloud instances, container platforms, benchmark libraries, profilers, or statistics packages;
- promise a specific hardware class or deployment topology;
- set numeric latency, throughput, capacity, availability, recovery, telemetry-overhead, or retention targets;
- define product strategy validity deadlines before the relevant trading scenarios exist;
- replace semantic, replay, recovery, security, or audit tests with performance tests;
- treat replay logical time as processing latency;
- infer external venue processing time from incomparable clocks;
- require distributed load infrastructure for a local-first product;
- optimize unimplemented future phases;
- permit a benchmark-only code path with different domain semantics from an operational path.

## Normative language

The words **must**, **must not**, **required**, and **prohibited** define planning requirements. **May** describes an allowed implementation choice. Numeric values are deliberately absent unless they are schema examples or phase-derived evidence. Each implementation phase records its accepted values in evidence artifacts governed by this method.

## Core concepts

| Concept | Meaning |
|---|---|
| **Scenario** | A product-relevant operating situation: mode, instruments, streams, strategy set, portfolios, controls, duration, failures, and expected decisions. |
| **Workload specification** | A versioned declaration of input population, event shape, ordering, timing, bursts, controls, faults, and expected semantic results. |
| **Reference workload** | An approved workload specification used for longitudinal comparison or a phase gate. |
| **Workload instance** | One concrete realization of a workload specification, including dataset IDs, seeds, partitions, and generated artifacts. |
| **Reference environment** | A versioned, fingerprinted host and runtime configuration approved for controlled comparisons. |
| **Benchmark configuration** | Build, runtime, workload, environment, instrumentation, affinity, resource, and execution-protocol settings for one benchmark family. |
| **Benchmark run** | One bounded execution under a fixed benchmark configuration and instrumentation-profile epoch. |
| **Trial** | One measured repetition inside a benchmark campaign. |
| **Campaign** | A predeclared collection of baseline and candidate runs analyzed together. |
| **Baseline** | The accepted comparison result for a specific workload, environment class, build/runtime contract, and metric set. |
| **Capacity boundary** | The measured load region after which declared latency, correctness, queue, loss, or recovery conditions are no longer satisfied. |
| **Stage budget** | An internal constraint allocated to a named segment or resource after end-to-end requirements and measured costs are known. |
| **SLO** | A product- or operation-facing objective over a named valid population, time window, workload envelope, and measurement method. |
| **Validity deadline** | The latest point at which an output remains useful and safe for its scenario. It is a domain/product input to SLO derivation, not a benchmark invention. |
| **Regression threshold** | A metric-specific, empirically accepted rule that determines when a candidate requires rejection or investigation. |
| **Invalid run** | A run that violates its protocol or lacks trustworthy evidence and cannot support a performance conclusion. |
| **Inconclusive comparison** | Valid evidence that is insufficient to establish improvement, equivalence, or regression under the registered decision rule. |
| **Waiver** | A temporary, owned acceptance of a specific measured deviation without changing the underlying target. |

## Performance principles

### Correctness precedes speed

A candidate that changes accepted domain facts, ordering, identities, causal lineage, mode boundaries, accounting results, safety outcomes, or required audit evidence fails regardless of measured speed.

Performance validation runs keep the correctness checks required for the workload. Expensive diagnostic telemetry may be disabled in an operational profile only when equivalent correctness evidence is obtained without changing the tested code path or domain behavior.

### Claims are scoped

Every claim names:

- workload specification and instance;
- scenario and execution mode;
- reference environment or environment class;
- build and runtime identities;
- instrumentation profile and epoch;
- metric definition and population;
- clock comparability class;
- sample and repetition protocol;
- result validity;
- uncertainty and decision rule.

“Chronos processes X events per second” or “latency is Y” is not an acceptable claim without that scope.

### Distribution before scalar

Latency, queue age, service time, recovery time, and resource use are distributions or time series. A mean, median, maximum, or percentile may summarize a declared population, but no single statistic represents the complete behavior.

### End-to-end before allocation

Product validity and end-to-end behavior are established before internal stage budgets. Stage budgets are derived from measured critical paths, variance, concurrency, queueing, and controllable cost; separate stage percentiles are never added to manufacture an end-to-end percentile.

### Saturation is a first-class result

Peak observed throughput is not sustainable capacity. Accepted capacity requires stable correctness, latency, queue behavior, resource use, and recovery over the declared interval. The load at which one invariant first fails is evidence, not an embarrassing result to omit.

### Absence is not zero

Missing samples, lost telemetry, unmatched causal endpoints, incomplete metric intervals, clock invalidation, and unavailable counters are explicit quality states. They do not become zero latency, zero loss, or healthy capacity.

### Local-first does not mean uncontrolled

Chronos may benchmark on one machine, but the host remains a measured part of the experiment. A local reference environment must be fingerprinted, isolated sufficiently for its decision class, and reproducible. Remote infrastructure is not required to make the method rigorous.

## Workload taxonomy

Every workload declares one primary class and may declare secondary attributes. Different classes answer different questions and are not substituted for one another.

### Semantic fixture workloads

Small, reviewable inputs with exact expected facts, state checksums, ordering, decisions, and failure outcomes. They establish that benchmark harnesses and optimized paths preserve semantics. They are not capacity evidence.

### Component microbenchmarks

Bounded tests of one operation or data structure, such as parsing, normalization, book application, feature computation, strategy evaluation, serialization, queue handoff, ledger posting, or metric recording.

Microbenchmarks:

- isolate a suspected cost;
- use production-equivalent code and representations;
- record setup separately from measured work;
- state which higher-level behavior they omit;
- cannot establish end-to-end SLO compliance.

### Pipeline benchmarks

Exercise a connected causal path using the Phase 01 segment vocabulary and lifecycle-exact telemetry points. Examples include capture-to-state, state-to-strategy, recommendation-to-risk, paper-intent-to-accounting, or full replay paths as those capabilities exist.

### Replay-throughput workloads

Process deterministic recorded or generated inputs without live pacing. They measure host processing duration, events per processing-time unit, logical-time progress, and replay-speed factor separately. They never label replay processing as live latency.

### Paced operational workloads

Deliver inputs according to a declared timing model approximating live receive patterns. They measure causal latency, queue behavior, deadline misses, resource use, and operational stability under an explicit input envelope.

### Burst workloads

Apply bounded, declared bursts over a steady background. They characterize queue absorption, tail latency, stale-work handling, admission behavior, and time to recover to the pre-burst operating region.

Each burst workload records:

- background rate or pacing model;
- burst event mix;
- burst arrival shape;
- burst duration or event count;
- inter-burst spacing;
- expected queue/admission policy;
- recovery completion condition.

### Saturation and overload workloads

Increase offered load through declared steps or a registered ramp until a correctness, deadline, queue, loss, resource, or recovery condition fails. They continue far enough to characterize safe degradation and fail-closed behavior, subject to test-environment safety.

### Soak workloads

Run a representative load long enough to expose accumulation and drift: memory retention, allocation growth, queue drift, clock behavior, storage growth, file descriptor or handle leaks, counter resets, telemetry retention, reconnect behavior, and degradation of latency or throughput.

Duration is derived from the phenomenon being tested and recorded in the workload manifest; “ran for a long time” is not a protocol.

### Recovery workloads

Combine load with a declared failure and recovery sequence. They measure detection, containment, loss/incompleteness status, restart or reconstruction, backlog handling, readiness restoration, and post-recovery fidelity.

### Control-path workloads

Exercise commands such as pause, stop, configuration changes, profile changes, and later kill switches under normal and overloaded data-plane conditions. They measure the exact lifecycle points established by telemetry contracts and do not infer authoritative acknowledgement from telemetry silence.

### Telemetry-overhead workloads

Use identical domain inputs and benchmark configuration while varying only the registered instrumentation profile or one controlled instrumentation factor. These workloads support attribution and profile acceptance, not domain feature comparisons.

### Adversarial and pathological workloads

Contain legal worst-case shapes and malformed or hostile inputs: deep books, high churn, skewed instrument activity, batch extremes, duplicate/gap storms, invalid payloads, cardinality pressure, rejection storms, reconnect loops, poison inputs, or recovery tails.

They are bounded and labeled. A pathological workload is not presented as representative normal capacity, but normal-capacity evidence is incomplete until relevant pathologies have explicit behavior.

## Workload dimensions

Each workload specification declares applicable dimensions rather than relying on a dataset filename:

- phase and activated authorities;
- execution mode and replay class;
- instrument, listing, venue, stream, and partition counts;
- book depth, update locality, churn, and snapshot/delta mix;
- trade/reference/control/external-observation proportions;
- event-size and batch-size distributions;
- source ordering, gaps, duplicates, late events, corrections, and reconnects;
- feature windows and history requirements;
- strategy count, strategy complexity class, and evaluation cadence;
- portfolio/account count and recommendation/target frequency;
- order, acknowledgement, fill, cancellation, correction, and reconciliation mix;
- input pacing, inter-arrival distribution, burst shape, and concurrency;
- persistence, snapshot, export, and telemetry activity;
- faults and their injection positions;
- expected semantic outputs and checksums;
- expected loss, rejection, degradation, and recovery behavior.

A dimension that is not applicable is marked explicitly. It is not silently omitted if omission could change interpretation.

## Workload manifest

Every reference workload has an immutable manifest with at least:

- workload identity and schema version;
- human-readable purpose and primary taxonomy class;
- owning phase and owner;
- scenario identity;
- dataset, capture, generator, seed, partition, and checksum references;
- replay class or pacing model;
- ordered input and control-event policy;
- declared dimensions and distributions;
- load-control algorithm and stopping rule;
- warmup and stabilization rule;
- measured interval definition;
- repetitions and randomization/blocking plan;
- required latency points, metrics, counters, and profiles;
- semantic oracle, invariants, and accepted checksum set;
- expected failure/degradation behavior;
- validity deadline inputs where known;
- comparison-compatible workload versions;
- known limitations and excluded claims;
- approval and retirement status.

Generated workloads preserve generator code/version, seed, parameter set, and generated-instance checksum. Captured workloads preserve source and normalization lineage according to their replay class.

## Reference workload lifecycle

Reference workloads move through:

```text
Draft -> Validated -> Accepted -> Superseded -> Retired
```

- **Draft** workloads support exploration but cannot gate releases.
- **Validated** workloads pass schema, determinism, semantic-oracle, and harness checks.
- **Accepted** workloads have an owner, representative-use rationale, stable identity, and approved comparison role.
- **Superseded** workloads remain reproducible but have a named successor and migration rationale.
- **Retired** workloads are no longer run routinely but remain retained when referenced by approved evidence.

### Versioning rules

A new workload version is required when a change can affect:

- accepted input population;
- event ordering or pacing;
- burst or fault shape;
- semantic outputs;
- load-control or stopping behavior;
- measured interval;
- required instrumentation;
- capacity interpretation;
- product representativeness.

Editorial metadata changes that cannot affect execution may retain the version if the manifest digest and audit history make that distinction explicit.

Results from different workload versions are not directly compared unless a compatibility study runs both versions on the same accepted baseline and documents the relationship. Backfilling a new workload result does not rewrite older evidence.

### Reference workload set

Each phase maintains a bounded set that includes, when applicable:

- one semantic fixture;
- one representative steady workload;
- one burst workload;
- one saturation/overload workload;
- one recovery or fault workload;
- one telemetry-overhead workload;
- one sustained workload when accumulation risk exists.

The set grows only when a new workload distinguishes material behavior. Large collections of near-duplicate benchmarks dilute gates and increase false discoveries.

## Environment fingerprinting

### Reference environment manifest

Controlled performance evidence records a canonical environment manifest containing applicable:

- environment identity and schema version;
- host identity class without exposing sensitive device identity;
- processor architecture, model/family, core topology, simultaneous-threading state, and exposed instruction capabilities;
- memory capacity, topology where observable, and configured limits;
- storage device class, filesystem, mount options relevant to the benchmark, free-space state, and cache policy;
- network interface/transport class for measured local or external boundaries;
- operating-system and kernel versions;
- firmware or microcode identity where available and relevant;
- runtime, compiler, linker, standard-library, interpreter, and dependency versions;
- build identity, optimization profile, debug/sanitizer state, and generated-code identities;
- process affinity, scheduler policy, priority, resource limits, and container/virtualization state;
- CPU frequency, power, thermal, sleep, and performance-governor settings where observable;
- clock sources, resolution, synchronization, and uncertainty;
- background-service policy and measured competing load;
- environment variables and static/run configuration in canonical redacted form;
- local collector/exporter topology and instrumentation profile;
- locale, timezone, and other settings that can alter parsing or scheduling;
- benchmark tool and harness versions;
- start/end environment observations and drift flags.

Unsupported or unavailable fields are marked `unknown` with reason. They are not guessed.

### Environment classes

Results declare one of these evidence roles:

| Class | Permitted use |
|---|---|
| **Developer exploratory** | Local diagnosis and hypothesis generation; no longitudinal gate. |
| **Shared CI** | Correctness, harness validation, gross-regression screening, and trend signals with broad thresholds. |
| **Controlled reference** | Baseline acceptance, metric-specific regression decisions, capacity derivation, and profile acceptance. |
| **Production-like local** | Operational validation, soak, recovery, and capacity confirmation for the supported deployment shape. |

An environment may satisfy more than one role only when its manifest and controls meet each role's requirements.

### Compatibility and drift

Before comparison, the harness computes an environment-compatibility result:

- `compatible`: all decision-relevant fields match or are covered by an approved equivalence rule;
- `conditionally_compatible`: declared differences are modeled or blocked in the experiment;
- `incompatible`: direct comparison is prohibited;
- `unknown`: evidence is insufficient.

Changes in hardware, runtime, build profile, power policy, affinity, virtualization, security mitigations, telemetry topology, or competing load are experimental variables unless compatibility is proven.

Environment drift during a run—including thermal throttling, frequency-policy change, clock invalidation, memory pressure, storage exhaustion, or unexpected competing load—is recorded and evaluated by the invalid-run policy.

No hardware upgrade is accepted as proof that a software regression is understood. A new environment requires a fresh baseline even when it is faster.

## Benchmark harness contract

The benchmark harness is treated as tested infrastructure. It must:

- consume immutable workload, environment, build, runtime, and instrumentation manifests;
- refuse undeclared profile transitions during a measured interval;
- record exact commands and resolved configuration without secret values;
- separate setup, warmup, stabilization, measurement, cooldown, and teardown;
- collect semantic outputs and checksums alongside performance data;
- use lifecycle-exact latency points from telemetry contracts;
- distinguish queue wait, service time, batching, retry, external wait, and recovery;
- record offered load separately from accepted and completed load;
- preserve raw-enough result data to recompute approved summaries;
- expose clock, counter, sampling, loss, and profile validity;
- avoid hidden network dependencies for local reference runs;
- fail closed when required evidence cannot be written or verified;
- emit a signed or checksummed machine-readable result artifact.

Harness overhead is itself measured. If load generation, collection, or result writing shares constrained resources with the system under test, that placement is explicit in the environment and workload manifests.

## Benchmark execution protocol

### Campaign registration

Before measured runs, the campaign records:

- hypothesis or decision being tested;
- baseline and candidate identities;
- experimental unit and unit of independent replication, such as trial, host restart, workload instance, time block, or reference host;
- primary and guardrail metrics;
- workload and environment;
- controlled and changing variables;
- run order or randomization/blocking method;
- warmup/stabilization and measured-interval rules;
- fixed repetition count or a statistically valid preregistered sequential stopping method;
- statistical decision method;
- confirmatory alpha or confidence level;
- minimum detectable effect and target power for hypothesis/equivalence decisions, or a precision target for characterization;
- invalid-run criteria;
- regression, equivalence, or improvement thresholds;
- confirmatory hypothesis family and multiplicity/error-control policy;
- expected replication probability and the replication/equivalence criterion when independent replication is required;
- owner and reviewer.

Changing the decision rule after seeing results creates a new exploratory analysis. It cannot be presented as the registered gate.

A confirmatory campaign cannot leave the experimental unit, alpha/confidence, effect or precision target, power, hypothesis family, multiplicity policy, or stopping method implicit. A fixed-sample campaign stops at its registered sample size. A sequential campaign must use a method that preserves its declared error rate under repeated looks, such as an approved group-sequential, alpha-spending, confidence-sequence, or sequential likelihood design; repeatedly inspecting ordinary fixed-sample results and stopping when favorable is prohibited.

### Preflight

Preflight validates:

- manifest schemas and checksums;
- semantic fixture and expected outputs;
- environment compatibility;
- build/runtime/profile identity;
- resource availability and storage headroom;
- required clock domains and counters;
- telemetry interval/reset/profile state;
- affinity and background-load policy;
- absence of stale processes or prior-run queues;
- local collector and artifact destination readiness;
- fault-injection safety and cleanup.

A failed preflight prevents the measured campaign from starting.

### Warmup

Warmup is designed to reach the intended runtime regime, accounting for applicable:

- code and data cache population;
- runtime compilation or interpreter specialization;
- allocator state;
- connection and file initialization;
- book/feature/history population;
- snapshot or persistence initialization;
- telemetry pipeline startup;
- CPU power/frequency transition;
- branch and page-fault transients.

Warmup events are excluded from the measured population unless the scenario is specifically startup or cold-path performance. Warmup outputs still pass semantic checks.

Warmup ends by a predeclared rule based on work completed, state reached, or observed stabilization—not by deleting unfavorable early samples after inspection.

### Stabilization

After warmup, the harness evaluates a predeclared stabilization rule over relevant indicators such as:

- throughput;
- latency distribution summaries;
- queue depth and oldest-item age;
- CPU utilization/frequency and scheduler activity;
- allocation and resident memory;
- storage/write behavior;
- collector backlog;
- error, rejection, and deadline-miss rates.

The rule specifies windowing, tolerated trend/variance, maximum wait, and failure behavior in the campaign manifest. If stabilization is not reached, the run is invalid for steady-state claims but may be retained as startup, drift, or saturation evidence when that interpretation was registered or separately reviewed.

### Baseline collection

An accepted baseline is not one run. Baseline collection:

- uses the fixed reference workload, environment, build/runtime contract, and benchmark profile;
- contains repeated independent trials;
- captures within-run distributions and between-run variation;
- randomizes or blocks run order when comparing variants;
- spans enough host restarts or clean states to reveal relevant initialization effects where applicable;
- records raw observations, summaries, and validity diagnostics;
- passes semantic and telemetry-quality gates;
- is reviewed before becoming the comparison baseline.

A baseline is refreshed when its workload, environment, supported build/runtime contract, or representativeness changes. Refreshing does not erase the previous baseline.

### Candidate collection

Candidate runs follow the same protocol and are paired or blocked with baseline runs where practical. A candidate may not receive extra warmup, a quieter environment, a different instrumentation profile, or a reduced correctness workload unless that difference is the registered experimental variable.

### Cooldown and independence

Campaigns declare how they prevent one trial from contaminating the next through caches, thermal state, persistent files, queues, ports, allocator state, or background work. Full cold reset is not always required; the chosen state must match the claim and remain consistent across variants.

## Latency method

### Populations

Each latency distribution names:

- canonical segment or registered subsegment;
- exact start and end points;
- causal matching method;
- execution mode and scope;
- clock comparability class;
- successful, rejected, expired, degraded, retry, or recovery population;
- inclusion and exclusion rules;
- workload interval and profile epoch;
- batch attribution method;
- missing/unmatched endpoint counts.

Different outcome populations are not merged merely to improve a percentile. Rejections and deadline expirations have their own latency and rate distributions.

### Tail treatment

Tail analysis includes:

- declared high-percentile summaries appropriate to the observed population;
- maximum only with sample count and duration;
- deadline miss count/rate where a validity deadline exists;
- exceedance curves or threshold counts when useful;
- confidence or uncertainty around reported tail estimates;
- between-trial variation;
- queue age and service-time context;
- identification of censored, incomplete, or lost observations.

The specific percentiles are selected when the workload population and operational consequences are known. Planning does not mandate arbitrary percentile levels.

Very high percentiles require enough valid observations to support them. If the sample cannot estimate a requested tail with the registered confidence, the result is inconclusive rather than extrapolated without an approved model.

### Outliers

Outliers are not removed solely because they are slow. Exclusion requires a predeclared, independently evidenced invalid condition such as clock failure, harness contamination, environment drift, or corrupt input. Both the excluded observation count and reason remain in the artifact.

Legitimate pauses, page faults, scheduler interference within the supported environment, garbage collection, allocation stalls, persistence waits, retries, and queueing are part of performance unless the scenario explicitly excludes them.

### End-to-end and stage latency

End-to-end latency is calculated from matched causal instances or an approved bounded relationship. It is not derived by summing percentile summaries from stages.

Stage budgets may use:

- instance-level critical-path decomposition;
- queue/service split;
- covariance and concurrency observations;
- controlled removal or optimization experiments;
- scenario-specific validity deadlines.

Parallel work and fan-in remain graph-shaped evidence. A stage can overlap another and still consume resources or contribute to the critical path.

### External time

Local send-to-local-receive round trips use the telemetry contract when clocks are comparable. Venue/source timestamp breakdowns remain apparent or incomparable unless synchronization uncertainty is bounded. No SLO claims precise remote processing or one-way network latency from unsupported clock arithmetic.

## Throughput and saturation method

### Throughput measures

Every throughput result reports at least:

- offered work;
- admitted/accepted work;
- completed work;
- rejected, shed, expired, failed, and retried work;
- semantic unit, such as source events, normalized facts, run inputs, state views, strategy evaluations, recommendations, orders, fills, or replay logical duration;
- measurement interval;
- concurrency and batching;
- resource use;
- latency and queue conditions for the same interval.

Throughput without completion and quality accounting is insufficient.

### Sustainable capacity

Sustainable capacity is the highest tested load region satisfying the campaign's registered conditions for:

- semantic correctness and deterministic outputs;
- accepted latency/tail/deadline objectives;
- bounded queue depth and age;
- no undeclared authoritative loss;
- allowed rejection/shedding policy;
- resource envelope;
- telemetry quality;
- stable behavior for the declared duration;
- recovery after a representative burst or disturbance.

It is reported as a tested envelope, not an unbounded extrapolation.

### Saturation search

The saturation protocol:

1. starts in a demonstrably unsaturated region;
2. increases load through declared steps or an adaptive method whose rule is fixed in advance;
3. allows stabilization at each level;
4. records the first violated guardrail and all concurrent symptoms;
5. continues as safely required to characterize overload policy;
6. reduces load to test recovery and hysteresis;
7. repeats boundary regions sufficiently to quantify variation.

The method may use open-loop offered load, closed-loop concurrency, trace-driven pacing, or replay acceleration. The choice and its queueing implications are explicit. Closed-loop tests cannot claim resilience to unbounded external arrival pressure.

### Coordinated-omission controls

Every externally paced or synthetic arrival that reaches the SUT has:

- a **scheduled arrival time** assigned by the workload model before observing SUT completion;
- an **actual generator emission time**;
- an **actual arrival time** at the declared SUT ingress boundary;
- generator lag from scheduled arrival to emission;
- ingress lag from emission to actual SUT arrival;
- the corresponding completion or terminal-outcome time.

Scheduled work that never reaches the SUT retains its scheduled time and a typed terminal generator outcome instead of a fabricated arrival time. The generator counts every scheduled arrival, including work emitted late, rejected before admission, or not emitted before the campaign deadline. It cannot silently lower offered load when the SUT slows.

Scheduled time and emission time use one registered generator clock domain. SUT ingress and completion points follow the telemetry clock-comparability contract. Cross-runtime ingress lag is a scalar only when clock comparability is bounded; otherwise the endpoint times and generator lag remain reportable, while the unsupported cross-clock lag and any dependent claim are invalid.

A tail-latency or capacity claim for an externally paced scenario requires at least one preregistered open-loop campaign whose arrival schedule does not wait for SUT completion. The generator must have measured capacity headroom independent of the SUT. If generator lag exceeds its preregistered tolerance:

- the raw actual-arrival latency remains available as diagnostic evidence;
- the campaign either applies a preregistered coordinated-omission correction that retains the scheduled-arrival population and reports both raw and corrected results, or marks the affected tail/capacity claim invalid;
- missed or late scheduled arrivals remain counted as offered work;
- a closed-loop result cannot replace the invalid open-loop claim.

Closed-loop tests remain useful for service cost, concurrency limits, and some saturation questions, but they are explicitly labeled and cannot be the sole evidence for arrival-driven tail behavior. A correction method, if used, is versioned in the campaign and analysis manifests; post-hoc correction selected after seeing the tail is exploratory only.

### Resource saturation

Capacity evidence tracks applicable:

- per-core and total CPU time/utilization;
- scheduling, run queue, context switches, migrations, throttling, and steal time where exposed;
- memory allocation, residency, paging, faults, allocator contention, and fragmentation indicators;
- queue occupancy, age, wait, service, rejection, and drops;
- storage throughput, latency, synchronization, queueing, and free-space pressure;
- network throughput, packet/error/retransmission indicators where applicable;
- lock/contention, cache/TLB/branch counters where supported;
- telemetry collector/exporter resource use;
- thermal, power, and frequency state.

A reported bottleneck distinguishes measured evidence from inference.

## Queue capacity and overload characterization

### Queue inventory

Every bounded asynchronous boundary has a queue-capacity record containing:

- queue identity, owner, producer(s), and consumer(s);
- authoritative, safety-operational, baseline, or optional payload class;
- unit and item-size distribution;
- configured capacity and memory cost;
- admission, batching, priority, and fairness policy;
- overflow/rejection/shedding behavior;
- stale/expiry rule;
- drain and shutdown behavior;
- queue wait/service telemetry;
- upstream and downstream failure interaction.

### Capacity derivation

Queue capacity is derived from:

- measured service-rate distribution;
- declared peak arrival and burst envelope;
- maximum tolerable queue age or validity deadline;
- item-size and memory envelope;
- downstream stall/recovery assumptions;
- priority and reserved-capacity requirements;
- acceptable rejection or shedding policy;
- recovery time after the burst.

The derivation is recorded. “Large enough” and unbounded growth are prohibited.

Queue capacity is not used to hide insufficient service capacity. Increasing a queue may reduce immediate rejection while worsening stale decisions, memory pressure, and recovery.

### Overload outcomes

Overload tests establish:

- which work is admitted, rejected, shed, paused, expired, or quarantined;
- whether authoritative evidence remains complete under its durability policy;
- whether safety/control capacity remains available;
- whether stale work is prevented from becoming plausible current decisions;
- whether optional telemetry sheds before domain safety or capture resources;
- whether loss and degradation are visible;
- whether unrelated scopes remain available;
- whether recovery is bounded and preserves semantic status.

Silent overwrite, hidden queue growth, and converting overload into plausible success fail the gate.

### Recovery from overload

The recovery interval begins when offered load or the injected stall returns to the declared operating envelope. Completion requires registered conditions such as:

- queue depth and oldest age return to accepted ranges;
- no stale work remains eligible;
- latency and throughput stabilize;
- health/readiness reflects actual capability;
- required reconciliation/reconstruction completes;
- semantic checksums and evidence status are valid.

Recovery time is a distribution over repeated trials, not one favorable observation.

## Telemetry overhead A/B method

Telemetry overhead uses the instrumentation profiles and profile-epoch rules defined in `telemetry-contracts.md`.

### Required variants

For each activated phase, the campaign compares as applicable:

- correctness-preserving semantic-minimal profile;
- proposed baseline operational profile;
- benchmark profile;
- diagnostic profile or selected diagnostic features;
- controlled removal or addition of one signal class/instrumentation point when attribution is required.

### Experimental control

The comparison holds constant:

- domain workload instance and ordering;
- build/runtime code except for registered compile-time instrumentation variants;
- environment and resource controls;
- domain configuration and seeds;
- warmup/stabilization protocol;
- measured interval;
- semantic assertions.

Run order is randomized or blocked to reduce temporal host bias. Profile changes never occur inside one comparison interval; each variant has a distinct declared profile identity and epoch.

### Measures

The result includes absolute and relative changes in applicable:

- end-to-end and segment latency distributions;
- deadline misses;
- offered, accepted, and completed throughput;
- CPU and scheduler use;
- allocation and resident memory;
- queue depth, contention, rejection, and loss;
- storage and network write volume;
- series/cardinality and aggregation cost;
- shutdown drain and recovery;
- domain semantic checksums.

### Acceptance

An instrumentation profile is accepted only when:

- semantic outputs and order remain equivalent;
- optional telemetry cannot backpressure authoritative work;
- loss/cardinality behavior remains within its accepted contract;
- measured resource and latency overhead has an approved numeric envelope;
- regression thresholds and exception policy are recorded;
- evidence is reproducible on the controlled reference environment.

Measuring overhead without accepting or rejecting an operating profile does not close the Phase 02 implementation gate.

## Determinism and correctness alongside speed

Every pipeline, replay, capacity, overhead, soak, and recovery campaign identifies a semantic oracle. Applicable checks include:

- accepted fact sequence and identities;
- state-lineage and semantic checksums;
- strategy-evaluation, signal/abstention, and recommendation cardinality;
- risk, reservation, execution-mode, and accounting invariants;
- duplicate economic-effect detection;
- complete control-event effective positions;
- exact arithmetic and ledger balance;
- expected invalid/degraded/recovery states;
- telemetry-profile non-interference;
- snapshot-plus-tail equivalence;
- repeated-run equivalence under supported scheduling and host variation.

### Performance nondeterminism

Domain determinism does not imply identical timing. Performance evidence separately characterizes:

- within-run timing variation;
- between-trial variation;
- host/restart variation;
- scheduling variation;
- workload-instance variation.

Timing variance is not allowed to change domain ordering where ordering is deterministic. If timing alters accepted semantic results, the campaign reveals a correctness defect rather than a performance distribution.

### Optimized representation equivalence

When an optimization changes representation, batching, memory layout, concurrency, or language boundary, the campaign includes:

- semantic equivalence against canonical fixtures;
- replay checksum equivalence;
- boundary and failure-path equivalence;
- performance comparison;
- profile/counter evidence explaining the change;
- rollback criteria.

## Budget and SLO derivation

### Distinct artifacts

Chronos distinguishes:

| Artifact | Purpose |
|---|---|
| **Measured baseline** | Describes observed behavior for a fixed workload/environment/profile. |
| **Capacity envelope** | Describes tested load and resource region where registered conditions hold. |
| **Validity deadline** | States when a scenario's result ceases to be useful or safe. |
| **SLO** | Commits to a measured service outcome for a declared population and operating envelope. |
| **Stage budget** | Allocates internal time/resource constraints to preserve an end-to-end SLO or safety deadline. |
| **Alert threshold** | Triggers investigation before or at a breach; it is not automatically the SLO. |
| **Regression threshold** | Detects a material change relative to an accepted baseline. |
| **Capacity trigger** | Initiates shedding, pausing, admission control, or scaling within the supported local topology. |

These values may differ and must not be copied between artifacts without rationale.

### Derivation order

Each phase derives budgets in this order:

1. Define the product scenario and exact correctness workload.
2. Identify decision validity, safety, and operator-response deadlines from product/domain needs.
3. Establish a controlled baseline and sustainable capacity envelope.
4. Identify causal paths, queueing, concurrency, and bottlenecks from instance-level evidence.
5. Select SLO populations, windows, exclusions, and breach consequences.
6. Reserve explicit margin for workload variation, measurement uncertainty, recovery, and expected evolution.
7. Allocate stage and resource budgets to owners based on measured contribution and controllability.
8. Define overload behavior when the envelope is exceeded.
9. Define regression thresholds stricter or earlier than product breach where justified.
10. Review the complete set against correctness, local resource, telemetry, and recovery contracts.

No numeric value is adopted merely because it matches the current implementation. A baseline informs feasibility; product validity and risk determine whether that baseline is acceptable.

### SLO schema

Every SLO records:

- identity, version, owner, and approving authority;
- capability, scenario, mode, and scope;
- reference workload family and supported operating envelope;
- exact metric/segment and valid population;
- clock and measurement method;
- objective statistic or condition;
- evaluation window and minimum valid population;
- allowed exclusions and their evidence;
- treatment of rejections, shedding, retries, recovery, and maintenance;
- error-budget or breach accounting method where applicable;
- warning and breach responses;
- dependency assumptions;
- supporting baseline/capacity evidence;
- review cadence and expiry/revalidation trigger.

An SLO with an undefined population or permissive post-hoc exclusions is invalid.

### Internal budget allocation

Stage budgets:

- preserve canonical segment meanings;
- are owned by the authority controlling the stage;
- include queue and service behavior where relevant;
- declare whether they are hard deadlines, planning allocations, or diagnostic targets;
- account for shared resources and concurrent branches;
- include measurement uncertainty;
- are rebalanced through review, not silently changed by local optimization;
- do not weaken end-to-end validity or safety controls.

Unused stage budget is not automatically transferable when stages are correlated, parallel, or subject to different tails.

### Availability and recovery objectives

Availability-style SLOs are introduced only when the relevant live capability and readiness semantics exist. They use capability-specific readiness from telemetry contracts and never equate process liveness with trading readiness.

Recovery objectives identify:

- failure class;
- detection and containment population;
- reconstruction/reconciliation scope;
- permitted loss/incompleteness policy;
- readiness-restoration condition;
- sustained post-recovery observation;
- workload/backlog assumptions.

## Regression and statistical decision method

### Registered decisions

Each comparison declares one of:

- **regression detection:** determine whether the candidate is materially worse;
- **equivalence/non-inferiority:** determine whether the candidate remains within an accepted degradation margin;
- **improvement:** determine whether the candidate is materially better;
- **characterization:** estimate behavior without a pass/fail claim;
- **capacity boundary:** locate the load region where guardrails stop holding.

The margin or threshold is metric-specific and empirically governed. It is not selected after viewing the candidate.

### Primary and guardrail metrics

A campaign declares a small set of primary metrics tied to the decision. Guardrails cover correctness, tail latency, deadline misses, throughput, queues, resource use, loss, recovery, and telemetry quality as applicable.

An improvement in a primary metric does not pass if a guardrail materially regresses or semantic correctness fails.

### Repetition and sample sufficiency

The campaign records:

- experimental unit and independent-replication unit;
- fixed independent trial count or valid sequential stopping design;
- within-trial observation count;
- expected variance source;
- minimum valid population for each statistic;
- tail-estimation sufficiency;
- treatment of autocorrelation and non-stationarity;
- whether inference is over events, time windows, trials, hosts, or workload instances.

Event-level samples from one run are not falsely treated as independent host repetitions.

For every confirmatory primary metric, the campaign preregisters:

- alpha or confidence level;
- minimum detectable regression/improvement, or equivalence/non-inferiority margin;
- target power under the declared effect and variance assumptions;
- sample-size derivation;
- hypothesis family and multiplicity/error-control policy;
- stopping rule and maximum sample size.

For characterization without a confirmatory hypothesis, the campaign preregisters a precision target, confidence level, and sample-size or sequential precision rule. If prior variance is insufficient to justify power or precision, a separate pilot is exploratory; its results may inform a later preregistered campaign but cannot be relabeled as that campaign's confirmation.

### Comparison methods

The implementation may use confidence intervals, bootstrap methods, nonparametric tests, regression models, change-point methods, or other justified techniques. The selected method must fit the metric distribution and experiment design.

Sequential methods must make repeated inspection part of the model and preserve the registered type-I error or coverage. Optional stopping with ordinary fixed-sample intervals or tests is invalid. Any adaptive sample-size rule records adaptation points, information measures, spending/coverage rule, maximum sample size, and termination outcomes.

At minimum, results report:

- absolute difference;
- relative difference where mathematically valid;
- uncertainty interval or equivalent evidence;
- baseline and candidate variation;
- sample and trial counts;
- registered threshold/margin;
- decision: pass, regress, improve, equivalent, inconclusive, or invalid.

### Paired and blocked designs

Paired input instances, interleaved run order, temporal blocks, or host-restart blocks are preferred when they reduce known noise without coupling candidate behavior to the baseline. The artifact records pairing and analyzes it accordingly.

### Multiple comparisons

When many metrics, workloads, variants, or commits are tested, the campaign defines:

- which metrics are confirmatory;
- which are exploratory;
- the confirmatory hypothesis family or families;
- family-wise error, false-discovery, hierarchical, gatekeeping, or other justified multiplicity policy;
- alpha allocation or confidence adjustment within each family;
- treatment of repeated workloads, variants, interim looks, and subgroup analyses;
- follow-up controlled confirmation.

Dashboard scanning alone cannot establish a release-blocking regression without a registered or reviewed confirmation path.

### Replication design

When a gate requires independent replication, the replication protocol is preregistered before replication data is observed. It records:

- the independent replication unit and what is intentionally held constant or varied;
- the effect or distributional quantity to reproduce;
- an equivalence interval, predictive interval, or other explicit replication-success criterion;
- alpha/confidence, target power, and multiplicity treatment;
- the expected probability that the replication will satisfy the criterion under the accepted baseline/effect model;
- the sample size and stopping rule;
- how heterogeneity across hosts, restarts, workload instances, or time blocks is modeled.

The expected replication probability must be high enough for the replication to be a meaningful gate under the accepted design; its required level is set with the phase's evidence, not invented in planning. A replication is evaluated against the registered quantitative criterion, not whether it happens to produce the same binary `pass` or `fail` label as the original campaign.

### Inconclusive evidence

A comparison is inconclusive when, for example:

- uncertainty overlaps both acceptable and unacceptable regions;
- sample size is insufficient for the requested tail;
- between-run variance dominates the expected effect;
- environment compatibility is conditional but unmodeled;
- workload representativeness changed;
- required counters or endpoints are incomplete but the run is otherwise valid.

Inconclusive is not pass. The owner may gather more evidence, improve experimental control, narrow the claim, or seek a temporary waiver.

### Baseline aging

Baselines are revalidated when:

- workload or scenario changes;
- environment or toolchain changes;
- architecture or algorithm changes materially;
- instrumentation profile changes;
- sustained production-like evidence shows the reference workload is no longer representative;
- the owning phase reaches its scheduled review.

A rolling baseline must retain fixed anchor releases or equivalent historical references so gradual regressions do not disappear through repeated rebasing.

## CI and controlled-host benchmarks

### Shared CI

Shared CI is required for:

- benchmark compilation and harness tests;
- semantic fixtures and deterministic checksums;
- schema/result validation;
- smoke microbenchmarks;
- gross latency, throughput, allocation, or artifact-size regressions using broad empirically justified thresholds;
- detection of missing latency points, counters, profiles, or evidence;
- trend collection labeled with its noisy environment class.

Shared CI results may block obvious failures, but passing shared CI does not establish a controlled performance SLO or capacity envelope.

### Controlled reference host

Controlled-host campaigns are required for:

- accepting or refreshing baselines;
- setting or changing regression thresholds;
- accepting instrumentation profiles and overhead envelopes;
- deriving stage budgets or SLOs;
- capacity and saturation decisions;
- release-significant regressions near thresholds;
- profiling that depends on stable hardware counters;
- production-like soak and recovery acceptance.

The host may be local. It must be reserved, fingerprinted, monitored for drift, and governed as test infrastructure.

### Trigger model

Performance work uses layered cadence:

- per-change correctness and smoke gates;
- affected-path CI comparisons;
- scheduled controlled regression campaigns;
- release-candidate capacity/recovery campaigns;
- milestone and architecture-change profiling;
- periodic soak and representative-workload review.

The exact cadence is recorded by each phase based on change rate, risk, and runtime cost.

### Suspected CI regression

A noisy-CI regression produces:

1. artifact validation and semantic check;
2. rerun under the same CI class if permitted by the registered rule;
3. controlled-host confirmation for release-significant or persistent findings;
4. profile/counter investigation if confirmed;
5. accepted fix, rollback, or governed waiver.

Repeated reruns are not used selectively until one passes. All attempts remain linked.

## Profiling and counters

### Profile-first rule

Optimization starts from a representative, valid workload and measured profile. A code change justified only by intuition must still produce before/after evidence.

### Profile types

Applicable profile evidence may include:

- sampled CPU profiles;
- instrumented call or span profiles;
- allocation and retention profiles;
- lock/contention profiles;
- scheduler and off-CPU profiles;
- storage and network traces;
- queue wait/service profiles;
- hardware performance counters;
- compiler/runtime optimization diagnostics;
- flame graphs or equivalent visual summaries.

No one profiler is authoritative. Profiling overhead and sampling bias are declared.

### Counter governance

Hardware and operating-system counters record:

- counter name and semantic definition;
- scope and multiplexing;
- availability and permissions;
- sampling interval;
- raw/scaled status;
- wrap/reset behavior;
- architecture dependence;
- known error or unsupported state.

Comparisons using different counter availability or scaling are marked incompatible or conditional. Missing counters do not invalidate unrelated measurements unless the campaign requires them to explain or gate the result.

### Profile interpretation

The investigation links:

- hot functions or blocked paths;
- canonical pipeline stages;
- input/workload dimensions;
- resource saturation symptoms;
- queue and latency evidence;
- candidate code or configuration changes.

A profile proves where observed samples accumulated under that campaign. It does not by itself prove causation; controlled experiments or additional counters confirm material hypotheses.

### Optimization acceptance

An optimization is accepted when:

- semantic and failure behavior remains valid;
- the registered primary metric improves or required regression is removed;
- no guardrail is materially worsened;
- the result is reproducible under the appropriate environment;
- complexity, portability, and maintainability costs are documented;
- any workload-specific tradeoff is explicit;
- evidence and rollback path are retained.

## Noise and invalid-run rules

### Noise sources

The harness observes applicable:

- background processes and interrupts;
- scheduler migration and contention;
- power/frequency/thermal changes;
- virtualization or host steal;
- memory pressure, paging, and cache state;
- storage cache, compaction, or free-space variation;
- network variability;
- runtime compilation, garbage collection, or allocator drift;
- collector/exporter backlog;
- clock reset, suspend, or uncertainty change;
- workload generator starvation;
- profile/counter multiplexing;
- process restart or instrumentation-profile transition.

### Exogenous violations versus SUT-induced pressure

Run validity distinguishes two classes:

1. **Independently evidenced exogenous harness/environment violation:** an occurrence outside the declared SUT/load interaction that breaks the registered experiment, such as an unrelated process consuming reserved resources, host-management action, ambient thermal condition present independently of SUT load, external clock failure, generator defect on independently provisioned capacity, undeclared environment mutation, or harness corruption. These may invalidate the affected claim when the evidence shows the violation was not caused by SUT behavior.
2. **SUT-induced or workload-induced pressure:** thermal throttling caused by SUT CPU demand, memory pressure or paging caused by SUT allocation/residency, scheduler contention caused by SUT threads/processes, configured resource-limit exhaustion, storage/network pressure, collector contention, queue growth, or generator interference caused by colocating the generator with a saturated SUT. These observations are valid saturation/overload evidence and cannot be discarded as environmental noise.

The classification requires evidence from environment observations, process/resource attribution, counters, and campaign topology. Ambiguous causation does not justify invalidation; the run remains valid overload evidence, while narrower steady-state or exact-arrival claims may be inconclusive or invalid for their specific measurement contract.

SUT-induced generator lag is handled by the coordinated-omission policy: it remains material capacity evidence, and any arrival-driven latency claim must use the preregistered correction or be marked invalid. It does not erase the fact that the configured topology saturated.

### Invalid-run conditions

A run is invalid for its registered claim if any required condition occurs, including:

- manifest, checksum, or semantic-oracle failure;
- undeclared code, configuration, workload, environment, or profile change;
- instrumentation profile transition inside a fixed comparison interval;
- incompatible or invalid clock for the claimed duration;
- missing required endpoints, counters, intervals, reset identity, or loss accounting;
- independently evidenced exogenous load-generator or harness failure that prevents the declared offered load and is not caused by SUT/resource interaction;
- independently evidenced exogenous process restart, host suspend, environment mutation, ambient thermal/power-policy violation, or resource-limit change;
- independently evidenced unrelated competing load beyond the campaign's registered tolerance;
- corrupted or incomplete result artifact;
- benchmark harness failure;
- unauthorized telemetry shedding or authoritative loss;
- manual intervention during the measured interval;
- failure to stabilize for a steady-state claim.

The artifact preserves invalid runs with reason; it does not merge them into valid summaries.

Thermal throttling, resource-limit exhaustion, memory pressure, scheduler contention, storage/network pressure, or failure to stabilize that arises from the SUT under the declared workload is not globally invalidated. It fails the steady-state guardrail where applicable and remains in saturation, overload, recovery, and capacity-boundary analysis.

### Valid but noisy runs

A run may remain valid but produce wide uncertainty. The statistical rule then returns inconclusive or lowers claim precision. Noise is not removed by arbitrary trimming.

### Retry policy

Retries follow a registered policy. Each attempt has a unique identity and links to the original campaign. The final decision considers all valid attempts required by the policy. Selective omission of slower valid runs is prohibited.

### Exploratory evidence

Developers may run unregistered experiments for diagnosis. Exploratory results are labeled and cannot satisfy a phase or release gate until reproduced under a registered campaign.

## Capacity, soak, and failure-injection cadence

### Capacity cadence

Capacity characterization runs:

- when a phase first activates a performance-sensitive path;
- after material algorithm, concurrency, queue, persistence, runtime, or topology changes;
- before adopting or changing a capacity envelope;
- before a production-like milestone;
- periodically while representative live-paper workloads evolve.

### Soak cadence

Soak tests run:

- before closing phases that introduce persistent processes, queues, stores, adapters, or long-lived state;
- before production-like live-paper operation;
- after changes affecting allocation, retention, recovery, telemetry storage, or reconnection;
- on a scheduled basis for supported operational profiles.

Each soak has a phenomenon-based duration and acceptance rule, including trend analysis rather than only terminal values.

### Failure-injection cadence

Failure injection runs:

- in deterministic fixtures for each relevant change;
- in affected integration pipelines;
- in scheduled production-like campaigns;
- before release of a new recovery, durability, execution, or control boundary;
- after changes to queue capacity, overload, persistence, or telemetry isolation.

Faults include only capabilities introduced by the owning phase. Future live-order failures are not simulated as Phase 02 implementation evidence, but the method and artifact schemas must admit them.

### Combined campaigns

At milestone gates, capacity, soak, and failure injection are combined where operationally meaningful:

- burst during sustained load;
- collector or exporter loss during normal domain work;
- process restart with a backlog;
- persistence slowdown followed by recovery;
- reconnect/gap recovery during paced input;
- later, control/kill-switch activation under saturation.

Combined tests do not replace isolated tests; they reveal interaction effects.

## Results artifact schema

Every benchmark campaign emits a machine-readable immutable result bundle.

### Campaign manifest

- campaign identity and schema version;
- purpose and registered decision;
- owner, reviewer, creation time, and status;
- baseline and candidate identities;
- workload specification and instance identities;
- reference environment and compatibility result;
- build, runtime, dependency, and configuration identities;
- instrumentation profiles and epochs;
- primary, guardrail, and exploratory metrics;
- experimental unit and independent-replication unit;
- alpha/confidence level, minimum detectable effect or precision target, target power, and sample-size derivation;
- thresholds, equivalence margins, statistical method, fixed or valid sequential stopping rule, and maximum sample size;
- confirmatory hypothesis families and multiplicity/error-control policy;
- expected replication probability and preregistered replication/equivalence criterion where applicable;
- run order, blocking, pairing, and randomization seed;
- invalid-run and retry policy;
- coordinated-omission policy, generator-lag tolerance, and correction/invalidity rule;
- expected artifacts.

### Per-run record

- benchmark run and trial identities;
- exact resolved manifest references;
- process/runtime/clock-domain identities;
- setup, warmup, stabilization, measured, cooldown, and teardown intervals;
- environment start/end observations and drift;
- offered, accepted, completed, rejected, shed, expired, failed, and retried work;
- scheduled arrival, generator emission, SUT ingress, completion, generator-lag, and ingress-lag evidence where paced arrivals apply;
- latency distributions with point/segment/population/comparability metadata;
- queue, resource, storage, network, allocation, scheduler, and counter observations;
- telemetry loss, profile epoch, aggregation reset, sampling, and cardinality status;
- semantic outputs, checksums, invariant results, and expected failure outcomes;
- fault injections and recovery transitions;
- raw-data or content-addressed sample references;
- profile/counter artifact references;
- validity classification and reasons;
- exogenous-violation versus SUT-induced-pressure classification with supporting evidence;
- logs needed to reproduce harness failures, subject to redaction.

### Analysis record

- included and excluded runs with reasons;
- data transformations and analysis-tool version;
- baseline and candidate summaries;
- absolute/relative effects and uncertainty;
- alpha/confidence, achieved precision or power diagnostics, experimental-unit accounting, and multiplicity adjustment;
- tail sample sufficiency;
- coordinated-omission raw/corrected/invalidity disposition and scheduled-arrival accounting;
- capacity/saturation boundary evidence;
- guardrail results;
- statistical decision;
- replication/equivalence result against its preregistered criterion and expected replication probability where applicable;
- sensitivity or robustness checks where applicable;
- known limitations;
- reviewer disposition.

### Decision record

- pass, regress, improve, equivalent, inconclusive, invalid, or waived;
- adopted baseline, capacity envelope, budget, SLO, profile, or no-change result;
- affected phases and contracts;
- remediation, rollback, or follow-up;
- waiver identity if used;
- approval identities and time;
- artifact checksums and retention class.

### Reproducibility

The bundle contains or references enough retained material to:

- reconstruct the workload instance;
- restore or identify the environment and build;
- rerun the benchmark protocol;
- deterministically reanalyze retained observations and reproduce canonical transformed data, summaries, uncertainty calculations, multiplicity adjustments, and decision outputs;
- verify semantic checksums;
- confirm the decision rule was not changed post hoc.

Reanalysis and replication are separate gates:

- **Deterministic reanalysis** uses the same immutable observations, method version, and manifest. It must reproduce the canonical analysis outputs exactly, except for explicitly normalized representation details whose equivalence is machine-verified.
- **Independent replication** generates new observations under the preregistered replication design. It passes only by satisfying the registered equivalence/predictive criterion with the declared alpha/confidence, power, multiplicity, and stopping policy.

Replication is not required to return the same binary statistical disposition by chance. Before data collection, the campaign reports the expected probability of satisfying the replication criterion under its accepted model. A replication design with inadequate expected success probability is not valid gate evidence; it must increase information, revise the claim prospectively, or remain exploratory.

Sensitive machine and configuration data is redacted canonically without removing decision-relevant environment facts.

## Waiver and exception governance

A waiver never changes a baseline, SLO, stage budget, or regression threshold. It temporarily permits a named deviation.

Every waiver includes:

- unique identity;
- owner and independent approver where practical;
- affected phase, workload, environment, profile, metric, and release scope;
- observed result and breached rule;
- evidence-backed cause or current uncertainty;
- correctness, safety, audit, security, and overload impact;
- temporary replacement constraint or containment;
- issue/remediation plan and rollback condition;
- creation, review, and non-renewing expiry;
- explicit prohibited expansion of scope;
- linked result artifacts.

Waivers cannot permit:

- semantic mismatch;
- hidden authoritative loss;
- execution beyond risk or control authority;
- invalid clock claims;
- unbounded queues or resource use;
- telemetry changing domain outcomes;
- secret leakage or disabled required audit evidence;
- fabricated or post-hoc altered benchmark evidence.

Renewal requires new evidence and a new approval decision. Repeated renewal is treated as an unmet engineering requirement and escalated at the cumulative phase review.

Expired, missing, unowned, or broader-than-evidenced waivers fail the gate.

## Phase-by-phase budget adoption

Phase 02 defines the method and accepts the first observability-profile overhead envelope. Later phases activate only the workloads, metrics, budgets, and SLOs supported by capabilities they introduce.

| Phase | Required performance adoption |
|---|---|
| **01 — Architecture** | Defines canonical latency segments, correctness-first rules, workload/environment hierarchy, and evidence obligations. No numeric performance budget. |
| **02 — Observability** | Implements benchmark/result schemas, clock-valid measurement, reference workload/environment method, telemetry overhead A/B, CI/reference-host split, an accepted baseline instrumentation profile with empirical envelope and thresholds, and a representative synthetic kill-switch command-to-fence harness with accepted empirical segment budgets under reference load. |
| **03 — Event infrastructure** | Adds capture, normalization, dispatch, persistence, snapshot/recovery, bounded-queue, burst, overload, durability, and reconstruction workloads; adopts budgets for implemented segments and recoverability paths. |
| **04 — Market data** | Adds representative depth/churn/stream workloads, state-apply and feature-input capacity, gap/reconnect recovery, stale-state deadlines, and market-state memory/residency budgets. |
| **05 — Strategy runtime** | Adds strategy complexity classes, evaluation cadence, feature/strategy latency, sandbox/resource limits, deadline/abstention behavior, and multi-strategy saturation. |
| **06 — Research/backtesting** | Adds replay throughput, experiment concurrency, dataset I/O, reproducibility, look-ahead guardrails, result-generation capacity, and cost/latency-model sensitivity workloads. |
| **07 — Portfolio/risk** | Replaces synthetic kill-switch authorities with real risk/reservation owners, adopts or tightens the Phase 02 budgets, and adds recommendation-to-risk, serialized exposure/reservation, concurrent-target, stale-input, and overload/fail-closed budgets and breach behavior. |
| **08 — Paper execution** | Adds intent, acknowledgement, fill, accounting, reconciliation, paper-model latency, ledger throughput, restart/rebuild, partial-fill, and queue-race workloads. |
| **09 — Operations console** | Adds query/read-model freshness, command responsiveness, evidence exploration, local UI/API load, operator workflow, and degraded-dependency behavior without making UI latency part of domain authority. |
| **10 — Live paper** | Validates production-like local topology, live capture pacing, sustained capacity, reconnect/recovery, operational SLOs, alert detection, runbook cadence, and empirically supported resource headroom. Multi-venue/L3 activation extends the workload dimensions rather than redefining them. |
| **11 — External discovery and live execution extensions** | External/Polymarket discovery adds source-quality, isolation, ranking, and opportunity workloads. Human-approved live execution adds credential-safe adapter, approval, send/ack/fill, ambiguous outcome, reconciliation, control-fence, and bounded-capital SLO evidence. Optional automation requires separate stricter evidence. |

### Adoption rules

At each phase:

1. Activate the phase's workload dimensions and telemetry schemas.
2. Validate semantic fixtures before performance measurement.
3. Accept or revise reference workloads and environment compatibility.
4. Establish baseline, saturation, overload, and recovery evidence.
5. Derive product SLOs only where product validity and operational populations exist.
6. Allocate stage budgets only after end-to-end evidence.
7. Accept regression thresholds and capacity triggers.
8. Re-measure telemetry overhead for affected paths.
9. Re-run affected earlier reference workloads and cumulative compatibility checks.
10. Record any superseded baseline, budget, or SLO without rewriting history.

## Security and integrity of performance evidence

Performance tooling must not become an alternate control or data-exfiltration path.

- Benchmark execution is authorized by environment and workload scope.
- Results and profiles follow data classification and redaction policy.
- Secret values never enter manifests, command captures, profiles, stack dumps, or exports.
- Diagnostic instrumentation activation follows telemetry profile authorization and epoch rules.
- Load generators cannot issue undeclared live orders or bypass mode boundaries.
- Fault injection is unavailable in production-like scopes unless explicitly authorized and contained.
- Result artifacts are checksummed or content-addressed.
- Baseline and decision mutations are attributable and append-only.
- Export destinations follow the observability allowlist; local evidence remains sufficient for operation and review.

## Review and change control

A performance-method change requires cumulative review when it changes:

- workload meaning or reference population;
- canonical latency endpoint use;
- clock comparability;
- result validity or invalid-run rules;
- statistical decision semantics;
- baseline refresh or regression policy;
- queue/capacity derivation;
- SLO population or exclusions;
- profile-overhead comparison;
- waiver authority;
- evidence retention or reproducibility.

Changes are versioned. Existing evidence remains interpreted under the method version that produced it. Migration studies are required before applying a new method retrospectively.

## Implementation evidence and exit gates

Planning approval fixes the contracts below. Artifacts become mandatory during Phase 02 implementation or the named activating phase. No future capability is required to exist merely to approve this planning leaf.

| Evidence ID and artifact | Owning phase | Required contents | Pass condition |
|---|---|---|---|
| **PS-E01 — Performance semantic registry** | 02 | Versioned definitions for scenario, workload, environment, benchmark run, latency population, throughput, saturation, capacity, SLO, budget, regression, invalid, inconclusive, and waiver | Terms validate and do not redefine Phase 01 or telemetry contracts |
| **PS-E02 — Workload manifest and lifecycle suite** | 02; extend every phase | Schema, generated/captured fixture, semantic oracle, versioning, compatibility, acceptance/supersession/retirement workflow | Workload instances are reproducible; material changes require a new version; incompatible versions cannot be silently compared |
| **PS-E03 — Reference environment fingerprint** | 02 | Canonical redacted host/runtime/build/profile manifest, start/end observations, compatibility rules, drift detection, unknown-field handling | Controlled comparisons reject incompatible/unknown required state; no premature hardware promise is embedded |
| **PS-E04 — Benchmark harness conformance** | 02 | Preflight, warmup, stabilization, measurement, cooldown, scheduled/emitted/ingress arrival capture, generator-lag accounting, artifact generation, profile-epoch enforcement, clock validation, harness-overhead checks | Harness failures and missing evidence invalidate affected claims; setup/warmup are not mislabeled as steady state; paced workloads cannot hide delayed/missed arrivals; artifacts reproduce |
| **PS-E05 — Latency distribution fixture** | 02 | Canonical points, causal matching, outcome populations, queue/service split, tails, missing endpoints, outliers, external-clock cases, end-to-end critical path | No percentile addition, unsupported clock arithmetic, post-hoc outlier removal, or insufficient-tail overclaim |
| **PS-E06 — Throughput/saturation and coordinated-omission campaign** | 02 harness baseline; extend performance-sensitive phases | Offered/accepted/completed/rejected work, scheduled and actual arrivals, generator lag, preregistered open-loop tail run or correction/invalidity policy, stepped/ramped load, resource/queue guardrails, recovery and hysteresis | Sustainable capacity is distinguished from peak throughput; delayed/missed arrivals remain counted; closed-loop evidence cannot conceal tail collapse; first breach and overload behavior are visible |
| **PS-E07 — Queue-capacity and overload proof** | 03 and each queue-introducing phase | Queue inventory, service/arrival/burst evidence, memory/age derivation, admission/overflow/stale/recovery policy | Every implemented queue is bounded and justified; silent overwrite/unbounded growth/stale-success behavior fails |
| **PS-E08 — Telemetry overhead A/B and accepted profile** | 02 | Paired/blocked semantic-minimal, baseline, benchmark, and applicable diagnostic runs; semantic checksums; resource/latency distributions; accepted envelope and thresholds | Domain outputs are identical; overhead is quantified and an operating profile is explicitly accepted or rejected |
| **PS-E09 — Preregistered statistical decision suite** | 02 | Experimental unit, alpha/confidence, minimum detectable effect or precision target, target power and sample derivation, fixed or valid sequential stopping method, primary/guardrail metrics, thresholds/equivalence margins, pairing, hypothesis families, multiplicity policy, invalid/inconclusive fixtures | Optional stopping and post-hoc thresholds/families cannot pass; experimental units are not pseudoreplicated; noisy or underpowered evidence returns inconclusive; all valid attempts remain linked |
| **PS-E10 — CI/reference-host policy proof** | 02 | Shared-CI smoke/gross-regression jobs, controlled-host workflow, confirmation path, artifact retention | CI is not sole performance truth; release-significant decisions use compatible controlled evidence |
| **PS-E11 — Profiling and counter evidence** | 02 harness; extend affected phases | Profile types, overhead, counter semantics, availability/scaling, workload linkage, before/after investigation record | Optimizations are evidence-led; unsupported/multiplexed counters are not presented as precise comparable truth |
| **PS-E12 — Noise, causation, and invalid-run suite** | 02 | Independently evidenced exogenous clock/profile/environment/harness/load-generator failures; SUT-induced thermal, memory, scheduler, storage, resource-limit, and generator-pressure fixtures; stabilization failure; retry and exploratory-label fixtures | Exogenous protocol violations invalidate only affected claims; SUT-induced pressure remains valid saturation/overload evidence; ambiguous causation is not used to discard slow runs; retries are not cherry-picked |
| **PS-E13 — Capacity/soak/failure cadence register** | 02; activate by phase | Trigger matrix, owning phase, workload, duration rationale, faults, acceptance and rerun rules | Every activated long-lived/performance-sensitive path has risk-proportionate scheduled evidence |
| **PS-E14 — Result reanalysis and replication drill** | 02 | Campaign/per-run/analysis/decision schemas, raw references, checksums, redaction, deterministic reanalysis fixture, preregistered independent replication/equivalence criterion, expected replication probability, alpha/confidence, power, multiplicity, and stopping method | Reanalysis of immutable observations reproduces canonical outputs exactly; new-data replication is judged by its preregistered quantitative criterion, not same binary disposition; underpowered or low-probability replication designs cannot satisfy the gate |
| **PS-E15 — Budget and SLO derivation record** | Each phase adopting numeric objectives | Scenario, validity deadline, baseline, capacity, population, window, uncertainty, stage allocation, overload response, owner, review trigger | Values are evidence-derived, scoped, and approved; baseline capability alone is not treated as product requirement |
| **PS-E16 — Waiver governance suite** | 02; cumulative | Valid/invalid waiver fixtures, non-renewing expiry, containment, prohibited safety/correctness weakening, linked evidence | Expired/unowned/broadened or safety-weakening waivers fail; waiver does not mutate the target |
| **PS-E17 — Cumulative performance compatibility review** | Every phase gate | New and affected prior workloads, baselines, profiles, budgets, SLOs, environments, queues, results, exceptions, and architecture contracts | No semantic redefinition, orphaned objective, stale baseline, unsupported claim, or unresolved material finding |

## Phase 02 implementation closure

The performance-method portion of Phase 02 is implemented only when:

- workload and environment manifests validate and reproduce;
- the benchmark harness proves lifecycle-exact, clock-valid measurement;
- confirmatory campaigns preregister experimental units, alpha/confidence, effect or precision targets, power, hypothesis families, multiplicity policy, and fixed or valid sequential stopping;
- warmup, stabilization, noise, invalid-run, and retry behavior are tested;
- externally paced campaigns prove scheduled-arrival accounting and coordinated-omission protection through open-loop evidence or the preregistered correction/invalidity path;
- exogenous protocol violations are evidenced separately from SUT-induced pressure, which remains in saturation/overload evidence;
- latency, throughput, saturation, queue, and recovery artifacts distinguish their populations correctly;
- semantic checksums remain stable across accepted instrumentation profiles;
- one baseline observability profile has a measured and approved overhead/resource envelope;
- regression thresholds and statistical decision rules are accepted for the Phase 02 reference workload/environment;
- shared CI and controlled-host responsibilities are operational;
- result bundles support deterministic reanalysis and preregistered independent replication with an expected replication probability and quantitative equivalence/predictive criterion;
- waiver rules are enforceable;
- all applicable `PS-E01` through `PS-E17`, Phase 01 evidence obligations, and `OT-E12` pass;
- independent critique and cumulative Phase 01/02 review find no unresolved material issue.

Phase 02 does not fail because later market, strategy, risk, paper, live, multi-venue, external-discovery, or execution workloads do not yet exist. It fails if the method cannot activate those capabilities without changing established measurement meaning, if Phase 02 overhead remains unbounded, or if accepted claims are not reproducible.

## Deferred implementation choices

The following remain evidence-driven implementation decisions:

1. Benchmark framework and load-generator products.
2. Statistical and visualization libraries.
3. Profile and hardware-counter tools.
4. Raw sample format, histogram representation, and compression.
5. Controlled-host hardware and operating system.
6. Affinity, scheduler, power, and cache-reset techniques supported by that host.
7. Exact warmup, stabilization, repetition, and stopping values per workload.
8. Exact latency percentiles and deadline thresholds.
9. Numeric telemetry-overhead and resource envelopes.
10. Queue capacities and overload trigger values.
11. Soak durations and fault cadences.
12. Retention duration for raw benchmark samples and profiles.
13. SLO and error-budget values for later operational capabilities.

Each choice must remain behind the versioned manifests and evidence contracts defined here. Selecting a tool or value may refine the method but may not bypass correctness, clock, workload, environment, statistical, or governance requirements.

## Cumulative review checklist

At every later phase gate:

1. Confirm new workload terms and metric populations preserve Phase 01 semantics.
2. Confirm latency points use the canonical telemetry registry and exact lifecycle boundaries.
3. Add or revise reference workloads only through lifecycle/version review.
4. Revalidate environment compatibility and refresh baselines when required.
5. Re-run affected semantic, overhead, saturation, noise, and invalid-run suites.
6. Confirm new queues are bounded and capacity-derived.
7. Confirm stage budgets follow measured end-to-end behavior and product validity deadlines.
8. Confirm SLO populations, exclusions, and breach responses are explicit.
9. Confirm shared CI is not being used as sole acceptance evidence.
10. Confirm profiles/counters explain claimed bottlenecks and do not overstate unsupported measurements.
11. Review capacity, soak, and failure-injection cadence for the newly activated risks.
12. Confirm result bundles remain reproducible and retained.
13. Review every active waiver and reject silent expiry or repeated unsupported renewal.
14. Re-run earlier evidence affected by implementation, workload, environment, or profile changes.
15. Record unresolved uncertainty as inconclusive work, not an approved claim.

Approval of this document fixes the performance-engineering method. Later phases may add workloads, metrics, stricter gates, budgets, and SLOs, but they may not redefine correctness as optional, use incomparable measurements as precise evidence, replace sustainable capacity with peak throughput, or convert noisy/incomplete results into a pass.
