# Phase 06: Backtesting and Validation

## Purpose

This document defines the planning contract for Chronos's backtest execution model, validation methodology, statistical evidence, and strategy-promotion workflow.

This leaf builds on the approved Phase 01 architecture and domain model, Phase 02 observability and performance method, Phase 03 event/persistence/replay/recovery contracts, Phase 04 market-data and market-state contracts, Phase 05 feature, strategy, and recommendation runtime contracts, and the companion Phase 06 `datasets-and-experiments.md` leaf.

Backtesting in Chronos must execute the same production replay, market-state, feature, strategy, and recommendation path used by live and replay runs. It may add research-only assumptions, labels, outcome observations, statistics, comparisons, and reports. It must not create a second semantic engine, bypass feature/strategy/recommendation authorities, or introduce portfolio construction, risk, paper broker, fills, ledger, accounting, or executable orders.

Before Phase 07 and Phase 08 introduce portfolio/risk and paper execution/accounting, a Chronos backtest answers:

- what recommendations the approved production path would have produced under a declared replay class;
- how those recommendations scored under declared cost, slippage, latency, horizon, and outcome-observation assumptions;
- whether evidence supports moving a strategy from research draft to backtested or paper-candidate status.

It does not prove executable trading performance, broker-realistic fills, portfolio-level risk, capital usage, drawdown under real sizing, tax/accounting results, or live execution readiness.

## Objectives

The backtesting and validation contracts must establish that Chronos can:

1. run backtests through the production event, state, feature, strategy, and recommendation runtime with no alternate strategy implementation;
2. bind every backtest to an admitted experiment, dataset, replay-class manifest, component versions, and assumption records from `datasets-and-experiments.md`;
3. distinguish faithful capture-order, normalized-fact, raw re-normalization, corrected event-time research, and derived/report-only analyses;
4. inject cost, slippage, latency, and outcome-observation assumptions as versioned research inputs without creating paper orders, fills, ledgers, positions, or P&L authority;
5. prevent look-ahead, survivorship, data snooping, post-hoc threshold selection, leakage through corrections, leakage through labels, and leakage through notebook-local analysis;
6. define train/validation/test and walk-forward split policies that are explicit before confirmatory claims;
7. compute strategy/recommendation metrics with registered populations, units, baselines, uncertainty, and invalid-run rules;
8. define comparison validity across datasets, replay classes, component versions, assumptions, seeds, splits, and telemetry profiles;
9. require sensitivity analysis for assumptions and parameters before promotion claims;
10. define promotion states from draft to backtested to paper-candidate as research-registry facts, not runtime activation or execution authority;
11. produce sealed reports and result bundles that can be reproduced, invalidated, superseded, and audited;
12. expose observability and performance evidence for backtest execution without redefining Phase 02 latency endpoints.

## Scope

### In scope

- backtest execution model and lifecycle;
- replay-class-specific backtest interpretation rules;
- binding to experiment manifests, dataset manifests, result bundles, comparison groups, cost/latency assumption records, and seed records;
- research-only outcome observation, label generation, and recommendation scoring;
- cost, slippage, latency, horizon, and benchmark assumption application;
- no-look-ahead, survivorship-bias, data-snooping, leakage, and post-hoc-analysis controls;
- train/validation/test, rolling, expanding, and walk-forward split policies;
- statistical metrics, uncertainty, hypothesis/estimation workflow, and multiple-comparison controls;
- baseline definitions, sensitivity analysis, ablations, and robustness checks;
- comparison validity rules and invalid comparison classification;
- research promotion states: `draft`, `backtest_candidate`, `backtested`, `paper_candidate`, `rejected`, `superseded`, and `invalidated`;
- invalid-run, failed-run, incomplete-run, non-faithful, and degraded-evidence rules;
- report contents, claims vocabulary, reproducibility packet, and sealed result-bundle expectations;
- observability, health, performance, and evidence gates for backtesting infrastructure.

### Out of scope

- portfolio construction, target positions, capital allocation, exposure aggregation, and portfolio-state reads;
- risk approval, risk preview, reservation, projected exposure, kill-switch execution semantics, or target authorization;
- paper broker simulation, order lifecycle, order book queue position, partial fills, ledger, accounting, marks, realized/unrealized P&L, reconciliation, or tax;
- live trading, live credentials, venue order submission, exchange acknowledgement, or execution automation;
- exact trading formulas, parameter values, alpha targets, or market-specific business decisions;
- UI design for research dashboards;
- concrete storage engine, compute framework, file format, scheduler, package manager, or ticket breakdown;
- external-signal and Polymarket-specific validation beyond preserving extension points and requiring lineage when those phases introduce those inputs.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Dataset/replay authority | Replay-class reconstruction, dataset eligibility, selected input partitions, corrected/non-faithful caveats, and replay-order validation | Backtest interpretation, statistical claims, promotion decisions |
| Research/backtest/experiment registry | Backtest specifications, methodology bindings, split manifests, metric plans, comparison validity, promotion records, reports, and result-bundle interpretation | Production domain facts, dataset content, runtime activation, paper or live execution |
| Run/configuration authority | Run initialization, mode, replay class, component versions, controls, timer policies, and terminal attestation | Promotion decisions, statistical validity, paper-candidate approval |
| Market-state authority | State views, bundles, state quality, lineage, and semantic checksums | Outcome labels, realized-return calculations, comparison claims |
| Feature authority | Feature observations and diagnostics from production runtime | Feature matrices as alternate upstream truth |
| Strategy runtime | Strategy evaluations, signals, abstentions, and explanation facts | Statistical validation, promotion decisions, recommendations |
| Recommendation authority | Exactly one portfolio-neutral recommendation per valid signal and its lifecycle | Backtest scoring, assumption-adjusted outcomes, portfolio sizing, risk, execution |
| Observability | Non-authoritative telemetry, health, performance profiles, benchmark evidence, and alerts | Domain validity, statistical validity, promotion authority |
| Query/report/notebook tools | Non-authoritative analysis, charts, report rendering, and exploratory views | Authoritative experiment/result identity, promotion state, or domain facts |

