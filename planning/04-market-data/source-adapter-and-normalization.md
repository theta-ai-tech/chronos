# Phase 04: Source Adapter and Normalization

## Purpose

This document defines the planning contract for Chronos's first live crypto market-data source adapter and its normalization boundary. It covers source selection, public-feed connectivity, raw capture, subscription lifecycle, venue-specific continuity, deterministic normalization, reference-data use, quality classification, recovery, security, telemetry, performance, and conformance evidence.

The first implementation targets one crypto venue and a small configured watchlist. Bybit is a tentative candidate, not a committed vendor choice. Venue selection remains an evidence-backed implementation decision.

This leaf builds on the approved Phase 01 architecture, Phase 02 observability and performance method, and Phase 03 event, persistence, replay, and recovery contracts. It specializes those contracts without redefining their authorities, lifecycle states, ordering scopes, replay classes, clock rules, or evidence semantics.

The companion Phase 04 market-state leaf owns L2 reconstruction, synchronized book state, freshness as consumed by strategies, immutable market-state views, and state validation. This leaf ends after source facts are published to normalization and normalized market facts are accepted with stream-authority-owned positions and published to the stream authority. Reference and control facts are consumed through their existing authorities; this leaf does not take ownership of them.

## Objectives

Phase 04 must establish that Chronos can:

1. select one initial venue through recorded capability, reliability, legal, operational, and performance evidence;
2. connect to public market-data endpoints without exposing execution credentials;
3. capture source bytes and framing evidence before semantic parsing;
4. maintain explicit connection, session, subscription, stream, and epoch lifecycles;
5. normalize snapshots, deltas, public trades, and required reference data deterministically;
6. preserve exactly-one-`SourceEvent` lineage for every non-synthetic normalized fact;
7. preserve immutable post-capture source assertions without mutating raw capture;
8. keep normalized acceptance, publication, stream consumption, and run-input consumption as separate recoverable frontiers;
9. select reference facts deterministically from a run-pinned reference/configuration lineage;
10. detect and classify duplicates, book and trade gaps, out-of-order input, late input, resets, reconnects, and unprovable continuity;
11. recover or establish a new epoch without inventing continuity;
12. validate snapshot bridges before accepting buffered normalized facts or allocating their positions;
13. isolate unsafe source subscriptions immediately without silently changing configured eligibility;
14. remain bounded and observable under normal, burst, overload, malformed, disconnect, and venue-throttle conditions;
15. produce immutable fixtures and simulator evidence independent of the concrete adapter implementation;
16. preserve additive seams for future venues and L3 without implementing either in this phase.

## Scope

### In scope

- evidence-driven selection of one initial crypto market-data venue;
- one public market-data source adapter;
- a small configuration-bounded watchlist;
- venue connectivity, framing, heartbeat, reconnect, resubscribe, and resume behavior;
- raw source capture before parsing or normalization;
- public channel authentication boundaries, including proof that private/execution credentials are unavailable;
- snapshot, delta, public-trade, heartbeat/control, and required listing/reference inputs;
- deterministic source decoding and venue-neutral normalization;
- venue sequence, transaction, message, channel, session, and epoch interpretation;
- source and normalized data-quality classification;
- source-clock quality and timestamp provenance;
- rate-limit, queue, parser, decompression, and backpressure policy;
- schema, normalizer, adapter, and reference-version compatibility;
- live read-only and reusable capture support;
- conformance simulator, canonical fixtures, fault injection, replay comparison, and evidence gates;
- extension boundaries for additional venues and L3.

### Out of scope

- L2 book application, synchronization, checksum validation of complete local book state, or immutable market-state views;
- feature calculation, strategy evaluation, recommendations, portfolio, risk, paper execution, or live execution;
- private account feeds, order entry, order acknowledgements, fills, or venue reconciliation;
- multi-venue aggregation, arbitrage, canonical best execution, or cross-venue ordering;
- L3 order-by-order reconstruction;
- external news, social, or prediction-market observations;
- broad venue-wide discovery or ingestion;
- implementation tickets, estimates, language/library selection, deployment products, or final venue commitment.

## Cumulative authority boundaries

The following Phase 01 authorities remain distinct.

| Authority | Owns in this leaf | Does not own |
|---|---|---|
| Source adapter and capture | Connectivity, transport/session state, source framing, earliest practical receive timestamp, capture partition/sequence, raw payload integrity, source-session health, raw snapshot-acquisition buffer, and explicit capture failures/isolation | Venue-neutral meaning, canonical instruments, accepted normalized stream order, market state |
| Normalization | Immutable decode enrichments/source assertions, deterministic decode, source-schema validation, reference-lineage selection, canonical mapping, snapshot candidate buffer/bridge proof, normalized event construction, normalized accepted frontier/publication state, source extensions, and typed normalization failures | Raw capture identity, reference truth, stream consumer/run-input cursors, downstream book mutation |
| Reference authority | Canonical instruments, venue listings, mappings, tick/quantity rules, listing lifecycle, reference versions, and compatibility validation | Market-event parsing, inferred book state, strategy equivalence |
| Stream/run-input authority | Lineage-qualified normalized stream identity, epoch/position allocation, normalized stream consumer cursor, duplicate/order acceptance, gap/invalidation state, and separately the later run-input cursor/merge | Transport connectivity, source parsing, normalized semantic ownership, semantic book application |
| Dataset/replay authority | Capture, enrichment, and normalized dataset manifests; replay-class and lineage validation; integrity and fidelity classification | Live connection management, normalization/reference-selection policy definition, stream acceptance |
| Observability | Telemetry schemas, latency comparability, health projections, alert projections, and evidence export | Source, normalized, continuity, or recovery truth |

No module may bypass these boundaries by reading another authority's store, interpreting telemetry as source truth, or allowing the adapter to mutate market state directly.

## Source-selection decision record

### No vendor commitment in planning

Bybit is a tentative candidate because the existing PRD names it and it may provide suitable public crypto L2/trade feeds. This document does not assert that its current API, license, regional availability, rate limits, sequencing semantics, or operational behavior satisfy Chronos.

The selected venue is fixed only by an approved source-selection evidence artifact. A selection may be revisited before implementation begins or after a material API, legal, access, reliability, or capability change.

### Candidate evaluation dimensions

Every candidate is evaluated using the same versioned matrix:

- public availability from the intended deployment region;
- terms of service, redistribution, capture, retention, and research-use constraints;
- supported instruments and market types;
- L2 snapshot and incremental update semantics;
- public trade semantics;
- source sequence or transaction identifiers and their documented scopes;
- snapshot/delta synchronization procedure;
- reconnect, replay, resume, and missed-data recovery capabilities;
- heartbeat and idle-channel behavior;
- source timestamps and clock-quality documentation;
- documented and observed rate, connection, subscription, and message limits;
- message-size, burst-rate, compression, and fragmentation behavior;
- schema-change and deprecation practices;
- testnet, sandbox, recorded samples, or other conformance support;
- operational reliability observed in a time-bounded probe;
- authentication requirements for public channels;
- security and dependency footprint;
- expected engineering complexity and ability to preserve raw evidence;
- compatibility with the local-first, small-watchlist scope;
- future L3 capability as an optional differentiator, not a V1 requirement.

Commercial popularity or familiarity is not sufficient selection evidence.

### Selection outcome

The decision artifact records:

- candidates and evaluation date;
- source documentation/version references used;
- probe configuration, region, network class, and observation window;
- capability claims separated from observed evidence;
- disqualifying gaps and accepted risks;
- selected candidate and alternatives;
- expiry/review triggers;
- legal/security review status appropriate to the project's use;
- exact capabilities activated in Phase 04;
- capabilities explicitly unsupported or deferred.

A material source-contract change invalidates the affected evidence and requires re-evaluation. It does not silently mutate an active run or retained dataset's interpretation.

## Adapter capability manifest

Each adapter build exposes an immutable, versioned capability manifest. It is validated before a capture session starts and referenced by capture and run evidence.

The manifest includes:

- `adapter_id` and implementation/build version;
- venue identity and environment class;
- supported transport and endpoint classes;
- public authentication mode;
- supported channel/message families;
- supported market/listing classes;
- snapshot, delta, trade, reference, heartbeat, and error capabilities;
- source sequence fields and documented scopes;
- snapshot/delta synchronization algorithm identity;
- resume/replay capabilities and their proof requirements;
- subscription and connection limits;
- compression/framing support;
- known source timestamp classes;
- maximum supported frame, message, nesting, and expansion limits;
- schema versions and compatibility window;
- declared extension fields preserved losslessly;
- conformance-suite version and result identity;
- unsupported capabilities, including private feeds, order entry, and L3 when absent.

Capability negotiation fails before session activation if the requested watchlist, channel set, schema version, transport behavior, or recovery policy is unsupported. Unsupported capabilities are not approximated.

## Configuration model

### Static adapter configuration

Static configuration is schema-validated, versioned, redacted, content-identified, and immutable for one process incarnation. It includes:

- endpoint class and approved host allowlist;
- transport and protocol options;
- TLS trust policy;
- proxy policy when explicitly permitted;
- frame/message/decompression/parser limits;
- connection, subscription, retry, and queue ceilings;
- capture storage policy reference;
- telemetry profile reference;
- public-authentication secret reference if the selected venue requires one;
- disabled private/execution capabilities;
- simulator or live source mode.

Changing static configuration requires a supervised restart unless a later approved contract explicitly reclassifies a field.

### Run-bound source configuration

Run-bound behavior includes:

- venue environment;
- watchlist of canonical listing identities;
- required and optional channel set;
- depth level where source-specific;
- selected source-schema policy;
- selected normalizer version and immutable reference/configuration lineage identity;
- allowed recovery modes;
- declared freshness and lateness inputs consumed by the companion leaf;
- capture and normalized-fact durability policies;
- applicable run mode, such as live read-only or later live paper, or an explicitly non-run-bound capture session owned by the dataset/capture workflow.

Behavior-changing changes are applied only through the Phase 03 command and exactly-one ordered `ControlOutcome` protocol. Accepted changes take effect at their reserved position and establish a new configuration epoch. A direct adapter-side watchlist mutation is prohibited.

## Security and authentication boundary

### Public-data principle

Phase 04 uses only public market-data capabilities. The preferred configuration requires no credential. If a venue requires authentication for a public data tier, the credential:

- is scoped only to public market-data access;
- has no trading, transfer, account-read, private-feed, or administrative permission;
- is referenced through the Phase 01 secret-reference contract;
- is unavailable to normalization, stream, market-state, strategy, UI, and replay modules;
- is never recorded in source payloads, logs, telemetry, manifests, quarantine, crash dumps, or fixtures;
- is rotated or revoked without rewriting historical capture evidence;
- is validated by negative permission tests.

The adapter runtime must not possess live-execution credentials. A future execution adapter uses a separate identity, process capability, configuration scope, and evidence gate.

### Network boundary

The adapter:

- connects only to approved venue hosts and explicitly approved redirects;
- validates TLS according to the deployment threat model;
- rejects downgrade, unexpected origin, cross-environment, and host-substitution attempts;
- applies bounded DNS, connect, TLS, handshake, read, idle, and shutdown timeouts;
- treats server-provided URLs, channel names, compression declarations, and error text as untrusted input;
- never loads code, schema, or configuration from source payloads;
- prevents payload-controlled file paths, allocations, recursion, decompression expansion, or dynamic dispatch;
- records endpoint identity and trust evidence without sensitive values.

