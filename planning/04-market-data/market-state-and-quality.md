# Phase 04: Market State and Quality

## Purpose

This document defines the planning contract for Chronos's first authoritative market-state implementation. It covers deterministic L2 reconstruction, independent book/trade/reference/control progress, complete state lineage, immutable views, synchronization and quality, recovery, bounded history, query projections, feature-consumability, observability, performance, and conformance evidence.

This leaf builds on the approved Phase 01 architecture, Phase 02 observability and performance method, Phase 03 event/persistence/replay/recovery contracts, and the approved Phase 04 source-adapter and normalization leaf. It specializes those contracts without redefining their authorities, lifecycle states, ordering scopes, replay classes, clock rules, source facts, normalized facts, or evidence semantics.

The companion Phase 04 source leaf ends when accepted normalized facts are published to and accepted by the stream authority. This leaf begins only when the stream/run-input authority publishes an accepted `RunInputSelectionRecord` to the market-state authority. The market-state authority never reads adapter buffers, normalization internals, another authority's persistence store, or telemetry as source truth.

## Objectives

Phase 04 market state must establish that Chronos can:

1. reconstruct one venue's L2 book and recent public trades for a small configured watchlist;
2. apply exactly one accepted run input atomically and idempotently;
3. preserve independent book, trade, trade-continuity, reference, market-control, and run-control progress;
4. publish listing-scoped immutable views and one run-wide composite synchronization cut after every applied input;
5. distinguish valid, fresh, stale, gapped, recovering, invalid, and unavailable state;
6. prevent non-consumable state from reaching trade-capable feature or strategy paths;
7. apply snapshots and deltas only under the selected venue's proven semantics;
8. preserve late, duplicate, correction, gap, crossed-book, zero/removal, and reference-change meaning without inventing market history;
9. restore from a validated market-state checkpoint plus a complete accepted run-input tail, or rebuild from origin;
10. expose stable authoritative view and disposable query/read-model contracts without leaking mutable state;
11. remain bounded under deep books, high churn, burst input, slow consumers, gaps, and recovery;
12. measure exact state-application, publication, freshness, memory, rebuild, and recovery behavior under the Phase 02 method;
13. prove deterministic live/replay semantic equivalence for equivalent accepted inputs;
14. preserve additive seams for later multi-venue and L3 state without implementing either in this phase.

## Scope

### In scope

- one venue and a configuration-bounded watchlist;
- normalized L2 snapshots, L2 deltas, public trades, listing/reference changes, market-status facts, and ordered run controls;
- authoritative per-listing L2 state;
- top-of-book, bounded depth, spread, locked/crossed status, and recent-trade windows;
- complete listing `StateLineage`, run-wide `StateViewBundle`, and synchronization cuts;
- immutable state-view publication;
- explicit quality, continuity, freshness, and feature-consumability;
- deterministic treatment of duplicates, late input, corrections, zero/removal, gaps, and recovery;
- market-state checkpoints, rebuild, validation, and semantic checksums;
- query-model publication and rebuild contracts;
- memory, retention, queue, history, and consumer bounds;
- telemetry, alerts, health, performance, property/model-based tests, replay equivalence, and evidence gates;
- extension seams for multiple venues and L3.

### Out of scope

- source connectivity, raw capture, source decoding, normalized acceptance, stream-position allocation, or snapshot acquisition buffers;
- defining feature formulas, feature windows beyond their market-state dependency declarations, strategy evaluation, or recommendation policy;
- portfolio construction, risk, paper execution, live execution, accounting, or reconciliation;
- cross-venue aggregation, consolidated books, synthetic best bid/offer, arbitrage resolution, or venue ranking;
- L3 order-by-order state;
- private account/order feeds;
- final numeric type, decimal library, data structure, language, IPC, storage, checkpoint format, or compression product;
- ticket-level implementation decomposition and estimates.

## Cumulative authority boundaries

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Source adapter and capture | Source/session evidence and raw acquisition | Local book state, synchronization, state freshness |
| Normalization | Venue-neutral snapshots, deltas, trades, corrections, reference links, and typed quality facts | Applying those facts to a book or trade window |
| Reference authority | Canonical instruments, listings, effective definitions, tick/quantity rules, and reference versions | Market observations or inferred book validity |
| Stream/run-input authority | Independent stream cursors/epochs, deterministic merge policy, selected input identity, `run_input_sequence`, effective positions, and publication to market state | Semantic application or state quality |
| Market-state authority | Idempotent sequenced application, listing-scoped L2 state, recent trades, listing frontiers/views, run-wide bundle frontier/cuts, complete lineage, synchronization, freshness, state checkpoints, and authoritative state-quality transitions | Source interpretation, feature meaning, strategy policy, query UI |
| Dataset/replay authority | Input manifests, replay class, ordered reconstruction, fidelity classification | Mutating market state or declaring a state valid without the state authority's checks |
| Query-model authority | Disposable projections built from published authoritative views and quality facts | Owning or modifying authoritative market state |
| Feature authority | Feature definitions, feature observations, feature-specific history requirements, and diagnostic versus valid feature outcomes | Mutable book access or weakening market-state consumability |
| Observability | Measurements, health/alert projections, evidence export | Creating state truth, continuity, freshness, or consumability |

The market-state authority is the only owner of applied L2 state. The stream authority proves which input is next; normalization proves what the input means; reference authority proves listing rules; market state proves the result of applying that input at one complete cut.

## Input acceptance boundary

### Accepted input

The market-state authority accepts only an immutable selection published by the stream/run-input authority containing:

- `selection_id`;
- `run_id`;
- `run_input_sequence`;
- selected fact identity and type;
- selected fact's `stream_id`, `stream_epoch`, and `stream_sequence`;
- complete pre-selection and post-selection run-input cursor vectors;
- merge-policy identity/version and deterministic tie-break evidence;
- active configuration/control epoch;
- applicable reserved/effective control position;
- publication identity and semantic checksum;
- selected normalized payload plus separate stream-owned selection metadata, including `SnapshotBridgeBinding` when the selected fact is a bridge-origin snapshot, with independent normalized/binding checksums and a covering envelope checksum;
- causation links to the accepted normalized/reference/control/trade-continuity/logical-timer/status fact;
- lifecycle evidence required by the Phase 03 publication protocol.

Publication to market state does not itself prove application. Market state returns an idempotent consumer acknowledgement tied to the original `selection_id`, input checksum, exact resulting listing-view identities, exact resulting `StateViewBundle` identity, and resulting frontiers.

### Listing and bundle application frontiers

For each run/listing, market state owns a listing application frontier:

- last applied state-affecting `run_input_sequence` for that listing;
- last applied `selection_id` affecting that listing;
- complete resulting listing `StateLineage`;
- exact last accepted and last published `ListingStateView` identity;
- active configuration/control epoch;
- pending publication state for the resulting listing view;
- semantic checksum of authoritative listing state at the frontier.

For each run, market state also owns one run-wide bundle frontier:

- last contiguously applied `run_input_sequence`;
- last applied `selection_id`;
- exact accepted `StateViewBundle` identity;
- canonical map of every configured listing to its exact current `ListingStateView` identity;
- shared run-control cursor, shared reference/configuration positions, merge-policy version, configuration epoch, and effective position;
- bundle publication state and semantic checksum.

The bundle frontier advances for every applied run input, even when only one listing changes. An unaffected listing remains represented by the exact prior listing-view identity; it is not silently resampled or reconstructed at the new run-input position. A shared control/reference input that affects multiple listings creates all affected listing views and the one bundle as a single semantic transition.

An accepted watchlist `ControlOutcome` changes bundle membership at its effective position. For an added listing, the same semantic transition creates a non-consumable origin listing view with explicit `initializing`/`unavailable` quality, origin cursors, active configuration epoch, and no invented book or trades; later snapshot/stream inputs advance it normally. For a removed listing, the same transition removes it from active bundle membership and emits a terminal historical listing view/status linked to the prior identity; it remains queryable and checkpointed as history but cannot satisfy active feature dependencies. Checkpoints and replay record membership before and after the boundary exactly. No listing appears in or disappears from an active bundle outside an ordered accepted control boundary.

A listing frontier advances only when the selected input affects that listing and its entire semantic effect has been applied atomically. The bundle frontier advances only when all required listing-view references and shared positions for that run-input cut are fixed. A duplicate publication of the same selection returns the original exact identities and result. A conflicting selection identity, checksum, pre-cursor vector, or run-input sequence stops the affected run as contradictory; it is never treated as a harmless retry.

### Accepted identities and recoverability

Every accepted listing-view identity, bundle identity, and corresponding frontier identity must satisfy exactly one registered identity policy:

1. the exact identities are accepted atomically with the state/frontier transition in a recovery source that survives the declared failure class; or
2. the identities are derived deterministically from a canonical, versioned function over the accepted run, selection, input checksum, complete lineage, state checksum, policy versions, and exact bundle membership.

The derivation function and all inputs are part of the run manifest and evidence. Recovery must reproduce the exact original identities byte-for-byte. It may not allocate a replacement identity, alias a recovered object to a new identity, use a semantically equivalent substitute, or publish a “recovery view” with a different identity for the same accepted cut.

Across independently created equivalent runs, opaque identities may differ under policy 1; equivalence is then proved by semantic checksums, membership, lineage, and state content. Byte-for-byte identity equality across independent runs is required only under registered deterministic identity policy 2. Within same-run recovery and idempotent re-publication, the exact original identity is always required.