The backtest methodology may consume accepted domain outputs and produce research records. It cannot revise the domain outputs it consumes. If a backtest discovers a defect in a feature, strategy, recommendation, dataset, or replay contract, it records an invalidation or comparison failure and causes the owning authority to rerun or repair; it does not silently patch the result.

## Canonical concepts

### Backtest specification

A `BacktestSpecification` is an admitted experiment manifest, plus a registered methodology binding, that declares how production-domain outputs will be evaluated.

It contains at minimum:

- experiment manifest identity from `datasets-and-experiments.md`;
- backtest methodology version;
- replay-class manifest identity;
- dataset and partition identities;
- feature, strategy, recommendation, run, timer, telemetry, and arithmetic version bindings;
- split manifest identity, if train/validation/test or walk-forward claims are made;
- metric plan identity;
- assumption-set identity;
- benchmark and baseline identities;
- comparison-group identity, if comparative claims are made;
- sensitivity-plan identity, if promotion claims are made;
- invalid-run policy;
- report template/version;
- preregistration status and mutation policy.

The specification is immutable after admission. A change to dataset, replay class, strategy version, parameters, assumptions, splits, metrics, baselines, or claim type creates a new specification or child experiment.

### Backtest run

A `BacktestRun` is a production replay/domain run executed under one admitted backtest specification. It emits the same domain facts as any other replay run for the activated capabilities:

- run-input selections;
- market-state views and bundles;
- feature observations and unavailable/diagnostic outcomes;
- strategy evaluations, signals, abstentions, interruptions, expirations, and supersessions;
- recommendation obligations, actionable recommendations, hold recommendations, expirations, and supersessions;
- publication and consumer-acknowledgement lifecycle facts where required.

The backtest layer may add research records that reference those facts:

- outcome-observation records;
- assumption-application records;
- metric-population records;
- statistic records;
- comparison records;
- validation-decision records;
- promotion records;
- reports.

Research records are not market events, state views, feature observations, strategy evaluations, recommendations, targets, orders, fills, positions, or ledger entries.

### Evaluation unit

A `BacktestEvaluationUnit` is the smallest unit eligible for outcome scoring. V1 evaluation units are recommendation-scoped:

- exactly one accepted `TradeRecommendation`;
- its source `StrategySignal` and `StrategyEvaluation`;
- input-cut lineage through features, state bundle, controls, timer facts, dataset, and replay class;
- recommendation issue position/logical time;
- declared horizon or outcome-observation window;
- cost, slippage, and latency assumption references used for scoring;
- outcome-observation record or declared unavailable label reason;
- inclusion/exclusion status under the metric population.

Abstentions may be counted in coverage, selectivity, data-quality, and opportunity-cost style metrics, but they are not recommendation-scored evaluation units because they produce no `TradeRecommendation`.

### Outcome observation

An `OutcomeObservation` is a research-owned record that describes what happened in the dataset after a recommendation under a declared observation policy.

It may use:

- future market-state views after the recommendation's issue cut;
- declared horizon windows;
- declared reference price or mark policies;
- declared exit/stop hint interpretation as research-only labels;
- declared cost, slippage, and latency assumptions;
- availability/fidelity status of the observed window.

It must contain:

- observation policy identity/version;
- input recommendation identity;
- observation window and boundary rule;
- data lineage and replay class;
- price/mark basis, units, and currency;
- latency-adjusted or unadjusted observation basis;
- unavailable, censored, gapped, stale, halted, or out-of-scope status where applicable;
- checksum and canonicalization version.

An outcome observation is not a fill, mark, ledger entry, realized P&L, unrealized P&L, or accounting fact. Reports must use terms such as `assumption-adjusted outcome`, `recommendation score`, `hypothetical return proxy`, or `research label` until Phase 08 introduces authoritative paper execution/accounting.

### Assumption set

An `AssumptionSet` binds one or more immutable assumption records from `datasets-and-experiments.md`:

- cost assumptions;
- slippage assumptions;
- latency assumptions;
- horizon/holding-window assumptions;
- benchmark construction assumptions;
- unavailable-label policy;
- stochastic assumption seed records, if any.

Assumption sets are explicit experimental variables. A result under one assumption set cannot be compared to another unless the comparison group declares the assumption difference as the intended variable or proves compatibility.

### Validation decision

A `ValidationDecision` is a research-registry fact that interprets one or more result bundles under a preregistered metric plan.

Allowed dispositions:

- `pass`;
- `fail`;
- `inconclusive`;
- `invalid`;
- `degraded`;
- `superseded`.

A validation decision is not runtime activation. It does not enable a strategy, change controls, create a recommendation, or authorize paper/live trading.

## Backtest execution model

### Production-path requirement

Every authoritative backtest must invoke the production path:

```text
Dataset/replay manifest
  -> stream/run-input reconstruction
  -> market-state authority
  -> feature authority
  -> strategy runtime
  -> recommendation authority
  -> research-owned outcome observation and statistics
```

The research layer may orchestrate runs and consume outputs, but it may not:

- compute market state from derived tables when production market-state replay is required;
- recompute features in a notebook instead of consuming accepted feature observations;
- run a strategy implementation outside the Phase 05 strategy runtime;
- infer recommendations from signals without the recommendation authority;
- fill missing recommendations to make reports complete;
- mutate production outputs based on research labels.

### Backtest lifecycle

