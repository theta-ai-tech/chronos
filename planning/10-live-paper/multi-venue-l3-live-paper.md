# Phase 10B: Multi-Venue and L3 Live Paper

## Purpose

This document defines the planning contract for extending Chronos live paper from one selected venue/L2 baseline to bounded multi-venue and optional L3 market-state support. The goal is not to jump to live execution. The goal is to prove that additional market-data complexity, cross-venue synchronization, opportunity resolution, and grouped paper execution can be added without redefining prior single-venue L2 meanings or weakening safety/accounting controls.

Phase 10B builds on Phase 10A operations evidence, Phase 04 market-state seams, Phase 05 recommendation cardinality, Phase 07 target/risk/reservation semantics, and Phase 08 paper execution/accounting.

## Objectives

Multi-venue/L3 live paper must establish that Chronos can:

1. add a second venue/source family with distinct symbols, cursors, timing, depth, and quality semantics;
2. preserve one authoritative state partition per venue listing and never collapse venue cursors into a fake scalar sequence;
3. construct run-wide bundles with explicit synchronization quality across venues/listings;
4. add L3 order-by-order state as an additive capability without changing existing L2 state meaning;
5. derive L2 projections from L3 only when capability and lineage explicitly declare the derivation;
6. support cross-venue features and opportunities only from declared complete bundles;
7. compose actionable recommendations into opportunities without creating duplicate `TradeRecommendation`s;
8. resolve opportunity legs to candidate listings before portfolio target construction;
9. simulate grouped paper execution and residual exposure for multi-leg/cross-venue opportunities without claiming venue atomicity;
10. measure resource, latency, synchronization, replay, recovery, and quality behavior under bounded venue/cardinality assumptions;
11. preserve paper/live isolation and all Phase 10A operational controls.

## Scope

### In scope

- second venue/source conformance and adapter contract extension;
- per-venue listing state partitions, lineage, quality, and synchronization metadata;
- multi-venue bundle construction and cross-venue feature eligibility;
- L3 capability negotiation, order-event state, derived-L2 projection policy, and resource bounds;
- opportunity and resolution pipeline contracts needed for cross-venue live paper;
- grouped paper execution/residual-exposure simulation placeholders;
- multi-venue/L3 soak, replay, recovery, and audit evidence;
- console/API projection requirements for multi-venue quality and grouped state.

### Out of scope

- live venue execution, live credentials, external order submission, or real-money cross-venue trading;
- external/Polymarket discovery beyond venue market-data sources, owned by Phase 11A;
- guarded automated live execution, owned by Phase 11C if pursued;
- unbounded venue count, unbounded instrument universe, or production market-making scale;
- exact arbitrage/strategy formulas or alpha claims.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Source adapter/capture | Per-source connectivity, source events, source cursors, gap/reconnect facts | Cross-venue meaning or strategy decisions |
| Normalization/reference | Venue-specific decoding, symbol/listing mapping, reference compatibility | Market-state mutation or opportunity ranking |
| Market-state authority | Per-venue listing state, L2/L3 capability-specific views, bundle lineage, synchronization quality | Strategy, opportunity ranking, risk approval |
| Feature authority | Cross-venue feature observations from declared bundles | Trade recommendations or target construction |
| Strategy/recommendation authority | Valid signals and exactly one actionable/hold `TradeRecommendation` per valid signal | Opportunity composition, portfolio sizing, risk/execution authorization |
| Opportunity/resolution authority | Opportunity composition, leg identity, candidate listing resolution, ranking/explanation | Strategy evaluation, target sizing, risk approval, execution authorization |
| Portfolio/risk/reservation authorities | Targets, risk decisions, reservations for resolved opportunities | Opportunity discovery/ranking or venue state |
| Paper execution/accounting | Grouped paper intents/orders/fills and ledger effects under paper models | Live venue truth or atomicity claims |
| Operations/observability | Quality, synchronization, capacity, soak, runbooks, projections | Domain facts or source continuity invention |

## Multi-venue market state

Multi-venue state must preserve:

- one source stream identity per venue/channel/scope;
- one normalized stream identity per venue/listing/event class as declared;
- one authoritative market-state partition per venue listing;
- venue-specific reference, fee, market-status, trading-rule, timestamp-quality, and depth semantics;
- complete `StateLineage` vectors for every bundle member;
- synchronization-quality metadata for cross-venue cuts.

Chronos must not:

- merge venue cursors into one scalar sequence;
- claim external simultaneity without synchronization policy and uncertainty;
- erase venue-specific freshness, gap, fee, halt, or status evidence;
- use a cross-venue feature cut outside an explicit complete bundle;
- treat missing venue data as neutral.

## L3 capability

L3 state is additive and capability-distinct. It contains order-by-order events only when the venue/source contract supports them and resource bounds are accepted.

Rules:

- native L2, native L3, and derived-L2 views are distinct capabilities;
- L3 may derive an L2 projection only through an explicit derivation policy with lineage;
- L3 unknown payloads may be preserved by source/normalization contracts but do not affect L2 state until the L3 authority accepts them;
- L3 resource use, history retention, and replay cost are separately budgeted;
- strategies/features must declare whether they consume native L2, native L3, or derived L2.

L3 cannot silently replace existing L2 semantics or make earlier L2 tests invalid.

## Bundle synchronization

A `MultiVenueBundle` is an immutable run-wide cut across venue listing views. It contains:

- bundle identity and schema version;
- member venue/listing state view identities;
- complete cursor vector for every member;
- local/replay logical time and source timestamp quality;
- synchronization policy version;
- maximum skew or uncertainty where measured;
- completeness/freshness/degraded flags per member and aggregate;
- quality reason codes;
- semantic checksum.

Features, opportunities, and explanations must cite the bundle. If a required member is stale, missing, halted, gapped, recovering, or incompatible, the bundle is non-consumable for any trade-capable cross-venue feature, opportunity, target, risk, reservation, or paper-execution path that declares that member as required. The allowed outcomes are explicit unavailable, abstention, no-change, construction rejection, or projection-only diagnostics.

A degraded path may produce trade-capable output only if the active feature/opportunity policy explicitly changes the dependency set before evaluation, proves the missing/degraded member is no longer required for that specific economic claim, and records the reduced dependency set, reason, quality downgrade, and lineage. It cannot silently treat missing venue data as neutral, carry forward stale values, or reuse a degraded/incomplete bundle while still claiming the original cross-venue dependency set was satisfied.

## Opportunity and resolution pipeline

An `Opportunity` may compose one or more actionable `TradeRecommendation`s into an economic candidate. It must not create a second strategy recommendation for the same signal.

Opportunity records include:

- source recommendation identities;
- opportunity identity and economic thesis type;
- leg definitions and intended relationships;
- source bundle and external/venue observations where applicable;
- ranking/scoring policy version;
- candidate listings and resolution status;
- explanation factors and quality flags.

`OpportunityResolution` maps opportunity legs to eligible listings and execution/accounting units. It occurs before target construction and cannot bypass portfolio, risk, reservation, or paper execution. Operator-facing “opportunity recommendation” cards are query models over opportunities and source recommendations, not new `TradeRecommendation` facts.

## Grouped paper execution and residual exposure

Multi-leg or cross-venue paper work may use execution groups. An execution group declares:

- related intents/orders/fills;
- per-leg reservation and risk lineage;
- partial-completion policy;
- residual exposure and unwind policy;
- cancellation/replacement coordination;
- grouped accounting and reconciliation expectations.

Execution groups do not claim venue atomicity. If one leg fills and another does not, residual exposure is explicit, reserved/accounted for under policy, and visible to risk, accounting, reconciliation, and operators.

## Resource and capacity constraints

Phase 10B must bound:

- venue count;
- listing/watchlist count;
- depth levels and L3 order-event count/rate;
- bundle member count;
- cross-venue feature and opportunity cardinality;
- metric dimension cardinality;
- storage growth and replay/rebuild cost;
- console projection complexity.

If bounds are exceeded, Chronos degrades safely through typed unavailable/degraded conditions. Increasing bounds without evidence is not an accepted fix.