### Atomic application protocol

For one selection:

1. validate run, mode, schema, registry, merge policy, selection identity, and expected next `run_input_sequence`;
2. validate the selected stream cursor against the current complete lineage and expected post-selection vector;
3. resolve the active reference/configuration/control state already established by ordered inputs;
4. compute the candidate listing transition(s) and run-wide bundle transition without exposing partial mutation;
5. validate all applicable invariants and derive quality consequences;
6. compute the exact listing-view, bundle, and frontier identities under the registered identity policy;
7. atomically accept the new authoritative listing state(s), listing frontier(s), exact listing-view identities, bundle membership, bundle frontier/identity, complete lineage, quality state, initial publication states, and the declared recovery source; when identities are deterministically derived, atomically retain every canonical derivation input needed to reproduce them exactly;
8. publish the immutable view through the append-only publication lifecycle;
9. acknowledge consumer application to the stream authority idempotently.

No consumer may observe a partially applied snapshot, half-applied delta batch, updated book with old lineage, updated trades with old quality, new reference rules with an unvalidated book, or a bundle that references only part of a multi-listing transition.

Concrete transaction, copy-on-write, persistent-data-structure, lock, actor, or journal mechanism is deferred. Evidence must prove the atomic semantic result.

## Market-state identity and complete lineage

### No scalar state version

A single book sequence, trade sequence, timestamp, or `run_input_sequence` is not a complete state version. Every authoritative listing view carries a `StateLineage` containing:

- run identity;
- listing and canonical-instrument identity;
- book `StreamCursor`;
- public-trade `StreamCursor`;
- trade-continuity/epoch-boundary `StreamCursor`;
- reference/definition `StreamCursor`;
- market-control/status `StreamCursor`;
- run-control `StreamCursor`;
- run-timer `StreamCursor` and resulting logical-clock position;
- any additional state-affecting stream cursor registered for the view schema;
- explicit origin cursors for registered streams not yet advanced;
- last applied `run_input_sequence`;
- merge-policy identity/version;
- active configuration epoch and effective control position;
- listing-definition/reference version;
- state-schema, state-transition-policy, arithmetic, and canonicalization versions.

The vector is canonically ordered and content-addressable. Omitting a registered cursor is invalid even when that stream has not emitted a fact.

Checkpoint choice, rebuild attempt, process instance, and other recovery-path provenance are append-only facts associated with the accepted view. They are not part of `StateLineage`, semantic view content, checksum, or identity because a different valid recovery path must reproduce the original exact identity.

### Independent cursors

Book, trade, trade-continuity, reference, market-control, and run-control cursors remain independent:

- advancement of one does not imply advancement, freshness, or continuity of another;
- cursor comparison is valid only within the same lineage-qualified stream and epoch;
- a new epoch is not greater than an old epoch unless a registered transition relation proves the state consequence;
- book snapshots do not reset trade/reference/control cursors;
- trade gaps do not erase valid book continuity;
- a public-trade cursor in a new epoch cannot advance in applied state until the ordered trade-continuity/epoch-boundary input for that transition has been applied;
- reference/control changes do not invent market observations;
- one aggregate “last sequence” is prohibited.

### Partial order

For compatible listing views A and B, A precedes or equals B only when:

- every corresponding cursor is in the same stream epoch and A's sequence is less than or equal to B's;
- A's `run_input_sequence` is less than or equal to B's;
- their policy/reference lineage is compatible under a registered relation.

Otherwise the views are concurrent, incomparable, or from distinct lineage. Query models may display newer local publication time, but publication time cannot override lineage comparison.

### Run-wide `StateViewBundle`

`StateViewBundle` is the authoritative run-wide synchronization cut after one applied `run_input_sequence`. It contains:

- run identity and exact `run_input_sequence`;
- exact causing `selection_id` and selected fact identity;
- a canonically ordered map from every configured listing identity to one exact `ListingStateView` identity;
- shared run-control cursor and active configuration epoch/effective position;
- shared run-timer cursor and logical-clock position;
- shared reference/configuration positions and versions, plus listing-specific reference positions where applicable;
- merge-policy, registry, state-schema, bundle-schema, arithmetic, and canonicalization versions;
- exact bundle semantic checksum and registered identity derivation/acceptance policy;
- prior bundle identity.

The bundle does not copy or merge listing books. It proves which exact listing views coexist at the run-wide cut. A listing view may therefore be referenced by multiple consecutive bundles when inputs affect other listings.

Before the first listing-specific market fact, every configured listing has one deterministic origin `ListingStateView` with explicit origin cursors and `starting`/`unavailable` quality. The initial and subsequent bundles reference those exact origin identities until an ordered input creates a later listing view.

Bundle comparison follows run-input order only within the same compatible run and policy lineage. Bundle membership cannot be rebuilt by asking for each listing's “latest” view.

## Authoritative state model

### Per-listing state

The initial authoritative state is partitioned by run and venue listing. It includes:

- canonical instrument and venue listing;
- bid levels keyed by exact canonical price;
- ask levels keyed by exact canonical price;
- aggregate non-negative quantity per side/price;
- best bid and best ask when present;
- spread when both sides exist;
- locked/crossed classification;
- configured depth completeness and truncation status;
- snapshot origin and subsequent accepted delta lineage;
- bounded recent public trades and validity metadata;
- listing/reference definition and active market status;
- complete `StateLineage`;
- synchronization, continuity, freshness, and feature-consumability state;
- state semantic checksum;
- state apply and view publication lifecycle identities.

Empty or one-sided books are representable but non-tradeable unless a later approved market contract explicitly says otherwise. Absence of a side is not encoded as price zero or an infinite synthetic quote.

### L2 level semantics

For one side and exact canonical price there is at most one aggregate quantity. A level update follows only the normalized event's registered quantity semantics:

- absolute quantity replaces the prior aggregate quantity;
- zero quantity removes the level;
- an explicit remove operation removes the level;
- delta quantity is permitted only when the selected venue contract and normalized type explicitly identify it as such;
- an absent level removed again is either a registered idempotent no-op or a typed contradiction according to the venue policy;
- negative resulting quantity is always invalid;
- bid/ask side cannot be inferred from price position;
- prices and quantities remain exact and definition-qualified.

The implementation may prune levels outside configured retained depth only under a declared depth policy. It must preserve whether the view is full to the source-declared depth, locally truncated, source-truncated, or completeness-unknown.

### Bounded-depth closure and exhaustion

For each side, a bounded-depth state records:

- retained best-to-boundary levels;
- exact retained boundary price and quantity when present;
- whether the source snapshot or a later proven refresh established closure beyond that boundary;
- whether unseen levels may exist beyond the retained boundary;
- the accepted input/cursor at which closure was last proven;
- one side status: `complete`, `bounded_with_proven_top`, `boundary_exhausted`, or `unknown`.

Retaining the best N levels is safe for top-of-book only while accepted updates preserve proof that no unseen level can become the best. Boundary exhaustion occurs when, for example:

- removals consume all retained levels on a side while unseen levels may remain;
- removals or reference changes invalidate the last retained level needed to prove the next best;
- a delta references a price beyond the retained boundary in a way that makes local ordering/completeness ambiguous;
- local pruning discards a level that can later become best without a source-supported replenishment rule;
- the source's bounded-depth semantics no longer prove closure after a sequence/reset/depth transition.

On boundary exhaustion:

- the affected side's best price and quantity become `unknown`, not empty or zero;
- spread, midpoint, locked/crossed classification, and any top-dependent value become `unknown` when they require that side;
- the listing view is non-consumable for every dependency requiring affected top/depth correctness;
- later deltas cannot restore known top merely by adding or removing retained levels unless the registered venue depth contract proves closure;
- recovery requires a fresh proven snapshot/depth refresh, a source-supported bounded-book replenishment proof, or another registered closure-establishing input;
- the prior last-known top may remain queryable only as historical evidence clearly marked non-current and non-authoritative.

A genuinely empty side is distinguishable from `boundary_exhausted`: emptiness requires source evidence proving no level exists in the represented scope. Unknown tail cannot be converted to an empty book.

### Top and depth

An immutable listing view exposes:

- best bid/ask and quantities;
- spread and optional exact midpoint representation;
- locked/crossed status;
- requested bounded depth slices for bids and asks;
- retained depth count and source-declared depth;
- depth completeness/truncation status;
- lineage and quality applying to the returned slice.

Top-of-book is derived from the same atomic state as depth. It is never updated through an independent cache that can diverge from the authoritative book.

### Recent trades

Recent-trade state stores normalized public trades in deterministic accepted order and supports declared bounded windows such as:

- last N accepted trades;
- trades within a declared logical/source-time interval;
- a bounded union needed by registered future feature definitions.

Every retained trade preserves its normalized identity, stream cursor, exact price/quantity/side semantics, source-time quality, correction relation, and fidelity status.

Trade-window results carry:

- requested window definition/version;
- included first/last trade cursor;
- book/reference/control cut from the same published view;
- completeness and validity;
- gap/invalidation boundaries crossed;
- source-time quality sufficient for time-based membership;
- truncation or capacity consequence;
- semantic checksum.

Missing trades are never interpreted as zero activity. A count window is invalid until enough complete post-gap trades exist. A time window is valid after a lossy boundary only when its complete interval lies after the boundary and its source-time basis satisfies the registered quality rule.