## Runtime identities and ordering scopes

The adapter and capture path use the Phase 03 identities without redefining them:

- `capture_session_id`: one bounded connection/file/generator capture session;
- `source_event_id`: one captured source occurrence;
- `stream_id`, `stream_epoch`, `stream_sequence`: one accepted normalized ordering domain;
- capture partition and capture sequence: local source-capture order within their declared partition;
- venue channel/session/message/transaction/sequence values: source assertions retained with explicit scope;
- `configuration_epoch`: active source/watchlist behavior interval;
- `registry_snapshot_id`, source schema, adapter, normalizer, and reference versions.

No source-provided sequence is treated as globally ordered. No bare sequence comparison is valid without its source-defined scope and Chronos stream epoch. Capture order, source order, accepted normalized stream order, run-input order, and event time remain distinct.

## Connectivity and session lifecycle

### Connection lifecycle

Each connection follows a typed lifecycle:

```text
configured
  -> resolving
  -> connecting
  -> transport_established
  -> protocol_negotiating
  -> session_ready
  -> subscribing
  -> active
  -> draining
  -> closed

from non-terminal operational states:
  -> degraded
  -> reconnect_wait
  -> recovering
  -> failed
```

Transitions are explicit facts or source-session evidence owned by capture. A socket being open is not proof that a session is ready, subscribed, synchronized, fresh, or continuous.

### Capture session boundaries

A new `capture_session_id` is required when any of the following prevents proof that the same bounded capture session continues:

- process/runtime restart;
- transport reconnect where source session continuity cannot be proven;
- endpoint or environment change;
- authentication identity/version change;
- framing or compression mode change that alters interpretation;
- source reset requiring new synchronization;
- configured operator restart;
- failover to another endpoint with unproven continuity.

A transport reconnect may remain part of a higher-level capture campaign or dataset, but it never hides the connection boundary. Dataset manifests retain every constituent session and declared continuity relation.

### Heartbeat and liveness

Heartbeat behavior is source-specific and versioned. The adapter distinguishes:

- transport activity;
- protocol heartbeat request and response;
- source channel heartbeat;
- actual market-data progress;
- subscription acknowledgement;
- source error/status messages.

Heartbeat success proves only the documented scope. It cannot establish market-data freshness or sequence continuity when no qualifying data or source cursor evidence exists.

Idle policy declares:

- expected heartbeat cadence;
- allowed response delay;
- market/channel idle expectations;
- detection threshold and uncertainty;
- probe/reconnect behavior;
- affected scope;
- telemetry and alert behavior.

Numeric values are selected from venue documentation and empirical evidence during implementation.

## Subscription and watchlist lifecycle

### Watchlist

The watchlist is a small, explicitly bounded set of canonical `listing_id` values resolved by the reference authority. Display symbols are never subscription authority.

Before admission:

- each listing must exist in the pinned reference version;
- venue, environment, market class, and listing status must match;
- required channel capabilities must be supported;
- subscription count and estimated rate must fit accepted capacity;
- source symbol/topic mapping must be unambiguous;
- tick/quantity semantics required for normalization must be present.

### Subscription lifecycle

Each logical subscription has a typed lifecycle:

```text
requested
  -> sent
  -> acknowledged
  -> active_unproven
  -> active
  -> resubscribe_required
  -> draining
  -> inactive
  | rejected
  | failed
```

`acknowledged` means the venue accepted the subscription request under its documented protocol. `active` requires the configured channel-specific readiness evidence, such as receipt of a valid snapshot or qualifying first message. It does not imply the companion market-state authority has synchronized an L2 book.

Subscription identity includes venue, environment, connection/session, listing, channel, parameter set, configuration epoch, and adapter version. Request retries use a declared idempotency/deduplication policy. Repeated acknowledgements or messages do not create duplicate logical subscriptions.

### Dynamic changes

Adding or removing a listing or channel is a behavior-changing command. The accepted `ControlOutcome` contains the complete new watchlist/channel change and effective position. The adapter receives the resulting configuration epoch through ordered control application.

The effective position governs which captured source occurrences may produce run-eligible normalized facts under the old or new configuration epoch. It does not pretend an external venue subscription side effect occurred atomically at that run-input position.

- An addition may be prepared before its effective position, but no resulting source occurrence is eligible under the new configuration epoch until the outcome is effective. If source activation completes later, the stream remains `starting` or `active_unproven`.
- A removal stops run eligibility at its effective position. Later in-flight venue messages may still be captured as source evidence, but they are rejected from old-epoch normalization/run eligibility under a typed post-removal disposition.
- Subscription request, acknowledgement, drain, and failure facts reference the controlling `ControlOutcome` and configuration epoch.
- A failed external subscribe/unsubscribe operation does not create a second control outcome or roll back accepted history. It produces an explicit source/subscription failure and readiness consequence; a further behavior change requires a new command.
- Immediate physical isolation for resource exhaustion, hostile input, trust failure, or protocol safety does not itself change configured eligibility. The adapter records `unavailable` or `degraded` source/subscription status and the configured required/optional classification remains in force until a new ordered accepted `ControlOutcome` changes it.

Removal:

- stops new normalization for the removed subscription only after its effective boundary;
- drains or explicitly discards already accepted source work under a registered policy;
- records final source and normalized cursors;
- does not delete retained source or normalized history.

Addition:

- creates a new subscription lifecycle;
- does not claim continuity before its first proven snapshot/cursor origin;
- creates a new stream or epoch where required by the source contract.

## Raw capture before parse

### Earliest capture boundary

Chronos captures source evidence at the earliest practical boundary after transport security processing and before semantic parsing. The source record retains enough information to reproduce deframing and decoding.

Depending on transport, one physical read may contain partial, one, or multiple protocol frames. The capture model therefore distinguishes:

- transport read/chunk evidence;
- protocol frame evidence;
- logical source-message evidence;
- normalized facts.

The selected framing policy must preserve a deterministic mapping from protected captured bytes to each `SourceEvent`. Physical chunk boundaries do not become semantic message boundaries unless the protocol defines them.

The raw capture unit is accepted before semantic field decoding. Lossless protocol framing needed to identify one logical source occurrence may precede `SourceEvent` acceptance, but symbol mapping, sequence interpretation, numeric conversion, side interpretation, and message-type semantics may not. If even framing cannot complete, the protected chunk/frame evidence and typed framing failure remain retained under the capture policy.

Source values discovered during deterministic decoding are recorded in an immutable decode-enrichment fact or linked typed decode failure, not backfilled by mutating the accepted `SourceEvent`. Together, the immutable raw event and its linked outcome preserve the Phase 03 source-value and validation evidence.

### Immutable decode enrichment and source assertions

Semantic decoding emits one immutable `SourceDecodeEnrichment` fact for each successfully decoded source message, or one immutable typed decode-failure fact. Both are owned by normalization and reference exactly one immutable `SourceEvent`.

`SourceDecodeEnrichment` contains only source-level assertions and decoding evidence:

- `source_event_id` and decode-enrichment identity;
- adapter, source framing/schema, decoder, registry, and canonicalization versions;
- source message type and member boundaries;
- source-asserted channel, topic, listing symbol, session, sequence, range, transaction, update, trade, snapshot, and checksum values exactly as encoded;
- source-asserted timestamps with unit, precision, clock-domain, and initial quality classification;
- raw decimal/string representations needed to prove later conversion;
- unknown fields preserved under the registered extension policy;
- structural/schema validation result and bounded warnings;
- semantic checksum over the immutable enrichment fact.

The enrichment:

- does not assign a canonical instrument/listing;
- does not select reference data;
- does not assign a Chronos stream, epoch, sequence, or run-input position;
- does not infer side, repair values, or claim venue-neutral meaning;
- cannot mutate or attach fields in place to the `SourceEvent`;
- is linked from later normalized facts through causation in addition to their required direct `source_event_id`;
- is distinct for a different decoder/schema/canonicalization lineage.

Zero or multiple successful enrichments for the same source event under one pinned decode lineage are invalid unless the source framing registry explicitly defines independent logical members. For multi-member messages, one enrichment may contain a stable member table; later normalized facts identify the member they consume while retaining the same single source-event lineage.

The decode-enrichment lifecycle uses `Accepted`, `Published`, `Recoverability-accepted`, and `Recoverable` exactly as defined in Phases 01–03. Crash recovery republishes the original accepted enrichment identity; it never reparses under current code and reports the result as the original fact.

### Source-event contents

Each `SourceEvent`, directly or through its enclosing capture batch, includes:

- `source_event_id`;
- adapter, build, venue, environment, endpoint class, and trust class;
- `capture_session_id` and runtime incarnation;
- connection and subscription references where applicable;
- capture partition and monotonic capture sequence;
- earliest practical `chronos_receive_time` and clock metadata;
- raw payload bytes or a lossless protected representation;
- payload size and integrity digest;
- framing, fragmentation, decompression, and content-encoding metadata;
- framing-level channel/topic discriminator only when obtainable without semantic interpretation;
- parse status only as the capture-time `not_attempted`/`unavailable` state;
- framing status;
- framing-known truncation/corruption status;
- static configuration, adapter capability, and schema-policy references;
- classification and access restrictions.

Capture never rewrites a malformed message into valid source evidence. Decode and normalization outcomes reference the immutable source event; the source event does not acquire reverse links or updated status after acceptance.

### Capture acceptance and durability

`source_event.accepted` retains the Phase 01–03 lifecycle meaning. Capture acceptance is distinct from publication, recoverability acceptance, and recoverability.

The capture durability policy is declared per mode and source capability:

- lossless/replayable upstream sources may permit reconstructable or bounded-loss capture only when equivalence is proven;
- ephemeral live public feeds require an explicit capture RPO and fidelity consequence;
- no capture policy may claim faithful continuity after an unmeasured or unbounded gap;
- `record_time` appears only after the Phase 03 recoverability proof;
- capture pressure or storage failure creates typed degradation/gap/fidelity facts rather than silent discard.

Raw capture publication to normalization must not wait on optional telemetry. Whether it waits on recoverability is an evidence-driven durability choice recorded in the capture policy; the chosen policy must preserve the declared RPO and resource bounds.

## Source framing and parser contract

Parsing is layered and bounded:

1. transport/frame validation;
2. decompression under expansion and resource limits;
3. structural decoding;
4. source envelope/type discrimination;
5. source-schema validation;
6. channel/subscription correlation;
7. semantic normalization against reference data.

Each layer returns a typed result. An error at one layer does not fabricate a later-layer result.

Parser rules include:

- bounded frame, message, field, string, array, nesting, decimal, and timestamp sizes;
- bounded decompression ratio and work;
- no locale-dependent number parsing;
- explicit decimal precision and overflow behavior;
- explicit handling of null, missing, unknown, duplicated, and conflicting fields;
- no dependence on object/map iteration order;
- no payload-driven class loading or dynamic code;
- deterministic validation order where the emitted reason is contractually relevant;
- safe bounded diagnostics that do not reproduce secrets or unlimited payloads.