```text
Draft specification
  -> admitted backtest specification
  -> replay/domain run launched
  -> production outputs accepted
  -> outcome observations materialized
  -> metric populations sealed
  -> statistics computed
  -> validation decision recorded
  -> result bundle sealed
  -> report published
```

Each transition has an owning authority and append-only fact. If a transition fails, the run or result bundle records the failure explicitly.

### Ordering and causality

For each evaluation unit, research scoring may use future observations only after the production recommendation has already been emitted in the replayed run. The scoring layer is downstream of recommendation publication and cannot feed back into the same run.

The following are prohibited:

- using future outcome labels to select features, parameters, thresholds, schedules, or recommendation policies inside the same admitted run;
- using corrected event-time data to claim faithful live behavior;
- replacing live/capture-order controls with post-hoc reconstructed controls;
- using unavailable future data to drop losing or ambiguous recommendations unless the exclusion rule was preregistered;
- using query-time wall clock or notebook execution order as semantic input.

### Completeness

A completed backtest result must account for every eligible production output in scope:

- every admitted strategy invocation is terminal or explicitly interrupted/incomplete;
- every valid signal has exactly one accepted recommendation or the result is incomplete/non-faithful;
- every in-scope recommendation is either included in a metric population or excluded by a preregistered reason;
- every unavailable/censored label is reported in coverage metrics;
- every aborted, failed, non-faithful, corrected, or degraded input condition is visible in the result bundle and report.

Partial outputs cannot be promoted by omitting missing obligations from metric populations.

## Replay-class interpretation

### Faithful capture-order backtest

A faithful capture-order backtest estimates what Chronos would have emitted under the original captured input/control/timer order and pinned component versions.

It requires:

- faithful replay-class manifest;
- original source/control/timer/run-input evidence;
- original or pinned normalizer/reference/schema/component versions required by the manifest;
- matching domain semantic checksums where expected;
- explicit treatment of late, missing, gapped, quarantined, or rejected facts.

Claims may say “would have emitted under the captured run contract” only if faithful replay evidence passes. Outcome observations may still look ahead for scoring, but they must be labeled as downstream research evaluation, not live decision input.

### Normalized-fact backtest

A normalized-fact backtest starts from accepted normalized facts and controls. It can validate strategy/recommendation behavior over those facts, but it does not test source decoding, normalization correctness, or parser behavior.

Reports must state that parser/source-capture defects outside the normalized-fact dataset are not covered.

### Raw re-normalization backtest

A raw re-normalization backtest decodes retained source capture through selected normalizer/reference/schema versions, then runs production state/feature/strategy/recommendation logic.

It creates new normalized lineage and a new result interpretation. It may compare against prior normalized-fact results only through an explicit comparison group that declares the changed interpretation versions.

### Corrected event-time research backtest

A corrected event-time research backtest uses repaired or reordered data under a correction policy. It is useful for research and robustness analysis, but it is not evidence that Chronos would have acted the same way live.

Reports must label:

- correction policy;
- repaired, added, removed, reordered, or excluded facts;
- material differences from faithful/normalized replay;
- which conclusions are research-only.

### Derived/report-only analysis

Derived feature matrices, notebook extracts, and denormalized tables may support exploratory analysis. They cannot be used for authoritative backtest claims unless the production path has already produced the referenced domain outputs and the derived artifacts are clearly downstream of sealed result bundles.

## Assumption injection

### Injection boundary

Cost, slippage, latency, and horizon assumptions enter only after the recommendation authority has accepted a recommendation, unless the specific assumption is already part of a declared strategy or recommendation policy version.

Research-only assumptions may affect:

- outcome-observation boundary;
- adjusted entry/exit proxy;
- recommendation score;
- sensitivity analysis;
- benchmark calculation;
- validation metric.

They may not affect:

- market-state reconstruction;
- feature values;
- strategy signals;
- abstentions;
- recommendation existence;
- recommendation type;
- recommendation indicative size;
- run-input ordering;
- control effective positions.

If an assumption is intended to alter strategy or recommendation behavior, it belongs in a new strategy/recommendation configuration and a new production-path run, not in downstream scoring.

### Cost assumptions

Cost assumptions may subtract or annotate:

- fee estimates;
- spread penalties;
- funding/carry placeholders;
- conversion charges;
- minimum notional/quantity caveats;
- liquidity proxy penalties.

They must not create:

- order quantity;
- order price;
- fill price;
- cash balance;
- inventory;
- realized P&L;
- ledger entries.

Cost-adjusted metrics must be named as research estimates, for example `assumption_adjusted_return_proxy`, not `realized_pnl`.

### Slippage assumptions

Slippage assumptions are research scoring rules before Phase 08. They may be:

- fixed spread multiples;
- basis-point penalties;
- depth-proxy penalties derived from market-state observations;
- latency-and-volatility proxy penalties;
- stochastic penalty distributions with deterministic seed records.

They must declare:

- scope and applicability;
- source/provenance;
- deterministic or stochastic draw policy;
- unsupported market-state conditions;
- whether the assumption is symmetric or side-specific;
- uncertainty or sensitivity bounds.

Slippage assumptions do not model queue priority, partial fills, cancels, venue matching, order routing, or execution residuals until later phases introduce those authorities.

### Latency assumptions

Latency assumptions may shift the observation basis or scenario labels, for example:

- score outcome at `recommendation_time + assumed_decision_delay`;
- compare zero-delay versus measured p95 strategy/recommendation delay;
- perturb outcome windows using a registered latency distribution.

They must use Phase 02 endpoint vocabulary when referring to observed or empirical latency. Synthetic latency assumptions are not live telemetry and cannot be reported as observed performance.

### Assumption application records

Every evaluation unit that uses assumptions records:

- assumption-set identity;
- assumption record versions;
- applied formula identity;
- deterministic seed/draw identity if stochastic;
- affected fields;
- unavailable or unsupported conditions;
- sensitivity group membership;
- semantic checksum.

If an assumption cannot be applied, the unit receives a typed assumption-application status. The report cannot silently drop the unit unless the metric plan preregistered that exclusion.

## No-look-ahead and bias controls

### Look-ahead prevention

The backtest contract must prove that production-domain outputs are generated without access to future labels, future market-state cuts, future corrections, or future report decisions.

Required controls:

- replay/domain run materializes recommendations before outcome observations are computed;
- strategy and recommendation runtime has no access to research label stores;
- experiment admission validates that strategy parameters and policies were fixed before confirmatory runs;
- corrected datasets are labeled as corrected and cannot support faithful-live claims;
- split manifests prevent training or threshold selection on validation/test windows;
- reports distinguish exploratory findings from preregistered confirmatory claims.

### Survivorship bias

Dataset and split manifests must define the selection universe before outcome scoring.

For listing or instrument universes, the manifest records:

- membership source and version;
- inclusion/exclusion rules;
- listing lifecycle status;
- delisted, halted, missing, and failed-source handling;
- symbol/reference-data changes;
- whether the universe is static, point-in-time, or event-driven.

Dropping listings, venues, days, or regimes after observing outcomes is prohibited unless recorded as exploratory and excluded from confirmatory claims.

### Data snooping and parameter search

Parameter exploration must be explicit.

Each parameter search declares:

- search space and bounds;
- search algorithm;
- seed/draw policy;
- objective metric;
- training/screening window;
- stopping rule;
- number of tried configurations;
- failed and invalid configurations;
- correction or validation policy for multiple comparisons;
- selected candidate rationale.

A parameter value chosen after viewing validation or test outcomes cannot be reported as a preregistered validation/test result. It creates a new exploratory result or requires a new held-out validation/test run.

### Leakage through labels and reports

Outcome labels, report tables, notebook outputs, comparison ranks, and benchmark outcomes are downstream research artifacts. They may not be imported into feature, strategy, recommendation, or run-configuration authorities as if they were contemporaneous inputs.

If later strategy development uses prior backtest labels, the next experiment must record that lineage and choose new validation/test windows or a methodology that accounts for the dependency.

## Split and walk-forward policy

### Split manifest

A `SplitManifest` is required for any claim that uses train/validation/test, out-of-sample, walk-forward, or promotion-supporting evidence.

It contains:

- split identity/version;
- dataset and replay-class scope;
- selection universe;
- split unit: time, session, listing, venue, regime, event block, or another declared unit;
- train, validation, test, embargo, and warmup intervals;
- overlap rules;
- correction and late-data policy per split;
- feature warmup handling;
- parameter-search eligibility per split;
- benchmark and baseline eligibility per split;
- leakage review evidence.

### Basic train/validation/test

For static validation:

- training windows may be used for exploratory parameter search and model/threshold development;
- validation windows may be used for candidate selection under a registered search policy;
- test windows are held out until the final candidate and metric plan are fixed;
- test failures cannot be repaired by changing parameters and reusing the same test as if it were still held out.

Warmup data may precede each split only to initialize state/features. Warmup intervals are not counted as tradable/scored unless the split manifest explicitly permits it.

### Walk-forward validation

Walk-forward validation uses repeated chronological folds:

```text
train_1 -> validate/test_1
train_2 -> validate/test_2
...
```

The manifest records:

- fold construction;
- expanding versus rolling windows;
- re-optimization schedule;
- embargo and gap between training and scoring windows;
- whether parameters are refit per fold or fixed;
- aggregation method across folds;
- fold-level invalidity and missing-data policy.

A walk-forward result must report fold dispersion and not only aggregate performance.

### Regime and stress splits

Promotion-supporting evidence should include explicitly named regimes or stress windows when data is available:

- high volatility;
- low liquidity;
- feed gaps;
- abnormal spread;
- venue incidents;
- trend, mean-reversion, and range-bound windows;
- market open/close or funding/settlement windows where relevant;
- corrected-data versus faithful-capture differences.

Regime labels must be defined by rules fixed before the confirmatory run or clearly labeled exploratory.

## Metrics and uncertainty

### Metric plan

A `MetricPlan` defines:

- metric identities and formulas;
- populations and inclusion/exclusion rules;
- primary, secondary, diagnostic, and guardrail metrics;
- aggregation units;
- uncertainty method;
- statistical decision rule;
- multiple-comparison policy;
- minimum sample and coverage requirements;
- invalid-run conditions;
- report vocabulary.

Metrics must name their population. A return-like metric over actionable recommendations is not comparable to a coverage metric over all strategy invocations unless the relationship is explicitly defined.

### Required metric families

For Phase 06, the minimum metric families are:

- **coverage and selectivity:** invocation count, signal rate, abstention rate, actionable rate, hold rate, recommendation completeness, unavailable-label rate;
- **directional quality:** hit rate under declared horizon, side-specific accuracy, calibrated confidence diagnostics where confidence is declared;
- **score quality:** rank correlation, bucketed outcome by score/strength, monotonicity checks, calibration curves where applicable;
- **assumption-adjusted outcome proxies:** gross and cost/slippage/latency-adjusted return proxies per recommendation and per horizon;
- **risk-proxy diagnostics:** downside-tail proxy, adverse excursion proxy, volatility-conditioned outcome, drawdown-like sequence proxy clearly labeled non-accounting;
- **stability:** fold dispersion, regime dispersion, sensitivity ranges, parameter-neighborhood robustness;
- **operational quality:** runtime interruption rate, stale/gapped input rate, replay fidelity, latency profile, deadline misses, resource envelope;
- **benchmark-relative quality:** differences versus baselines under matching populations and assumptions.

