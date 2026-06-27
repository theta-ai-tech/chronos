# Phase 11A: External and Polymarket Discovery

## Purpose

This document defines Chronos's external-observation, candidate-market discovery, Polymarket discovery, opportunity ranking, and opportunity-resolution planning contract. Discovery can enrich signals and operator-facing opportunities. Discovery does not grant execution authority.

This leaf builds on approved Phases 01-10. It preserves the rule that every valid strategy signal produces exactly one `TradeRecommendation`, and that external observations/opportunities cannot bypass target construction, risk, reservation, approval, execution, accounting, or reconciliation.

## Objectives

External and Polymarket discovery must establish that Chronos can:

1. ingest external observations as source-specific facts with provenance, quality, timing, correction, and availability state;
2. isolate external-source failures from the low-latency market-data core when disabled, degraded, slow, or hostile;
3. normalize Polymarket or other external market discovery separately from venue market-state authority;
4. rank candidate markets/listings/opportunities under versioned, replayable policies;
5. compose opportunities from actionable recommendations and external observations without creating duplicate `TradeRecommendation`s;
6. resolve opportunity legs to candidate listings/markets before portfolio target construction;
7. keep every influential external observation visible in downstream causal lineage and explanations;
8. mark stale, missing, low-quality, corrected, unavailable, or untrusted external evidence as non-consumable for trade-capable claims unless policy explicitly permits a reduced dependency set;
9. expose operator-facing opportunity cards as query models, not strategy recommendations or executable intents;
10. prove discovery replay, correction, ranking, quality, isolation, and audit behavior before any live execution adapter consumes a discovered opportunity.

## Scope

### In scope

- external observation authority, source adapters, quality scoring, correction, availability, and provenance;
- Polymarket discovery and candidate-market metadata normalization;
- opportunity identity, leg definitions, ranking, eligibility, and resolution;
- explanation lineage for external observations and discovery rankings;
- replay/rebuild of external observations, candidate rankings, and opportunity resolutions;
- isolation, rate limiting, schema validation, hostile-payload handling, and quality degradation;
- console/API projections for discovered opportunities;
- evidence gates proving discovery does not grant execution authority.

### Out of scope

- live Polymarket or venue execution adapters, live credentials, order submission, acknowledgements, fills, or settlement;
- human-approved-live approvals, owned by Phase 11B;
- guarded automation, owned by optional Phase 11C;
- final alpha formulas, production capital allocation, legal/compliance review, or ticket-level implementation plan.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| External-observation authority | Source-specific external observations, quality, correction, availability, provenance | Market-book state, opportunity ranking, strategy evaluation |
| Candidate discovery authority | Candidate market/listing discovery, metadata, eligibility inputs, Polymarket discovery facts | Strategy signals, portfolio sizing, execution authorization |
| Opportunity/resolution authority | Opportunities, legs, candidate ranking, resolution to eligible listings/markets, explanation lineage | Strategy evaluation, risk approval, reservation, execution |
| Strategy/recommendation authority | Exactly one recommendation per valid signal and signal explanations | Duplicating recommendations from opportunity cards |
| Portfolio/risk/reservation authorities | Targets, risk decisions, and reservations after resolved opportunity output | Discovery or ranking semantics |
| Operations/query models | Operator-facing opportunity views and investigation workflows | Authoritative recommendation, target, risk, or execution facts |
| Observability/audit | External-source health, quality, latency, isolation, evidence export | Discovery truth or execution authorization |

Discovery authorities may inform downstream decisions only through typed, versioned facts with explicit quality and lineage.

## Canonical concepts

### External observation

An `ExternalObservation` is a normalized fact from a source outside Chronos's venue market-data state, such as news, social data, oracle data, event metadata, market-discovery pages, or prediction-market state.

It contains:

- `external_observation_id`;
- source identity, source type, adapter version, and capture metadata;
- raw-source reference or content hash where retention permits;
- normalized payload schema and semantic version;
- source timestamp, capture timestamp, clock-quality metadata, and uncertainty;
- quality score/status and reason codes;
- correction/supersession lineage;
- availability/degraded/unavailable state;
- trust, redaction, and data-classification metadata;
- replay class and semantic checksum.

External observations are not market-book events. They cannot mutate L2/L3 market state or masquerade as normalized venue events.

### Candidate market

A `CandidateMarket` is a discovered tradeable or potentially tradeable market/listing reference. For Polymarket, this may include market slug, condition/event identifiers, outcomes, resolution rules, fees, status, liquidity metadata, and URL/source references.

Candidate market records include:

- candidate identity and source;
- canonical instrument/listing mapping status where available;
- source quality, freshness, and correction state;
- eligibility flags and unsupported-reason codes;
- discovery/ranking policy versions;
- references to external observations and venue/reference data;
- operator-facing explanation.

A candidate market is not an approved target and not executable.

### Opportunity

An `Opportunity` is an economic thesis that may compose one or more actionable `TradeRecommendation`s and external observations. It contains:

- `opportunity_id`;
- source recommendation identities;
- contributing external observations and candidate markets;
- opportunity type and leg definitions;
- ranking/scoring policy version;
- eligibility and quality status;
- explanation factors;
- replay and correction lineage.

An opportunity never creates another `TradeRecommendation` for the same valid signal. Operator-facing opportunity cards are query models over opportunities, source recommendations, and resolutions.

### Opportunity resolution

`OpportunityResolution` maps opportunity legs to eligible listings/markets under declared constraints. It contains:

- resolution identity and policy version;
- selected candidate markets/listings and rejected alternatives;
- reference-data/listing mapping lineage;
- external observation and market-state bundle references;
- quality/freshness/synchronization status;
- ranking and tie-break evidence;
- output eligibility for portfolio construction.

Resolution occurs before target construction. It cannot size a portfolio target, approve risk, reserve capacity, or create an executable intent.

## Fact taxonomy activated by this leaf

Minimum external-observation fact types:

- `external.source.registered`;
- `external.observation.accepted`;
- `external.observation.rejected`;
- `external.observation.corrected`;
- `external.observation.superseded`;
- `external.observation.unavailable`;
- `external.quality.updated`;
- external publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

Minimum opportunity/discovery fact types:

- `opportunity.candidate_market.discovered`;
- `opportunity.candidate_market.rejected`;
- `opportunity.candidate_market.updated`;
- `opportunity.created`;
- `opportunity.updated`;
- `opportunity.unavailable`;
- `opportunity.ranked`;
- `opportunity.resolution.accepted`;
- `opportunity.resolution.rejected`;
- `opportunity.resolution.superseded`;
- `opportunity.resolution.unavailable`;
- opportunity publication, recovery, and consumer-acknowledgement facts where required by Phase 03.

No `execution.*`, `risk.*`, `ledger.*`, or `execution.approval.*` facts are activated by discovery.

## Quality and consumability

Trade-capable downstream use requires:

1. source adapter and schema support;
2. accepted external observation or candidate fact;
3. quality/freshness above policy threshold;
4. correction/supersession state resolved;
5. source timing uncertainty acceptable for the claim;
6. causal lineage recorded in signal, opportunity, resolution, recommendation, target, or explanation as applicable;
7. policy-declared dependency set satisfied.

If required external evidence is stale, corrected, missing, unavailable, hostile, low-quality, or incompatible, trade-capable output must abstain, become unavailable, no-change, or projection-only unless the active policy explicitly changes the dependency set before evaluation and records why the external evidence is no longer required.

External observations cannot be silently treated as neutral or omitted from explanations after influencing an outcome.

## Isolation and safety

External discovery must be isolated from the hot market-data core:

- bounded queues and payload sizes;
- schema allowlists and source-type allowlists;
- quarantine for malformed/hostile payloads;
- rate limiting and backoff;
- optional process isolation for slow/untrusted integrations;
- no blocking external calls on market-state, strategy, risk, execution, or accounting hot paths;
- no credentials/secrets in telemetry, logs, fixtures, or exports;
- disabled/degraded external discovery cannot stall live-paper or live execution safety paths.