### Ordered trade continuity and epoch boundaries

Every detected trade gap, failed backfill disposition, lossy transition, verified bridge completion, and trade epoch transition that affects window validity is represented by an immutable state-affecting trade-continuity input. The stream/run-input authority selects that input into run order.

For a transition to a new public-trade epoch:

1. the trade-continuity/epoch-boundary fact is accepted on its registered stream;
2. the run-input authority selects and publishes that boundary before any trade from the new epoch;
3. market state applies the boundary, advances the explicit trade-continuity cursor, closes the prior continuity interval, records the old/new epoch relation and fidelity, and invalidates affected windows;
4. only then may a selected new-epoch trade advance the public-trade cursor and enter recent-trade state.

A new-epoch trade received before its boundary is applied cannot affect market state and causes the run to wait, reject the selection as contradictory, or fail under the registered ordering policy. Market state may not infer the boundary merely from seeing a changed trade epoch.

Each listing view lineage includes both the public-trade cursor and the trade-continuity/epoch-boundary cursor. The view also records the active trade epoch, last boundary identity, boundary run-input position, and fidelity disposition. This boundary remains explicit after windows become valid again.

### Reference and market control

Reference and market-control inputs may alter:

- listing identity/status;
- price/quantity interpretation;
- tick and step validation;
- market phase or trading status;
- source-declared depth or book-reset semantics;
- whether a book remains interpretable under the new definition.

A reference change never silently reinterprets already stored numeric levels. The transition policy must choose one deterministic outcome:

- existing state remains valid because the new definition is proven semantically compatible;
- state becomes non-consumable pending a fresh snapshot;
- state is transformed by an explicitly versioned, evidence-backed migration that produces new lineage;
- listing state closes/terminates.

Current mutable reference aliases are never consulted to interpret historical state.

## Snapshot and delta application

### Snapshot preconditions

The market-state authority receives only snapshot facts already accepted into a stream epoch after the source leaf's `SnapshotBridgeProof`. It independently validates:

- snapshot type/schema and semantic checksum;
- listing, stream, epoch, and position;
- reference/configuration lineage;
- bridge-proof identity and relation to the accepted epoch;
- level uniqueness, exact numeric validity, and declared depth;
- expected first run-input relation;
- compatibility with current market-state recovery status.

Market state does not re-own or duplicate the acquisition bridge algorithm. It does verify that the accepted facts and proof are sufficient for semantic application.

### Snapshot effect

A valid snapshot:

- replaces the authoritative L2 book for that listing and book epoch atomically;
- establishes a new snapshot origin;
- applies any already accepted bridged deltas only when they arrive as their own later ordered run inputs;
- records from the stream-owned `SnapshotBridgeBinding` the allocated snapshot position and terminal accepted bridge-tail stream position that must be applied before recovery can complete;
- does not reset trade/reference/market-control/run-control progress;
- records prior book lineage and recovery causation;
- remains `recovering` and non-consumable until every bridged delta through that terminal position has been applied contiguously and the resulting listing view records the completed bridge frontier;
- completes the bridge immediately at snapshot application when the binding marks an empty tail and its terminal position equals the snapshot position;
- transitions to `synchronized` only at that completed bridge frontier according to the recovery state machine;
- publishes no consumable intermediate state.

If the snapshot itself violates market-state invariants, the new book is not published as consumable. The accepted input remains auditable, the state becomes `invalid` or remains `recovering`, and recovery requests a new source-supported attempt rather than editing the snapshot.

### Delta preconditions and effect

A delta may affect the book only when:

- its exact stream/epoch is either the active synchronized book epoch or the active recovering epoch established by a valid `SnapshotBridgeBinding`, and in the latter case its position is within the bound bridged-delta range;
- the expected predecessor/range rule is satisfied;
- it is the next selected run input under the run-input authority;
- its reference lineage is compatible;
- the book is synchronized, or is recovering specifically through the bound bridge-tail application path;
- each operation satisfies registered side, price, quantity, and removal semantics.

All operations in one normalized delta fact are one atomic state transition. If any required member is invalid, the event-level atomicity policy determines whether the entire fact is rejected or a source-declared independent member can fail separately. Partial application by local convenience is prohibited.

### Duplicate input

A duplicate publication of an already applied `selection_id` produces the original acknowledgement and no new state view. A distinct accepted normalized fact identified by the stream authority as an economic duplicate has no state effect under its typed disposition but may still advance the relevant cursor if and only if the accepted event contract says the no-op fact occupies that position. Cursor advancement and semantic no-op remain visible.

## State quality and synchronization

### Orthogonal quality dimensions

Market quality is not one boolean. Each view records at least:

- structural book validity;
- book continuity;
- book synchronization;
- trade continuity/window completeness;
- reference compatibility;
- market-control availability;
- run-control application;
- freshness by required stream;
- depth completeness;
- numeric validity;
- source/capture fidelity inherited from manifests;
- reconstruction/checkpoint status;
- overall feature-consumability.

One dimension cannot silently overwrite another. A fresh but gapped book is non-consumable; a continuous but stale book is non-consumable; a valid book with an invalid required trade window may remain usable only for features that do not require trades.

### Authoritative status vocabulary

Per listing and dependency scope:

| Status | Meaning |
|---|---|
| `unavailable` | Required origin/input/state does not exist or cannot be accessed. |
| `starting` | Initialization is in progress before a valid synchronized cut exists. |
| `valid` | Structural and semantic invariants pass, independent of freshness. |
| `fresh` | Required evidence ages are within the active policy and clock quality supports the claim. |
| `stale` | A required freshness deadline was exceeded or freshness cannot be proven within policy. |
| `gapped` | Continuity is disproven or a required cursor interval is missing. |
| `recovering` | A declared recovery procedure is active from known evidence. |
| `invalid` | Accepted input or reconstructed state contradicts structural, numeric, reference, or semantic invariants. |
| `closed` | The listing/market is authoritatively inactive under market-control/reference state. |
| `unknown` | Required evidence is missing, contradictory, stale, or cannot be interpreted safely. |

The view contains dimension-specific statuses plus one derived consumability result. It does not force every dimension into one lossy status.

### Synchronization state machine

Book synchronization follows:

```text
unavailable
  -> starting
  -> recovering
  -> synchronized
  -> stale
  -> synchronized
  -> gapped
  -> recovering
  -> synchronized
  | invalid
  | unavailable
  | closed
```

Transitions are authoritative market-state facts caused only by accepted ordered state-affecting inputs, including registered logical-timer/status inputs, recovery-validation inputs, or explicit termination controls. Observability and query wall time may project or conservatively downgrade but cannot create authoritative transitions.

`synchronized` proves that:

- a valid active snapshot origin exists;
- all required accepted book positions through the current cursor were applied exactly once;
- active reference/control lineage is compatible;
- no unresolved gap or invalid transition exists.

It does not by itself prove freshness, trade-window completeness, or strategy consumability.

### Freshness

Freshness is evaluated by a versioned policy over named evidence, not socket liveness. A policy declares:

- affected listing and stream/dependency;
- qualifying source facts and heartbeat/status evidence;
- age start point and clock domain;
- source-time versus local-receive-time use;
- clock-quality requirement;
- deadline and hysteresis;
- expected idle/closed-market behavior;
- transition and recovery rules;
- whether the policy affects book, trades, reference, controls, or a feature dependency.

Exact deadlines are deferred until measured workloads and venue behavior exist. The contract fixes that deadlines are finite, evidence-derived, run-pinned, and cannot be changed retroactively.

Authoritative age, freshness, staleness, and consumability may change only when market state applies an ordered state-affecting input selected by the run-input authority. Permitted causes are:

- a registered logical-timer input that advances the run's logical clock;
- an ordered source/market status input;
- an ordered normalized market, reference, trade-continuity, or run-control input whose accepted timestamps/status alter the policy result.

The resulting listing view and bundle record the logical-clock position, causing input, policy version, evaluated age, and transition result. Host wall-clock polling, query execution time, telemetry age, process scheduling, or an unrecorded callback cannot mutate authoritative freshness or consumability. Live timers must become accepted ordered inputs; a merely “equivalent recorded transition” created after the fact is insufficient.

The existing run/configuration authority owns run-timer generation policy and requests timer facts; the stream sequencing authority owns timer-stream epochs, sequence progression, continuity, and merge eligibility. A run manifest pins the timer policy, cadence/deadline derivation, clock domain, stream identity, epoch/origin, ordering relative to other candidates, and missed-timer behavior. In live mode the run/configuration authority converts monotonic-clock deadline crossings into timer candidates accepted and sequenced by the stream authority through the Phase 03 envelope contracts; in replay the recorded timer facts are consumed and never regenerated from host time. Timer gaps, duplicates, epoch changes, and recovery use the same cursor and fidelity rules as other required state-affecting streams. Market state owns only application of selected timer inputs, not their generation or sequencing.

Query and read-model code may compare wall time with the authoritative view's publication/observation evidence only to conservatively downgrade the returned projection to `stale_projection` or `unknown_projection`. Such a downgrade:

- does not mutate the authoritative listing view or bundle;
- cannot change `stale` to `fresh`, `unknown` to valid, or non-consumable to consumable;
- cannot grant strategy/feature entitlement;
- records the query clock domain, uncertainty, local threshold, source view/bundle identity, and downgrade reason;
- is clearly labeled as projection safety, not authoritative market-state transition.

