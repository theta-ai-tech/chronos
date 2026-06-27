# Phase 06: Datasets and Experiments

## Purpose

This document defines the planning contract for Chronos's authoritative dataset catalog, replay-class manifests, experiment registry, immutable result bundles, and reproducibility controls.

This leaf builds on the approved Phase 01 architecture and domain model, Phase 02 observability and performance method, Phase 03 event/persistence/replay/recovery contracts, Phase 04 market-data contracts, and Phase 05 feature, strategy, and recommendation runtime contracts. It specializes the Phase 01 dataset/replay authority and research/backtest/experiment registry without redefining run-input ordering, replay classes, feature semantics, strategy evaluation, recommendation cardinality, telemetry endpoints, recovery states, or market-state ownership.

Research and backtesting must invoke the same production domain path used by live and replay runs. This leaf must not create a second market-state engine, feature engine, strategy engine, recommendation engine, risk engine, paper broker, accounting engine, or ad hoc notebook truth source.

The companion Phase 06 backtesting-methodology leaf owns statistical methodology, look-ahead tests, validation workflow, parameter-search rules, acceptance thresholds, and strategy-promotion decision process. This leaf provides the identity, lineage, manifest, assumption, result-bundle, and reproducibility substrate that the companion methodology leaf must consume.

## Objectives

The dataset and experiment contracts must establish that Chronos can:

1. catalog raw/source-capture, normalized-fact, raw re-normalization, and corrected research datasets without conflating their replay classes;
2. preserve immutable lineage from source capture through normalization, correction, replay input selection, and experiment result bundles;
3. pin all versions needed to reproduce data interpretation, feature computation, strategy evaluation, and recommendation construction;
4. define replay-class manifests that make faithful, normalized-fact, raw re-normalization, and corrected event-time research runs distinguishable and auditable;
5. register experiments as immutable specifications with explicit inputs, versions, assumptions, random seeds, comparison groups, and analysis boundaries;
6. bind strategy, feature, recommendation, arithmetic, timer, telemetry, and runtime-profile versions to each experiment;
7. register research assumption sets as versioned records, including cost, slippage, latency, horizon/holding-window, benchmark-construction, unavailable-label, and stochastic-seed assumptions, without pretending they are portfolio/risk/paper-execution truth before later phases introduce those authorities;
8. produce immutable result bundles with semantic checksums, artifact manifests, logs, metrics, and domain-output references;
9. allow notebooks, scripts, and ad hoc analysis to consume registered results without becoming authoritative;
10. prove repeated-run reproducibility under the declared identity policy and replay class;
11. enforce access, redaction, retention, and export rules for sensitive captures, proprietary data, strategy code, and result artifacts;
12. expose observability and performance evidence for dataset reconstruction, replay feeding, experiment execution, and result materialization.

## Scope

### In scope

- dataset catalog records, dataset identities, manifests, partitions, snapshots, lineage, integrity, retention, and access metadata;
- source-capture/raw datasets, accepted normalized-fact datasets, raw re-normalization output datasets, and corrected event-time research datasets;
- replay-class manifests and constraints that preserve the approved Phase 01 and Phase 03 replay-class semantics;
- dataset correction policies, correction provenance, and corrected-dataset caveats;
- experiment specification identity, lifecycle, parent/comparison relationships, version binding, and result references;
- feature, strategy, recommendation, arithmetic, runtime, timer, telemetry, and static/run configuration version binding;
- assumption-set, cost, slippage, latency, horizon/holding-window, benchmark-construction, unavailable-label-policy, and stochastic-seed records as immutable research assumptions;
- deterministic seed, pseudorandom algorithm, draw-order, shrink, and generated-case retention records;
- comparison-group identity and controlled-variable declarations;
- notebook, report, export, and ad hoc analysis boundaries;
- immutable result bundles, result semantic checksums, artifact manifests, reproducibility attestations, and invalidation/supersession;
- access control, redaction, provenance, retention, legal/licensing metadata, and evidence pinning for datasets and results;
- observability, resource budgets, and performance evidence for dataset and experiment infrastructure;
- evidence gates for implementation-time conformance.

### Out of scope

- statistical hypothesis design, power analysis, multiple-comparison policy, walk-forward validation, train/test splitting methodology, and parameter-search rules;
- deciding whether a strategy is profitable, acceptable, promoted, retired, or capital-eligible;
- portfolio construction, risk approvals, reservations, execution, simulated fills, ledger, P&L, reconciliation, or accounting correctness;
- concrete cost/slippage/fill simulation models beyond registering assumption records;
- external-signal and Polymarket discovery semantics beyond preserving extension seams and version references;
- exact storage engine, file format, database, object store, parquet schema, column layout, compression codec, or compute framework;
- UI design for experiment comparison or reporting;
- ticket-level implementation decomposition and estimates.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Dataset/replay authority | Dataset catalog, dataset manifests, replay-class manifests, source/normalized/corrected dataset identity, lineage, integrity, reconstruction, retention, and faithful/non-faithful classification | Run lifecycle, run initialization manifest, strategy semantics, experiment acceptance decisions |
| Research/backtest/experiment registry | Experiment specifications, immutable experiment identity, comparison groups, assumption bindings, result-bundle references, lifecycle, invalidation, supersession, and review metadata | Dataset content, replay reconstruction, production domain outputs, notebook conclusions |
| Run/configuration authority | Run initialization manifest, mode, replay class/live mode selection, component versions, configuration epochs, and terminal attestation | Dataset validity, experiment identity, comparison validity |
| Stream/run-input authority | Merge-policy version, run-input selection, control effective positions, timer-stream sequence, and ordered dispatch facts | Dataset cataloging, experiment result interpretation |
| Market-state authority | State views, bundles, state lineage, state checkpoints, and state semantic checksums | Dataset correction, experiment comparison, strategy decisions |
| Feature authority | Feature definitions, implementations, observations, diagnostics, unavailable outcomes, feature windows, checkpoints, and feature semantic checksums | Research result identity, experiment acceptance criteria |
| Strategy runtime | Strategy definitions, implementations, evaluations, signals, abstentions, and signal explanations | Experiment comparison, recommendation ownership, portfolio/risk decisions |
| Recommendation authority | Exactly one portfolio-neutral recommendation per valid signal, recommendation validity, explanation, and semantic checksum | Experiment acceptance, risk, execution, portfolio-specific target construction |
| Observability | Non-authoritative telemetry schemas, profiles, metrics, traces, logs, benchmark evidence, and export manifests | Proving domain validity, creating experiment results, correcting datasets |
| Query-model/reporting/notebook tools | Disposable analysis, visualizations, summaries, derived tables, and human-readable reports | Authoritative dataset, experiment, run, or result identity |

