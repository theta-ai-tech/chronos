# Phase 07: Portfolio Construction

## Purpose

This document defines the planning contract for Chronos's portfolio-construction authority: the component that consumes active actionable `TradeRecommendation`s and produces portfolio-scoped absolute target positions or explicit no-change outcomes.

This leaf builds on approved Phases 01-06. It adopts the Phase 01 authority split between recommendation, opportunity/resolution, portfolio construction, risk, reservation, execution, accounting, run/configuration, dataset/replay, and observability. It adopts Phase 02 latency and telemetry semantics without redefining them. It adopts Phase 03 command, `ControlOutcome`, ordering, persistence, replay, recovery, and lifecycle rules. It adopts Phase 04 market-state/reference/listing lineage through upstream features and recommendations. It adopts Phase 05's rule that `TradeRecommendation`s are portfolio-neutral, advisory, non-executable, and exactly one actionable-or-hold output per valid signal. It adopts Phase 06's research/backtest boundary and introduces the first production-domain step that can turn paper-candidate research into portfolio-specific paper targets.

The downstream boundary is a portfolio target candidate that is eligible to be submitted to the companion risk leaf. This leaf does not approve risk, reserve capacity, produce executable order intents, simulate fills, update ledgers, compute accounting P&L, or submit trades. If a portfolio target cannot be constructed safely, the terminal output is a typed no-change or rejection-to-construct outcome, not a hidden omission.

## Objectives

Portfolio construction must establish that Chronos can:

1. define portfolio, account, sleeve, capital, and trading-universe scope for research-to-paper transition;
2. consume only active actionable `TradeRecommendation`s that satisfy publication, validity, lineage, and strategy-promotion prerequisites;
3. ignore hold recommendations, abstentions, invalid signals, expired recommendations, and superseded recommendations for new target construction;
4. aggregate multiple active actionable recommendations deterministically into one portfolio-scoped absolute target per target key, or one explicit no-change outcome;
5. preserve the distinction between strategy-indicative exposure, portfolio desired exposure, risk decision, reservation, executable intent, order, fill, position, and accounting balance;
6. use versioned sizing policies and state snapshots without reading unowned authority stores or query models;
7. support future multi-listing, multi-venue, and external-opportunity composition through explicit seams without implementing those products here;
8. keep operator direct target entry out of scope unless a later approved phase introduces it as an owned command path;
9. define deterministic scheduling, tie-breaking, target validity, supersession, pause, stop, reset, and recovery behavior;
10. carry complete lineage and ranked explanations from recommendations, strategy signals, features, state, controls, timers, datasets, assumptions where applicable, and portfolio policy;
11. expose observability and performance evidence for recommendation-to-target construction while treating telemetry as non-authoritative;
12. define implementation evidence gates that prove the portfolio-construction boundary is safe before Phase 08 paper execution/accounting consumes it.

## Scope

### In scope

- portfolio, account, sleeve, capital, and trading-universe scope definitions for paper-candidate strategies;
- portfolio-construction authority ownership, records, target keys, target identities, and lifecycle;
- consumption of active actionable `TradeRecommendation`s and exclusion of all non-actionable upstream outputs;
- absolute `TargetPosition` semantics and target delta derivation for explanations only;
- deterministic aggregation across strategies and recommendations;
- sizing-policy inputs, versioning, canonicalization, bounds, and deterministic arithmetic;
- no-change outcomes, construction rejections, invalid input dispositions, and target supersession;
- target validity intervals, expiry, invalidation, and downstream eligibility;
- seams for opportunity/resolution, multi-listing, multi-leg, multi-venue, and external/Polymarket discovery work;
- state snapshot requirements for positions, cash/capital, active targets, control epochs, reference/listing versions, and later reservation/execution/accounting projections;
- deterministic scheduling from recommendation publications, controls, timers, snapshots, and portfolio policy cadence;
- pause, stop, abort, and reset behavior under accepted ordered `ControlOutcome`s;
- lineage, explanations, audit references, observability, performance, testing, and evidence gates.

### Out of scope