Malformed, unsupported, or hostile input remains available as protected source evidence subject to quarantine policy. It produces no normalized market fact.

## Reference-data contract

### Required reference concepts

Normalization resolves source symbols/topics to canonical:

- `canonical_instrument_id`;
- `listing_id`;
- venue and environment;
- market/product class;
- base, quote, and settlement assets;
- price and quantity units;
- tick size, quantity step, minimums, and applicable multipliers;
- listing status and effective interval;
- source symbol/topic aliases;
- source-specific rule extensions required to interpret payloads.

The exact reference payload is owned by the reference authority and versioned independently of the adapter.

### Deterministic reference selection

Each run initialization manifest pins one immutable `reference_configuration_lineage_id`. A non-run-bound capture/re-normalization workflow pins the equivalent lineage in its dataset manifest. The lineage identifies:

- the allowed immutable reference fact set and predecessor graph;
- the ordered reference/configuration events visible to the run;
- venue environment and market/product scope;
- the registered semantic-key extraction policy;
- effective-interval clock/cursor policy;
- ambiguity, missing-reference, and supersession rules;
- lineage schema and selection-policy versions.

Normalization never accepts a caller-supplied “current reference version.” For each decode enrichment it derives one registered venue semantic key from source assertions, such as:

```text
(venue, environment, product_class, source_listing_key)
```

The exact key is source-contract versioned. Selection then:

1. restricts candidates to the pinned reference/configuration lineage;
2. matches the registered venue semantic key;
3. evaluates candidate effective intervals using the lineage's declared basis;
4. selects exactly one accepted reference fact;
5. records the selected reference fact/version, lineage identity, semantic key, effective-basis evidence, and selection-policy version in the normalized fact.

The effective-interval basis is deterministic and registered per source/reference family. It may use:

- source session and sequence/range assertions from `SourceDecodeEnrichment`;
- source event time only when its registered quality is sufficient for that policy;
- capture acceptance sequence/time for reference facts whose contract is explicitly capture-effective;
- ordered run reference/configuration position for run-effective changes.

There is no “best timestamp available” fallback. If the registered basis is missing, low quality, contradictory, outside every interval, or matches multiple reference facts, normalization fails with a typed missing/ambiguous/incompatible outcome. Receive time cannot silently replace source sequence or source time.

Reference changes:

- are immutable new reference facts/versions;
- enter run behavior through the Phase 03 ordered reference/control path;
- never retroactively mutate accepted normalized facts;
- may require a new normalized stream epoch when sequence interpretation, units, listing identity, or validation semantics change;
- cause explicit incompatibility when a pinned lineage cannot interpret the new source payload safely.

Using a mutable `latest` reference alias as run provenance is prohibited.

The same raw capture normalized under a different reference/configuration lineage creates:

- a distinct normalization/re-normalization lineage;
- a distinct normalized dataset identity;
- distinct normalized stream identities or lineage-qualified stream generations;
- new normalized fact identities unless the registry proves identity equivalence is safe, which it may not assume;
- an explicit comparison relation to the prior dataset without overwriting it.

Two lineages that happen to select byte-identical reference payloads remain distinct provenance unless a registered lineage-equivalence proof says otherwise. They cannot share an accepted normalized frontier or stream consumer cursor accidentally.

### Reference acquisition

Reference input may come from a venue endpoint, curated configuration, or another evidence-backed source. Regardless of origin:

- its source and acquisition time are recorded;
- raw source evidence is retained when externally acquired;
- validation and approval are owned by the reference authority;
- normalization consumes only accepted immutable versions;
- selection occurs only through the run/dataset-pinned lineage and registered semantic-key/effective-interval policy;
- market messages cannot silently redefine reference truth.

## Normalized market-data taxonomy

The concrete type registry activates the Phase 03 namespaces without changing the event envelope.

### Book snapshot

`market.book.observation.snapshot` represents one venue-emitted L2 snapshot message or independently framed snapshot member.

Minimum semantics:

- canonical listing and instrument;
- source snapshot identity where provided;
- source sequence/transaction/message values and exact scopes;
- source event time and quality where provided;
- ordered or canonicalized bid and ask levels;
- explicit price and quantity decimal semantics;
- depth/completeness class;
- source checksum value when provided, retained as an assertion;
- source extensions required for later synchronization;
- exact `source_event_id`, `source_decode_enrichment_id`/member, normalizer, schema, selected reference fact, and reference/configuration lineage versions;
- proposed normalized stream identity and epoch context.

One source snapshot containing many levels remains one normalized snapshot fact unless the registered source schema defines independently meaningful members. Levels do not each claim separate source events.

### Book delta

`market.book.observation.delta` represents one venue-emitted incremental L2 change message or independently framed member.

Minimum semantics:

- canonical listing and instrument;
- source previous/current/range sequence values where provided;
- transaction/update identity where provided;
- source event time and quality;
- canonical bid/ask changes;
- explicit set/delete/absolute/increment semantics;
- source checksum assertion where provided;
- exact source event, decode-enrichment/member, selected reference fact/lineage, and version lineage;
- proposed stream/epoch context.

Normalization must not guess whether quantity means absolute level size, delta size, deletion, or order count. Unsupported semantics fail closed.

### Public trade

`market.trade.observation.executed` represents one public trade fact. If one source message contains multiple independently identified trades, normalization may emit one normalized trade per trade member; each references the same single source event and preserves its member index/identity.

Minimum semantics:

- canonical listing and instrument;
- source trade identity where provided;
- source event/trade time and quality;
- canonical price and quantity;
- aggressor/taker side only when the venue contract supports that meaning;
- source sequence or transaction relation where provided;
- correction/cancel/bust relation where the public feed supports it;
- exact source event, decode-enrichment/member, selected reference fact/lineage, and version lineage;
- member index and source extensions needed to distinguish facts.

Chronos does not infer aggressor side from price movement when the source does not assert it. Unknown side remains explicit.

### Reference and market-control facts

Activated reference and market-control facts include source-specific payloads for:

- listing/rule/version observations submitted to the reference authority;
- heartbeat and source status;
- subscription acknowledgement/rejection;
- sequence origin/reset/wrap indication;
- detected duplicate/gap/out-of-order/late condition;
- reconnect, resubscribe, recovery start/result;
- source throttle/rate-limit warning;
- source schema/deprecation/incompatibility notice;
- capture or normalization degradation.

These facts have one semantic owner. Telemetry about them is separate.

### Snapshot and delta are observations, not state

The terms `snapshot` and `delta` describe source-normalized facts. They do not prove:

- a synchronized local book;
- completeness beyond the declared source depth;
- successful application;
- continuity with prior state;
- market-state freshness;
- tradeability.

The companion market-state authority decides whether a sequence of accepted normalized facts yields a valid synchronized L2 state.

## Deterministic normalization

For a fixed:

- `SourceEvent`;
- immutable `SourceDecodeEnrichment`;
- source framing/schema version;
- adapter capability manifest;
- normalizer implementation and policy version;
- registry snapshot;
- reference/configuration lineage and deterministically selected reference fact;
- decimal/time/canonicalization policies;

normalization produces the same ordered zero/one/many normalized facts, identities when the registered ID policy is deterministic, semantic fields, validation outcomes, source extensions, and semantic checksums.

Normalization may produce:

- zero normalized facts plus one typed unsupported/malformed/quarantined outcome;
- one normalized fact;
- multiple normalized facts from one source message containing independent members.

Every non-synthetic/non-administrative normalized fact references exactly one `source_event_id`. A many-source aggregate is never a normalized event.

Normalization does not:

- repair missing source sequence values;
- interpolate levels or trades;
- reorder separate source events;
- choose run-input order;
- mutate accepted history;
- apply deltas to a book;
- derive features;
- infer corrections from multiple facts;
- use current reference/schema state in place of pinned versions.

### Normalized identity

The type registry defines whether a normalized event ID is opaque or deterministically derived. In both cases:

- duplicate source deliveries may have distinct source and normalized event IDs;
- economic/source deduplication uses registered fingerprints and stream policy, not identity coincidence;
- raw re-normalization creates new lineage and dataset identity;
- the original accepted normalized history is never overwritten;
- collision or ambiguous member identity fails closed.

## Venue sequence and epoch contract

### Source sequence specification

For the selected venue, the approved source contract documents for every channel:

- each sequence-like field;
- exact scope: connection, session, topic, listing, channel, shard, product, or another bounded domain;
- origin and reset behavior;
- monotonicity and permitted jumps;
- wrap behavior;
- whether messages cover one value or a range;
- relationship among snapshot, delta, trade, and heartbeat sequences;
- whether subscriptions share ordering;
- whether reconnect can resume a prior source session;
- whether source documentation or observation leaves semantics unknown.

Unknown sequence semantics are not reverse-engineered into authoritative continuity without evidence.

### Chronos stream assignment

Normalization proposes a logical stream partition from canonical listing, channel/fact family, source-ordering scope, and normalization/reference lineage. The stream authority validates and allocates the final `stream_id`, `stream_epoch`, and `stream_sequence`.

### Normalized acceptance, publication, and consumer frontiers

Four frontiers remain separate:

| Frontier/state | Owner | Meaning |
|---|---|---|
| Normalized accepted frontier | Normalization | Highest contiguous normalized stream position for which the immutable normalized fact and its exact position were accepted and have a valid recovery source |
| Normalized publication state | Normalization | Append-only state of publication attempts for each accepted normalized fact to the stream authority |
| Normalized stream consumer cursor | Stream/run-input authority | Highest contiguous normalized position accepted idempotently into the stream authority's eligible-input set |
| Run-input consumer cursor | Stream/run-input authority | Position derived only from recoverable `RunInputSelectionRecord`s and their publication/application state; it is not advanced by normalized publication or stream acceptance |

Normalized-fact acceptance and position allocation use one declared recoverable coordination boundary:

1. normalization computes a candidate semantic fact and requested ordering scope;
2. the stream authority validates the current epoch, duplicate/order policy, lineage-qualified stream identity, and proposed next position;
3. for book recovery buffers, the snapshot-bridge protocol below must already have validated the complete bridge before any position is allocated;
4. one atomic recoverable operation allocates the exact normalized stream position, accepts the immutable normalized fact bound to it, records `publication_state = not_published`, and advances the normalization-owned accepted frontier;
5. only after that operation may publication to the stream authority begin.

This operation does not advance the normalized stream consumer cursor or the run-input cursor. A duplicate, gap, invalid snapshot bridge, incompatible lineage/epoch, or ordering rejection yields a typed outcome and no normalized acceptance, position allocation, or frontier advancement.

Publication uses append-only states:

```text
not_published
  -> publication_in_progress
  -> published_to_stream_boundary
  -> stream_consumer_accepted
  | publication_failed_retryable -> publication_in_progress
  | publication_failed_terminal
```

`published_to_stream_boundary` means the accepted normalized fact crossed the declared boundary and became available to the stream authority. It does not prove stream acceptance. `stream_consumer_accepted` references the stream authority's idempotent acceptance acknowledgement and resulting consumer cursor.

The stream authority:

- validates the original normalized identity, position, epoch, lineage, and semantic checksum;
- accepts each position idempotently;
- advances its consumer cursor only across a contiguous accepted prefix;
- may retain later positions as pending without claiming the cursor crossed a gap;
- never creates, renumbers, or semantically modifies the normalized fact;
- makes only consumer-accepted eligible normalized facts available to run-input selection.

Crash recovery:

1. reconstructs the normalized accepted frontier from atomic accepted-fact/position records;
2. folds append-only publication states for every accepted position;
3. reconstructs the stream consumer cursor and pending accepted set from stream-authority records;
4. republishes `not_published` and `publication_failed_retryable` facts with the same identity and position;
5. reconciles `publication_in_progress` and `published_to_stream_boundary` against the stream consumer cursor, then republishes idempotently only when consumer acceptance is unproven;
6. never automatically retries `publication_failed_terminal`;
7. never allocates a replacement position, reparses under current code, or advances the run-input cursor;
8. stops the affected stream when accepted-fact, position, publication, or consumer-cursor evidence is contradictory or incomplete.

An unresolved `publication_failed_terminal` blocks allocation of later normalized positions in that stream. Explicit recovery may repair and republish the original position or terminate the stream/start a new epoch under a recorded fidelity disposition; it cannot skip the hole.

Concrete transaction/IPC technology is deferred. Evidence must prove that a crash cannot create an accepted normalized fact without its position/recovery source, lose a position while advancing the accepted frontier, infer consumer acceptance from publication, or advance run-input state from normalized publication alone.

Book, trade, reference, and market-control streams remain independent unless the selected venue contract proves a shared ordering domain and the registered policy adopts it. No arrival-time merge creates source order.

### Epoch origin and transition

A new stream epoch is required when continuity cannot be proven, including:

- first origin;
- reconnect without documented and verified resume continuity;
- source session reset;
- sequence reset/wrap that cannot be represented safely within the current epoch;
- snapshot resynchronization after a gap;
- subscription re-creation with unproven continuity;
- source schema or reference change that changes interpretation;
- adapter/process restart where retained evidence cannot prove continuation;
- endpoint failover with unknown continuity;
- operator-requested restart;
- detected contradiction in source sequencing.

An epoch transition is accepted by the stream authority and recoverable before new-epoch normalized facts are accepted/published. Cross-epoch bare sequence comparisons are prohibited.

### Snapshot anchoring

The source-specific contract defines:

- whether deltas may arrive before, during, or after snapshot acquisition;
- which sequence/range links a snapshot to subsequent deltas;
- whether buffering is required;
- overlap and duplicate rules;
- maximum bounded buffer policy;
- when synchronization is impossible and a new recovery attempt/epoch is required.

### Snapshot acquisition and bridge buffer ownership

Snapshot acquisition is a coordinated adapter/normalization workflow, not a market-state mutation.

The adapter/capture authority owns:

- initiating the source-supported snapshot request or subscription recovery action;
- retaining every raw snapshot/delta `SourceEvent`;
- capture order, request/response/session identity, and transport failure evidence;
- a bounded raw-source acquisition buffer of source-event identities/bytes while no bridge is proven;
- immediate physical cancellation/isolation when acquisition is unsafe.

Normalization owns:

- immutable decode enrichments for the snapshot and buffered deltas;
- deterministic candidate normalization under the pinned schema/reference lineage;
- a bounded candidate buffer containing no accepted normalized facts and no Chronos stream positions;
- validation of source-level snapshot/delta bridge predicates defined by the registered venue contract;
- the bridge-proof artifact or typed bridge failure.

The stream authority owns:

- creation/acceptance of the new stream epoch after bridge validation;
- exact normalized position allocation;
- an immutable `SnapshotBridgeBinding` accepted with the new epoch and attached by the stream authority to the selection/publication envelope alongside the unchanged normalized snapshot fact, linking the proof to the allocated snapshot position, ordered bridged-delta positions, and terminal bridge-tail position;
- accepted frontier/consumer cursor behavior under the preceding section.

Neither adapter nor normalization may allocate an epoch or normalized stream position. The companion market-state authority does not own the acquisition buffer and receives nothing from it until the bridge succeeds and accepted normalized facts are published normally.

A valid `SnapshotBridgeProof` contains:

- acquisition attempt, capture session, listing/channel, and prior gap/epoch references;
- pinned adapter, decoder, normalizer, reference/configuration lineage, schema, and bridge-policy versions;
- snapshot source identity and asserted base/range;
- complete ordered buffered-delta source identities and asserted ranges;
- proof that overlap, duplicate, predecessor/successor, and first-post-snapshot rules hold;
- bounded-buffer counts, bytes, oldest age, and no-loss evidence;
- deterministic candidate semantic checksums;
- proposed new-epoch origin and deterministic position-allocation rule, with no concrete epoch/position allocated yet;
- one terminal result: `valid`, `invalid`, `overflowed`, `timed_out`, `incompatible`, or `unknown`.

The proof intentionally contains no allocated positions. After validating it, the stream authority accepts `SnapshotBridgeBinding` atomically with epoch/position allocation and publishes it as stream-owned selection metadata alongside the normalized snapshot through the ordinary selection boundary. It does not mutate the normalization-owned payload or semantic checksum. The selection carries separate `normalized_fact_checksum`, `bridge_binding_checksum`, and an envelope checksum covering both identities. Market state consumes that attached binding, not a cross-authority store or the pre-allocation proof alone, to determine the exact recovery tail.

For a snapshot-only bridge with zero buffered deltas, the binding sets `terminal_bridge_tail_position` equal to the allocated snapshot position and marks `bridge_tail_empty = true`; applying the snapshot completes the bridge immediately if all other synchronization conditions pass.

Only `valid` permits the stream authority to accept a new epoch and then atomically allocate/accept positions in deterministic bridge order: snapshot first, followed by the exact accepted delta sequence defined by the venue policy. The bridge proof becomes causation for the epoch transition and each accepted buffered normalized fact.

Before `valid`:

- buffered candidates remain computed only;
- the normalized accepted frontier does not advance;
- no publication state exists;
- the stream consumer and run-input cursors do not advance;
- no consumer may observe the candidate snapshot/deltas as accepted facts.

On invalidity, timeout, incompatibility, or overflow:

1. no buffered candidate is accepted and no position is allocated;
2. raw source events and decode enrichments remain immutable evidence;
3. the affected stream stays gapped/recovering;
4. a typed bridge-failure/overflow fact records the abandoned attempt;
5. the stream authority records a gapped discontinuity transition when required, but this is not assignment of a usable book-data epoch and allocates no normalized market-fact position;
6. recovery either starts a fresh acquisition attempt with a new proposed usable epoch or stops the scope;
7. no later successful snapshot retroactively validates or reuses the abandoned candidate buffer.

Crash recovery reconstructs the acquisition attempt and candidate buffer from retained raw/enrichment evidence when the declared buffer policy supports it. Otherwise it abandons the attempt visibly and begins a new attempt/new epoch; it never partially accepts a pre-crash buffer.

This leaf validates and emits the necessary source-normalized evidence. The companion market-state leaf owns the later synchronized-state transition after consuming the accepted snapshot and deltas.

## Duplicate, gap, ordering, and lateness policy

### Distinct concepts

- **Duplicate delivery:** a source/economic fact believed to have been delivered more than once under the registered source fingerprint policy.
- **Capture duplicate:** two capture occurrences with distinct `source_event_id` values.
- **Sequence duplicate:** repeated source sequence/range within one proven epoch.
- **Gap:** expected source continuity cannot be proven from accepted evidence.
- **Out of order:** a fact arrives behind the currently observed source frontier but may be admissible under a bounded source policy.
- **Late:** arrival or event-time delay exceeds a versioned threshold; lateness does not itself establish sequence invalidity.
- **Correction:** a new source message explicitly corrects a prior source-normalized fact.
- **Re-normalization:** retained source evidence is interpreted under an explicitly selected normalization lineage.

These terms are never collapsed.

### Duplicate handling

Capture retains each occurrence according to capture policy. The stream authority decides whether a normalized occurrence advances or has state effect.

Deduplication:

- is scoped by venue/channel/listing/session/epoch as required;
- uses source identities/ranges when trustworthy;
- may use a versioned fingerprint only with collision handling;
- preserves evidence of each captured occurrence;
- cannot drop a distinct legitimate fact merely because payload bytes match;
- survives restart by reconstruction from authoritative records where duplicate effect remains possible.

### Gap handling

On a suspected gap:

1. stop claiming continuity for the affected normalized stream;
2. publish typed gap/quality evidence;
3. prevent affected book deltas from being represented as continuous accepted input beyond the stream policy;
4. attempt only venue-supported bounded recovery;
5. resnapshot/resubscribe under the documented algorithm when required;
6. establish a new epoch if continuity cannot be proven;
7. preserve the original gap in capture/dataset manifests.

A later snapshot may restore usable state but does not erase the historical gap or make the interval faithful.

### Trade-gap policy

Every selected venue's public-trade contract declares one of two capabilities for each trade stream.

#### Verified backfill available

Backfill is usable only when the venue supplies enough stable trade identity/order evidence to prove a complete bridge. The policy defines:

- source endpoint/channel and authentication class;
- maximum recoverable time/sequence range;
- stable trade identity and ordering key;
- overlap/deduplication rule against live capture;
- pagination/cursor completeness proof;
- source timestamp and correction behavior;
- rate-limit and bounded-retry policy.

On a detected trade gap:

1. the normalized trade consumer cursor stops before the gap;
2. later live trade source events continue to be captured and may be held as unaccepted candidates within declared bounds;
3. backfill responses are captured as new source events with their own immutable decode enrichments;
4. normalization validates one complete bridge from the pre-gap cursor through backfill and held live trades;
5. only a complete, duplicate-safe bridge permits acceptance in deterministic source order within the existing epoch;
6. an incomplete, ambiguous, expired, or overflowing bridge follows the no-backfill policy below.

Successful verified backfill receives fidelity `complete_after_verified_backfill`; it does not pretend the original live arrival was uninterrupted. Capture-order faithful replay retains the original gap and later backfill arrival, while a normalized dataset may classify the accepted trade stream as semantically complete under the registered backfill policy.

#### No verified backfill

When no authoritative backfill exists, or a backfill bridge fails:

1. emit an authoritative trade-gap fact with the last proven and first observed source positions;
2. close the prior trade continuity interval;
3. start a new trade stream epoch classified `degraded_lossy`;
4. accept no fabricated, interpolated, or inferred missing trades;
5. preserve post-gap trades prospectively in the new epoch;
6. mark affected capture/normalized/run manifests with the exact gap and fidelity labels.

The required fidelity vocabulary includes:

- `complete_live`;
- `complete_after_verified_backfill`;
- `lossy_gap_declared`;
- `unknown_continuity`;
- `non_faithful_corrected_research`.

The dataset/replay authority owns aggregate fidelity classification from these facts; the adapter or normalizer cannot upgrade it.

#### Trade-window and feature invalidation

A trade gap emits a stream-owned invalidation boundary containing:

- affected listing/trade stream and old/new epochs;
- last complete trade position and first post-gap position;
- gap detection and backfill disposition;
- minimum evidence needed to restore each declared recent-trade window class;
- fidelity label.