Dataset/replay and experiment-registry records may share storage initially, but their authority remains separate. A dataset proves what inputs are available and how they can be reconstructed. An experiment proves what was intentionally run and which results belong to that specification. Neither authority may silently reinterpret production domain outputs.

## Canonical concepts

### Dataset

A `Dataset` is an immutable, cataloged collection of input facts, derived facts, partitions, or reconstruction instructions that can be used to initialize or feed one or more runs under an explicit replay class.

Every dataset record contains at minimum:

- `dataset_id`;
- dataset type;
- semantic version or immutable content revision;
- owner and producer authority;
- creation reason and source lineage;
- permitted replay classes;
- content-addressed partition references;
- schema, envelope, registry, canonicalization, and checksum versions;
- time-span and stream/listing coverage;
- continuity, fidelity, and known-limitation summary;
- access classification, retention class, and license/provenance metadata;
- redaction state and secret-scan evidence;
- compatibility and deprecation status.

Dataset identity is not a filename, directory name, table name, notebook path, or dashboard label. Moving storage, changing compression, or repartitioning without semantic change may create a representation revision, but it does not create a new semantic dataset unless content, ordering, interpretation, fidelity, or lineage changes.

### Dataset catalog

The dataset catalog is the authoritative index of dataset identity, lineage, integrity, access, retention, and replay eligibility. It stores metadata and references to content-addressed artifacts. It does not duplicate every payload as catalog metadata and does not replace the source, normalized, run-input, or result records owned by their authorities.

The catalog must answer:

- what exact dataset exists;
- which replay classes it can support;
- which raw/source records, normalized facts, corrections, reference data, and control facts it depends on;
- whether gaps, late facts, corrected facts, quarantine, fidelity loss, or redaction exist;
- which versions are required to interpret it;
- whether it is eligible for faithful replay, normalized-fact replay, raw re-normalization, corrected research replay, or only non-domain analysis;
- which experiments and result bundles consumed it.

### Replay-class manifest

A `ReplayClassManifest` binds a run or experiment to exactly one approved replay class:

- faithful capture-order replay;
- normalized-fact replay;
- raw re-normalization replay;
- corrected event-time research replay.

The manifest names:

- replay class and replay-class contract version;
- dataset IDs and partition IDs;
- source, normalized, control, timer, reference, and correction streams included;
- selected merge-policy identity/version from the stream/run-input authority;
- initial cursors, terminal cursors, and continuity expectations;
- original or selected normalizer/reference/schema versions as required by the class;
- expected semantic checksum sets;
- late/gap/correction treatment;
- missing evidence and non-faithful caveats;
- run-input reconstruction procedure identity;
- validation artifacts and replay eligibility status.

No experiment, report, or notebook may call a run “faithful” unless the replay-class manifest satisfies the faithful capture-order replay requirements inherited from Phase 01 and Phase 03.

### Experiment

An `Experiment` is an immutable research/backtest specification registered before authoritative result acceptance. It describes the intended domain run(s), dataset(s), replay class, component versions, assumptions, randomization, comparison group, outputs, and review context.

An experiment is not a notebook, shell command, dashboard, branch name, or informal analysis. A notebook or script may create a proposed experiment specification, but the registry must validate and accept the specification before its results become authoritative.

### Result bundle

A `ResultBundle` is the immutable, content-addressed output package for one completed or terminally incomplete experiment run, comparison run, or result group. It references production-domain outputs rather than rewriting them.

It contains:

- experiment identity and version;
- run identities and terminal attestations;
- dataset and replay-class manifest references;
- component and assumption bindings;
- artifact manifest with checksums;
- semantic checksum summary;
- telemetry/profile/benchmark evidence references;
- status, caveats, invalidation, and reproducibility attestations.

## Dataset types

### Source-capture/raw dataset

A source-capture dataset contains immutable captured source envelopes and source-owned metadata. It may include raw venue payloads, source session metadata, adapter framing facts, receive timestamps, capture integrity evidence, gap/duplicate/out-of-order facts, quarantine facts, and original capture ordering metadata.

It must preserve:

- original payload bytes or a lossless canonical source representation;
- source stream identity, epoch, sequence, and source timestamps when available;
- Chronos capture accept and record lifecycle evidence;
- source adapter, parser envelope, framing, and integrity versions;
- gap, duplicate, malformed, quarantine, and loss records;
- redaction evidence if any payload field was removed or masked;
- license/provenance restrictions and redistribution status.

Source-capture datasets may support faithful capture-order replay only when the original control/timer/run-input evidence and pinned interpretation versions are available. A source dataset alone is not enough to prove faithful replay.