### Gapped and recovering

When book continuity is gapped:

- no later delta from the affected prior epoch may restore consumability;
- the last valid view remains queryable as historical evidence but is marked non-current/non-consumable;
- recovery waits for a newly accepted proven snapshot epoch;
- trade/reference/control state remains independently represented;
- a later snapshot begins a new valid lineage and does not erase the gap.

When trade continuity is lossy:

- book synchronization may remain valid;
- windows crossing the invalidation boundary are incomplete;
- feature-consumability is evaluated per declared dependency;
- fidelity remains visible in view and run/dataset evidence.

### Crossed and locked books

Every listing view classifies:

- `normal`: best bid below best ask;
- `locked`: best bid equals best ask;
- `crossed`: best bid above best ask;
- `one_sided`;
- `empty`;
- `unknown`.

The selected venue contract and market phase define whether locked or transient crossed state is expected. Until evidence proves a permitted use:

- crossed state is non-tradeable;
- locked state is separately classified and feature policy must opt in explicitly;
- crossed/locked observations are never silently repaired by deleting levels, swapping sides, or widening quotes;
- persistent or impossible shapes trigger typed invalidity/recovery according to policy;
- raw and normalized evidence remains unchanged.

## Synchronization cuts

### Cut construction

Every applied state-affecting run input produces one distinct authoritative listing view for each affected listing. Every applied run input, including a shared control/reference/timer input or an input affecting only one listing, produces one distinct run-wide `StateViewBundle`.

Each listing cut includes:

- complete `StateLineage`;
- exact authoritative state checksum;
- state-quality vector;
- active freshness-policy version and evaluated ages/status;
- snapshot origin and book epoch;
- recent-trade window boundaries and invalidation markers;
- reference/definition and market/control state;
- prior view identity;
- causing selection and fact identity;
- apply lifecycle and publication state.

The bundle cut includes the exact identities of all configured listing cuts at that run-input position and the shared positions defined in `StateViewBundle`. Listing-view and bundle identities are accepted/recoverable under the exact identity contract; neither may be substituted during recovery.

Recovery/checkpoint ancestry is published as associated provenance and query evidence; it cannot alter this cut's semantic content, checksum, or identity.

### Cross-stream meaning

The cut says: “this is the state after applying all selected inputs through this run-input position under this merge policy.” It does not claim external simultaneity among venue streams.

Single-listing feature consumers may request:

- a specific listing-view identity;
- the listing view referenced by a specific bundle;
- a lineage-bounded single-listing history window;
- a dependency-qualified consumable view.

They may not request “latest book plus latest trades” by independently reading mutable stores, because that creates an unrecorded cut.

Multi-listing features must declare:

- the exact `StateViewBundle` identity;
- the required listing set;
- whether every required listing must have changed at that cut or unchanged carried-forward views are allowed;
- shared control/reference requirements;
- per-listing quality/freshness/depth/trade dependencies;
- permitted cross-listing synchronization/age skew under the bundle's run-input policy.

A multi-listing feature consumes only the exact listing-view identities referenced by that declared bundle. Combining independently requested listing views, “latest per listing,” views from different bundles, or views under incompatible shared control/reference positions is prohibited. Until a feature declares a bundle contract, it is single-listing only.

### Control and reference effective positions

Control/reference inputs are applied at their accepted effective positions:

- inputs before the position use prior policy/definition;
- the transition is atomic before the first affected market input;
- the resulting cut records the new cursor, version, configuration epoch, and effective position;
- a view can never carry new control/reference behavior with old lineage;
- rejected control outcomes remain ordered control history but do not enter the run-input merge or alter state, as fixed by Phase 03.

## Late input and corrections

### Late normalized facts

Late arrival does not authorize retroactive mutation of already published live state. The stream/run-input authority decides whether a late accepted fact is eligible prospectively under the registered policy. Market state then:

- applies it at its actual selected run-input position if semantically valid prospectively;
- records lateness and resulting quality;
- does not rewrite prior views;
- starts recovery or rejects semantic application when source ordering rules make prospective application unsafe.

Corrected event-time research replay may build a different run and lineage. It cannot overwrite or be labeled equivalent to faithful live state.

### Source corrections

An accepted correction:

- references the original normalized fact;
- is applied only under a registered prospective correction policy;
- creates a new immutable view and causal relation;
- never mutates the prior accepted fact or prior view;
- invalidates or adjusts bounded recent-trade state only when the source semantics and retained evidence support it;
- may force rebuild from a prior trusted cut if prospective incremental correction is not semantically safe.

Unsupported corrections make the affected dependency invalid or require a new run; they are not ignored.

## Feature-consumability contract

### Dependency declaration

Before a feature definition can consume market state, it declares:

- required listing(s);
- single-listing view or multi-listing `StateViewBundle` scope;
- exact bundle requirements when more than one listing is required;
- required view schema/capability version;
- required book depth and completeness;
- required book synchronization and freshness class;
- whether locked or crossed state is permitted;
- required trade window type, size/horizon, and completeness;
- required source-time quality;
- required reference and market-control status;
- tolerated fidelity classes;
- maximum state age and publication lag;
- whether diagnostic evaluation is allowed when requirements fail.

Market state evaluates single-listing requirements against one immutable listing cut and multi-listing requirements against one immutable declared bundle and its exact member views. It returns one of:

- `consumable`, with exact listing-view identity or bundle identity/member identities and satisfied dependency evidence;
- `diagnostic_only`, with typed failed requirements and no trade-capable entitlement;
- `not_consumable`, with typed reasons;
- `unknown`, when evidence cannot prove a result.

### Safety rule

Only `consumable` state may produce a valid trade-capable feature observation. `diagnostic_only`, `not_consumable`, or `unknown` state may produce explicitly diagnostic observations only when the feature definition permits it. Diagnostic outputs cannot be accepted as strategy inputs, signals, recommendations, targets, or risk evidence.

A feature requiring more than one listing without an exact declared bundle is rejected as contract-invalid before evaluation.

Market state does not decide whether a feature is economically useful. It decides only whether the authoritative state satisfies the feature's declared data contract.

### Stable reasons

Reason families are bounded and versioned, including:

- book unavailable/starting/stale/gapped/recovering/invalid;
- insufficient/truncated depth or bounded-depth closure exhausted;
- empty/one-sided/locked/crossed book not permitted;
- trade window incomplete/gapped/stale/insufficient;
- trade-continuity boundary missing/not applied;
- reference incompatible/unknown;
- market closed/status unknown;
- control epoch mismatch;
- multi-listing bundle missing/incompatible/member mismatch;
- view too old or lineage unavailable;
- checkpoint/rebuild unverified;
- fidelity class prohibited;
- capability/version incompatible.

Free-form text may supplement but never replace stable reason codes.

## Immutable views and publication

### Listing-view and bundle contract

An authoritative `ListingStateView` is the listing-scoped specialization of Phase 01's immutable market-state view. It is:

- immutable after acceptance;
- identified independently from its memory address or publication transport;
- content/checksum verifiable;
- scoped by run and listing;
- complete in lineage and quality;
- safe for concurrent read without observing mutation;
- versioned and capability-declared;
- reconstructable from an accepted checkpoint plus complete tail or from origin.

An authoritative `StateViewBundle` is also immutable, content/checksum verifiable, reconstructable, and identified independently from publication transport. It references exact listing-view identities; it never embeds mutable pointers or resolves “latest” membership at read time.

Consumers receive a value, immutable handle, or snapshot token for one exact listing view or bundle. They never receive a mutable order-book container or a dynamic collection whose members can change after acceptance.

### Publication lifecycle

Listing-view publication follows:

```text
not_published
  -> publication_in_progress
  -> published_to_feature_boundary
  -> feature_consumer_accepted
  | publication_failed_retryable -> publication_in_progress
  | publication_failed_terminal
```

Additional named consumers, such as query models or checkpoint writers, have independent publication states. Failure of an optional query consumer does not block authoritative state or feature publication unless a later approved capability contract makes it mandatory.

Bundle publication has the same lifecycle with bundle-specific fact names. A multi-listing feature is not ready until the exact bundle and every referenced member view required by its contract are available. Consumer acknowledgement identifies the exact bundle and member identities.

The canonical Phase 02 `state_apply` segment remains:

`run_input.published.market_state` to `market_state_view.published.feature_authority`.

`market_state_view` remains the canonical telemetry family name and covers listing views plus the run-wide bundle publication needed by the active feature contract; this leaf does not rename or redefine the Phase 02 endpoint.

Mutation completion, view acceptance, feature publication, query publication, recoverability acceptance, and recoverability proof remain separate points/subsegments.

### Slow consumers

Consumers cannot force unbounded view retention. Each subscription declares:

- capability and view cadence;
- queue/count/bytes/age bounds;
- coalescing eligibility;
- gap/resubscribe behavior;
- whether every view or only the latest complete view is required;
- acknowledgement and cursor semantics.

Feature consumers requiring every cut use a bounded lossless contract and make the run unready on overflow. Query consumers may use latest-view coalescing if skipped view identities and source positions remain explicit. Coalescing is never allowed to alter authoritative state or claim every-view delivery.

## Queries and read models

### Authoritative query surface