- final risk approval, risk modification, risk rejection, risk previews, risk limits, kill-switch execution semantics, and risk-policy decisions;
- exposure serialization, projected-exposure ledger, reservation outcomes, reservation lifecycle, reservation consumption, transfer, release, and `risk_sequence` ownership;
- executable order-intent creation, routing, order slicing, broker simulation, live venue submission, acknowledgement, cancellation, fills, accounting, marks, P&L, reconciliation, and tax;
- operator-entered target construction or manual overrides, except as a deferred product decision;
- UI workflows and operations-console read models;
- concrete trading formulas, exact capital values, final portfolio allocations, or alpha claims;
- concrete storage products, process topology, programming language, scheduler implementation, or ticket-level work breakdown.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Recommendation authority | Accepted, published, active actionable `TradeRecommendation`s, recommendation validity, recommendation explanations, and recommendation lifecycle facts | Portfolio state, target construction, sizing from portfolio capital, risk decisions |
| Opportunity/resolution authority | Later composition of recommendations into economic opportunities and listing/leg resolution before target construction | Portfolio sizing, risk approval, execution authorization, recommendation cardinality |
| Portfolio-construction authority | Portfolio scope, sizing policy application, deterministic aggregation, absolute target positions, no-change outcomes, target validity, target supersession, and target explanations | Risk approval, exposure reservation, executable intents, fills, accounting positions |
| Account/capital state owner | Authoritative account/capital snapshot inputs exposed through approved state contracts | Portfolio target semantics or strategy aggregation policy |
| Run/configuration authority | Run-manifest mode selection at run creation, portfolio policy activation, strategy-to-portfolio assignment, paper eligibility, pauses, resets, and behavior-changing controls through ordered `ControlOutcome`s | Target values, recommendation facts, risk decisions, or mode changes within an existing run |
| Stream/run-input authority | Ordering of accepted controls, timers, market/control inputs, effective positions, and selected cuts | Portfolio sizing meaning or target approval |
| Market-state/reference authorities | Listing, instrument, reference, and market-state lineage consumed through recommendation and approved snapshots | Portfolio target construction or risk authorization |
| Dataset/replay and research registries | Backtest result bundles, promotion state, experiment lineage, and paper-candidate evidence | Runtime target construction, paper execution, or risk authority |
| Risk-policy authority | Companion-leaf final target assessment and risk decision | Creating portfolio targets or rewriting sizing policy outputs |
| Exposure/reservation authority | Companion/later serialized projected exposure and reservation lifecycle | Creating portfolio targets or risk decisions |
| Observability | Non-authoritative metrics, traces, logs, health projections, latency points, and evidence export | Target validity, missing target inference, risk approval, or capital truth |
| Query/report/UI tools | Disposable read models of recommendations, targets, and explanations | Authoritative target state or behavior-changing control |

Portfolio construction may be implemented in the same process as risk during early development, but the authorities remain separate. The companion risk leaf consumes target candidates and owns approval/rejection/modification semantics. This leaf stops at construction.

## Canonical concepts

### Portfolio

A `Portfolio` preserves the Phase 01 meaning: the accounting and risk scope within which target positions, exposure, cash, positions, and P&L are evaluated. In this leaf, portfolio construction owns only the portfolio configuration needed to turn eligible recommendations into portfolio-scoped target candidates. It does not own risk-policy approval, exposure reservation, execution, accounting positions, or P&L.

The portfolio record may bind to finer-grained `risk_scope_id` values in the companion risk leaf, but that binding refines downstream limit and serialization partitions; it does not redefine the portfolio or make portfolio construction the risk authority.

A portfolio record contains at minimum:

- `portfolio_id`;
- mode: `Replay analysis`, `Backtest/paper replay`, `Live read-only`, `Live paper`, `Human-approved live`, or later `Guarded automated live`, with `paper_candidate` represented separately as a Phase 06 promotion state rather than a runtime mode;
- owner and permitted operators;
- base currency or unit of account;
- allowed venues, instruments, listings, market types, and external-opportunity classes;
- strategy instances and recommendation streams eligible to influence the portfolio;
- sizing-policy version and aggregation-policy version;
- target validity and supersession policy;
- downstream `risk_scope_id` reference for the companion leaf, when the portfolio's canonical risk/accounting scope is partitioned for limit evaluation or reservation serialization;
- data-quality, reference, and listing eligibility policy;
- capital-snapshot requirement and freshness policy;
- reset lineage and active configuration epoch.

A portfolio is not an account ledger, venue account, broker balance, executable order book, or UI watchlist. It may map to one or more accounts, sleeves, or risk scopes, but the target policy must declare that relationship explicitly. Portfolio construction may not use that mapping to issue risk decisions or reservations.

The portfolio mode is inherited from the immutable run manifest. Portfolio construction may validate mode-compatible policy and assignment records, but it cannot accept or own a behavior-changing mode transition. Changing from one canonical mode to another requires a new run or child run with fresh manifest, recovery lineage, and mode-compatible component activation.

### Account

An `Account` is an externally or internally represented custody/execution context that can later support paper or live balances, orders, fills, and reconciliation. In this leaf, account information is input only through approved snapshots. Portfolio construction cannot mutate an account and cannot infer account state from UI projections, broker screens, or cached execution objects.

Minimum account reference fields for target construction are:

- `account_id`;
- mode and account type: research placeholder, paper simulated, live read-only, or later live executable;
- eligible portfolio bindings;
- base currency and conversion policy reference;
- listing/venue eligibility;
- snapshot identity, source authority, freshness, and completeness status;
- restrictions relevant to construction, such as disabled account, read-only account, unknown balance, or unsupported market.

If account/capital evidence is missing, stale, recovering, or incompatible, portfolio construction must produce an explicit no-change or construction-rejected outcome according to policy. It must not reuse a hidden previous balance.

### Capital scope

`CapitalScope` defines the amount and type of capital a sizing policy may consider for a target decision. It may be:

- static paper notional configured for research-to-paper transition;
- paper account equity/cash snapshot once Phase 08 introduces authoritative paper accounting;
- live account/balance observations only in modes that explicitly authorize live-account evidence, such as live-read-only projections or later Phase 11 live execution. They are not live-paper sizing truth and cannot be used to convert a paper account into a live account;
- a sleeve allocation inside a portfolio;
- a strategy budget, group budget, or opportunity budget declared by policy.