### Normalized-fact dataset

A normalized-fact dataset contains previously accepted normalized facts and required companion facts under their original stream, cursor, schema, canonicalization, reference, and correction policies.

It must preserve:

- normalized fact identities and semantic checksums;
- stream cursor, stream epoch, publication lifecycle, and consumer acknowledgement where relevant;
- source lineage references when retained;
- reference-data versions used during normalization;
- normalizer implementation and schema versions;
- correction, deduplication, gap, and late-event policies;
- accepted control and run-timer facts required to reconstruct run inputs;
- validation against its own manifest.

Normalized-fact replay does not rerun source decoding or normalization. It cannot prove parser correctness for the source payloads it bypasses.

### Raw re-normalization dataset

A raw re-normalization dataset is the newly produced normalized lineage obtained by decoding retained source capture through explicitly selected normalizer, reference, registry, and policy versions.

It must:

- create a new dataset identity;
- preserve parent source-capture dataset identity;
- record selected normalizer/reference/schema/correction versions;
- record comparison relationship to any prior normalized-fact dataset;
- preserve newly accepted normalized fact identities or deterministic derivation evidence;
- report semantic diffs against prior normalized outcomes when a comparison target is declared;
- never overwrite or masquerade as the original normalized dataset.

If raw re-normalization reproduces prior normalized values, it is still a new dataset/run unless the registered deterministic identity policy and identical versions prove the same identity is valid.

### Corrected event-time research dataset

A corrected event-time research dataset contains a separately versioned reconstruction of estimated market chronology or repaired input facts under an explicit correction policy.

It may include:

- event-time ordering or watermarking;
- backfilled or repaired gaps;
- duplicate suppression changes;
- late-event insertion into corrected chronology;
- source-quality filters;
- venue-specific repair rules;
- external reference corrections;
- manual correction proposals that passed review.

It must include:

- correction-policy identity/version;
- parent dataset identities;
- every added, removed, reordered, repaired, synthesized, or excluded fact;
- reason, actor/system, evidence, and review state for each material correction;
- corrected ordering and cursor model;
- fidelity caveat that it is not faithful capture-order replay;
- semantic checksum of the corrected dataset;
- comparison relationship to uncorrected parents.

Corrected research data is useful for research. It must not be used to claim Chronos would have made the same decision live unless a separate faithful replay proves that claim.

### Derived analysis dataset

A derived analysis dataset contains non-authoritative aggregations, extracts, denormalized tables, feature matrices, visualization-ready files, or notebook outputs.

It must be labeled as derived and non-authoritative unless it is accepted by the owning domain authority as a real domain fact. It cannot feed production-domain replay as if it were source capture, normalized facts, feature observations, strategy evaluations, or recommendations.

Derived datasets may support exploratory analysis and reporting. They are not proof of live/replay equivalence.

## Dataset lineage

### Required lineage graph

Every dataset must have a lineage graph whose nodes and edges are content-addressed and versioned.

Minimum node types:

- source-capture dataset;
- reference-data snapshot;
- normalizer/schema/canonicalization version;
- correction-policy record;
- normalized-fact dataset;
- corrected research dataset;
- replay-class manifest;
- run initialization manifest;
- run terminal attestation;
- experiment specification;
- result bundle;
- derived analysis artifact.

Minimum edge types:

- captured-from;
- normalized-by;
- corrected-from;
- re-normalized-from;
- filtered-from;
- partitioned-from;
- replayed-by;
- consumed-by-experiment;
- produced-result;
- exported-as;
- superseded-by;
- invalidated-by.

Lineage edges are append-only. If a relationship is later found wrong, the registry records an invalidation/correction edge; it does not edit history in place.

### Lineage completeness

For a dataset to be replay-eligible, lineage must prove:

- all required parent datasets exist and pass integrity checks;
- every semantic interpretation version is pinned;
- all required control and run-timer facts for the selected replay class are present;
- reference-data versions are selected by the policy declared in the parent contracts;
- partition coverage matches the manifest;
- gaps, corrections, quarantine, redaction, and fidelity limitations are explicit;
- content checksums match the catalog.

Incomplete lineage may still allow exploratory analysis, but it fails authoritative replay eligibility.

## Version pinning

### Component versions

An experiment manifest must pin every semantic component that can affect outputs.

For approved Phases 01-05, this includes:

- domain-model/schema registry versions;
- event envelope and fact taxonomy versions;
- replay-class contract version;
- merge-policy identity/version;
- source adapter and normalizer versions;
- reference-data and listing-definition versions;
- correction policy version;
- market-state implementation and state-policy versions;
- feature definitions, implementations, instances, parameters, arithmetic, schedules, windows, and resource profiles;
- strategy definitions, implementations, instances, parameters, schedules, arithmetic, explanation policies, and resource profiles;
- recommendation policy, implementation, explanation policy, sizing-basis semantics, and validity policy;
- run-control, run-timer, static configuration, run configuration, and effective-position policy versions;
- telemetry profile and benchmark/profile epochs where performance comparison is part of the result;
- deterministic identity and semantic-checksum algorithms.

Future phases extend this list with portfolio, risk, reservation, execution, paper broker, fill model, ledger, mark, reconciliation, external-observation, opportunity, and live-execution versions when those authorities are introduced.

### Version resolution

Version resolution must be explicit. An experiment cannot say “latest,” “current main,” “default config,” “whatever the notebook imported,” or “current reference data” as an authoritative binding.

Allowed references:

- immutable content hash;
- released semantic version plus locked dependency manifest;
- registry identity plus immutable revision;
- run manifest identity;
- dataset manifest identity;
- build artifact identity with dependency lock evidence.