Until Phase 08, metrics must avoid accounting terms that imply filled trades or positions. Use `outcome_proxy`, `sequence_proxy`, `drawdown_proxy`, or `recommendation_score` where appropriate.

### Uncertainty and inference

Confirmatory claims must follow the Phase 02 statistical decision method. The metric plan preregisters:

- experimental unit and independent replication unit;
- confidence level or alpha;
- power or precision target;
- minimum detectable effect or equivalence/non-inferiority margin;
- sample-size or sequential stopping design;
- multiplicity policy;
- treatment of autocorrelation and clustered observations;
- treatment of overlapping horizons;
- treatment of missing/censored/unavailable outcomes.

Event-level observations from one continuous replay are not automatically independent samples. Where dependence exists, analysis must use fold-level, block-bootstrap, clustered, paired, or other justified methods.

Exploratory statistics may be reported, but they must be labeled exploratory and cannot be used as the sole basis for promotion.

### Invalid metric populations

A metric population is invalid if:

- inclusion/exclusion depends on observed outcome unless preregistered;
- required recommendations are missing;
- labels use data outside the permitted replay/correction scope;
- cost/latency/slippage assumptions are missing or incompatible;
- split membership is ambiguous;
- sample size or coverage falls below preregistered minimum;
- telemetry/profile evidence needed for the claim is missing;
- the comparison group is invalid.

Invalid metric populations remain in the result bundle with invalid status; they are not deleted.

## Benchmarks and baselines

### Baseline types

Backtests must compare against at least one relevant baseline when making quality or promotion claims.

Allowed baseline classes:

- no-action baseline;
- always-hold baseline;
- random-direction baseline with deterministic seed and matched action rate;
- naive momentum baseline;
- naive mean-reversion baseline;
- spread/imbalance threshold baseline;
- previous approved strategy version;
- parameter-neighborhood baseline;
- ablation baseline with one feature or rule removed;
- benchmark from a sealed prior result bundle.

The baseline must run through the same production strategy/recommendation path if it emits signals/recommendations. A notebook-only baseline may be used for exploratory context but not as an authoritative controlled baseline unless registered as a strategy/recommendation implementation or explicitly labeled non-domain.

### Baseline compatibility

A baseline comparison declares:

- same dataset/split/replay class or intended difference;
- same outcome-observation policy;
- same cost/slippage/latency assumptions or intended difference;
- same metric population definition or mapped population;
- same telemetry/profile relevance where performance is compared;
- same access and retention eligibility for reproducibility.

If the candidate emits fewer recommendations, comparison must report both conditional quality and coverage/selectivity. Hiding low coverage by comparing only surviving recommendations is invalid.

## Sensitivity and robustness analysis

### Required sensitivity dimensions

Promotion-supporting backtests must include sensitivity analysis over:

- fee/cost levels;
- slippage penalties;
- latency assumptions;
- horizon/exit observation rules;
- parameter perturbations around the selected candidate;
- data-quality filters;
- split/fold choices where reasonable;
- market regimes;
- replay class or corrected-versus-faithful differences where available;
- benchmark variants.

Sensitivity analysis may be lighter for early exploratory research, but promotion to `paper_candidate` requires registered sensitivity coverage or an explicit accepted waiver with rationale and expiry.

### Robustness interpretation

A candidate is not robust merely because one metric passes once. The validation decision must identify:

- dimensions where results are stable;
- dimensions where results are fragile;
- assumptions that dominate the result;
- regimes where the strategy should be disabled or treated as unvalidated;
- whether fragility blocks promotion or creates paper-mode guardrails for later phases.

Sensitivity runs are separate experiments or child experiments with explicit comparison relationships and result bundles.

## Comparison validity

### Compatibility requirements

Comparisons are valid only when the comparison group from `datasets-and-experiments.md` and this methodology agree on:

- replay class;
- dataset lineage and coverage;
- merge-policy identity/version;
- run-input reconstruction policy;
- split manifest;
- correction policy;
- feature, strategy, recommendation, and assumption versions;
- outcome-observation policy;
- metric plan;
- seeds and stochastic draw policies;
- environment/profile compatibility when performance, resource, latency, throughput, or runtime-overhead results are compared;
- telemetry/profile where performance is compared;
- intended variable under test.

If a difference is not controlled or declared as the intended variable, the comparison is invalid for confirmatory claims.

Merge-policy and run-input reconstruction differences are material even when the same source dataset is used. They can change which facts are visible at a cut, the order in which controls/timers/market inputs affect state, and therefore feature, strategy, and recommendation outputs. Such differences must be either identical, declared compatible by the dataset/replay authority, or declared as the intended experimental variable.

When performance is compared, environment/profile compatibility must cover the Phase 02 environment manifest, runtime profile, telemetry profile, resource ceilings, workload profile, scheduler/concurrency assumptions, storage/cache state where relevant, and benchmark-profile epoch. A comparison may still be qualitative if these differ, but it cannot make a confirmatory performance claim unless the difference is controlled or intentionally varied.

### Comparison dispositions

Each comparison receives one disposition:

- `valid_confirmatory`;
- `valid_exploratory`;
- `qualitative_only`;
- `invalid_missing_evidence`;
- `invalid_incompatible_inputs`;
- `invalid_post_hoc`;
- `invalid_statistical_design`;
- `invalid_merge_policy_mismatch`;
- `invalid_run_input_reconstruction_mismatch`;
- `invalid_environment_profile_mismatch`;
- `invalid_runtime_incomplete`.

Reports may include invalid or qualitative comparisons only if the disposition and reason are visible next to the result.

## Promotion workflow

### Promotion states