The companion market-state authority must mark recent-trade windows crossing that boundary invalid/incomplete. Later feature definitions that depend on trade windows must consume that validity and abstain or emit a diagnostic outcome; they may not treat missing trades as zero activity.

Validity may return only after the registered window policy proves the complete window lies wholly after the lossy boundary, or verified backfill closes the gap. Time passage alone is sufficient only for explicitly time-bounded windows with a valid source-time basis; count-based windows require the declared count coverage. This leaf defines and tests the invalidation evidence but does not create feature observations.

### Out-of-order handling

Out-of-order buffering is allowed only where the venue contract provides enough sequence information and the accepted policy defines:

- bounded count, bytes, sequence distance, and age;
- ordering key and tie-break;
- duplicate interaction;
- timeout/overflow behavior;
- restart reconstruction or explicit loss consequence;
- telemetry and evidence.

If bounds are exceeded or ordering remains ambiguous, the stream becomes gapped/recovering; Chronos does not sort by receive time and claim continuity.

### Late data

Source event time and Chronos receive time are retained separately. Live normalization does not retroactively insert a late source fact into an already accepted earlier stream position. It follows the source-order policy and records lateness/quality.

Corrected event-time research replay may reorder facts only under the Phase 03 non-faithful replay class and immutable manifest. It cannot be used as evidence of live decision equivalence.

## Reconnect, resubscribe, and resume

### Recovery hierarchy

The adapter attempts the least disruptive source-supported recovery whose continuity can be proven:

1. continue the existing healthy session;
2. protocol-level resume using a documented token/cursor and verified server acceptance;
3. reconnect and resume the prior sequence domain when the venue contract proves equivalence;
4. reconnect, resubscribe, and obtain a new snapshot;
5. establish a new stream epoch;
6. stop the affected scope when recovery is unsupported, repeatedly failing, or unsafe.

The chosen path and proof are recorded. A successful TCP/WebSocket reconnect alone never proves sequence continuity.

### Retry policy

Retry behavior is:

- bounded by attempts and elapsed time;
- exponential or otherwise evidence-based with jitter where appropriate;
- aware of venue throttle and retry-after assertions;
- isolated by endpoint/connection/subscription scope;
- protected against synchronized retry storms;
- cancelable during shutdown or configuration change;
- observable without logging sensitive request details.

Permanent authentication, permission, schema incompatibility, unsupported subscription, and policy failures do not enter infinite retry loops.

### Recovery readiness

The source adapter may report transport ready while a stream remains:

- `starting`;
- `unsynchronized`;
- `gapped`;
- `recovering`;
- `degraded`;
- `incompatible`;
- `stalled`;
- `unknown`;
- `ready`.

Readiness is per connection, subscription, stream, and listing. Aggregate readiness cannot hide a failed required stream.

## Rate limits, backpressure, and bounded resources

### Venue limits

The selected source contract records:

- connection creation limits;
- concurrent connection limits;
- subscription request and topic limits;
- message/control request limits;
- heartbeat expectations;
- snapshot/reference endpoint limits;
- server throttle/error semantics;
- documented retry behavior;
- empirically observed safe operating envelope.

Chronos uses a versioned limiter policy and does not treat undocumented capacity as guaranteed.

### Internal bounded resources

Every adapter/normalization boundary declares:

- frame and message buffers;
- decompression workspace;
- capture ingress queue;
- capture-to-normalization publication queue;
- parser/normalizer work queue where asynchronous;
- out-of-order/recovery buffer;
- subscription command queue;
- quarantine allocation;
- retry state;
- per-listing and aggregate limits.

For each, the plan fixes producer, consumer, partition/order key, count/byte/age bound, overflow action, stale-work rule, shutdown/drain behavior, recovery source, and telemetry.

### Pressure policy

Priority follows Phase 01 and Phase 03:

1. required source capture and continuity evidence;
2. required stream/epoch/gap status and ordered control application;
3. normalization needed to preserve the declared live/replay capability;
4. required operational health;
5. optional diagnostics and exports.

When pressure threatens the declared capture RPO or continuity:

- affected readiness degrades immediately;
- optional telemetry follows the Phase 02 shedding policy;
- the adapter may immediately disconnect, unsubscribe, or isolate a physical connection/subscription when continuing would violate security, integrity, memory, parser, decompression, capture-RPO, or source-rate safety bounds;
- immediate physical isolation emits an authoritative unavailable/degraded source status with cause, scope, last proven cursor, configuration epoch, and recovery requirement;
- physical isolation does not alter the run manifest's required/optional classification, configured watchlist eligibility, normalized merge eligibility, or control epoch;
- an optional subscription becomes configurationally shed only after a newly ordered accepted `ControlOutcome` changes its eligibility at the recorded effective position;
- until that outcome is effective, required subscriptions remain required and optional subscriptions remain configured but unavailable/degraded; aggregate readiness and fidelity must reflect the missing source;
- an automated pressure controller may request the eligibility change but cannot accept its own request outside the normal command authority/barrier;
- new subscription changes may be rejected;
- source backpressure is used only when the protocol permits it without unbounded upstream loss;
- capture loss or ambiguity opens a gap and may require a new epoch;
- no queue silently overwrites authoritative facts;
- quarantine exhaustion isolates or stops the affected ingest scope under the Phase 03 deterministic policy.

Blocking is permitted only when its source-side loss, latency, and deadlock consequences are explicitly tested.

Reconnection after immediate isolation follows the original configured eligibility unless a later accepted `ControlOutcome` changed it. If safety conditions persist, the adapter remains physically isolated and unavailable rather than silently changing configuration.

## Source clocks and time quality

### Time fields

The adapter preserves:

- earliest practical local ingress `chronos_receive_time`;
- source event/trade/publish times exactly as asserted in immutable decode enrichment;
- source timezone/epoch/unit/precision;
- source clock domain identity;
- parse and acceptance times under Phase 02 lifecycle points;
- uncertainty and quality classification.

### Quality classes

Each source timestamp is classified using a versioned bounded vocabulary such as:

- absent;
- malformed;
- source_asserted_unknown_quality;
- documented_precision_unverified_accuracy;
- empirically_compared;
- synchronized_with_bound;
- contradictory;
- corrected_by_source.

The exact vocabulary is registered. “Exchange time” alone is not a quality claim.

### Clock rules

- Source timestamps do not order Chronos acceptance unless the stream policy explicitly uses them with a deterministic tie-break.
- Local receive minus source time is `apparent_external` unless a measured synchronization relationship supplies an uncertainty bound.
- Negative or implausible apparent delays are retained as clock-quality evidence, not silently clamped.
- Source time cannot replace missing source sequence.
- Same-local-monotonic-domain processing segments follow Phase 02 precision rules.
- Clock jumps, unit changes, overflow, and regressions produce typed quality failures.

## Data-quality model

### Quality dimensions

Source and normalized facts carry applicable dimensions:

- structural validity;
- schema compatibility;
- reference compatibility;
- source continuity;
- duplicate status;
- ordering/lateness status;
- timestamp quality;
- depth/completeness assertion;
- numeric validity;
- subscription/session confidence;
- correction status;
- capture/replay fidelity;
- truncation or resource-limit status.

Quality is structured, versioned, and bounded. It is not one boolean or free-form text.

### Quality state

Per source/subscription/stream/listing projections distinguish:

```text
unknown
  -> starting
  -> healthy
  -> degraded
  -> gapped
  -> recovering
  -> incompatible
  -> stalled
  -> failed
```

Transitions reference authoritative source, normalization, and stream facts. Observability projects these states but does not create them.

Missing, stale, contradictory, or incomplete evidence yields `unknown` or the applicable degraded state, never healthy.

### Corrections

When a source explicitly corrects or cancels a prior public market fact:

- the correcting message is captured as a new `SourceEvent`;
- one immutable decode enrichment records the correcting source assertions;
- the normalized correction references exactly that source event and enrichment/member;
- it references the prior normalized fact through the registered correction relation;
- accepted history remains append-only;
- the stream and companion market-state policies decide prospective effect;
- faithful replay preserves the original correction arrival/order;
- corrected research replay remains explicitly non-faithful where it changes history order.

Chronos-inferred discrepancies are derived quality/reconciliation facts, not normalized source corrections.

## Schema, normalizer, and compatibility evolution

### Version set

Interpretation is pinned by:

- adapter capability manifest/version;
- source framing and schema version;
- event-envelope and type-registry snapshot;
- normalizer implementation and policy version;
- canonicalization and decimal/time policy versions;
- reference version;
- stream/epoch policy version;
- capture and replay manifest versions.

### Change classification

Source changes are classified as:

- additive and safely preservable;
- additive but requiring consumer capability;
- transformable under an explicit migration;
- requiring re-normalization;
- incompatible;
- unknown.

Decoder success alone does not prove compatibility.

Unknown fields may be preserved as opaque extensions only when the registry says they cannot affect current semantics. Unknown message types, side semantics, sequence semantics, numeric units, snapshot/delta meanings, or correction semantics fail closed for normalization while raw capture may continue.

### Schema detection

The adapter never silently guesses between incompatible source schemas. Detection must use:

- explicit protocol/version negotiation;
- endpoint/channel version;
- a validated discriminator;
- or a statically pinned configuration.

Ambiguous detection yields incompatibility/quarantine.

### Historical interpretation

Retained capture is interpreted only with manifest-pinned versions:

- faithful capture-order replay uses the original pinned version set and verifies expected normalized semantic checksums;
- normalized-fact replay does not rerun normalization;
- raw re-normalization uses a selected new version set and creates new lineage;
- no active alias changes historical meaning.

## Failure model