The market-state authority may answer bounded point-in-time queries for:

- exact listing view or bundle by identity;
- latest authoritative bundle and its publication/application status;
- latest listing view only together with the exact bundle that references it;
- top-of-book;
- bounded depth slice;
- recent trades under a registered window;
- complete lineage;
- synchronization/quality/freshness;
- consumability evaluation for a declared dependency contract;
- checkpoint/rebuild provenance.

Queries return the listing-view and bundle identity used. A multi-field single-listing response is from one immutable listing view. A multi-listing response is from one immutable bundle and its exact members, never assembled across mutable moments or independent “latest” lookups.

A query may apply the conservative wall-clock projection downgrade defined under freshness. It cannot upgrade, refresh, or otherwise change authoritative age, quality, or consumability.

### Operator read models

The query-model authority builds disposable projections for:

- watchlist summary;
- top/depth display;
- age and freshness;
- synchronization/recovery progress;
- last proven cursors and view publication lag;
- trade-window completeness;
- crossed/locked/empty status;
- checkpoint/rebuild status;
- source and state fidelity.

Every projection contains:

- source authoritative view/fact identities;
- source bundle identity and exact listing-view membership where applicable;
- source lineage or cursor;
- projection build position;
- projection age and completeness;
- rebuild status;
- skipped/coalesced source-view information where applicable.

A read model cannot be used as feature, strategy, risk, or recovery input. It may be deleted and rebuilt without changing authoritative state.

## Checkpoint, rebuild, and recovery

### Checkpoint role

A market-state checkpoint is a verified accelerator, not an independent source of truth. It contains or references:

- checkpoint identity and market-state owner;
- run scope and every included listing scope;
- exact bundle identity/frontier and exact member listing-view identities/frontiers;
- complete per-listing `StateLineage`;
- active configuration/control/reference state;
- book, top/depth, recent trades, invalidation boundaries, and quality state;
- state schema, transition policy, arithmetic, canonicalization, and registry versions;
- snapshot origin and checkpoint predecessor;
- accepted input tail origin;
- semantic checksum and byte integrity;
- creation method and consistency-cut proof;
- validation status and evidence identity.

### Checkpoint creation

Creation must prove one of:

- atomic capture with the bundle and member application frontiers;
- quiescence at the cut;
- copy-on-write/versioned retention of one complete cut;
- deterministic replay construction from an earlier trusted cut.

A checkpoint may be created asynchronously only when the captured immutable bundle and all exact member views remain available and their exact cuts are retained. Mixing a newer book with older trade/reference/control state or changing bundle membership is corruption.

### Recovery algorithm

1. Validate run manifest, replay/live mode, registry, exact identity policy/derivation function, policies, and required stores.
2. Reconstruct the stream/run-input selection and publication frontier under Phase 03.
3. Select the newest trusted compatible market-state checkpoint.
4. Validate scope, lineage, versions, predecessor, integrity, semantic checksum, and absence of later invalidation.
5. Replay the complete accepted selection tail in `run_input_sequence` order.
6. Rebuild idempotency indexes and listing-view/bundle publication states.
7. Reproduce and verify the exact original listing-view, bundle, and frontier identities plus lineage, quality state, and semantic checksums against retained evidence.
8. Republish only the original pending listing views and bundles idempotently with their exact identities.
9. Enable feature consumption only after synchronization/freshness/consumability prerequisites pass.

If any accepted identity cannot be reproduced exactly, recovery fails even when state content appears semantically equivalent. If the checkpoint is invalid, recovery falls back to an earlier trusted checkpoint or origin. If the complete tail is unavailable or contradictory, same-run faithful recovery fails. The system may start an explicitly related child run or new live epoch with new identities; it cannot substitute those identities into the prior run or claim continuity.

### Checkpoint versus venue snapshot

A venue L2 snapshot and a Chronos market-state checkpoint are distinct:

- venue snapshot is a normalized market observation and establishes book origin under source semantics;
- checkpoint is a Chronos reconstruction accelerator for a complete cross-stream state cut;
- a checkpoint cannot repair a source gap;
- a venue snapshot does not capture trade/reference/control/application state;
- neither may substitute for the other in manifests or telemetry.

### Recovery states

During rebuild, state is `recovering` and non-consumable until validation completes. Queries may expose the last trusted view and recovery progress, clearly labeled historical. They may not present partially replayed state as current.

## Numeric and deterministic constraints

### Fixed constraints

Although exact representation is deferred:

- prices, quantities, and derived exact values cannot use unqualified binary floating point;
- every price/quantity carries instrument/listing and definition context;
- parsing is exact or fails; no silent rounding at ingestion;
- arithmetic overflow/underflow is detected and typed;
- comparison, canonical ordering, hashing, and serialization are deterministic;
- side ordering and price tie-breaking are fixed by canonical policy;
- zero is canonical and cannot have multiple semantic encodings;
- negative aggregate quantity is invalid;
- integer/decimal scale changes require explicit compatible reference transition or resynchronization;
- any rounding needed for derived display values is versioned and cannot affect authoritative level identity;
- semantic checksums use canonical ordering independent of container iteration order.

### Deferred numeric choices

Implementation planning may choose fixed-point integers, arbitrary precision decimals, checked decimal types, tick-index representations, or another exact scheme. The choice must prove:

- round-trip preservation of normalized values;
- correct range for selected venue fixtures and stress bounds;
- deterministic behavior across supported runtimes/platforms;
- bounded cost under the accepted workload;
- explicit conversion and migration behavior;
- no semantic change between live and replay.

## Validation invariants

The following are cumulative:

1. Only an accepted, published selection with the exact next run-input sequence may affect state.
2. One selection has at most one semantic effect; idempotent redelivery returns the original result.
3. Every listing view contains every registered state-affecting cursor, including explicit origins, and every run input produces one exact bundle containing every configured listing.
4. No stream cursor is inferred from another stream, publication time, or wall clock.
5. One side/price has at most one non-negative aggregate quantity.
6. Zero/removal leaves no zero-quantity resident level.
7. Snapshot replacement and every multi-member delta apply atomically.
8. Top-of-book and depth derive from the same authoritative listing cut.
8a. When retained bounded depth no longer proves closure, the affected side/top is unknown and non-consumable until a closure-establishing input is applied.
9. A view cannot be `synchronized` without one valid snapshot origin and complete book tail.
10. `synchronized` does not imply `fresh`, trade-complete, or feature-consumable.
11. A gap is never erased by a later snapshot or checkpoint.
12. A late fact never retroactively mutates an already published faithful live view.
13. Reference/control changes take effect only at ordered effective positions and are present in lineage.
14. Existing levels are never silently reinterpreted under a new incompatible definition.
15. Crossed, locked, empty, one-sided, truncated, and unknown states remain explicit.
16. Missing trades are never represented as zero activity.
16a. Every trade gap/epoch boundary is an ordered state-affecting input applied before any new-epoch trade and retained explicitly in lineage.
17. Trade windows crossing unresolved lossy boundaries are incomplete.
18. Single-listing feature consumers receive one immutable listing cut; multi-listing feature consumers receive one declared bundle and its exact members, never independently sampled mutable components.
18a. A multi-listing feature without an exact declared bundle is prohibited.
19. Non-consumable state cannot produce valid trade-capable feature observations.
20. Query projections and telemetry cannot become authoritative state or recovery inputs.
21. A checkpoint is accepted only after complete bundle/member-cut, exact-identity, integrity, compatibility, and semantic validation.
22. Recovery publishes no partial state and cannot advance beyond the complete accepted tail.
23. Exact same manifest and accepted input order produce identical semantic content, quality transitions, memberships, and checksums. Deterministic identity policy also produces identical identities; opaque identity policy requires exact identity only for same-run retry/recovery and semantic equivalence across independent runs.
23a. Authoritative age, freshness, and consumability change only through ordered inputs/logical clock; query wall time can only conservatively downgrade a non-authoritative projection.
24. Every queue, retained view set, depth, trade history, checkpoint tail, and query response is bounded.
25. Future L3 or multi-venue fields cannot redefine existing L2 or single-venue meaning.

## Capacity and memory contract

### Bounded state

Every implementation declares and enforces:

- maximum configured listings;
- maximum retained levels per side and behavior beyond the limit;
- maximum normalized operations per snapshot/delta fact;
- maximum recent-trade count, time horizon, and bytes;
- maximum retained immutable views and/or age;
- maximum in-flight state applications;
- maximum feature and query subscribers;
- per-subscriber queue/count/bytes/age;
- checkpoint frequency/concurrency and retained tail;
- maximum rebuild work before escalation;
- state/checksum scratch-space bounds;
- total market-state memory envelope and reserved safety margin.

The source-declared book may exceed the locally supported bound. Chronos must reject the capability, retain a declared truncated state that is non-consumable for deeper dependencies, or choose another supported subscription depth. It cannot silently discard levels while claiming complete depth.

Bounded retention must also reserve enough evidence to preserve the registered closure guarantee. If that guarantee is exhausted, the implementation follows the bounded-depth exhaustion transition; memory pressure cannot justify continuing to publish a plausible best price from an unknown tail.

### Pressure behavior

Under memory or consumer pressure:

1. preserve authoritative application correctness and required control/status processing;
2. preserve the current authoritative state and its reconstruction evidence;
3. preserve feature-boundary delivery required by the active run or make that capability unready;
4. coalesce or shed optional query projections and diagnostics under declared policy;
5. reduce optional retained historical views without deleting required checkpoint/tail evidence;
6. stop or isolate the affected listing/run before violating depth, history, lineage, or replay claims.

Authoritative facts are never silently overwritten. Increasing queue or history bounds is not accepted as a substitute for proving sustainable service capacity.

## Failure model

| Failure | Immediate authoritative consequence | Permitted continuation | Recovery |
|---|---|---|---|
| Missing/contradictory next selection | Affected run `invalid`/unready | Queries of last trusted view only | Repair selection evidence or start child/new run |
| Duplicate selection delivery | No new effect | Return original acknowledgement | Reconcile publication/acknowledgement |
| Book gap/new epoch pending | Book `gapped` then `recovering` | Capture/trades/reference and historical queries may continue | Proven new snapshot epoch |
| Invalid snapshot/delta | State `invalid` or remains recovering | No trade-capable consumption | New source-supported recovery |
| Trade lossy gap | Affected windows incomplete | Book-only dependencies may continue if declared | Window moves wholly post-boundary or verified backfill in original policy |
| Trade epoch boundary missing/out of order | New-epoch trade has no state effect; run/listing unready or contradictory | Prior trusted view/query only | Apply the exact ordered boundary first or stop/start a new run under policy |
| Incompatible reference change | Non-consumable | Historical query only | Compatible migration proof or fresh snapshot/new lineage |
| Crossed/locked shape outside policy | Non-tradeable/invalid as configured | Diagnostics and evidence | Subsequent valid ordered state or recovery |
| Bounded-depth closure exhausted | Affected side/top and dependent values `unknown`; non-consumable | Historical last-known query only | Proven closure input, depth refresh, or resnapshot |
| Feature publication overflow | Feature capability unready | Authoritative application may continue only if retained recovery policy permits | Drain/replay retained views or restart/child run |
| Query projection failure | Query degraded/stale | Authoritative state and feature path continue | Rebuild projection |
| Checkpoint corruption | Ignore checkpoint | Use earlier checkpoint/origin | Rebuild and record rejection |
| Tail missing/contradictory | Faithful recovery impossible | No same-run continuation | Child/new run with explicit fidelity |
| Memory bound approached | Degraded and pressure policy active | Only declared bounded work | Shed optional consumers, stop affected scope before breach |
| Clock/freshness evidence invalid | Freshness `unknown`/stale | No affected trade-capable consumption; query may only downgrade projection | Restore valid ordered timer/status evidence |

No failure is converted into healthy state because the process remains alive.

## Observability and alerts

### Mandatory telemetry

Use Phase 02 schemas and exact lifecycle points for:

- selected run input received by market state;
- application validation start/result;
- state transition accepted;
- state checkpoint/recovery responsibility accepted and recoverable where materialized;
- authoritative listing view and bundle accepted;
- listing-view and bundle publication attempts and each named consumer acceptance;
- listing frontiers, bundle frontier/membership, exact view/bundle identity, and complete-lineage progression;
- book/trade/reference/control cursor lag;
- trade-continuity/epoch-boundary cursor and ordering;
- synchronization and quality transitions;
- freshness age, deadline, expiry, and recovery;
- snapshot application and recovery completion;
- delta operation count and affected levels;
- top/depth/trade-window construction;
- bounded-depth closure/exhaustion and unknown-side/top propagation;
- feature-consumability outcome and stable reason family;
- listing-view, bundle, and checkpoint semantic checksum;
- checkpoint creation/validation/fallback;
- rebuild inputs, work, duration, and disposition;
- retained levels/trades/views/bytes and consumer queue use;
- crossed/locked/empty/one-sided/truncated conditions;
- invalid/duplicate/late/correction outcomes;
- optional query-model lag and skipped/coalesced views;
- authoritative logical-clock/freshness transitions versus query-only wall-clock projection downgrades.

High-cardinality identities remain in restricted logs/traces/evidence rather than general metric dimensions.

### Canonical latency

The canonical `state_apply` parent segment remains unchanged:

- start: `run_input.published.market_state`;
- end: `market_state_view.published.feature_authority`.

Registered subsegments may include:

- input validation;
- queue wait;
- snapshot application;
- delta application;
- trade application;
- reference/control transition;
- invariant/quality evaluation;
- immutable view construction;
- exact listing-view and bundle identity acceptance/derivation;
- listing-view and bundle acceptance;
- publication to feature boundary;
- feature-consumer acceptance;
- checkpoint handoff;
- query projection publication.

Subsegments state whether they partition or overlap. Recoverability and query projection are not silently included in hot-path `state_apply` unless the architecture later introduces an explicit fence.

### Health and readiness

Health projections distinguish:

- market-state process liveness;
- per-run recovery readiness;
- per-listing book synchronization;
- book/trade/reference/control freshness;
- feature-consumability;
- feature-boundary publication health;
- query projection freshness;
- checkpoint/rebuild readiness;
- memory/capacity pressure.

`live_read_only_ready` for a listing requires the approved source/stream prerequisites plus a validated authoritative state cut appropriate to monitoring. `strategy_ready` additionally requires the active feature dependency contracts to be consumable. Query readiness does not imply either.

### Alerts

Use the Phase 02 alert schema for:

- book gap, invalid transition, or prolonged recovery;
- stale/unknown required stream;
- incompatible reference transition;
- persistent crossed/locked state outside policy;
- feature publication lag/overflow;
- view lineage/checksum contradiction;
- checkpoint rejection/fallback;
- rebuild failure or RTO breach;
- memory/depth/trade/view capacity threshold and breach;
- query-model stale/rebuild failure;
- live/replay semantic mismatch.

Alerts remain projections; they do not change market state or create control outcomes.

## Performance method

Phase 04 adopts the Phase 02 workload, environment, arrival, coordinated-omission, statistical, profile-overhead, and waiver rules.

### Workload dimensions

Market-state campaigns vary:

- listings and independent stream count;
- source-declared and locally retained depth;
- snapshot size/frequency;
- delta operations per message;
- price-level locality and churn;
- add/update/remove distribution;
- trade rate and window size;
- reference/control transition frequency;
- duplicates, late facts, corrections, gaps, reconnects, and recovery;
- immutable-view publication cadence and subscriber count;
- query depth/window shapes;
- checkpoint frequency and tail length;
- normal, burst, sustained, overload, and hostile worst-case input;
- cold rebuild and checkpoint-plus-tail recovery.

### Required measurements

Evidence establishes:

- state-apply and publication latency distributions by input class;
- offered, accepted, applied, published, rejected, and recovered rates;
- queue wait versus service time;
- sustainable capacity and first saturation boundary;
- memory by listing, level, trade, retained view, subscriber, and checkpoint activity;
- depth/trade/history truncation or rejection behavior;
- bounded-depth closure exhaustion and resnapshot/closure recovery behavior;
- freshness transition deadline accuracy and timer overhead;
- gap-to-non-consumable and recovery-to-consumable duration;
- checkpoint creation overhead;
- bundle construction/publication and multi-listing feature-cut overhead;
- rebuild throughput, RTO, fallback, and tail catch-up;
- telemetry profile overhead;
- semantic checksum equality throughout campaigns.

Exact budgets are not set in planning. Phase 04 implementation must accept empirical numeric envelopes, regression thresholds, and exception policy for the supported workload/environment before closure.

## Testing strategy

### Unit and invariant tests

Cover:

- exact numeric parsing/application and overflow;
- level add/update/remove/zero behavior;
- bid/ask ordering and top/depth;
- bounded-depth closure, boundary exhaustion, unknown-side/top propagation, and closure restoration;
- empty/one-sided/locked/crossed classification;
- snapshot replacement and atomic delta application;
- independent cursor advancement and full-lineage canonicalization;
- listing-frontier, exact listing-view identity, bundle membership, and exact bundle/frontier identity;
- freshness and status transitions;
- ordered logical timer/status transitions and query-only conservative wall-clock downgrade;
- trade-window membership, invalidation, correction, and completeness;
- ordered trade-gap/epoch boundary before new-epoch trades;
- reference/control effective-position transitions;
- feature-consumability reasons;
- semantic checksum stability.

### Property and model-based tests

Use an independent reference model to generate:

- arbitrary valid and invalid books;
- snapshots followed by long delta sequences;
- bounded books where removals exhaust retained closure while hidden tail levels may exist;
- duplicate, omitted, reordered, late, and corrected facts;
- zero/removal and repeated-removal cases;
- epoch changes and gap/recovery sequences;
- trade epoch transitions with boundary before/after/missing relative to new-epoch trades;
- independent interleavings of book/trade/reference/control streams;
- multiple listings where each input changes one, several, or no listing views while every bundle fixes exact membership;
- control/reference changes before, at, and after effective positions;
- crossed/locked/empty transitions;
- trade gaps and time/count-window recovery;
- checkpoint cuts and replay tails;
- publication retry and acknowledgement loss;
- bounded pressure and consumer lag.

Properties include:

- implementation state equals reference-model state;
- invariant failure never leaves partial mutation;
- duplicate selections are idempotent;
- full lineage matches applied inputs exactly;
- every bundle references the exact current listing view for every configured listing and shared positions match the cut;
- top/depth matches canonical level maps;
- a best price is never produced from an exhausted unknown boundary; only a proven closure input restores it;
- checkpoints plus tails equal origin replay;
- recovery reproduces exact accepted listing-view, bundle, and frontier identities with no substitute;
- scheduling permutations with the same accepted run-input order produce identical results;
- no non-consumable cut yields valid feature entitlement;
- no multi-listing feature is consumable without one declared exact bundle;
- wall-clock query age can only downgrade projection status and never changes authoritative state.

### Contract and integration tests

Cover:

- stream/run-input publication to market-state acknowledgement;
- source leaf snapshot/epoch facts to state synchronization;
- listing-view and bundle publication to feature boundary;
- trade-continuity boundary selection/application before new-epoch trade application;
- multi-listing bundle feature requests and prohibited independent latest-view composition;
- query-model projection and rebuild;
- persistence checkpoint validation and fallback;
- Phase 02 telemetry endpoint placement;
- schema capability negotiation and unknown fields/types;
- backpressure and slow-consumer behavior.

### Failure and crash tests

Inject failure:

- before/after input validation;
- during candidate mutation;
- before/after atomic state acceptance;
- before/after atomic exact-identity/recovery-source establishment;
- during each view-publication state;
- after feature consumer application with lost acknowledgement;
- during checkpoint capture/validation;
- during checkpoint-plus-tail rebuild;
- under feature/query queue overflow;
- during freshness transition and gap recovery.

Recovery must reconstruct the exact same listing frontiers, listing-view identities, bundle frontier/identity/membership, lineage, state checksum, quality, and publication obligations or stop safely.

## Live and replay equivalence

### Equivalence contract

Given:

- the same accepted normalized/reference/control facts;
- the same original stream epochs/cursors;
- the same `RunInputSelectionRecord` order and effective positions;
- the same state, reference, arithmetic, canonicalization, freshness, and merge-policy versions;
- the same initial state/checkpoint;
- the same logical timer inputs;

live processing and normalized-fact replay produce semantically identical:

- semantically identical listing and bundle application frontiers, with exact identities only when the registered policy is deterministic or the comparison is same-run recovery;
- complete state lineages;
- identical listing-view and bundle content/membership, with identity comparison governed by the registered identity policy;
- L2 levels, top/depth, and recent trades;
- quality/synchronization/freshness transitions;
- feature-consumability outcomes and reason codes;
- semantic checksums.

Processing latency, host addresses, memory layout, telemetry sampling, and query publication timing are not semantic outputs.

### Replay-class distinctions

- Faithful capture-order replay must reconstruct the original accepted normalized outcomes under pinned source versions before comparing market state.
- Normalized-fact replay begins from original accepted normalized facts and cursors.
- Raw re-normalization creates a new normalized lineage and therefore new market-state lineage even if semantic content happens to match.
- Corrected event-time research replay creates a different run and cut sequence; it cannot be called faithful live equivalence.

Equivalence is proven per replay class and manifest, not inferred from similar P&L or top-of-book samples.

## Multi-venue and L3 seams

### Multi-venue

The initial state is venue-listing scoped and its run-wide bundle may already contain multiple listings from the one selected venue. Future multi-venue support may extend the same bundle contract with:

- one authoritative state partition per venue listing;
- canonical economic-instrument relationships;
- explicit cross-venue synchronization quality;
- exact per-venue-listing member views and cross-venue bundle synchronization metadata;
- opportunity/resolution consumers.

It may not:

- merge venue cursors into one scalar sequence;
- claim simultaneous external state without a declared synchronization policy;
- mutate per-venue L2 meaning;
- let a consolidated read model become source truth;
- assemble a cross-venue feature cut outside an exact declared bundle;
- erase venue-specific freshness, gaps, fees, or market status.

A second mock venue with incompatible symbols, sequence scopes, depth, and timestamp quality must pass the extension seam evidence.

### L3

Future L3 state is additive and distinct:

- order identities, lifecycle, queue semantics, and attribution require new contracts;
- L3 may derive an L2 projection, but it cannot silently replace the existing L2 source/state meaning;
- capability negotiation identifies native L2, native L3, and derived-L2 views;
- L3 resource and fidelity requirements are separately bounded;
- consumers declare which capability they require.

Unknown L3 payloads may be preserved by source/normalization contracts but do not affect L2 state before their owning phase is approved.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of Phase 04 and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **MS-E01 — Market-state schema and authority registry** | Listing-view, bundle, input, checkpoint, query schemas; capability/identity versions; ownership; dependencies; status/reason vocabularies | Market state is sole applied-state owner; listing and run-wide cuts are distinct; schemas preserve prior identities/lifecycles and reject unknown required capability |
| **MS-E02 — Complete lineage, bundle, and independent-cursor suite** | Per-listing book/trade/trade-continuity/reference/market-control/run-control/run-timer origins and advances; logical-clock positions; exact bundle membership/shared positions; epoch transitions; incomparable cuts; canonical ordering | Every listing view has all registered cursors including run-timer; every run input has one exact all-listing bundle; no scalar, latest-view assembly, or inferred progress |
| **MS-E03 — Selection/application/identity atomicity crash matrix** | Crash/retry/conflict at every application, exact identity acceptance/derivation, recovery-source, publication, and acknowledgement boundary; duplicate selection; checksum mismatch | One selection has at most one complete effect; exact listing-view/bundle/frontier identities are atomically recoverable or exactly derivable; no substitute identity; contradictions stop safely |
| **MS-E04 — L2 snapshot/delta and bounded-depth model conformance** | Independent full-book oracle, bounded retained model, snapshot replacement, adds/updates/removes, boundary exhaustion, hidden tail, closure restoration, multi-member atomicity, top/depth, source bridge linkage | Implementation equals model; exhaustion makes side/top unknown and non-consumable; only proven closure restores top; no partial mutation or duplicated source ownership |
| **MS-E05 — Exact numeric and level invariant suite** | Venue edge values, scales, tick/step versions, overflow, zero, negative, repeated remove, canonical serialization/hash | No unqualified float semantics, silent rounding, resident zero level, negative quantity, or nondeterministic checksum |
| **MS-E06 — Book-shape and market-status suite** | Normal, locked, crossed, empty, one-sided, closed, unknown, transient/persistent cases | Shapes remain explicit; unsupported crossed/locked state is non-tradeable and never silently repaired |
| **MS-E07 — Trade state, ordered epoch boundary, and window completeness suite** | Count/time windows, gap/epoch boundary facts, boundary-before-new-trade ordering, missing/out-of-order boundary, verified backfill lineage, lossy transitions, corrections, source-time quality, retention pressure | Boundary is applied and explicit in lineage before new-epoch trade effect; missing trades are not zero; crossing windows are incomplete; validity returns only under registered proof |
| **MS-E08 — Reference/control effective-position suite** | Compatible/incompatible reference changes, market status, controls before/at/after positions, rejected outcomes | New semantics appear exactly at effective positions; rejected controls do not alter state; prior levels are not silently reinterpreted |
| **MS-E09 — Quality/synchronization/freshness state machine** | Generated valid/stale/gapped/recovering/invalid/unavailable/unknown transitions; ordered timer/status inputs; logical-clock lineage; query wall-clock downgrade attempts; hysteresis; idle/closed cases | Authoritative age/consumability changes only through ordered inputs; query wall time only downgrades projection; `unknown` is never healthy; synchronized is not fresh/consumable |
| **MS-E10 — Listing-cut, bundle-cut, and immutable-view proof** | Concurrent readers, every input class, exact all-listing bundle membership/shared positions, carried-forward member views, top/depth/trades from one listing view, mutation/latest-assembly attempts | No torn/mixed cut, mutable access, or independent latest-view bundle; exact listing and bundle identities reproduce all returned values and lineage |
| **MS-E11 — Single/multi-listing feature-consumability suite** | Dependency declarations across depth/book/trade/reference/control/fidelity; exact bundle scope for multiple listings; incompatible/missing bundle; diagnostic cases; stable reasons | Only satisfied immutable listing cuts or exact declared bundles return consumable; undeclared multi-listing composition and diagnostic/non-consumable entitlement fail |
| **MS-E12 — View publication and slow-consumer suite** | Feature/query consumers, every publication state, retry/terminal failure, lost acknowledgement, coalescing, overflow | Feature delivery obeys lossless active-run policy or becomes unready; optional query coalescing is explicit; no unbounded retention |
| **MS-E13 — Checkpoint/rebuild/exact-identity recovery proof** | Atomic bundle/member cut creation, identity policy inputs, corrupt/incompatible checkpoints, fallback, complete/missing tails, origin rebuild, semantic checksums | Trusted checkpoint plus complete tail reproduces exact listing-view/bundle/frontier identities and content; substitute identities and partial/unproven recovery fail |
| **MS-E14 — Query/read-model boundary suite** | Exact listing/bundle queries, prohibited independent latest composition, bounded depth/trades, projection lag/completeness/rebuild, wall-clock downgrade/upgrade attempts, forbidden authority use | Responses identify one authoritative listing/bundle cut; wall time can only downgrade projection; projections cannot drive domain decisions, grant validity, or recovery |
| **MS-E15 — Capacity, memory, overload, and adversarial campaign** | Depth/churn/snapshot/trade/subscriber/checkpoint dimensions, all bounds, pressure states, hostile worst cases | Supported envelope is empirical and bounded; no silent truncation/overwrite; overload preserves correctness or stops affected scope |
| **MS-E16 — Telemetry, alerts, and latency endpoint extension** | Phase 02 schemas, canonical state_apply endpoints, subsegments, quality/freshness/rebuild/memory signals, alert lifecycle | Endpoints are not redefined; telemetry remains non-authoritative; missing evidence is unknown; accepted profile stays within envelope |
| **MS-E17 — Performance/SLO adoption (`PS-E01`–`PS-E17`)** | Versioned semantics, workload/environment, arrivals, tails, saturation, coordinated-omission controls, memory/residency, recovery, profile A/B, statistical record | Numeric envelopes and thresholds are evidence-derived; semantic checksum passes at normal, burst, overload, and recovery loads |
| **MS-E18 — Property/model-based sequence corpus** | Seeded generated sequences, shrink/reproduction artifacts, independent full/bounded-book oracle, bundle interleavings, trade boundaries, logical timers, effective positions, pressure/failure cases | Failures reproduce from retained seed/manifest; implementation and oracle agree on closure exhaustion, bundles, lineage, exact identities, quality, and accepted state |
| **MS-E19 — Live/replay exact-equivalence proof** | Each replay class, repeated scheduling, checkpoint/origin starts, ordered logical timers/status, listing-view/bundle/frontier identities, memberships, semantic checksums and quality transitions | Equivalent manifests/orders produce identical semantics; deterministic identity policies reproduce identical IDs, while opaque policies compare semantic checksums/membership/lineage across independent runs; same-run recovery always preserves exact original IDs; replay classes and lineages remain distinct |
| **MS-E20 — Multi-venue/L3 seam review** | Second mock venue, incompatible cursor/time/depth semantics, mock L3 capability, derived/projection attempts | Additions require no redefinition of single-venue L2, complete lineage, authority, quality, or consumability |
| **MS-E21 — Security and resource-isolation review** | Malformed query/depth/window requests, capability spoofing, cross-run/listing access, memory exhaustion, diagnostic data access | Untrusted input cannot mutate state, escape bounds, cross scope, or obtain restricted evidence |
| **MS-E22 — Independent critique and cumulative Phase 01–04 review** | Critique disposition, companion source-leaf alignment, affected prior evidence and PRDs | Zero unresolved material findings; no authority, lifecycle, ordering, lineage, clock, replay, recovery, telemetry, or evidence contract is weakened |