Strategy-promotion state is owned by the research/backtest/experiment registry. It is evidence about research maturity, not runtime activation.

Allowed states:

```text
draft
  -> backtest_candidate
  -> backtested
  -> paper_candidate
```

Terminal or side states:

```text
rejected
superseded
invalidated
expired
```

State meanings:

- `draft`: strategy or parameter idea exists; no admitted promotion-supporting backtest is required.
- `backtest_candidate`: strategy definition/implementation/configuration is pinned and eligible for admitted backtests.
- `backtested`: at least one admitted promotion-eligible backtest completed with a sealed result bundle, visible validation decision, eligible fidelity classification for the stated claim, non-degraded status, and no unresolved incomplete production-domain obligations.
- `paper_candidate`: promotion evidence satisfies the registered paper-candidate criteria and may be proposed to later portfolio/risk/paper phases.
- `rejected`: evidence failed or owner decided not to proceed.
- `superseded`: newer candidate replaces the interpretation.
- `invalidated`: evidence defect makes the state unsafe.
- `expired`: evidence is too old or no longer compatible with current contracts.

Non-faithful, degraded, or incomplete backtests may be reported, studied, compared qualitatively, and used to generate new hypotheses. They cannot satisfy promotion to `backtested`. For this phase, `backtested` promotion requires faithful capture-order replay with a completed result bundle and no unresolved incomplete production-domain obligations. Normalized-fact, raw re-normalization, and corrected event-time replay may support narrower research reports, but they remain research-only and cannot move a candidate into `backtested` or `paper_candidate`.

### Promotion criteria

Promotion to `backtested` requires:

- admitted experiment and backtest specification;
- completed production-domain run under a promotion-eligible replay class;
- no unresolved incomplete production-domain obligations, including run-input reconstruction, market-state publication, feature observation cardinality, strategy evaluation cardinality, signal/recommendation cardinality, recommendation publication, and required terminal attestations;
- no non-faithful, degraded, incomplete, invalid, or partial-result status on the result bundle or on any evidence required by the promotion metric plan;
- sealed completed result bundle;
- metric plan execution;
- validation decision;
- report.

Promotion to `paper_candidate` additionally requires:

- out-of-sample or walk-forward evidence under an approved split policy;
- valid baseline comparison;
- sensitivity analysis;
- assumption register;
- operational-performance evidence sufficient for paper-mode planning;
- no unresolved critical invalidity, leakage, missing-recommendation, or reproducibility defect;
- explicit recommended paper-mode guardrails for later phases, labeled as proposals only.

`paper_candidate` does not enable paper trading. Later phases must still define portfolio/risk, paper broker, accounting, and operational controls before any paper run uses the candidate.

### Promotion record

A `PromotionRecord` contains:

- strategy definition/implementation/instance/configuration identity;
- source experiments and result bundles;
- validation decisions;
- split and metric plan references;
- baseline and sensitivity references;
- unresolved caveats;
- proposed next phase;
- owner, reviewer, and approval timestamp;
- expiry/revalidation trigger;
- invalidation/supersession links.

Promotion records are append-only. If evidence changes, a new promotion record or invalidation record is created.

## Failure and invalid-run rules

### Backtest failure classes

Backtest failures are classified as:

- `admission_failed`: invalid specification, missing versions, access denial, or replay ineligibility;
- `domain_run_failed`: production replay/runtime failed;
- `runtime_incomplete`: accepted domain obligations missing after recovery;
- `recommendation_incomplete`: valid signals without required recommendations;
- `labeling_failed`: outcome observations unavailable or invalid beyond policy;
- `assumption_application_failed`: required assumption could not be applied;
- `metric_invalid`: metric population or statistical design invalid;
- `comparison_invalid`: comparison group invalid;
- `report_sealing_failed`: result/report bundle could not be sealed;
- `operator_aborted`;
- `system_aborted`.

Each failure class has a declared effect on result-bundle status, comparison eligibility, and promotion eligibility.

### Invalid-run conditions

A backtest or result is invalid for confirmatory claims if:

- it did not run through the production domain path when required;
- replay class is missing, mixed, or mislabeled;
- merge-policy identity/version is missing, incompatible, or changed outside the intended experimental variable;
- run-input reconstruction policy is missing, incompatible, or changed outside the intended experimental variable;
- component versions are unpinned;
- strategy parameters were selected using validation/test outcomes outside the registered policy;
- required recommendations are missing;
- future labels leaked into production runtime;
- survivorship universe was chosen after outcome inspection;
- required cost/latency/slippage assumptions are missing or changed post-hoc;
- performance comparison uses incompatible or missing environment/profile evidence;
- telemetry/profile or performance evidence required by the claim is missing;
- statistical stopping, multiplicity, or sample-size rules were violated;
- sealed result bundle or reproducibility packet is incomplete.

Invalid does not mean useless. Invalid results may remain as exploratory artifacts if reports label them correctly and block promotion claims.

An incomplete production-domain obligation is promotion-blocking even if the downstream metric population could be computed without it. This includes missing feature observations or typed unavailable outcomes for accepted feature obligations, missing terminal strategy evaluations for accepted evaluation keys, valid signals without exactly one terminal recommendation, missing recommendation publication where required, unrecovered runtime obligations, and incomplete run terminal attestations.

### Degraded evidence

Evidence is degraded rather than invalid when it remains useful but cannot support a stronger claim, for example:

- corrected research replay supports research insight but not faithful-live behavior;
- normalized-fact replay validates strategy behavior but not source parsing;
- small sample supports characterization but not promotion;
- missing raw capture prevents future re-normalization but not normalized-fact reproduction;
- redacted data allows report review but not full independent reproduction.

Reports must surface degradation next to the affected claim.