In `Live paper`, trade-capable sizing must use paper-accounting snapshots, configured paper capital, or explicitly labelled assumptions permitted by policy. A live-read-only balance display may inform an operator projection, but it is non-authoritative for live-paper target construction unless a later approved live-mode phase introduces a governed live-account snapshot contract.

Capital scope is an input to sizing, not risk approval. A sizing policy may say “construct a desired target using at most this capital scope,” but only risk/reservation can decide whether the target may proceed. Unknown capital scope fails closed into no-change or construction rejection.

### Portfolio state snapshot

A `PortfolioStateSnapshot` is the immutable input cut used for one target-construction decision. It is not a mutable portfolio object.

It contains:

- portfolio identity, configuration epoch, and policy versions;
- account and sleeve snapshot references;
- current known holdings or paper-position references when such authorities exist;
- capital scope and freshness status;
- active target records already accepted by portfolio construction;
- downstream target statuses published back from risk/reservation/execution/accounting when those phases exist;
- reference/listing versions and market-state lineage needed for target interpretation;
- control effective position, timer cursor, and run-input sequence/cut;
- completeness, freshness, recovery, and degraded-state flags;
- semantic checksum and reconstruction lineage.

Before Phase 08, current holdings and account balances may be configured paper-transition assumptions or research-derived placeholders, but they must be explicitly typed as assumptions. They cannot be presented as fills, ledger entries, or actual positions.

### Target key

A `TargetKey` defines the uniqueness and supersession scope for portfolio construction. V1 single-instrument target keys are absolute-position scoped:

```text
(portfolio_id, account_or_sleeve_scope, canonical_instrument_or_opportunity_scope, target_policy_version)
```

The exact key schema is versioned. It must declare whether the target is:

- single listing;
- canonical instrument across equivalent listings;
- opportunity/resolution output with selected listing;
- multi-leg opportunity placeholder for a later phase;
- external/Polymarket opportunity placeholder for a later phase.

For V1 single-instrument absolute targets, long/short direction, side, and signed amount are values inside `TargetPosition`, not identity fields. Opposing recommendations for the same portfolio/account/instrument/policy must resolve into one target, one no-change outcome, or one construction rejection; they cannot avoid supersession by occupying separate long and short keys.

Future target-key schemas may add a registered `exposure_domain` only when the domains are economically distinct and can safely coexist, such as explicitly separated strategy sleeves, option-like payoff domains, or approved multi-leg opportunity components. That field must not encode ordinary buy-versus-sell direction for the same absolute-position scope.

Two targets with the same active key cannot both be current. A later accepted target for the same key supersedes the earlier target under the target lifecycle policy. Distinct keys may coexist only when their risk-scope bindings and opportunity semantics prove they are economically distinct and safe to evaluate independently.

### Absolute target position

A `TargetPosition` is the portfolio-construction authority's immutable desired absolute exposure for one target key.

It contains at minimum:

- `target_id`;
- target key and schema version;
- portfolio, account/sleeve, and mode;
- target side/direction or exposure domain;
- absolute desired quantity, notional, normalized exposure, probability-risk unit, or other registered unit;
- reference price/mark/term used only to interpret the target amount;
- current-state snapshot identity used to construct it;
- computed delta versus the snapshot, clearly labeled as explanatory and non-executable;
- sizing-policy version, aggregation-policy version, arithmetic profile, rounding policy, and canonical checksum;
- source recommendation set and weights/contributions;
- validity interval, expiry, supersession scope, and downstream eligibility;
- construction status: `target_constructed`;
- ranked explanation factors and complete lineage;
- publication and consumer-acknowledgement lifecycle facts required by Phase 03.

The target amount is absolute: “desired final exposure for this key,” not “buy this many” or “sell this many.” Any delta is a derived explanation and is not an order instruction. Later phases may translate an approved and reserved target into order intents, but this leaf does not.

### No-change outcome

A `PortfolioNoChange` is an accepted terminal outcome for a selected target-construction obligation when the correct portfolio action is to leave the current absolute target unchanged or create no new target.

No-change is required when policy selects the construction cut but one of the following applies:

- active actionable recommendations aggregate to the same absolute target after rounding and minimum-change policy;
- all eligible actionable recommendations cancel out under declared aggregation rules;
- current target remains active and no stronger/superseding recommendation exists;
- capital scope is zero by policy in a way that requires no target rather than construction rejection;
- target would violate a portfolio-construction-only bound that is not a risk decision;
- recommendation input becomes expired or superseded before the selected construction cut;
- configured policy intentionally suppresses churn within declared hysteresis.

No-change is not a hold recommendation, risk rejection, reservation denial, failed execution, or missing output. It must preserve causal references, policy versions, and snapshot identity so replay can prove why no target changed.

### Construction rejection

A `PortfolioConstructionRejected` outcome records that portfolio construction selected a cut but could not safely construct a target or no-change because the request or inputs were invalid for this authority.

Examples:

- unknown portfolio, account, strategy, listing, unit, policy, or schema version;
- malformed recommendation reference;
- recommendation is hold, expired, superseded, unpublished, unacknowledged, or not actionable;
- portfolio-state snapshot is stale, incomplete, recovering, or unavailable beyond policy;
- capital scope is unknown or incompatible;
- listing/reference resolution is ambiguous and no approved opportunity-resolution seam exists;
- strategy is not assigned to the portfolio or is not in a permitted promotion state;
- policy output fails validation, arithmetic bounds, or canonicalization.

Construction rejection is terminal for the selected obligation. It does not imply risk rejection and must not be counted as an executed, approved, or reserved target.

## Fact taxonomy activated by this leaf

This leaf activates the `portfolio.*` construction namespace additively. It does not activate `risk.*`, `reservation.*`, `order.*`, `fill.*`, `ledger.*`, or `execution.*` semantics.

Minimum portfolio-construction fact types:

- `portfolio.definition.registered`;
- `portfolio.assignment_definition.registered`;
- `portfolio.sizing_policy.registered`;
- `portfolio.aggregation_policy.registered`;
- `portfolio.snapshot.accepted`;
- `portfolio.construction_obligation.accepted`;
- `portfolio.target.constructed`;
- `portfolio.no_change`;
- `portfolio.construction_rejected`;
- `portfolio.construction.interrupted`;
- `portfolio.target.expired`;
- `portfolio.target.superseded`;
- `portfolio.target.invalidated_by_control`;
- `portfolio.target.invalidated_by_snapshot`;
- `portfolio.target.withdrawn_by_run_lifecycle`;
- portfolio target publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Behavior-changing assignment, sizing-policy activation, aggregation-policy activation, pause, resume, stop, abort, and reset facts remain authoritative only as accepted ordered `ControlOutcome`s under `run.control.*`. The `portfolio.*` namespace records portfolio-owned construction facts and derived lifecycle facts; it does not create a second activation event for behavior-changing controls.

Concrete encoded names are fixed in the schema registry. Metrics, traces, query rows, UI controls, notebooks, spreadsheets, comments, or imported files are not authoritative portfolio-construction facts.

## Research-to-paper transition scope

Phase 06 can promote strategy/recommendation evidence to `paper_candidate` as a research-registry fact. That state is necessary but not sufficient for portfolio construction.

For a strategy instance to influence a portfolio target, the run/configuration authority must activate an assignment containing:

- strategy instance and recommendation stream identity;
- research promotion state and result-bundle references;
- permitted portfolio(s), account/sleeve scopes, instruments/listings, and mode;
- sizing policy and aggregation policy bindings;
- capital scope binding;
- input freshness and target validity policy;
- maximum simultaneous target keys and opportunity classes;
- observability profile and benchmark workload binding;
- effective position through an accepted ordered `ControlOutcome`.

The assignment does not mutate the strategy, recommendation, or research result. It only makes otherwise active actionable recommendations eligible for portfolio construction at or after the assignment's effective position.

Before Phase 08, paper-transition portfolios may use configured capital assumptions and initial-position assumptions. These assumptions must be versioned, lineage-bearing, and clearly labeled. They are not paper-accounting truth. Once paper accounting exists, the policy must declare whether assumption-backed snapshots are still permitted for research replay only or are disallowed for paper operation.

## Input eligibility

Portfolio construction may consume a `TradeRecommendation` only when all conditions hold:

1. it is `actionable`;
2. it is accepted and published by the recommendation authority;
3. it is active at the selected construction cut;
4. its signal, evaluation, feature, state, timer, control, and recommendation lineage are complete under the run's replay/mode policy;
5. it has not expired, been superseded, or been invalidated before the selected construction cut;
6. its strategy instance is assigned to the portfolio at the cut's control effective position;
7. its strategy/recommendation family is permitted by the portfolio policy and paper-candidate gate;
8. its listing/instrument/opportunity scope resolves under the declared target-key policy;
9. its indicative size unit and direction can be interpreted by the sizing policy;
10. its publication/consumer acknowledgement state satisfies Phase 03 downstream-consumption requirements.

Hold recommendations terminate before portfolio construction. Abstentions produce no recommendation and therefore no portfolio-construction input. Diagnostic or unavailable upstream facts may appear only as lineage explaining why no eligible actionable recommendation exists; they cannot be converted into target size.

`Live read-only` targets are proposed-target projections only. They may be constructed for operator visibility when the run manifest and policy permit proposed targets, but they are not downstream-eligible for authoritative risk obligations, reservation requests, executable intents, paper orders, live orders, or fills. Downstream eligibility must be mode-gated in the target lifecycle before publication to risk.

If an upstream recommendation is valid but not eligible for the portfolio, the target-construction obligation records an exclusion reason. Exclusion is not an error unless the policy required that recommendation to be eligible.

## Sizing policy

A `PortfolioSizingPolicy` converts eligible actionable recommendations and a portfolio snapshot into an absolute target amount. It is versioned and immutable.

It declares:

- target key schema;
- eligible recommendation families and promotion states;
- capital scope and account/sleeve mapping;
- unit conversion and reference-price policy;
- strategy weights, caps, floors, budgets, confidence/score mappings, and concentration inputs that are construction-level rather than risk-level;
- aggregation function and conflict-resolution policy;
- minimum target size, minimum target change, hysteresis, and rounding rules;
- stale snapshot behavior;
- missing recommendation behavior;
- recommendation expiry and target expiry interaction;
- deterministic arithmetic profile;
- stable tie-breakers;
- explanation ranking policy;
- no-change versus construction-rejection rules;
- downstream risk-scope mapping;
- compatibility and migration rules.