If a version is missing or ambiguous, experiment admission fails. If a later version is used intentionally, it creates a new experiment or a child experiment with a declared comparison relationship.

## Experiment specification

### Minimum manifest

An `ExperimentManifest` contains at minimum:

- `experiment_id`;
- immutable experiment specification version;
- title, purpose, owner, and creation time;
- hypothesis or research question, if any;
- explicit statement of whether the experiment is exploratory, conformance, regression, performance, comparison, or promotion-supporting;
- dataset IDs, partitions, and replay-class manifests;
- run mode and permitted output boundary;
- component version bindings;
- static and run configuration identities;
- feature, strategy, and recommendation bindings;
- assumption-set identity and member assumption records, if used;
- deterministic seed records, if any seeded randomness is permitted;
- comparison-group definition and controlled-variable set;
- expected output artifact classes;
- observability/profile requirements;
- security/access classification;
- retention class;
- companion methodology reference, when statistical or promotion claims are made;
- preregistration status and mutation policy;
- admission validation result.

The manifest is immutable once admitted. Corrections create a new manifest revision or child experiment. Result bundles always reference the exact manifest revision used.

### Experiment lifecycle

An experiment has one of these registry states:

```text
Draft -> Admitted -> Running -> Completed
                  |       |          |
                  |       +--------> Failed
                  |       +--------> Aborted
                  +----------------> Rejected
Completed -> Superseded
Completed -> Invalidated
Failed    -> Superseded
```

- `Draft` is editable and non-authoritative.
- `Admitted` means the registry has validated identity, inputs, versions, assumptions, access, and replay eligibility.
- `Running` means one or more domain runs or analysis jobs are executing under the admitted manifest.
- `Completed` means the expected result bundle has been accepted and sealed.
- `Failed` means a required run, validation, or result-bundle acceptance failed.
- `Aborted` means explicit operator/system termination.
- `Rejected` means the draft could not be admitted.
- `Superseded` means a later experiment replaces its interpretation without deleting it.
- `Invalidated` means a discovered defect invalidates the result or comparison claim.

Lifecycle transitions are registry facts. They do not mutate datasets, run manifests, or domain outputs.

### Parent and comparison relationships

Experiment relationships must be explicit:

- parent/child rerun;
- version-upgrade comparison;
- dataset correction comparison;
- parameter sweep member;
- ablation member;
- control/treatment pair;
- benchmark repetition;
- regression baseline;
- promotion-candidate evidence group.

The relationship records:

- controlled variables;
- intended varying variables;
- inherited manifest fields;
- changed fields;
- comparability prerequisites;
- companion methodology reference when statistical interpretation is intended.

The registry may group experiments for comparison, but comparison validity is not inferred merely because outputs have similar columns or names.

## Strategy, feature, and recommendation binding

### Feature binding

Each experiment that runs features must bind:

- feature definition, implementation, and instance versions;
- feature parameters and units;
- dependency graph identity;
- schedule and timer policy;
- feature-window and checkpoint policies;
- arithmetic and canonicalization policies;
- resource profile;
- expected semantic checksum algorithm;
- conformance corpus identity where relevant.

Feature matrices exported for analysis must reference accepted feature observations or explicitly declare themselves derived/non-authoritative. A feature matrix cannot become a substitute upstream input for strategy replay unless the feature authority accepts it as authoritative under a future explicit contract.

### Strategy binding

Each experiment that runs strategy evaluation must bind:

- strategy definition, implementation, and instance versions;
- parameters, units, and schedule policy;
- required feature versions and compatibility;
- explanation policy;
- arithmetic/canonicalization policy;
- deterministic seeded-randomness policy, if permitted;
- resource profile and isolation profile;
- strategy conformance evidence identity.

A strategy binding cannot use notebook-local code, unpinned imports, environment variables, current wall time, current reference data, mutable global state, or network calls as semantic inputs.

### Recommendation binding

Each experiment that runs recommendations must bind:

- recommendation policy and implementation versions;
- hold/actionable rule version;
- strategy-indicative sizing-basis version;
- explanation-derivation policy;
- validity, expiration, and supersession policy;
- semantic checksum algorithm;
- conformance evidence identity.

The experiment registry must preserve the Phase 05 invariant that each valid signal has exactly one portfolio-neutral recommendation in complete strategy/recommendation runs. Result bundles that lack required recommendations are incomplete or non-faithful, not partial successes.

## Research assumption records

### Assumption set

An `AssumptionSetRecord` is the authoritative registry identity for research assumptions used by an experiment, backtest, comparison, or report. It binds zero or more immutable assumption records and states which assumptions are deliberately absent.

It may include:

- cost assumptions;
- slippage assumptions;
- latency assumptions;
- horizon or holding-window assumptions;
- benchmark-construction assumptions;
- unavailable-label, censoring, stale/gapped, and exclusion policy assumptions;
- stochastic assumption seed and draw-order records.

The record contains:

- assumption-set identity/version;
- member assumption record identities and versions;
- scope and experiment/replay applicability;
- declared intended-variable status for comparison groups;
- compatibility relation to other assumption sets;
- reviewer/owner and admission status;
- semantic checksum.

Assumption sets are immutable once admitted. Changing any member, compatibility rule, or intended-variable status creates a new assumption set. Assumption sets are research inputs only; they do not create fills, ledger entries, risk approvals, live latency claims, or accounting facts.

### Cost assumptions

A `CostAssumptionRecord` is an immutable research assumption used by backtests, reports, or later paper-simulation components. It may describe:

- fee schedule;
- spread treatment;
- slippage approximation;
- funding/borrow/carry cost;
- conversion cost;
- minimum notional or quantity assumptions;
- liquidity constraint approximation;
- tax or venue charge placeholders, if explicitly research-only.

The record contains:

- assumption identity/version;
- scope: listing, venue, canonical instrument, strategy, experiment group, or global;
- effective interval or dataset interval;
- units and currency;
- source/provenance;
- calculation formula identity where formula exists;
- limitations and caveats;
- owner/reviewer;
- semantic checksum.

Before Phase 08 introduces paper execution/accounting, cost assumptions are research annotations and methodology inputs. They do not create fills, ledger entries, P&L, executable intents, or authoritative accounting facts.

### Slippage assumptions

A `SlippageAssumptionRecord` describes research-only treatment of spread crossing, queue position approximation, price impact placeholder, or fill-price proxy before Phase 08 introduces paper execution.

It records identity/version, scope, formula or table identity, units, provenance, limitations, stochastic seed policy where applicable, owner/reviewer, and semantic checksum.

### Latency assumptions

A `LatencyAssumptionRecord` is an immutable research assumption used to model or tag delay in replay/backtest analysis.

It may describe:

- assumed market-data delay;
- strategy/recommendation processing delay;
- decision delay;
- paper/live submission placeholder delay;
- venue acknowledgement or fill placeholder distribution;
- latency replay offset or perturbation;
- empirical distribution derived from Phase 02 performance evidence.

The record contains:

- assumption identity/version;
- source evidence or synthetic rationale;
- delay domain and lifecycle endpoints;
- units, distribution, quantiles, sample provenance, and uncertainty where applicable;
- replay-class applicability;
- deterministic seed/draw policy if stochastic;
- compatibility with Phase 02 latency endpoint definitions;
- caveats and owner/reviewer.

Latency assumptions may alter research scenarios only through declared experiment inputs. They cannot redefine Phase 02 telemetry endpoints or be reported as live observed latency.

### Horizon, benchmark, and unavailable-label assumptions

`HorizonAssumptionRecord` defines holding-window, evaluation-window, or outcome-observation timing used by research labels. It does not define a real position lifecycle.

`BenchmarkConstructionAssumptionRecord` defines no-action, naive, random matched-action, previous-version, ablation, or other benchmark construction rules, including matching variables and exclusion rules.

`UnavailableLabelPolicyRecord` defines how unavailable, stale, gapped, censored, halted, out-of-scope, or unresolved outcome labels are represented. It cannot silently drop observations unless the methodology manifest preregisters the exclusion.

Each record has an identity/version, scope, provenance, limitations, compatibility relation, owner/reviewer, and semantic checksum.

## Randomness and generated cases

### Seed records

Any permitted deterministic randomness must be bound by a `SeedRecord`:

- seed identity;
- pseudorandom algorithm and version;
- seed material;
- stream/substream allocation policy;
- draw-order contract;
- scope of use;
- generated-case manifest;
- shrink/minimization history for property tests;
- security classification if seed material could expose private logic or data;
- semantic checksum.

Unseeded randomness is prohibited in domain replay and experiment semantics. If an experiment uses stochastic assumptions, every draw that can affect a domain or reported result must be reproducible from the seed record and draw-order contract.

### Generated datasets and cases

Generated fixtures, synthetic datasets, fuzz cases, and property-test cases must declare:

- generator identity/version;
- seed record;
- generation parameters;
- validity constraints;
- intended use;
- retained minimized failing case if applicable;
- lineage to any source data used for calibration;
- access and retention class.

Generated data must never be labeled as live capture, source venue data, or faithful replay input.

## Comparison groups

### ComparisonGroup

A `ComparisonGroup` defines which experiments may be compared and why.

It contains:

- group identity/version;
- member experiment IDs;
- baseline/control member;
- treatment/member roles;
- controlled variables;
- intended varying variables;
- dataset compatibility relation;
- replay-class compatibility relation;
- component-version compatibility relation;
- assumption compatibility relation;
- telemetry/profile compatibility relation where performance is compared;
- methodology reference for statistical interpretation;
- invalidation and supersession policy.

This leaf records comparison identity and compatibility prerequisites. The companion methodology leaf decides how to compute, test, and interpret the comparison.

### Direct comparability rules

Two result bundles are directly comparable only if the comparison group declares compatibility for:

- replay class;
- dataset lineage and coverage;
- correction policy;
- merge policy;
- feature/strategy/recommendation semantic versions;
- cost/latency assumptions;
- seed/draw policy if stochastic assumptions are used;
- telemetry profile if performance is compared;
- environment profile if resource or latency results are compared.

If any of these differ and are not declared as the intended experimental variable, direct comparison fails. Reports may still discuss the results qualitatively if they label the comparison as non-controlled.

## Notebook and ad hoc analysis boundaries

### Non-authoritative consumers

Notebooks, spreadsheets, scripts, dashboards, and ad hoc SQL queries are non-authoritative consumers unless they submit records that the owning registry validates and accepts.

They may:

- propose experiment manifests;
- launch admitted experiments through approved APIs;
- read cataloged datasets and result bundles according to access policy;
- create derived analysis datasets;
- generate charts, reports, and exploratory notes;
- attach human commentary to a result bundle.

They may not:

- edit admitted experiment manifests;
- change dataset lineage;
- create production domain facts;
- overwrite result bundles;
- infer missing replay-class eligibility;
- reclassify corrected research replay as faithful;
- treat notebook-local calculations as accepted strategy/recommendation outputs;
- bypass access, redaction, or retention rules.

### Promotion of ad hoc work

Ad hoc analysis becomes authoritative only by promotion through registry validation:

1. create a proposed experiment manifest or derived-dataset record;
2. declare all inputs, versions, assumptions, and generated artifacts;
3. pass access, lineage, and reproducibility checks;
4. accept the manifest or derived-dataset record;
5. rerun or seal results under registry control.

A screenshot, manually edited CSV, notebook cell output, or copied table is not a result bundle.

## Result bundles

### Result-bundle manifest

A result bundle contains:

- `result_bundle_id`;
- experiment ID and manifest revision;
- run IDs and terminal attestations;
- dataset and replay-class manifest IDs;
- component, configuration, assumption, and seed bindings;
- artifact list with content hashes;
- accepted domain-output references;
- semantic checksum summary;
- telemetry/profile evidence references;
- logs and diagnostic references;
- environment/reference workload references where relevant;
- completion status and caveats;
- reproducibility status;
- access and retention classification;
- invalidation/supersession links.

The bundle is immutable after sealing. A later correction creates a new bundle or invalidation record.

### Domain-output references

Result bundles reference accepted domain facts and projections from their owning authorities. For Phases 01-05 this may include:

- run-input selections;
- market-state views and bundles;
- feature evaluations and observations;
- strategy evaluations, signals, abstentions, expiration/supersession facts;
- trade recommendations and recommendation lifecycle facts;
- operational interruptions and incomplete/non-faithful classifications.

The bundle may include compact extracts or summaries, but the extract is not the authoritative domain fact unless the owning authority says so.

### Semantic checksums

Each bundle records multiple checksum classes:

- content checksum for stored artifacts;
- manifest checksum for experiment identity and configuration;
- dataset checksum for selected inputs;
- replay-order checksum for reconstructed run-input sequence;
- domain semantic checksum for accepted outputs;
- telemetry/profile checksum for performance evidence;
- comparison checksum for comparison-group membership and controlled variables;
- export checksum for portable/report packages.

Semantic checksums use canonical encodings and registered algorithms. They exclude host-specific processing timestamps, file paths, thread IDs, process IDs, storage row order, UI sort order, and opaque IDs that the relevant authority explicitly allows to differ across independent equivalent runs.

Same-run recovery must reproduce accepted result-bundle identities exactly. Independent equivalent experiments compare semantic checksums, manifest content, lineage, and declared deterministic identities according to each authority's identity policy.

## Reproducibility

### Reproducibility classes

Chronos uses these reproducibility classes:

- **Exact same-run recovery:** accepted identities and pending obligations must be restored exactly for the same run or experiment after supported failures.
- **Faithful replay reproduction:** reproduces original live/capture behavior under the faithful capture-order replay manifest.
- **Normalized-fact deterministic reproduction:** reproduces domain outputs from accepted normalized facts and controls without re-running normalization.
- **Raw re-normalization reproduction:** reproduces a newly declared normalized lineage under selected versions, not the original unless identity policy proves it.
- **Corrected research reproduction:** reproduces corrected research outputs under the corrected dataset and correction policy.
- **Derived analysis reproduction:** reproduces notebook/report outputs from sealed result bundles and derived artifact manifests.

Reports must name the class they claim. “Reproducible” without a class is insufficient.

### Minimum reproduction packet

A result bundle is reproducible only if the packet contains or references:

- experiment manifest revision;
- dataset and replay-class manifests;
- all component and policy versions;
- run initialization manifests and terminal attestations;
- required source/normalized/control/timer/reference/correction evidence;
- cost and latency assumption records;
- seed records and generated-case manifests;
- environment/static configuration evidence where relevant;
- telemetry profile and performance evidence where relevant;
- artifact hashes and storage locations;
- access permissions or approved redacted/export form;
- expected semantic checksums.

If any required record is missing, the bundle's reproducibility status is degraded, incomplete, or failed. It cannot be silently treated as reproducible because charts or summary metrics exist.

### Reproduction attempts

Every reproduction attempt records:

- reproducer identity and environment;
- source result bundle;
- replay class and dataset versions;
- component versions used;
- whether versions match, are compatible, or intentionally differ;
- checksum comparison;
- allowed identity exclusions;
- mismatches and their owning authority;
- final disposition.

A reproduction attempt with changed versions is a compatibility or comparison experiment, not proof that the original result is exactly reproducible.

## Access, security, and compliance

### Data classification

Datasets, manifests, experiments, and result bundles must carry an access class:

- public fixture;
- internal non-sensitive;
- proprietary market data;
- strategy-sensitive;
- credential-adjacent metadata;
- personal/operator data;
- restricted export;
- quarantine/security evidence.

Access class determines allowed storage locations, export formats, retention, redaction, logging, and notebook access.

### Security controls

The dataset and experiment infrastructure must enforce:

- producer identity and authorization for catalog writes;
- separate read/write/admin permissions;
- access checks for raw payloads and proprietary captures;
- redaction before export or notebook exposure where required;
- secret scanning for payloads, logs, manifests, notebooks, and result bundles;
- path traversal and unsafe URI rejection;
- content-hash validation before use;
- import validation for external datasets;
- audit facts for admission, access changes, export, deletion/expiry, invalidation, and security exceptions.

Derived artifacts inherit the most restrictive applicable access class unless a validated redaction/export policy lowers it.

### External and imported datasets

Imported datasets are untrusted until validated. Validation must cover:

- declared provenance and license;
- schema and envelope compatibility;
- bounds, size, compression, and resource limits;
- checksum and malware/secret scan;
- stream/listing identity mapping;
- replay-class eligibility;
- quarantine behavior for malformed or hostile inputs.