Degraded and non-faithful results are valid research artifacts only for the claims their evidence supports. They are not valid `backtested` promotion evidence, even when their metrics look favorable.

## Reporting

### Required report contents

A `BacktestReport` references a sealed result bundle and includes:

- experiment and backtest specification identity;
- strategy/recommendation versions and parameter summary;
- dataset, replay class, split, and correction caveats;
- assumption set;
- metric plan and populations;
- primary and guardrail results;
- uncertainty and statistical decisions;
- baselines and comparison dispositions;
- sensitivity results;
- invalid/degraded/incomplete evidence;
- operational/performance evidence summary;
- reproducibility packet reference;
- promotion decision or explicit “no promotion decision” statement;
- reviewer notes and follow-up actions.

Reports must distinguish:

- faithful versus corrected research claims;
- exploratory versus confirmatory claims;
- recommendation outcome proxies versus paper/live execution results;
- cost/slippage/latency assumptions versus observed telemetry;
- promotion recommendation versus runtime activation.

### Claim vocabulary

Allowed precise terms:

- `faithful capture-order backtest`;
- `normalized-fact backtest`;
- `raw re-normalization backtest`;
- `corrected event-time research backtest`;
- `assumption-adjusted recommendation outcome`;
- `return proxy`;
- `drawdown proxy`;
- `coverage`;
- `selectivity`;
- `paper-candidate`.

Terms requiring later phases:

- `paper P&L`;
- `realized P&L`;
- `filled trade`;
- `position`;
- `execution quality`;
- `risk-approved`;
- `capital efficiency`;
- `live trading ready`.

Those later-phase terms are prohibited in Phase 06 reports unless explicitly labeled as future work or non-authoritative analogy.

## Reproducibility

### Minimum reproduction packet

A backtest result is reproducible only if the result bundle references:

- admitted experiment manifest;
- backtest specification and methodology version;
- dataset and replay-class manifests;
- split manifest;
- metric plan;
- assumption set;
- benchmark/baseline manifests;
- component and configuration versions;
- seed/draw records;
- production run manifests and terminal attestations;
- accepted domain-output references;
- outcome-observation records;
- statistic and validation-decision records;
- report artifact checksums;
- environment/profile evidence where relevant;
- access/redaction status.

If any required record is absent, the reproduction status is degraded, incomplete, or failed.

### Reproduction dispositions

Allowed dispositions:

- `exact_same_run_recovered`;
- `faithful_replay_reproduced`;
- `normalized_fact_reproduced`;
- `raw_renormalization_reproduced`;
- `corrected_research_reproduced`;
- `statistical_reanalysis_reproduced`;
- `compatible_with_declared_difference`;
- `degraded_missing_evidence`;
- `failed_mismatch`;
- `invalid`.

Statistical reanalysis of identical sealed observations must reproduce the same calculations and decision. A new independent rerun may legitimately produce a different categorical result near a threshold; replication claims must use the registered replication/equivalence criteria from the metric plan.

## Observability and performance

### Instrumentation

Backtesting extends Phase 02 telemetry without redefining canonical domain endpoints.

Required telemetry covers:

- backtest specification admission;
- split validation;
- assumption application;
- replay/domain run launch and completion;
- outcome-observation materialization;
- metric population construction;
- statistic computation;
- comparison validation;
- sensitivity campaign progress;
- validation decisions;
- report and result-bundle sealing;
- promotion-state transitions;
- invalidation and supersession;
- access denials and security exceptions.

Telemetry is non-authoritative. It cannot make an invalid metric valid, infer missing recommendations, prove strategy profitability, or promote a candidate.

### Performance evidence

Implementation must define representative workloads for:

- small public fixture backtests;
- medium historical single-listing backtests;
- multi-listing strategy/recommendation backtests where supported by prior phases;
- corrected versus faithful replay comparison;
- parameter sweeps;
- walk-forward campaigns;
- sensitivity campaigns;
- baseline/candidate comparison groups;
- report sealing and reproduction attempts;
- hostile or malformed methodology inputs.

The Phase 02 performance method applies:

- registered workload and environment manifest;
- scheduled-versus-actual arrival accounting where replay pacing matters;
- overhead bounds;
- tail behavior;
- resource envelope;
- saturation and overload evidence;
- correctness checksums;
- statistical decision rules.