Sizing policy may use:

- portfolio configuration and assignment records;
- eligible active actionable recommendations;
- approved portfolio/account/capital snapshots;
- current active portfolio targets for supersession/churn control;
- reference/listing conversion inputs exposed through approved snapshots;
- research promotion metadata and result-bundle references;
- deterministic seeds only if declared and included in the construction key.

Sizing policy may not use:

- unapproved query-model values;
- current wall time or host scheduling state;
- arbitrary database reads;
- UI state;
- account secrets or credentials;
- open order, fill, or ledger state except through an approved snapshot once later phases own those facts;
- risk limits, risk decision state, reservation availability, kill-switch state, or execution-adapter health as target-construction logic.

Risk-relevant constraints may appear as labels or downstream risk-scope hints, but their approval/rejection effect belongs to the companion risk leaf.

## Aggregation across strategies

Portfolio construction must be deterministic when multiple recommendations influence the same target key.

The aggregation policy defines:

- grouping from recommendation scope to target key;
- ordering by run-input position, recommendation issue position, strategy priority, policy weight, and stable identity tie-breaker;
- conflict handling for opposing directions;
- duplicate recommendation reference handling;
- maximum recommendation age and validity overlap;
- whether the policy is latest-wins, weighted blend, strongest-signal, budget allocator, ensemble, or another declared method;
- how strategy-indicative exposure is normalized into portfolio sizing units;
- how current target state participates in hysteresis and no-change logic;
- exact rounding points and canonical output encoding.

Aggregation cannot create extra `TradeRecommendation`s, edit recommendation size, or pretend conflicting recommendations are abstentions. It may produce one target, one no-change outcome, or one construction rejection per selected target key according to policy.

If multiple target keys compete for the same capital scope, this leaf may rank or allocate desired targets according to the construction policy. That ranking does not reserve capital and does not prove the set is risk-feasible. The companion risk/reservation path must still serialize and approve/deny downstream economic use.

## Target validity and supersession

A constructed target is immutable. Later changes occur through lifecycle facts:

- `portfolio.target.expired`;
- `portfolio.target.superseded`;
- `portfolio.target.invalidated_by_control`;
- `portfolio.target.invalidated_by_snapshot`;
- `portfolio.target.withdrawn_by_run_lifecycle`;
- later risk/execution/accounting status references published by their authorities.

The target validity policy declares:

- validity start and end;
- maximum age in logical positions, timer facts, or source-time basis;
- supersession key;
- required recommendation-active overlap;
- behavior when source recommendations expire;
- behavior when portfolio snapshot becomes stale or is replaced;
- behavior when account/capital scope changes;
- behavior when assignment, sizing policy, or portfolio configuration changes;
- downstream eligibility after expiry or supersession.

An expired or superseded target cannot create new downstream risk work. Already accepted downstream risk or reservation work remains governed by its own authority and lifecycle. Portfolio construction may reference downstream statuses for future sizing snapshots only through approved feedback snapshots; it cannot mutate them.

## Multi-listing, opportunity, and external seams

The Phase 01 architecture reserves opportunity/resolution as a distinct authority. This leaf provides seams without implementing those products.

V1 single-listing flow:

```text
active actionable TradeRecommendation
  -> portfolio construction groups by target key
  -> absolute TargetPosition or PortfolioNoChange
  -> companion risk leaf
```

Future opportunity flow:

```text
active actionable recommendations
  -> opportunity/resolution authority resolves economic opportunity, legs, listings, and canonical scope
  -> portfolio construction sizes the resolved opportunity into absolute target(s)
  -> companion risk leaf
```

Portfolio construction must not independently choose equivalent venues, compose multi-leg trades, infer cross-listing arbitrage, or rank Polymarket/external opportunities unless those decisions are activated by the opportunity/resolution authority with lineage. Until that phase exists, ambiguous listing or opportunity scope produces construction rejection or no-change under policy.

The target key schema must reserve fields for:

- canonical instrument versus listing-specific targets;
- opportunity identity;
- leg identity and hedge relationship;
- external-observation lineage;
- venue/listing eligibility;
- target netting policy.

Reserved fields must be explicit and versioned; empty extension slots cannot silently change target identity later.

## Operator target exclusion

Operators may configure portfolios, activate assignments, pause/resume/stop/reset construction, and inspect targets through approved control and query paths. They may not directly enter “target position = X” as an authoritative portfolio target in this leaf.

Manual target entry would require a later approved product decision defining:

- owning authority;
- command schema and authorization;
- actor identity and approval policy;
- effective-position behavior through exactly one ordered `ControlOutcome`;
- interaction with strategy-generated targets;
- risk/reservation/execution handoff;
- audit, replay, and recovery semantics;
- UI safeguards and evidence gates.

Until that exists, direct target edits, spreadsheet imports, console overrides, and notebook-created targets are non-authoritative and cannot enter the risk path.

## Scheduling and deterministic construction

Portfolio construction is scheduled from deterministic inputs:

- publication of active actionable recommendations to the portfolio authority;
- recommendation expiry/supersession lifecycle facts;
- portfolio-state snapshot publications;
- accepted ordered `ControlOutcome`s for portfolio assignment, sizing policy, pause/resume, stop, abort, reset, and mode-compatible configuration changes;
- recorded run-timer facts for validity, cadence, and churn-control policies;
- explicit portfolio policy cadence.

Each selected construction obligation has a canonical construction key:

- run identity and mode;
- portfolio identity and configuration epoch;
- target key;
- selected recommendation set and lifecycle statuses;
- portfolio-state snapshot identity;
- active target identity, if relevant;
- sizing and aggregation policy versions;
- reference/listing versions;
- control effective position and timer cursor;
- deterministic arithmetic and identity policy;
- replay class and dataset/run-input lineage where applicable.

Mode is an immutable input from the run manifest, not a portfolio-construction control. A mode-incompatible configuration change is rejected by run/configuration authority or requires a new/child run. Portfolio construction must not convert an existing target, portfolio, account binding, or downstream eligibility from one mode to another in place.

The key excludes host wall time, thread identity, queue order, storage iteration order, telemetry state, UI state, current risk queue state, execution-adapter state, and arbitrary account cache state.

If a policy selects a construction cut, the authority must accept the obligation and terminate it exactly once as:

- `target_constructed`;
- `no_change`;
- `construction_rejected`;
- `portfolio_construction_interrupted` operational fact with recovery/incomplete disposition under Phase 03.

Resource pressure may not silently skip, coalesce, reorder, or drop selected obligations. Sparse cadence, latest-only semantics, batching, and deduplication are allowed only when declared by policy and represented in the construction key.

## Pause, stop, abort, and reset

Portfolio construction lifecycle changes are behavior-changing controls. They take effect only through accepted ordered `ControlOutcome`s and recorded effective positions. Accepted controls contain the complete behavior change and no second activation fact is emitted. Rejected controls are ordered history but have no target-construction effect.

Pause behavior:

- blocks new post-effective construction admissions;
- does not mutate existing targets;
- does not erase accepted construction obligations before the effective position;
- causes post-effective recommendation publications to remain unconsumed or explicitly excluded according to policy;
- may allow diagnostic projection and snapshot capture to continue.

Stop/abort behavior:

- prevents new construction admissions after the effective position;
- resolves pre-effective accepted obligations according to the terminal policy;
- cannot fabricate no-change outcomes merely to hide incomplete work;
- must mark affected run scope incomplete/non-faithful if accepted obligations cannot be recovered or terminally resolved.

Reset behavior:

- creates a new portfolio-construction epoch or child run as declared by Phase 03 run lifecycle;
- does not delete prior targets, no-change outcomes, rejections, explanations, or audit history;
- invalidates or supersedes active targets only through explicit lifecycle facts;
- requires fresh assignment, snapshot, and policy lineage before new target construction.

Host command receipt time, UI delay, queue delay, or process restart cannot change the effective position of a portfolio lifecycle control.

## Persistence, recovery, and replay

The portfolio-construction authority must persist or reconstruct:

- portfolio definitions, assignments, sizing policies, aggregation policies, and target-key schema versions;
- accepted construction obligations and construction keys;
- terminal target/no-change/rejection outcomes;
- target lifecycle facts;
- selected recommendation sets and exclusion reasons;
- portfolio-state snapshot references and semantic checksums;
- explanation factors and lineage references;
- publication and consumer-acknowledgement lifecycle facts;
- operational interruption and recovery dispositions.

Same-run recovery must reproduce accepted target identities and pending publication obligations exactly where identity was already accepted. Independent equivalent runs compare deterministic target identities only when the identity policy declares deterministic derivation; otherwise they compare semantic lineage, target key, target amount, no-change/rejection reason, checksums, and explanation equivalence.

Faithful replay must use the original pinned portfolio definitions, assignments, sizing policies, aggregation policies, recommendation facts, target-key schema, snapshots, timer/control facts, reference/listing versions, and replay manifest. Current-code sizing, current portfolio assignments, current reference data, or patched recommendation sets cannot be substituted and still called faithful replay.

## Lineage and explanations

Every terminal construction outcome carries causal lineage and ranked explanations.

For `TargetPosition`, explanations include:

- source recommendations and their strategy/signal lineage;
- strategy weights and aggregation contributions;
- capital scope and snapshot facts used by sizing;
- current target/position snapshot contribution where applicable;
- reference/listing conversion terms;
- policy caps/floors/hysteresis/rounding effects;
- recommendation exclusions from the same construction cut;
- control epoch and assignment state;
- timer/validity basis.

For `PortfolioNoChange`, explanations include:

- selected recommendation set or reason no eligible recommendations existed;
- current target/snapshot comparison;
- exact no-change rule triggered;
- rounding/hysteresis/cancel-out evidence where applicable;
- expired/superseded input evidence where applicable.

For `PortfolioConstructionRejected`, explanations include:

- stable rejection reason code;
- invalid or missing input references;
- policy and schema version that rejected the construction;
- whether the failure is configuration, data-quality, snapshot, eligibility, lineage, or arithmetic related.