An imported dataset cannot become faithful Chronos capture unless it contains the required Chronos capture/control/timer/run-input evidence. Otherwise it is an external research dataset or corrected research dataset with appropriate caveats.

## Retention and lifecycle

### Retention classes

Retention policy must distinguish:

- short-lived scratch artifacts;
- reproducibility-critical experiment packets;
- public fixtures;
- proprietary captures;
- security/quarantine evidence;
- promotion-supporting evidence;
- invalidated/superseded result history;
- exported report packages.

Retention records include:

- retention class;
- legal/licensing basis;
- minimum and maximum retention;
- evidence pinning status;
- allowed deletion/expiry condition;
- compaction or redaction policy;
- owner approval requirements;
- tombstone/invalidation behavior.

### Expiry and deletion

Expiry or deletion must not make an approved result look stronger than it is. If required evidence expires or is deleted:

- the catalog records a tombstone;
- affected experiments and result bundles receive degraded reproducibility or invalidation status;
- comparison groups that require the missing evidence are marked non-comparable;
- reports and notebooks consuming the bundle must surface the degraded status.

Deletion of raw payloads may be allowed by retention policy, but the system must not continue to claim faithful replay eligibility if the faithful replay evidence no longer exists.

## Observability and performance

### Instrumentation boundaries

Dataset and experiment infrastructure extends Phase 02 telemetry without redefining domain latency endpoints.

Required telemetry covers:

- catalog admission attempts and outcomes;
- manifest validation latency;
- dataset partition integrity checks;
- replay reconstruction throughput and backpressure;
- experiment admission, launch, completion, failure, and abort;
- result-bundle materialization and sealing;
- artifact export and retention actions;
- access denials and security exceptions;
- checksum mismatch and lineage-incomplete events;
- reproduction-attempt outcomes.

Telemetry is non-authoritative. A metric cannot prove a dataset valid, a result reproducible, or an experiment comparable; it can only report what the owning authority accepted.

### Performance evidence

Implementation must define representative workloads for:

- small public fixtures;
- medium local historical datasets;
- large source-capture partitions;
- normalized-fact replay reconstruction;
- raw re-normalization;
- corrected-dataset validation;
- result-bundle sealing;
- comparison-group metadata operations;
- concurrent read-heavy notebook/report access;
- hostile imported dataset rejection.

The Phase 02 performance method applies: registered workload, environment manifest, overhead bounds, tail behavior, saturation evidence, resource envelopes, and correctness checksums are required before budgets are accepted.

Dataset reconstruction or result-bundle storage may be slower than hot-path domain processing, but it must not introduce hidden semantic changes, drop facts silently, or block the production hot path except through explicit bounded admission/backpressure policy.

## Failure and recovery

### Catalog and registry recovery

After a supported crash or restart, the dataset catalog and experiment registry must recover:

- admitted dataset records;
- manifest validation state;
- experiment lifecycle state;
- result-bundle sealing state;
- lineage edges;
- access and retention metadata;
- invalidation/supersession facts;
- pending publication/export obligations.

If an accepted manifest or sealed result bundle cannot be recovered exactly, the affected experiment becomes incomplete, failed, or non-reproducible according to policy. The registry must not recreate a different accepted manifest under the same identity.

### Partial result handling

Partial outputs are allowed only when explicitly represented:

- incomplete run;
- incomplete result bundle;
- failed artifact;
- missing recommendation obligation;
- interrupted feature or strategy runtime;
- degraded reproducibility;
- non-faithful replay;
- invalid comparison group.

Partial artifacts cannot be promoted to completed results by omitting missing outputs from the bundle.

### Invalidation

Invalidation facts identify:

- affected dataset, experiment, result bundle, comparison group, or report;
- defect owner and cause;
- discovered time and actor/system;
- required downstream status changes;
- replacement/superseding artifact if any;
- whether prior conclusions are unsafe, degraded, or merely superseded.