| Failure | Detection | Immediate state | Permitted continuation | Recovery owner and action | Fidelity consequence |
|---|---|---|---|---|---|
| DNS/connect/TLS failure | Typed transport outcome | Connection unavailable | Other independent captures/replays | Capture retries under bounded policy | None before session; explicit availability gap |
| Unexpected endpoint/redirect/certificate | Trust-policy rejection | Failed/security degraded | No affected connection | Capture/security operator review | No accepted source facts from rejected origin |
| Authentication/permission failure | Typed protocol response | Failed or degraded | Public unauthenticated sources if separately configured | Capture stops permanent retry; rotate/reconfigure | Availability gap |
| Heartbeat timeout | Missing expected response/activity | Degraded/reconnect wait | Unaffected subscriptions/connections | Capture probes/reconnects | Continuity unproven if data may be missed |
| Subscription rejection | Typed source response | Subscription rejected | Other admitted subscriptions | Capture reports reason; configuration decision | No false active state |
| Disconnect with proven resume | Transport loss plus verified resume proof | Recovering | Capture evidence and unaffected streams | Capture resumes; stream validates cursor | Faithful only if proof and coverage pass |
| Disconnect without proven resume | Transport loss | Gapped/recovering | Capture may reconnect | New snapshot and stream epoch | Gap retained; interval not faithful |
| Malformed/unsupported payload | Layered validation failure | Source retained, normalization rejected/quarantined | Other payloads within poison-input policy | Normalization/capture isolate | No normalized fact |
| Decode enrichment accepted but unpublished | Publication-state recovery | Normalization recovering | Capture and unrelated streams | Re-publish same enrichment identity; never mutate source/redecode as original | No semantic loss if recovery source validates |
| Decompression/resource attack | Limit breach | Affected input rejected; possible source isolation | Other safe scopes | Capture closes/isolate under policy | Explicit capture/availability impact |
| Duplicate | Registered fingerprint/sequence policy | Duplicate evidence | Continue | Stream authority suppresses duplicate effect | Capture occurrences retained |
| Sequence gap | Expected-versus-observed cursor evidence | Gapped | Capture may continue for evidence; state progression governed by companion contract | Stream/capture recovery hierarchy | Gap retained |
| Snapshot bridge invalid/overflowed | Bridge predicate or bound failure | Book stream gapped/recovering | Raw capture and unrelated streams | Abandon candidate buffer; new acquisition attempt/new epoch | No buffered normalized acceptance; attempt remains evidence |
| Trade gap with verified backfill | Cursor gap plus supported backfill contract | Trade stream recovering | Raw capture and bounded candidate holding | Complete deterministic bridge or fall back to lossy new epoch | `complete_after_verified_backfill` only after proof |
| Trade gap without verified backfill | Cursor gap or failed bridge | Trade stream `degraded_lossy` | Prospective post-gap capture/new epoch | Emit invalidation boundary and open new epoch | `lossy_gap_declared`; crossing windows invalid |
| Out-of-order beyond bound | Buffer distance/age/count breach | Gapped/recovering | Other streams | Stream opens recovery/new epoch | No invented order |
| Reference missing/ambiguous/incompatible | Pinned-lineage semantic-key/effective-interval selection failure | Normalization unavailable for affected listing | Raw capture continues if policy allows | Reference authority supplies a new accepted lineage; re-normalization creates new dataset/streams | Live interval may be incomplete; no current-version fallback |
| Source schema change | Compatibility failure | Incompatible | Raw capture where safely frameable | Adapter/normalizer upgrade and new evidence | No silent reinterpretation |
| Capture persistence lag/RPO threat | Pending age/bytes and storage health | Degraded/critical | According to declared policy | Capture backpressure/stop/new epoch | Explicit bounded loss or gap |
| Normalization overload | Queue/service deadline evidence | Degraded/stalled | Capture continues within bounds | Shed optional work, scale only behind evidence, or stop affected scope | Source retained for later re-normalization if complete |
| Safety isolation before eligibility change | Bound/security/integrity breach | Physical source unavailable/degraded; configuration unchanged | Other safe scopes | Isolate immediately; request ordered eligibility change if desired | Requiredness remains; readiness/fidelity degrades |
| Clock contradiction | Timestamp validation | Time quality degraded | Sequence-valid processing may continue | Record quality; do not use invalid clock for order | No false latency/freshness precision |
| Process crash | Runtime loss | Recovering | Other processes/scopes | Phase 03 recovery from source/normalized records and manifests | Same-run only if continuity/version proof passes |

Failure containment is per connection, subscription, listing, stream, and capture session where invariants permit. Shared connection or source-ordering scope may require escalation.

## Shutdown and restart

Graceful shutdown:

1. stops admission of new subscription changes;
2. records draining state;
3. sends source unsubscribe/close only when useful and bounded;
4. continues raw capture of in-flight frames through the declared drain boundary;
5. completes or records incomplete framing;
6. publishes final capture/normalization cursors and health;
7. seals or checkpoints capture evidence under the persistence policy;
8. closes without claiming source continuity beyond the last proven point.

Forced termination relies on Phase 03 valid-prefix and recovery rules. On restart:

- runtime and capture-session identities change;
- retained source and normalized evidence is validated;
- immutable decode enrichments are restored or reconstructed with the original pinned versions and registered identity policy; changed versions create a new lineage;
- normalized accepted/publication and stream consumer frontiers are reconciled before new publication or run-input selection;
- incomplete snapshot/backfill candidate buffers are reconstructed under their declared policy or abandoned without partial acceptance;
- adapter/reference/schema versions are checked;
- prior subscriptions are not assumed active;
- reconnect/resubscribe follows the recovery hierarchy;
- stream epochs continue only with affirmative proof;
- otherwise new epochs begin and the gap is retained.

## Observability contract

Phase 04 activates source, reference, and market stream schemas under the Phase 02 telemetry contract.

### Required latency points

At minimum:

- `source.ingress.local_received`;
- source frame/chunk accepted where separately modeled;
- `source_event.accepted`;
- `source_event.published.normalization`;
- `source_event.recoverability_accepted`;
- `source_event.recoverable`;
- decode enrichment started/completed;
- `source_decode_enrichment.accepted`;
- `source_decode_enrichment.published.normalization_mapping`;
- `source_decode_enrichment.recoverability_accepted`;
- `source_decode_enrichment.recoverable`;
- reference-lineage selection started/completed;
- normalization started/completed;
- `normalized_event.accepted`;
- `normalized_stream_position.allocated`, with its declared atomic relationship to normalized-fact acceptance;
- normalized accepted frontier advanced;
- normalized publication attempt/state transition;
- `normalized_event.published.stream_authority`;
- normalized stream consumer accepted/cursor advanced;
- `normalized_event.recoverability_accepted`;
- `normalized_event.recoverable`;
- subscription requested/sent/acknowledged/active;
- disconnect detected, reconnect attempt, transport restored, subscription restored, first valid source fact, continuity result;
- gap detected, recovery started, snapshot obtained, epoch accepted, recovery outcome;
- snapshot acquisition/buffer/bridge validation and abandonment points;
- trade backfill requested/page received/bridge validated or failed, lossy epoch opened, and window invalidation published;
- physical source isolation, unavailable/degraded publication, eligibility-change command outcome/effective application, and reconnect;
- reference lookup/version validation started/completed;
- queue enqueue/dequeue/expiry/rejection points for each asynchronous boundary.

The canonical `normalization` segment remains:

```text
source_event.accepted
  -> normalized_event.published.stream_authority
```

Subsegments may refine it but may not redefine its endpoints. Capture publication, queue wait, service time, stream acceptance, and recoverability remain separately measurable.

### Metrics

Bounded-dimension metrics cover:

- connection/session/subscription state and transition counts;
- bytes, frames, messages, and source events received/accepted/published/recoverable;
- message family and bounded size class;
- decompression ratio/work and parser/normalizer outcomes;
- normalized facts by bounded type and outcome;
- decode-enrichment outcomes and pending publication/recovery state;
- normalized accepted frontier, publication-state counts/oldest age, stream consumer cursor, and cursor distance;
- run-input cursor distance reported separately from normalized stream cursor distance;
- duplicate, gap, out-of-order, late, correction, and epoch-transition counts;
- snapshot candidate-buffer count/bytes/age, bridge outcomes, abandoned attempts, and pre-acceptance overflow;
- trade gaps, backfill pages/range/bridge outcomes, lossy epochs, fidelity labels, and invalidated-window scopes;
- source and normalized stream frontier/lag without unbounded IDs as dimensions;
- reconnect/resubscribe attempts, outcomes, and recovery durations;
- heartbeat/progress age;
- rate-limit tokens, source throttle responses, and rejected subscription requests;
- queue depth/capacity/oldest age/rejection/expiry;
- capture and normalization pending recoverability;
- quarantine use and exhaustion;
- reference/schema incompatibility;
- clock-quality classes and apparent-external delay quality;
- telemetry loss/profile epoch.

Listing, session, event, payload, source sequence, and trace identities remain in restricted logs/traces/evidence unless the accepted cardinality model explicitly permits a bounded listing dimension.

### Health and readiness

Health projections are capability-specific:

- process liveness;
- endpoint reachability;
- transport/session readiness;
- subscription readiness;
- capture readiness and RPO status;
- normalization readiness;
- reference compatibility;
- stream continuity/recovery;
- aggregate required-watchlist readiness.

A healthy socket cannot make capture, normalization, or stream continuity healthy. Missing evidence is `unknown`.

### Alerts

Using the Phase 02 alert schema, Phase 04 defines alerts for:

- required connection/subscription unavailable;
- reconnect storm or persistent recovery failure;
- heartbeat/progress timeout;
- source gap or repeated epoch reset;
- capture RPO at risk/breached;
- normalization backlog/deadline breach;
- source schema/reference incompatibility;
- ambiguous/missing reference selection under pinned lineage;
- parser/decompression/quarantine attack pattern;
- rate-limit exhaustion or source throttle;
- clock-quality contradiction;
- telemetry blind spot affecting source readiness;
- watchlist capacity or required-stream readiness failure;
- physical subscription isolation while configured eligibility remains active;
- normalized accepted/publication/stream-consumer frontier divergence beyond policy;
- snapshot bridge buffer pressure/failure;
- trade backfill failure, lossy epoch, or invalid trade-window persistence.

Alerts remain projections and cannot create recovery, subscription, or stream facts.

## Performance and capacity method

Phase 04 follows `performance-and-slo-method.md`. Numeric limits and SLOs are empirical implementation outputs, not invented here.

### Workload manifest

The reference workload includes:

- selected candidate venue/source schema;
- configured watchlist sizes from minimum through supported maximum;
- snapshot, delta, trade, heartbeat, status, and reference message mix;
- small, median, high-percentile, and maximum accepted message sizes;
- compression and fragmentation classes;
- normal, burst, sustained-high, overload, disconnect/recovery, and adversarial arrival shapes;
- scheduled-versus-actual arrival evidence to prevent coordinated omission;
- duplicate, gap, out-of-order, late, and correction rates;
- decode-enrichment publication/recovery lag and crash states;
- reference-lineage selection with missing, overlapping, and boundary-effective intervals;
- normalized accepted/publication/stream-consumer frontier divergence and recovery;
- snapshot acquisition buffers through bridge success, invalidity, timeout, overflow, and crash;
- trade-gap backfill success/failure and no-backfill lossy epochs with window invalidation;
- immediate physical subscription isolation before ordered eligibility change;
- cold/warm parser, reference, and schema-cache states;
- capture persistence healthy/lagging/failing cases;
- baseline and diagnostic telemetry profiles;
- source simulator version and deterministic seed.

### Required measurements

Evidence measures:

- ingress, source acceptance, normalization parent segment, and subsegment distributions;
- throughput by messages, normalized facts, and bytes;
- CPU, scheduling, allocations, resident memory, and decompression work;
- queue depth, wait, saturation, rejection, and stale expiry;
- capture and normalized recoverability lag;
- decode-enrichment and normalized publication recovery;
- accepted-to-published-to-stream-consumer frontier distances;
- reconnect/resubscribe/recovery durations;
- snapshot bridge acquisition/buffer cost and abandoned-attempt cost;
- trade backfill bridge and lossy-epoch recovery duration;
- parser and normalizer failure cost;
- per-listing and aggregate fairness;
- source-control path availability during data overload;
- shutdown drain and restart recovery;
- telemetry overhead and semantic checksum invariance.

### Capacity envelope

The accepted Phase 04 profile fixes:

- supported watchlist/channel set;
- message/byte/burst envelope;
- queue and buffer capacities;
- parser/decompression limits;
- connection/subscription request budget;
- maximum recovery buffer;
- snapshot candidate buffer and trade backfill/held-live buffer envelopes;
- capture RPO and pending-recoverability bounds;
- latency/throughput regression thresholds;
- resource ceilings and overload transition points;
- source-control reserved capacity;
- exception and review policy.