## Leaf exit conditions

This leaf is implementation-complete only when:

- accepted run inputs apply atomically and idempotently with listing frontiers plus one run-wide bundle frontier;
- every accepted listing-view, bundle, and frontier identity is atomically recoverable or exactly deterministically derivable, and recovery reproduces the exact identity without substitution;
- every listing view has a complete book/trade/trade-continuity/reference/market-control/run-control/run-timer lineage vector and logical-clock position, and every run input has one exact all-listing bundle with shared control/reference/timer positions;
- L2 snapshot/delta and bounded-depth behavior matches an independent full/bounded-book model, including unknown top after closure exhaustion and proven restoration;
- exact numeric, zero/removal, depth, top, crossed/locked, and reference-version invariants pass;
- recent-trade windows preserve ordered gap/epoch boundary inputs, explicit boundary lineage, corrections, source-time quality, completeness, and bounded retention;
- freshness, authoritative age, synchronization, and consumability transitions occur only through ordered timer/status/domain inputs and are deterministic/replayable;
- query wall time can only downgrade a non-authoritative projection and cannot grant authoritative validity or consumability;
- non-consumable state cannot reach valid feature output, and multi-listing features cannot evaluate without an exact declared bundle;
- listing views and bundles cannot tear, mutate, substitute identities, or combine different cuts;
- feature publication, optional query publication, coalescing, overflow, and recovery are bounded and explicit;
- trusted checkpoint plus complete tail reproduces origin rebuild's exact accepted identities and content; corruption/missing tails fail safely;
- live and replay semantics match under equivalent manifests while replay classes remain distinct; exact identities also match only under deterministic identity policy, while opaque policies use semantic checksums/lineage/membership across independent runs and exact IDs for same-run recovery;
- Phase 04 empirical state-apply, capacity, memory, recovery, and telemetry-overhead envelopes are accepted;
- multi-venue and L3 seam tests pass without implementing production multi-venue or L3;
- `MS-E01` through `MS-E22` pass;
- affected `MD-E01` through `MD-E22`, `EC-E01` through `EC-E17`, applicable persistence/replay evidence, Phase 02 telemetry/performance evidence, and Phase 01 architecture evidence pass cumulatively;
- independent critique and cumulative review of all approved prior leaves have no unresolved material finding.

This leaf does not fail because feature formulas, strategies, paper trading, production multi-venue, L3, or live execution are not implemented. It fails if those capabilities would require bypassing immutable views, weakening complete lineage, changing accepted state meaning, inventing continuity, making query/telemetry authoritative, or creating a second market-state implementation for research.

## Deferred choices

The following remain for implementation planning or later evidence-driven phases:

1. Concrete in-memory L2 structure, persistent structure, copy-on-write mechanism, lock/actor model, and language.
2. Exact price/quantity numeric type and canonical binary/text encoding.
3. Numeric freshness deadlines, hysteresis, depth limits, trade-window bounds, view retention, queue sizes, checkpoint cadence, and memory envelopes.
4. Exact semantic hash/checksum algorithm and view/checkpoint identity format.
5. Concrete immutable-view representation and retention optimization, provided every applied input retains one distinct authoritative bundle identity, every affected listing retains one distinct listing-view identity, and optional query-delivery coalescing never changes feature-boundary or replay semantics.
6. Concrete persistence, checkpoint, journal, serialization, and compression products.
7. Exact query protocol, API pagination, cache, transport, and UI projection schema.
8. Venue-specific locked/crossed tolerance after venue selection evidence.
9. Feature definitions and their exact dependency contracts.
10. Production multi-venue synchronization, consolidated views, and opportunity inputs.
11. L3 order model and L3-to-L2 derivation policy.
12. Hardware-specific optimization, SIMD, allocator, NUMA, or kernel/network tuning.

Deferral does not authorize incompatible local conventions. Every choice must stay behind the approved contracts and pass the relevant evidence before activation.

## Cumulative review checklist

At this and every later phase gate:

1. Confirm market state consumes only accepted selected run inputs and does not read adapter/normalizer/storage internals.
2. Confirm stream sequencing, semantic normalization, reference truth, state mutation, query projection, and observability remain separate authorities.
3. Confirm every listing view contains explicit book, trade, trade-continuity, reference, market-control, and run-control cursors plus run-input/effective-position provenance.
4. Confirm every run input produces one exact `StateViewBundle` containing every configured listing's exact view identity and shared control/reference positions.
5. Confirm no scalar sequence/time/version or independent latest-view assembly substitutes for complete state lineage/bundle membership.
6. Confirm snapshot acquisition bridge ownership remains in the source leaf while semantic snapshot application remains here.
7. Confirm snapshots/deltas apply atomically and duplicates are idempotent.
8. Confirm accepted listing-view, bundle, and frontier identities are atomically recoverable or exactly derivable and never substituted during recovery.
9. Confirm exact numeric, zero/removal, side, depth, top, and checksum semantics remain deterministic.
10. Confirm bounded-depth closure exhaustion makes the affected side/top unknown and non-consumable until a proven closure input.
11. Confirm crossed, locked, empty, one-sided, truncated, stale, gapped, recovering, invalid, unavailable, closed, and unknown meanings remain explicit.
12. Confirm synchronization, freshness, structural validity, trade completeness, reference compatibility, and consumability remain orthogonal.
13. Confirm authoritative age/freshness/consumability transitions use ordered logical-timer/status/domain inputs; query wall time only conservatively downgrades projections.
14. Confirm every trade gap/epoch boundary is selected/applied before new-epoch trades and remains explicit in lineage/window history.
15. Confirm reference/control changes occur exactly at ordered effective positions and do not reinterpret prior state silently.
16. Confirm late facts/corrections create prospective immutable transitions or explicit new replay lineage, never retroactive live mutation.
17. Confirm single-listing features use one immutable listing cut and multi-listing features use one exact declared bundle; non-consumable state cannot produce valid trade-capable features.
18. Confirm query/read models and telemetry remain disposable/non-authoritative and cannot grant validity.
19. Confirm checkpoints are complete bundle/member cuts and cannot substitute for venue snapshots or repair source gaps.
20. Confirm checkpoint-plus-tail and origin replay produce exact accepted identities and semantic state or recovery stops.
21. Confirm all levels, trades, views, bundles, queues, subscribers, queries, checkpoints, and rebuild work are bounded.
22. Confirm the canonical `state_apply` endpoints remain unchanged and new measurements are declared subsegments.
23. Confirm live/replay exact equivalence is scoped by replay class, manifest, versions, ordered timer inputs, and accepted order.
24. Confirm future multi-venue state preserves per-venue lineage/quality and future L3 remains capability-distinct and additive.
25. Re-run affected source, event, persistence, telemetry, performance, architecture, and independent-critique evidence.

Approval of this document fixes the Phase 04 authoritative market-state baseline. Later phases may add feature definitions, larger supported envelopes, more venues, L3, or stricter quality rules, but they may not expose mutable book state, omit lineage dependencies, infer continuity, treat projections as authority, reinterpret historical cuts, or create a semantically different research/backtest state engine.