Invalidation is append-only. It does not delete prior evidence.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of this leaf and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **RB-E01 — Dataset catalog authority registry** | Dataset types, owners, lifecycle, access classes, lineage edge types, retention classes, replay eligibility states, and registry schemas | One owner for dataset/replay records and one owner for experiment records; query/report/notebook artifacts are non-authoritative |
| **RB-E02 — Replay-class manifest suite** | Faithful capture-order, normalized-fact, raw re-normalization, corrected event-time research, missing-evidence, and mixed-class cases | Each run/experiment names exactly one replay class; corrected research cannot masquerade as faithful; missing class evidence fails replay eligibility |
| **RB-E03 — Source/raw dataset fixture** | Raw payloads, source metadata, gaps, duplicates, quarantine, redaction, capture integrity, and license metadata | Source-capture lineage and limitations are explicit; faithful eligibility requires original control/timer/run-input and pinned-version evidence |
| **RB-E04 — Normalized-fact dataset fixture** | Accepted normalized facts, source lineage, stream cursors, schema/reference/normalizer versions, control/timer facts, and checksums | Normalized-fact replay reconstructs accepted facts without rerunning normalization and rejects mismatched checksums |
| **RB-E05 — Raw re-normalization fixture** | Parent source dataset, selected normalizer/reference/schema versions, newly produced lineage, semantic diffs, and comparison relation | Re-normalization creates a new dataset/run identity and never overwrites original normalized facts |
| **RB-E06 — Corrected research dataset fixture** | Correction policy, repaired/reordered/added/removed facts, actor/evidence/review metadata, corrected ordering, and caveats | Corrected datasets are reproducible, auditable, and always labeled non-faithful to original live observation |
| **RB-E07 — Lineage completeness and graph integrity suite** | Required nodes/edges, parent existence, partition coverage, reference/correction links, invalidation edges, and tombstones | Replay-eligible datasets have complete lineage; incomplete lineage is explicitly non-authoritative or exploratory |
| **RB-E08 — Version-pinning validation suite** | Component, schema, feature, strategy, recommendation, arithmetic, timer, telemetry, configuration, and dependency-lock bindings | Unknown/latest/current/default/unpinned semantic versions fail experiment admission |
| **RB-E09 — Experiment manifest admission suite** | Draft/admitted/rejected specs, invalid inputs, access failures, missing assumptions, replay-class mismatch, and manifest immutability | Only valid admitted manifests can produce authoritative result bundles; admitted specs are immutable |
| **RB-E10 — Experiment lifecycle and relationship suite** | Parent/child, rerun, baseline/treatment, parameter sweep, ablation, failed, aborted, superseded, and invalidated cases | Lifecycle facts are append-only; comparison relationships state controlled and varying variables |
| **RB-E11 — Feature/strategy/recommendation binding suite** | Versioned feature graphs, strategies, recommendations, policy versions, conformance evidence, and invalid notebook-local code | Experiments bind the exact Phase 05 production contracts and preserve one-recommendation-per-valid-signal completeness |
| **RB-E12 — Research assumption registry** | Assumption sets plus fee/cost, slippage, latency, horizon/holding-window, benchmark-construction, unavailable-label-policy, stochastic seed/draw records, scope, units, provenance, uncertainty, replay applicability, compatibility, and caveats | Assumptions are immutable research inputs; assumption-set identity owns comparison compatibility; assumptions cannot create paper fills, ledger entries, live latency claims, risk approvals, or accounting facts |
| **RB-E13 — Seed and generated-case suite** | Seed algorithm, draw order, generated fixtures, property-test shrinks, stochastic assumption draws, and access class | All semantic randomness is reproducible; unseeded randomness fails admission |
| **RB-E14 — Comparison-group compatibility suite** | Compatible and incompatible datasets, replay classes, versions, assumptions, telemetry profiles, and intended variables | Direct comparison is allowed only when compatibility is declared or the difference is the intended experimental variable |
| **RB-E15 — Notebook/ad hoc boundary suite** | Notebook-created drafts, derived datasets, manually edited exports, screenshots, report tables, and promoted analyses | Ad hoc outputs remain non-authoritative until registry validation accepts a manifest, derived dataset, or result bundle |
| **RB-E16 — Result-bundle sealing suite** | Completed, partial, failed, interrupted, non-faithful, invalidated, and superseded bundles with artifact manifests | Sealed bundles are immutable, content-addressed, and cannot hide missing required outputs |
| **RB-E17 — Semantic checksum and identity suite** | Manifest, dataset, replay-order, domain-output, telemetry/profile, comparison, export, and allowed opaque-ID cases | Checksums are canonical; same-run recovery reproduces identities; independent runs compare checksums under declared identity policy |
| **RB-E18 — Reproducibility packet and reproduction-attempt suite** | Complete, degraded, missing-version, changed-version, missing-raw, redacted, and incompatible-environment attempts | Reproduction claims name a class and disposition; changed-version reruns are comparison experiments, not exact reproduction |
| **RB-E19 — Access, redaction, import, and security suite** | Unauthorized writes/reads, path traversal, hostile imports, secret leakage, export redaction, producer spoofing, and audit facts | Sensitive datasets/results are protected; imported data is hostile until validated; derived artifacts inherit restrictive access |
| **RB-E20 — Retention, expiry, and tombstone suite** | Expired raw data, retained manifests, evidence-pinned promotion bundles, legal deletion, degraded reproducibility, and report propagation | Expiry cannot preserve stronger claims than remaining evidence supports; tombstones and downstream degradation are explicit |
| **RB-E21 — Observability/performance adoption (`OT-E*`, `PS-E*`)** | Dataset admission, validation, reconstruction, experiment execution, result sealing, export, and reproduction workloads | Telemetry remains non-authoritative; accepted budgets follow Phase 02 method and preserve semantic checksums under load |
| **RB-E22 — Failure/recovery matrix** | Crashes during catalog admission, experiment admission, replay reconstruction, result sealing, export, invalidation, and retention action | Accepted records recover exactly or affected scope is marked incomplete/failed/non-reproducible; no substitute identity is invented |
| **RB-E23 — Cross-phase compatibility review** | Approved Phases 01-05 plus this leaf's authority, lifecycle, replay, telemetry, checksum, access, and identity mappings | No replay class, lifecycle state, feature/strategy/recommendation invariant, telemetry endpoint, or ownership boundary is redefined |

## Deferred choices

The following choices are intentionally deferred and must be resolved before implementation or activation of affected workflows:

- exact storage products, table layouts, object layouts, compression formats, and partition schemes;
- concrete dataset-catalog API shape and query language;
- concrete artifact packaging format for result bundles;
- exact semantic-checksum algorithms where upstream leaves have not already fixed them;
- exact public fixture datasets and licensing model;
- exact dataset-size thresholds, local disk quotas, and cache eviction limits;
- statistical validation, train/test split policy, walk-forward procedure, multiple-comparison control, parameter-search governance, and promotion workflow, owned by the companion Phase 06 methodology leaf;
- exact slippage, fill, P&L, mark, risk, and accounting models, owned by later portfolio/risk and paper-execution phases;
- UI/reporting presentation for experiment comparison;
- external-signal and Polymarket dataset semantics, owned by later external/opportunity phases.

None of these deferred choices may weaken replay-class separation, version pinning, experiment immutability, result-bundle integrity, one-recommendation-per-valid-signal completeness, or the rule that research invokes the production domain path rather than a second semantic engine.