An average-rate pass is insufficient. Tail latency, burst recovery, sustained overload, and quality/fidelity consequences must pass the preregistered statistical decision rules.

## Conformance simulator and fixture strategy

### Independence

The conformance oracle must not be only the production adapter's parser. At least one independent fixture generator/reference decoder or cross-implementation verifier validates canonical expected outcomes.

### Simulator capabilities

The deterministic source simulator models:

- connect, handshake, heartbeat, idle, disconnect, and reconnect;
- subscription acknowledgement, rejection, delay, duplicate acknowledgement, and partial channel activation;
- snapshots, deltas, trades, reference/status messages, and mixed framing;
- source sequence origins, ranges, gaps, duplicates, reset, wrap, and contradictory fields;
- immutable decode-enrichment creation, publication loss, and crash recovery;
- pinned reference lineages with semantic-key/effective-interval boundaries and ambiguity;
- fragmentation, coalescing, compression, and truncated/corrupt frames;
- schema versions, unknown fields/types, incompatible changes, and deprecation warnings;
- rate limits, throttles, retry-after, server errors, and connection caps;
- delayed, reordered, duplicated, and burst delivery;
- clock skew, regression, unit changes, missing timestamps, and bounded synchronized clocks;
- crash points around source acceptance, publication, normalization, and epoch transitions;
- crash points across normalized acceptance frontier, publication state, stream consumer cursor, and run-input cursor;
- snapshot acquisition bridge success/failure/overflow before any position allocation;
- public-trade backfill complete/incomplete/unavailable paths and feature-window invalidation;
- immediate physical subscription isolation followed by accepted, rejected, delayed, or absent eligibility-change outcomes;
- backpressure, capture lag, normalization lag, and quarantine exhaustion.

It emits a machine-readable schedule with intended source facts, wire/framing bytes, clock assertions, and expected protocol behavior. It does not prescribe Chronos's normalization output; expected normalized outcomes live in independently reviewed fixtures.

### Canonical fixtures

Fixture families include:

- valid snapshot/delta/trade/reference messages;
- zero/one/many normalized outputs per source event;
- immutable decode enrichment/source assertions and decode-failure facts without SourceEvent mutation;
- exact-one-source lineage;
- decimal precision, overflow, sign, zero, delete, and tick/step edge cases;
- symbol/listing ambiguity, semantic-key selection, effective-interval boundaries, overlapping/missing references, distinct reference/configuration lineages, and reference-version transitions;
- source schema additive/unknown/incompatible cases;
- sequence scope, origin, duplicate, gap, out-of-order, reset, wrap, reconnect, and epoch transitions;
- snapshot/delta overlap and recovery boundaries;
- candidate snapshot buffers with no pre-bridge acceptance/position, bridge success, invalidity, timeout, overflow, abandonment, and crash;
- normalized accepted frontier, per-fact publication state, stream consumer cursor, run-input cursor, lost acknowledgement, idempotent re-publication, and contradictory recovery evidence;
- trade gaps with verified backfill, failed/absent backfill, degraded-lossy epoch, fidelity labels, and time/count window invalidation/revalidation;
- malformed, unsupported, hostile, oversized, deeply nested, and decompression-bomb inputs;
- source clock absent/skewed/regressed/contradictory/bounded cases;
- explicit source correction and prior-fact relation;
- raw capture round trip and semantic checksum;
- faithful capture-order, normalized-fact, raw re-normalization, and corrected event-time replay classes;
- control-effective watchlist changes before/at/after reserved positions;
- immediate physical isolation while configured eligibility remains unchanged, followed by ordered accepted/rejected/no eligibility change;
- loss/backpressure and typed fidelity consequences.

Fixtures are immutable, checksum-addressed, language-neutral, and include expected acceptance, rejection reason, lineage, canonical fields, stream proposal, quality, cursor effect, and emitted evidence.

### Live observation probes

Before venue selection and periodically afterward, a bounded read-only probe records:

- endpoint and schema identity;
- actual framing/compression behavior;
- subscription lifecycle;
- sequence fields and observed transitions;
- snapshot/delta/trade samples;
- burst/message-size distributions;
- heartbeat/idle behavior;
- disconnect/reconnect behavior where safely testable;
- rate-limit responses without abusive load;
- source timestamp characteristics.

Observed behavior never overrides documented or approved semantics silently. A contradiction blocks or narrows capability until resolved.

## Invariants

The following are cumulative with Phases 01–03:

1. Raw source evidence is accepted before semantic normalization; malformed or unsupported input is not rewritten into plausible source truth.
2. Source assertions discovered after capture exist only in immutable, versioned decode-enrichment/failure facts linked to one `SourceEvent`; accepted capture is never mutated.
3. Every non-synthetic/non-administrative normalized market or source correction fact references exactly one `SourceEvent` and the immutable decode enrichment/member it consumed.
4. Capture order, source sequence, normalized accepted order, normalized publication state, normalized stream consumer order, run-input order, and event time remain distinct.
5. Every sequence comparison is scoped by stream and epoch; no reconnect or snapshot silently proves continuity.
6. An epoch transition is recoverable before new-epoch inputs are accepted/published.
7. Normalization is deterministic for pinned source, schema, normalizer, registry, reference/configuration lineage, reference-selection, and policy versions.
8. Reference selection uses only the registered semantic key and effective-interval basis within the pinned lineage; zero/multiple matches fail, and a different lineage creates distinct normalized datasets/streams.
9. Normalization does not apply a book, aggregate multiple source events, infer unsupported side/unit/sequence semantics, or mutate reference truth.
10. Normalized accepted frontier, publication state, normalized stream consumer cursor, and run-input cursor are separately recoverable; none is inferred from another.
11. No snapshot candidate receives an epoch position or normalized acceptance until one valid complete bridge exists; failed/overflowed attempts are abandoned visibly.
12. Duplicate capture occurrences remain evidence even when duplicate normalized stream effect is suppressed.
13. A gap, overflow, source ambiguity, or incompatible schema cannot be relabeled healthy or faithful by a later snapshot.
14. A trade gap without a complete verified backfill opens a lossy new epoch, preserves fidelity loss, and invalidates crossing trade/feature windows.
15. Public market-data code has no execution credentials or private-feed permissions.
16. Every queue, parser, decompressor, retry loop, snapshot/backfill buffer, recovery buffer, and quarantine allocation is bounded and observable.
17. `record_time` means proven recoverable recording; telemetry, enqueue, publication, or handoff does not populate it.
18. Source timestamps do not create unsupported local/venue latency precision or replace source sequencing.
19. Behavior-changing watchlist/subscription eligibility changes use exactly one ordered accepted/rejected `ControlOutcome` and the Phase 03 effective-position barrier.
20. Physical source isolation may happen immediately for safety, but it leaves configured eligibility unchanged and explicitly unavailable/degraded until an ordered accepted outcome changes it.
21. Faithful replay uses the original pinned decode/normalization/reference/schema policy and verifies expected outcomes; new interpretation is re-normalization with new lineage.
22. Observability projects source/stream health and recovery; it does not own them.
23. Accepted normalized-fact and stream-position effects are atomically coordinated without conflating normalization and stream ownership.
24. Venue-specific extensions cannot redefine canonical envelope, authority, lifecycle, or quality semantics.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of Phase 04 and are rerun cumulatively when affected.