If isolation fails, external discovery degrades or stops; core trading safety does not widen.

## Replay, correction, and recovery

Replay must reproduce:

- captured external observations and corrections;
- quality scoring;
- candidate-market discovery and ranking;
- opportunity creation and resolution;
- downstream explanation references.

Corrections are linked facts, not in-place edits. If a correction invalidates an opportunity/resolution already consumed downstream, later phases must receive explicit supersession/invalidation facts and decide according to their own policies. Discovery cannot edit a target, risk decision, reservation, order, fill, or ledger entry.

## Mode behavior

- `Replay analysis` and `Backtest/paper replay` may replay external discovery under declared datasets.
- `Live read-only` may display discovered opportunities as non-authoritative projections and recommendations where upstream strategy rules permit, but cannot produce risk/reservation/execution/accounting facts.
- `Live paper` may use resolved opportunities through the established target/risk/reservation/paper path.
- `Human-approved live` may use resolved opportunities only if Phase 11B live execution gates are satisfied.
- `Guarded automated live` may use discovery only if optional Phase 11C explicitly permits it.

Discovery evidence alone never enables live execution.

## Observability and performance

Observability must include:

- external-source availability, latency, freshness, correction, and quality;
- candidate discovery/ranking throughput and rejection reasons;
- opportunity/resolution status and stale/unavailable reasons;
- isolation queue pressure and quarantine events;
- replay/rebuild duration and mismatch rates;
- explanation completeness for external contributions;
- operator-facing projection freshness.

Metrics are non-authoritative and cannot create discovery facts or execution authority.

## Tests and evidence gates

| Gate | Evidence | Pass condition |
|---|---|---|
| **ED-E01 — External observation schema/quality suite** | Accepted, rejected, corrected, stale, hostile, unavailable, low-quality observations | Quality and correction states are explicit and trade consumability follows policy |
| **ED-E02 — Polymarket candidate discovery suite** | Candidate metadata, unsupported markets, corrections, duplicate candidates, stale pages | Candidate facts preserve provenance and never become executable |
| **ED-E03 — Opportunity cardinality suite** | One signal, multiple recommendations, multiple opportunities, operator cards | No duplicate `TradeRecommendation`; cards are query models |
| **ED-E04 — Resolution boundary suite** | Candidate rankings, rejected alternatives, mapping failures, stale external evidence | Resolution occurs before targets and cannot bypass portfolio/risk/reservation |
| **ED-E05 — Isolation/fault suite** | Slow source, hostile payload, rate limit, quarantine exhaustion, disabled discovery | Core market/trading paths do not block or widen safety |
| **ED-E06 — Replay/correction suite** | Replayed observations, corrections, superseded opportunities, downstream invalidation | Replay is deterministic and corrections are linked facts |
| **ED-E07 — Explanation lineage suite** | External observation influences signal/opportunity/recommendation/resolution | Every influential contribution appears in causal lineage and explanation |
| **ED-E08 — Execution-authority negative suite** | Attempts to create risk/reservation/intents/orders from discovery facts | Discovery grants no execution authority |

## Phase exit criteria

Phase 11A planning is complete when:

- external observations, candidate markets, opportunities, and resolutions have unambiguous ownership and fact taxonomy;
- external quality/correction/consumability semantics are explicit;
- discovery cannot block or distort core market/trading authorities;
- opportunity/recommendation cardinality remains consistent;
- resolved opportunities can enter portfolio/risk only through established gates;
- discovery evidence cannot authorize live execution.

## Deferred decisions

Deferred to implementation or later phases:

- exact external sources and Polymarket endpoints;
- first ranking formulas and quality thresholds;
- legal/compliance constraints for external data use;
- live Polymarket execution support;
- guarded automation use of discovered opportunities.