Explanation ranking is deterministic and versioned. Equal ranks use declared stable tie-breakers. Explanation text cannot hide influential contributors, collapse external observations into anonymous scores, or introduce unbounded metric-label cardinality.

## Observability and performance

Observability is non-authoritative. It may report construction health, target/no-change/rejection rates, stale snapshot rates, recommendation exclusion populations, supersession rates, publication state, queue depth, deadline misses, and explanation completeness. It cannot infer target validity from absence of errors or create a target from metrics.

Canonical Phase 02 endpoint adoption:

- parent `portfolio_risk` segment remains `trade_recommendation.published.portfolio_authority` to `risk_decision.accepted`;
- this leaf defines a portfolio-construction subsegment from `trade_recommendation.published.portfolio_authority` to `portfolio_target.accepted`;
- companion risk defines the later subsegment from `portfolio_target.published.risk_authority` to `risk_decision.accepted`;
- publication, recoverability, and consumer acknowledgement are measured as explicit lifecycle points, not collapsed into acceptance.

Metrics and traces must distinguish:

- target constructed, no-change, construction rejected, and operationally interrupted outcomes;
- input excluded because hold, expired, superseded, not assigned, not paper-candidate, invalid lineage, stale snapshot, or unresolved listing;
- single-recommendation versus multi-recommendation aggregation;
- single-listing versus reserved opportunity/multi-listing seams;
- recommendation publication delay, construction queue wait, computation, output validation, publication, and consumer acknowledgement;
- canonical runtime modes: `Replay analysis`, `Backtest/paper replay`, `Live read-only`, `Live paper`, `Human-approved live`, and later `Guarded automated live`;
- Phase 06 promotion state, including `paper_candidate`, as a separate eligibility dimension rather than a mode label;
- portfolio/account/sleeve dimensions only under bounded cardinality and access-control rules.

Performance evidence follows the Phase 02 method. Phase 07 must register workloads for:

- recommendation bursts into one portfolio;
- many portfolios consuming one recommendation stream;
- many strategies competing for one target key;
- stale snapshot and snapshot-refresh races;
- pause/reset under backlog;
- large explanation sets under bounded cardinality;
- replay throughput and externally paced paper-mode arrival;
- overload and recovery without silent target loss.

No numeric latency, capacity, or SLO target is accepted merely because an implementation happens to pass it. Budgets must be evidence-derived and scoped to a workload, environment, instrumentation profile, and correctness population.

## Testing strategy

### Contract and unit tests

Cover:

- portfolio/account/capital schema validation;
- strategy-to-portfolio assignment eligibility and promotion-state checks;
- actionable recommendation consumption;
- hold/abstention/non-recommendation exclusion;
- target-key canonicalization and duplicate handling;
- absolute target versus delta labeling;
- sizing-policy unit conversion, rounding, caps, floors, and hysteresis;
- no-change outcome rules;
- construction rejection reason taxonomy;
- target validity, expiry, invalidation, and supersession;
- operator target-entry rejection;
- publication and consumer acknowledgement lifecycle facts.

### Property and model-based tests

Generate:

- recommendation publication, expiry, supersession, and duplicate sequences;
- multiple strategies with opposing, reinforcing, stale, and boundary-sized recommendations;
- portfolio assignment changes around exact effective positions;
- snapshot freshness and replacement races;
- capital scopes at zero, near bounds, unknown, stale, and incompatible units;
- target-key collisions and distinct opportunity/listing scopes;
- pause, stop, abort, reset, crash, and replay permutations;
- arithmetic boundary cases and stable tie-breakers.

Properties include:

1. only active actionable recommendations can produce constructed targets;
2. hold recommendations and abstentions never produce targets;
3. each accepted construction obligation terminates exactly once;
4. each target is absolute and immutable;
5. at most one active target exists per target key;
6. no-change is explicit and lineage-bearing;
7. construction rejection never masquerades as risk rejection;
8. sizing uses only declared snapshots and policy inputs;
9. outputs do not depend on host timing, queue order, UI state, query models, risk state, reservation state, or execution state;
10. equivalent manifests produce equivalent target semantics under the identity policy.

### Failure, recovery, and security tests

Inject failure:

- before/after construction-obligation acceptance;
- during recommendation-set selection;
- during snapshot lookup/validation;
- during sizing and aggregation;
- before/after target/no-change/rejection acceptance;
- during target lifecycle publication;
- during pause/reset effective-position crossing;
- during replay and same-run recovery.

Security tests attempt:

- forged recommendation identities;
- stale or unauthorized account snapshots;
- direct target entry through UI/API/test fixtures;
- cross-portfolio recommendation leakage;
- strategy assignment spoofing;
- query-model substitution;
- risk/reservation/execution-state reads;
- unbounded explanation labels;
- registry downgrade or schema spoofing.