| Evidence ID and artifact | Required contents | Pass condition |
|---|---|---|
| **MD-E01 — Venue selection dossier** | Candidate matrix, documentation snapshots, legal/access review, regional probe, capability versus observation, risks, expiry triggers, selected scope | One candidate is selected by evidence; Bybit or another venue is not committed by assumption; unsupported claims are excluded |
| **MD-E02 — Adapter capability and configuration registry** | Capability manifest, static/run schemas, endpoint allowlist, public-auth mode, channel/listing limits, schema versions, unsupported private/execution capabilities | Requested Phase 04 capability validates before session start; incompatible/unknown capability fails closed; redacted identity is reproducible |
| **MD-E03 — Connectivity/session/subscription lifecycle suite** | Generated valid/invalid transitions, handshake, heartbeat, idle, subscription ack/reject, partial activation, drain, reconnect, shutdown, restart | Socket state is never conflated with session/subscription/stream readiness; transitions are bounded, typed, and recoverable as required |
| **MD-E04 — Raw-capture and immutable decode-enrichment proof** | Fragmented/coalesced/compressed/malformed/unsupported frames, earliest receive point, source-event construction, integrity round trip, decode enrichments/failures, post-capture sequence/timestamp assertions, crash/republication | Protected capture reproduces original bytes; source assertions exist in immutable one-source enrichment facts; capture is never mutated; malformed input creates no normalized market fact |
| **MD-E05 — Public-data security and credential-isolation suite** | No-auth and public-auth cases, permission inspection, secret canaries, host/redirect/TLS policy, private/trading endpoint attempts, payload attacks | Adapter cannot access or use execution/private capabilities; secret canaries do not escape; untrusted origin/downgrade/payload actions fail |
| **MD-E06 — Snapshot/delta/trade/reference normalization and selection suite** | Canonical valid/invalid fixtures, zero/one/many outputs, decimals, sides, units, members, extensions, source corrections, pinned lineage, semantic keys, interval boundaries, missing/overlapping references, distinct-lineage re-normalization | Outputs and failures match independent oracle; unsupported meaning is not guessed; exactly one reference is selected deterministically; different lineages produce distinct datasets/streams |
| **MD-E07 — Exactly-one source and enrichment lineage extension (`EC-E12`)** | Every activated normalized type, source corrections, multi-member messages, malformed input, aggregation attempt, raw re-normalization, missing/mismatched enrichment | Every normalized fact has exactly one source event and matching immutable enrichment/member; multi-source aggregate is rejected; re-normalization creates new lineage |
| **MD-E08 — Venue sequence/epoch conformance extension (`EC-E05`)** | Channel-specific sequence specification; origins, ranges, duplicate, gap, out-of-order, reset, wrap, reconnect, failover, snapshot anchor, reference-lineage stream identity, epoch recovery | No bare/cross-epoch comparison or unproven continuity; lineage-qualified epoch origin/transition is explicit and recoverable before new-epoch publication |
| **MD-E09 — Duplicate/gap/out-of-order/late/correction suite (`EC-E08`, `EC-E09`)** | Source/capture duplicates, fingerprint collision, book/trade gaps, bounded reorder, late data, overflow, explicit corrections, backfill/no-backfill, restart rebuild | Distinct concepts retain distinct effects; no duplicate accepted stream effect; overflow/gap has typed recovery/fidelity; append-only history and correction lineage survive |
| **MD-E10 — Reconnect/resubscribe/resume recovery campaign** | Disconnect at every lifecycle point, verified resume, unverifiable resume, resnapshot, retry exhaustion, source throttle, process crash | Least-disruptive proven recovery is used; unverifiable continuity opens a gap/new epoch; no infinite retry or false active state |
| **MD-E11 — Reference/schema/version evolution drill** | Listing mapping/rule changes, semantic-key/effective-interval policies, additive/incompatible schema changes, ambiguous detection, mutable-alias rejection, lineage equivalence/non-equivalence, migration/re-normalization | Pinned runs remain interpretable; zero/multiple reference matches fail; unsafe changes fail closed; accepted history is not rewritten; new lineage creates distinct normalization provenance |
| **MD-E12 — Source-clock quality and latency fixture (`OT-E04`, `OT-E05`)** | Missing/skewed/regressed/unit-shift clocks, bounded synchronization, same-clock processing points, apparent external latency | Only comparable clocks produce precise durations; quality and uncertainty survive; source time does not order unsupported streams |
| **MD-E13 — Data-quality and health model** | Structured quality dimensions, generated state transitions, missing/stale/contradictory evidence, per-scope and aggregate readiness | Unknown never becomes healthy; socket, subscription, capture, normalization, and continuity readiness remain distinct |
| **MD-E14 — Bounded-resource, rate-limit, and adversarial suite** | All queue/buffer/parser/decompressor/retry/quarantine/snapshot/backfill bounds; normal, burst, overload, throttle, poison, storage/telemetry failure, immediate isolation | No unbounded work or silent overwrite; physical safety isolation is immediate; configured eligibility is not silently changed; fidelity/health consequences are explicit |
| **MD-E15 — Telemetry, alert, and performance campaign** | Exact Phase 02 points/segments, decode enrichment, four frontiers, snapshot bridge, trade gap/backfill/invalidation, physical isolation/control eligibility, workload/environment manifests, coordinated-omission controls, accepted numeric profile | `normalization` endpoints are unchanged; frontiers are separately observable; telemetry does not become authority; tail/burst/overload evidence passes preregistered rules |
| **MD-E16 — Capture/replay/fidelity proof** | Capture/enrichment/normalized manifests, faithful capture-order comparison, normalized-fact replay, raw re-normalization across distinct reference lineages, corrected event-time replay, trade/book gaps, backfill, fidelity labels, semantic checksums | Replay classes and lineages cannot be confused; faithful replay uses original versions and matches expected outcomes; gaps/window invalidations remain visible |
| **MD-E17 — Multi-venue/L3 seam review** | Second mock venue with incompatible symbols/sequences and optional mock L3 payload passed through SDK/registry boundaries | New adapter/L3 types can be added without changing canonical authority, envelope, existing L2 fact meaning, or single-venue behavior |
| **MD-E18 — Normalized frontier and publication crash matrix** | Crash/fault at position allocation, normalized acceptance, frontier advance, every publication state, retryable/terminal failure, stream consumer acceptance/ack loss, pending contiguous gap, and run-input selection | Frontiers reconstruct independently; same identity/position republishes idempotently; terminal holes block later allocation until explicit recovery; no cursor is inferred or advanced incorrectly |
| **MD-E19 — Snapshot bridge ownership and pre-acceptance buffer proof** | Raw/candidate ownership, valid bridge, duplicate/overlap, invalidity, timeout, overflow, crash, abandoned attempt, new epoch, position allocation | No epoch position or normalized acceptance exists before valid bridge; failure leaves raw/enrichment evidence, accepts none of the buffer, and starts a new attempt/epoch or remains gapped |
| **MD-E20 — Public-trade gap, backfill, fidelity, and window suite** | Complete/partial/ambiguous/unavailable backfill, held live trades, deduplication, lossy new epoch, fidelity labels, time/count window invalidation and revalidation | Existing epoch continues only after complete verified bridge; otherwise no fabricated trades, lossy epoch is explicit, and crossing windows/features remain invalid |
| **MD-E21 — Physical isolation versus configured eligibility suite** | Security/resource isolation before control outcome; accepted/rejected/delayed/no eligibility-change command; reconnect; required/optional aggregate readiness | Physical source can stop immediately; eligibility/configuration changes only at ordered accepted effective position; interim status remains unavailable/degraded and configured requiredness is preserved |
| **MD-E22 — Independent critique and cumulative compatibility review** | Critique disposition and comparison against all approved Phase 01–03 leaves and affected PRDs | Zero unresolved material findings; no authority, lifecycle, ordering, clock, recovery, replay, telemetry, security, or evidence contract is weakened |

### Phase 04 leaf exit conditions

This leaf is implementation-complete only when:

- one venue is selected through `MD-E01`, not by tentative PRD wording;
- the selected public adapter passes its capability, security, lifecycle, raw-capture, and conformance suites;
- every activated normalized type has deterministic fixtures, exactly-one-source lineage, and one immutable decode-enrichment/member lineage;
- source sequence/timestamp assertions discovered after capture are immutable enrichment facts and never mutate `SourceEvent`;
- the normalized accepted frontier, per-fact publication state, stream consumer cursor, and run-input cursor reconstruct independently across every crash point;
- reference selection is deterministic within a pinned reference/configuration lineage, and different lineages produce distinct normalized datasets/streams;
- the venue sequence/epoch/snapshot recovery contract is documented and machine tested, with no snapshot-buffer position or acceptance before a valid bridge;
- reconnect, book/trade gap, duplicate, out-of-order, late, schema-change, overload, crash, and recovery behavior are typed and bounded;
- trade streams prove verified backfill or open explicit degraded/lossy epochs with manifest fidelity and crossing-window invalidation;
- physical source isolation can protect safety immediately while configured eligibility changes only through the ordered `ControlOutcome` path;
- source and reference lineage versioning supports faithful replay and explicit re-normalization without rewriting history;
- capture and normalization durability/RPO policies pass the applicable Phase 03 persistence and recovery evidence;
- source, normalization, and stream quality/readiness cannot be conflated;
- the Phase 02 telemetry points, alert schema, performance method, accepted observability profile, and statistical rules remain intact;
- a Phase 04 empirical capacity/performance profile is accepted for the versioned workload and environment;
- multi-venue and L3 seam tests pass without implementing production multi-venue or L3 behavior;
- `MD-E01` through `MD-E22` pass;
- `EC-E01` through `EC-E17`, including the control-barrier, batch, sequencing, lineage, lifecycle, and cumulative-review extensions affected by this leaf, plus applicable persistence/replay gates and Phase 01–02 evidence extensions pass;
- independent critique and cumulative Phase 01–03 review have no unresolved material finding.

This leaf does not fail because L2 market state, strategies, paper trading, multi-venue operation, L3 reconstruction, or live execution are not implemented. It fails if those capabilities would require redefining accepted source/normalized facts, lineage, ordering, epochs, clocks, replay classes, authority boundaries, or safety/security assumptions.

## Deferred choices

The following remain for implementation planning, the companion Phase 04 leaf, or later evidence-driven phases:

1. Final venue selection and endpoint/channel names.
2. Concrete transport, WebSocket/HTTP, TLS, DNS, event-loop, parser, compression, and client libraries.
3. Serialization, schema-definition, registry, capture-journal, and dataset products.
4. Exact IDs, hashes, checksums, signatures, and canonical binary/text encodings.
5. Numeric timeouts, queue/buffer sizes, message limits, retry schedules, lateness bounds, watchlist maximum, and rate-limit budgets.
6. Exact capture RPO, retention duration, backup topology, and storage format.
7. Exact source-specific sequence, snapshot, checksum, heartbeat, and resume algorithm until venue selection evidence is approved.
8. L2 state application, synchronization readiness, state checksums, freshness deadlines, and query views.
9. Production multi-venue adapters, cross-venue identity resolution, and merge policy.
10. L3 payload and state model beyond reserved additive extensions and seam fixtures.
11. Private account data and execution authentication.
12. Regulatory, contractual, tax, or commercial data-retention requirements beyond the selected public-feed use.

Deferral does not authorize incompatible local conventions. Each choice remains behind the approved contracts and must satisfy this leaf's evidence gates before activation.

## Cumulative review checklist

At this and every later phase gate:

1. Confirm source capture, normalization, reference, stream, dataset/replay, observability, and market-state authorities remain distinct.
2. Confirm raw source evidence precedes semantic parsing and malformed input cannot become plausible normalized truth.
3. Confirm post-capture source assertions exist only in immutable decode-enrichment/failure facts and never mutate `SourceEvent`.
4. Confirm each non-synthetic/non-administrative normalized fact has exactly one source event and matching enrichment/member, including source corrections and multi-member messages.
5. Confirm capture order, source order, normalized accepted order, publication state, stream consumer order, run-input order, and event time remain distinct.
6. Confirm normalized accepted frontier, publication state, stream cursor, and run-input cursor recover independently and idempotent re-publication never reallocates identity/position.
7. Confirm all source and normalized sequences retain explicit scope and epoch; reconnect, snapshot, or receive time never silently proves continuity.
8. Confirm every new epoch is recoverable before new-epoch normalized publication and every historical gap remains visible.
9. Confirm normalization remains deterministic for pinned source/schema/normalizer/registry/reference-configuration-lineage/selection-policy versions.
10. Confirm reference selection uses the registered semantic key and effective basis, rejects zero/multiple matches, and distinct lineages create distinct normalized datasets/streams.
11. Confirm mutable aliases, current code, or current reference data cannot reinterpret historical runs as faithful.
12. Confirm no snapshot candidate receives an epoch position or accepted normalized identity until the complete bridge validates; failure/overflow abandons the buffer visibly.
13. Confirm duplicates, book/trade gaps, out-of-order facts, late facts, corrections, backfill, and re-normalization remain different concepts.
14. Confirm trade gaps either close through verified backfill or create degraded/lossy new epochs with fidelity and crossing-window invalidation.
15. Confirm source clocks follow Phase 02 comparability and uncertainty rules.
16. Confirm public market-data runtimes have no execution/private credentials or permissions.
17. Confirm each parser, decompressor, queue, retry path, snapshot/backfill/reorder buffer, and quarantine allocation is bounded and fault tested.
18. Confirm Phase 02 lifecycle-exact points preserve the canonical `ingress` and `normalization` endpoints and optional telemetry cannot block authoritative work.
19. Confirm capture/decode/normalization lifecycle uses `Accepted`, `Published`, `Recoverability-accepted`, and `Recoverable` exactly as approved, and `record_time` proves recoverability.
20. Confirm faithful, normalized-fact, raw re-normalization, and corrected event-time replay classes cannot be confused.
21. Confirm watchlist/subscription eligibility changes use the single `ControlOutcome` and effective-position barrier without an adapter-side alternative control path.
22. Confirm physical disconnect/isolation may happen immediately for safety while configured eligibility/requiredness remains unchanged and explicitly unavailable/degraded until an accepted outcome changes it.
23. Confirm external subscription side effects are causally linked but are not misrepresented as atomic with control effective positions; failed venue operations create source readiness failures, not duplicate control outcomes.
24. Confirm normalized semantic acceptance and position allocation are atomic while publication, stream-consumer acceptance, and run-input selection remain separate.
25. Confirm later market-state, multi-venue, L3, external, and execution capabilities can add contracts without changing this leaf's accepted meaning.
26. Re-run affected Phase 01–03 fixtures, performance baselines, compatibility checks, and independent critique.

Approval of this document fixes the source-adapter and normalization semantic baseline. Later phases may add venue adapters, event types, stricter quality rules, deeper books, L3 facts, or stronger durability, but they may not invent continuity, erase raw evidence, weaken exactly-one-source lineage, reinterpret pinned history, conflate telemetry with authority, or give public market-data code execution capability.