Backtesting may be slower than the hot path, but it must not block live capture or production hot-path processing except through explicit bounded resource policy. Running a large research campaign must not change domain outputs, replay class interpretation, or telemetry endpoint meaning.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of this leaf and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **BV-E01 — Backtest authority and lifecycle registry** | Backtest specification, run, outcome-observation, metric, validation, report, and promotion record schemas plus ownership mapping | Research records are distinct from production domain facts and cannot redefine strategy/recommendation outputs |
| **BV-E02 — Production-path conformance suite** | Fixtures proving backtests invoke replay, market-state, feature, strategy, and recommendation authorities | Notebook/derived-table alternate engines fail authoritative backtest admission |
| **BV-E03 — Replay-class interpretation suite** | Faithful, normalized-fact, raw re-normalization, corrected research, and derived/report-only scenarios | Claims and reports preserve replay-class distinctions and block corrected data from faithful-live claims |
| **BV-E04 — Assumption injection suite** | Cost, slippage, latency, horizon, stochastic, unavailable, and unsupported assumption cases | Assumptions affect only downstream research scoring unless encoded in a new production strategy/recommendation configuration |
| **BV-E05 — Outcome-observation and label suite** | Horizon boundaries, latency-adjusted boundaries, censored windows, gaps, stale/recovering state, unavailable labels, and checksum validation | Labels are downstream, reproducible, and never feed back into production-domain outputs |
| **BV-E06 — No-look-ahead and leakage suite** | Future-label access, corrected-data misuse, parameter leakage, notebook leakage, post-hoc exclusions, and report-to-runtime feedback cases | Leakage attempts are rejected or labeled exploratory/invalid and cannot support promotion |
| **BV-E07 — Survivorship and universe suite** | Static, point-in-time, event-driven, delisted, halted, missing, and failed-source membership cases | Universe membership is preregistered and outcome-dependent exclusion fails confirmatory claims |
| **BV-E08 — Parameter-search and data-snooping suite** | Search spaces, tried configurations, stopping rules, selected candidates, multiple comparisons, and held-out reuse cases | Parameter selection is auditable; validation/test contamination blocks confirmatory claims |
| **BV-E09 — Split and walk-forward suite** | Train/validation/test, rolling, expanding, embargo, warmup, fold failure, and regime/stress splits | Split manifests prevent leakage and reports show fold/regime dispersion |
| **BV-E10 — Metric plan and population suite** | Coverage, directional, score, outcome proxy, stability, operational, and benchmark-relative metrics with inclusion/exclusion rules | Metrics name populations, units, uncertainty, minimum samples, and invalidity rules |
| **BV-E11 — Statistical decision suite (`PS-E*` adoption)** | Alpha/confidence, power/precision, independent units, clustered/dependent samples, sequential rules, multiplicity, replication criteria | Confirmatory decisions follow Phase 02 method; exploratory results are labeled and cannot alone promote |
| **BV-E12 — Baseline and benchmark suite** | No-action, random matched-action, naive strategies, previous-version, ablation, and sealed-result baselines | Baseline comparisons use compatible populations/assumptions or receive explicit invalid/qualitative disposition |
| **BV-E13 — Sensitivity and robustness suite** | Fee, slippage, latency, horizon, parameter-neighborhood, data-quality, regime, split, replay-class, and benchmark sensitivity runs | Promotion claims include stable/fragile dimension analysis or an accepted waiver with expiry |
| **BV-E14 — Comparison validity suite (`RB-E14` adoption)** | Compatible, incompatible, intended-variable, missing-evidence, post-hoc, invalid-runtime, merge-policy mismatch, run-input reconstruction mismatch, and environment/profile mismatch comparison groups | Comparisons receive explicit dispositions; merge policy, run-input reconstruction, and performance environment/profile compatibility are enforced; invalid comparisons cannot support confirmatory claims |
| **BV-E15 — Promotion workflow suite** | Draft, backtest-candidate, backtested, paper-candidate, rejected, superseded, invalidated, and expired states; non-faithful, degraded, incomplete, normalized-fact, raw re-normalization, corrected-event-time, and complete faithful-capture bundles | Promotion to `backtested` requires a faithful capture-order completed result bundle with no unresolved incomplete production-domain obligations; non-faithful/degraded/incomplete and non-faithful-capture replay classes may be studied but cannot satisfy `backtested`; promotion records never activate runtime or authorize paper/live trading |
| **BV-E16 — Invalid-run and degraded-evidence suite** | Admission failure, domain failure, incomplete feature/strategy/recommendation obligations, incomplete recommendation publication, labeling failure, assumption failure, metric invalidity, operator abort, degraded and non-faithful evidence | Failure/degradation is explicit in bundles and reports; partial, non-faithful, degraded, or incomplete outputs cannot be promoted by omission or caveat |
| **BV-E17 — Reporting and claim-vocabulary suite** | Reports with replay class, assumptions, metrics, uncertainty, baselines, sensitivity, invalidity, reproducibility, and promotion statement | Reports avoid paper/live/accounting terms and distinguish exploratory, confirmatory, faithful, corrected, and proxy claims |
| **BV-E18 — Reproducibility and reanalysis suite** | Complete packets, missing records, changed versions, statistical reanalysis, independent reruns, and allowed opaque-ID differences | Reproduction claims name class and disposition; identical sealed observations reanalyze deterministically |
| **BV-E19 — Observability and performance suite** | Backtest admission, execution, labeling, metrics, comparison, sensitivity, reporting, promotion, and invalidation workloads | Telemetry remains non-authoritative; budgets and regressions follow Phase 02 method under representative workloads |
| **BV-E20 — Access, redaction, and report-export suite** | Proprietary data, strategy-sensitive outputs, redacted reports, unauthorized notebooks, export packages, and audit facts | Reports and exports respect inherited dataset/result access classes and cannot leak restricted data |
| **BV-E21 — Cross-phase compatibility review** | Approved Phases 01-05, `datasets-and-experiments.md`, and this leaf's replay, merge-policy, run-input reconstruction, authority, metric, assumption, promotion, telemetry, environment/profile, and reproducibility mappings | No replay class, lifecycle state, telemetry endpoint, recommendation cardinality, domain authority, dataset/result ownership boundary, merge-policy contract, or run-input reconstruction contract is redefined |

## Deferred choices

The following choices are intentionally deferred and must be resolved before implementation or activation of affected workflows:

- exact metric formulas and thresholds for any specific strategy;
- exact initial public fixture datasets for validation;
- exact benchmark set for each strategy family;
- exact train/validation/test calendar windows and fold sizes;
- exact cost, slippage, and latency assumption values;
- exact promotion thresholds for `paper_candidate`;
- exact report template and visualization design;
- exact compute scheduler, parallelization method, cache strategy, and storage format;
- exact support for external-signal and Polymarket validation, owned by later external/opportunity phases;
- exact portfolio/risk, paper broker, fill, ledger, mark, P&L, and accounting models, owned by later phases.

None of these deferred choices may weaken the production-path requirement, replay-class separation, no-look-ahead controls, recommendation cardinality, assumption-boundary rules, statistical preregistration, result-bundle reproducibility, or the prohibition on treating backtest outcome proxies as paper/live execution results.