Failures must be typed, bounded, and recoverable or explicitly marked incomplete/non-faithful under Phase 03. They must not leak credentials, account secrets, proprietary strategy internals beyond approved explanations, or source payloads.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of this leaf and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **PC-E01 — Portfolio/account/capital registry** | Portfolio, account reference, sleeve, capital scope, target key, sizing policy, aggregation policy, fact type, lifecycle, and owner schemas | One owner for portfolio construction; account/capital inputs are snapshots with freshness and lineage; unknown required semantics fail closed |
| **PC-E02 — Recommendation consumption suite** | Actionable, hold, abstention, expired, superseded, unpublished, duplicate, unassigned, non-paper-candidate, stale-lineage, and wrong-portfolio cases | Only active actionable eligible recommendations enter target construction; all exclusions are explicit and lineage-bearing |
| **PC-E03 — Absolute target semantics suite** | Quantity, notional, normalized exposure, zero, sign, rounding, current-target comparison, delta explanation, and canonical checksum cases | Targets encode desired absolute exposure; deltas are explanatory only; no target is an order or executable instruction |
| **PC-E04 — Aggregation and no-change suite** | Reinforcing, opposing, latest-wins, weighted, tie, cancel-out, hysteresis, minimum-change, duplicate, and multi-strategy cases | Exactly one target or no-change/rejection per selected key; tie-breaking is deterministic; no-change is explicit |
| **PC-E05 — Sizing-policy input isolation** | Attempts to read query models, UI state, risk limits, reservations, execution health, open orders, fills, ledgers, wall time, or arbitrary stores | Sizing uses only declared policy inputs and approved snapshots; forbidden access fails closed |
| **PC-E06 — State snapshot conformance** | Fresh, stale, recovering, incomplete, incompatible, replaced, and assumption-backed paper-transition snapshots | Snapshot identity and semantic checksum are carried; stale/unknown snapshots produce declared no-change or construction rejection |
| **PC-E07 — Target validity and supersession suite** | Active, expired, superseded, invalidated by control, invalidated by snapshot, reset, and downstream-status feedback cases | Targets are immutable; lifecycle facts are monotonic; at most one active target exists per target key |
| **PC-E08 — Multi-listing/opportunity seam fixture** | Single listing, canonical instrument, unresolved equivalent listings, reserved opportunity fields, multi-leg placeholder, external-observation placeholder | Unimplemented opportunity choices are rejected or no-change under policy; target identity reserves seams without silent semantic change |
| **PC-E09 — Operator target exclusion suite** | API, UI, import, notebook, replay, and test attempts to create direct targets | No direct operator-created target can become authoritative before a later approved command path exists |
| **PC-E10 — Control lifecycle fixture** | Assignment, policy change, pause, resume, stop, abort, reset, rejected controls, and effective-position boundary cases | Behavior-changing controls use exactly one ordered `ControlOutcome`; rejected outcomes have no effect; host receipt time has no semantic effect |
| **PC-E11 — Scheduling and deterministic replay suite** | Recommendation publications, timers, snapshots, controls, batches, sparse cadence, latest-only policy, and storage/order permutations | Same ordered inputs produce same construction obligations and semantic outputs; policy-declared coalescing is explicit in the key |
| **PC-E12 — Persistence and recovery matrix** | Crash points around obligation acceptance, target/no-change/rejection acceptance, lifecycle publication, consumer acknowledgement, and reset | Same-run recovery preserves accepted identities and pending obligations; unrecoverable loss marks run/scope incomplete or non-faithful |
| **PC-E13 — Explanation and lineage corpus** | Constructed target, no-change, rejection, multi-strategy, stale snapshot, exclusion, and later opportunity placeholder cases | Explanations are deterministic, bounded, ranked, and causally complete back to recommendations, snapshots, controls, timers, policies, and datasets |
| **PC-E14 — Observability and latency extension** | Phase 02 schemas, portfolio-construction subsegments, bounded dimensions, profiles, alerts, loss cases, and publication states | Telemetry does not redefine canonical endpoints or become authority; populations and incomplete segments are distinguishable |
| **PC-E15 — Performance/SLO adoption** | Registered workloads, environments, arrival models, correctness oracles, tails, saturation, resource use, and regression thresholds | Budgets are evidence-derived; overload cannot silently skip selected construction obligations |
| **PC-E16 — Cross-phase compatibility review** | Approved Phases 01-06 plus companion risk boundary checklist | No upstream authority, lifecycle, ordering, replay, telemetry, recommendation cardinality, research boundary, or risk/reservation boundary is weakened |

## Deferred choices

The following choices are intentionally deferred:

- exact initial portfolio definitions, account mappings, sleeves, and capital amounts;
- exact sizing formulas, strategy weights, caps, floors, and allocation objectives;
- whether V1 target units are quantity, notional, normalized exposure, or multiple registered unit families;
- whether paper-transition snapshots may include configured starting positions or must wait for Phase 08 paper accounting;
- exact opportunity/resolution product, including multi-listing, multi-leg, venue selection, and Polymarket/external discovery;
- exact downstream risk decision schema, risk limits, risk modifications, reservations, and projected-exposure serialization;
- exact paper broker, fill model, ledger, accounting, and P&L semantics;
- whether and how operator-entered targets become a later product;
- concrete persistence implementation, process boundaries, scheduling mechanism, metric names, dashboards, and alert thresholds;
- numeric performance budgets and SLO values.

None of these deferred choices may weaken the approved separation between recommendation, portfolio target construction, risk approval, reservation, execution, and accounting. A target remains an absolute desired portfolio state, not permission to trade.