## Replay and recovery

Replay must support:

- faithful capture-order replay with venue-specific arrival/capture order;
- normalized-fact replay with per-venue cursor vectors and bundle policy;
- L3 replay with model/versioned state application;
- opportunity/resolution replay from source recommendations and bundles;
- grouped paper execution replay with deterministic residual-exposure outcomes.

Recovery must preserve venue-specific gaps and synchronization quality. A recovered run cannot claim cross-venue opportunity validity if required member continuity is unproven.

## Mode behavior

- `Live paper` remains the primary mode and still routes only to paper broker/accounting.
- `Backtest/paper replay` may replay multi-venue/L3 datasets and grouped paper paths.
- `Live read-only` may display multi-venue opportunities as non-authoritative projections but cannot create risk/reservation/execution/accounting facts.
- `Human-approved live` and `Guarded automated live` are not enabled by Phase 10B evidence.
- Multi-venue/L3 evidence does not grant live execution authority.

## Observability and operations

Observability must include:

- per-venue source lag, gap, reconnect, and timestamp quality;
- bundle completeness, skew/uncertainty, and freshness;
- L3 event rate, state size, derivation cost, and replay cost;
- cross-venue feature/opportunity throughput and rejection reasons;
- grouped execution partial/residual outcomes;
- per-venue and aggregate resource use;
- synchronization-quality alerts and incident runbooks.

Console/API projections must show venue-specific quality and avoid presenting an aggregated “best” value without source and synchronization evidence.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **LP10B-E01 — Second-venue conformance suite** | Venue with incompatible symbols/cursors/timing/depth/status | Source/normalization/reference contracts preserve venue-specific meaning |
| **LP10B-E02 — Multi-venue bundle suite** | Complete, stale, gapped, skewed, halted, recovering venue members | Bundles carry complete lineage and synchronization quality; non-consumable required members cannot feed trade-capable output; degraded trade-capable paths require explicit reduced dependency sets |
| **LP10B-E03 — L3 additive suite** | Native L2, native L3, derived L2, unknown L3 payloads | L3 does not redefine existing L2 semantics and resource bounds are measured |
| **LP10B-E04 — Cross-venue feature/opportunity suite** | Declared bundles, missing members, duplicate source recommendations, opportunity cards | Opportunities do not create duplicate `TradeRecommendation`s, bypass target/risk, or treat incomplete required bundles as valid |
| **LP10B-E05 — Grouped paper execution suite** | Multi-leg fills, one-leg fill, cancel race, unknown leg, residual exposure | Residual exposure is explicit; no venue atomicity is claimed |
| **LP10B-E06 — Capacity/soak campaign** | Bounded venue/watchlist/L3/opportunity workloads | Measured budgets hold or degrade safely with classified breaches |
| **LP10B-E07 — Replay/recovery suite** | Capture replay, normalized replay, L3 rebuild, bundle reconstruction, opportunity replay | Replay reproduces semantic outcomes or marks continuity/quality unproven |
| **LP10B-E08 — Operations/console suite** | Multi-venue quality projections and incidents | Operators see per-venue quality/freshness and synchronization uncertainty |
| **LP10B-E09 — Paper/live isolation suite** | Attempts to infer live execution from multi-venue opportunity evidence | All execution remains paper-only; live authority is not introduced |

## Phase exit criteria

Phase 10B planning is complete when:

- second-venue and L3 additions preserve prior L2/single-venue meanings;
- multi-venue bundles have complete lineage and synchronization-quality semantics, and incomplete required bundles cannot produce valid trade-capable downstream output;
- opportunity/resolution pipeline preserves recommendation cardinality and cannot bypass portfolio/risk/reservation/execution;
- grouped paper execution handles residual exposure explicitly;
- resource bounds, replay, recovery, operations, and console evidence are defined;
- live execution remains deferred to Phase 11.

## Deferred decisions

Deferred to implementation or later phases:

- exact second venue and L3 source;
- exact synchronization/skew thresholds until measured;
- first concrete cross-venue feature/opportunity formulas;
- external/Polymarket discovery;
- live cross-venue execution and real-money residual management.
