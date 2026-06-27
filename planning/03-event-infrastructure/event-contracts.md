# Chronos Event Contracts

## Purpose

This document defines the planning-level event and command contracts for Chronos. It fixes the stable logical envelopes, taxonomy, identities, lineage, ordering, deterministic run-input merge, command outcomes, effective-position behavior, correction rules, compatibility policy, safe failure behavior, and conformance evidence required before event infrastructure is implemented.

It builds on the approved Phase 01 domain model and architecture and the approved Phase 02 observability contracts. Those documents remain authoritative for domain meaning, authority ownership, fact-publication lifecycle, latency semantics, and telemetry. This document makes their event-facing obligations concrete without selecting a serializer, queue, journal, database, schema product, or deployment topology.

The contract applies wherever a command or immutable fact crosses a logical authority boundary, including an in-process call. Transport and storage may optimize representation, but they may not weaken the logical contract.

## Objectives

The event infrastructure must:

1. Preserve one unambiguous meaning for commands and facts across live processing, replay, recovery, audit, and datasets.
2. Keep source evidence immutable while allowing deterministic normalization, correction, and reprocessing.
3. Make every ordering claim explicit in its stream, epoch, sequence, and merge-policy scope.
4. Reproduce the exact run-input order and control timing consumed by a run.
5. Prevent duplicate delivery, retries, batching, crashes, and rebuilds from duplicating state or economic effects.
6. Fail closed when an unknown or incompatible message could affect state, behavior, safety, or economics.
7. Permit additive evolution without silently changing historical meaning.
8. Remain portable across in-memory, journal, dataset, and future IPC representations.
9. Keep authoritative domain and audit facts distinct from non-authoritative telemetry.
10. Provide machine-verifiable evidence for compatibility, determinism, recovery, security, and performance.

## Authority and scope

This document does not create new runtime or domain authorities. Phase 01 ownership remains unchanged.

The architecture-owned **canonical contracts governance function** owns:

- logical envelope definitions and their versions;
- event and command type registrations;
- schema identities, compatibility declarations, and support windows;
- per-type ordering, correction, idempotency, unknown-type, and consumer-impact metadata;
- canonical fixture identities and conformance expectations;
- registry publication, integrity, and deprecation records.

This is repository and release governance, not an additional mutable domain authority or hot-path service. It does not own the facts described by registered types. Each command and event type has exactly one semantic owner from the Phase 01 authority model. Only that owner may accept facts in its namespace or define the state transition they represent.

The **stream and run-input authority** remains the sole owner of:

- stream identity, epoch, and accepted sequence progression;
- merge-policy definition and version;
- deterministic run-input selection;
- `run_input_sequence`;
- run-input cursor vectors and dispatch facts;
- enforcement of the control-admission/dispatch barrier;
- application of accepted `ControlOutcome` facts at their effective positions.

The **run and configuration authority** remains the sole owner of run lifecycle, run manifests, configuration epochs, ordered accepted/rejected `ControlOutcome` facts for behavior-changing commands, and reset lineage.

Source capture, normalization, reference, execution, accounting, audit, and observability retain their Phase 01 owners. A shared library, process, journal, or database never transfers semantic ownership.

## Non-goals

This document does not:

- choose a wire format, schema-definition language, code generator, registry product, queue, log, database, or object store;
- define venue-specific market-data leaf payloads, order protocols, paper-fill models, or ledger posting schemas owned by later phases;
- define retention periods, replication factors, flush intervals, or numeric recovery objectives;
- create a global order across independent external sources;
- make telemetry authoritative for domain state;
- permit an event journal to become an unowned shared-state integration API;
- define ticket-level implementation work.

## Contract principles

### Commands request; events prove

A command asks one authority to attempt a transition. It is not evidence that the transition happened. An event is an immutable past-tense fact accepted or observed by its owning authority.

Every newly admitted behavior-changing command identity produces exactly one ordered terminal `ControlOutcome`: `accepted` or `rejected`. The `ControlOutcome` is both the command decision and the run-control fact; an accepted command does not produce a second activation/control event. An accepted outcome carries the behavior change and its reserved effective position. A rejected outcome carries the rejection and no behavior effect. A retry recognized as the same idempotent request returns a `deduplicated` response referencing that original outcome; it does not create another outcome or state transition. Infrastructure failure before admission may return a transport/admission failure, but it may not fabricate a domain rejection or acceptance.

The control-admission/dispatch barrier makes the ordered outcome visible to the stream/run-input authority before dispatch may cross the reserved boundary. Faithful replay consumes the accepted/rejected `ControlOutcome` history, not the original imperative command as proof of decision or effect.

### Acceptance, publication, and recoverability remain distinct

The Phase 01 lifecycle is preserved:

```text
computed
  -> accepted
  -> published
  -> recoverability-accepted
  -> recoverable
```

An event envelope represents an accepted or observed fact. Envelope creation does not by itself prove publication or recoverability. Journals and persistence adapters must expose their exact handoff state; a successful enqueue, batch append request, or asynchronous flush request is not called durable or recoverable. `record_time` retains the Phase 01 meaning: it is populated only when the event is proven recorded at its declared recoverable boundary. `recoverability_handoff_time` is the separate name for an attempted or accepted persistence/reconstruction handoff that has not yet proved recoverability.

### Append-only history

Accepted facts are never edited in place. Corrections, supersessions, invalidations, reversals, busts, expirations, and tombstones are new typed facts linked to the prior facts they affect. Historical bytes may be re-encoded or migrated only when semantic equivalence and original identity remain provable.

### Ordering is scoped

No bare sequence is meaningful. Every ordered event position is interpreted with its stream and epoch. Run-input order is a Chronos-created deterministic order for one run; it is not a claim that external facts occurred in one globally true order.

### Unknown is not success

An unknown event, command, schema version, enum value, correction relation, or state effect is never coerced into a familiar meaning. Consumers preserve unknown data where possible and fail closed where its effect cannot be proven safe.

### Representation is replaceable; semantics are stable

The same logical command or event may be represented as a typed in-process value, a bounded queue item, an individual journal record, a batch member, or a dataset row. Every representation must reconstruct the same identities, ordering, causation, version, and payload meaning.

## Message classes

Chronos distinguishes the following classes. These classifications are normative and may not be collapsed for implementation convenience.

| Class | Meaning | Authoritative status | May enter run-input merge? | Correction model |
|---|---|---|---|---|
| **Source fact** | Captured source bytes or values plus capture/session metadata before Chronos assigns domain meaning | Authoritative evidence of what Chronos received; not authoritative market meaning | No; normalization or a declared raw-replay provider consumes it | New source/capture facts; original capture is immutable |
| **Normalized fact** | Venue-neutral market-domain statement derived from source evidence or an explicit synthetic/administrative origin | Authoritative accepted input fact for downstream domain processing | Yes, when its registered type and stream are selected by the run | Linked correction/supersession; live history is not retroactively rewritten |
| **Reference fact** | Versioned instrument, listing, mapping, venue-rule, or trading-status definition | Authoritative reference meaning owned by reference authority | Yes, when behavior/state depends on its effective version | New effective version or explicit correction linked to the prior definition |
| **Control fact** | One ordered accepted/rejected `ControlOutcome` for a behavior-changing command; accepted outcomes carry the change and explicit effective position | Accepted outcome is authoritative for run behavior from its effective position; rejected outcome is authoritative evidence of no change | Accepted outcomes enter the dedicated run-control merge; rejected outcomes remain ordered control history outside `run_input_sequence` | New superseding accepted outcome; prior decisions/effects remain historical |
| **Run-timer fact** | One run/configuration-owned logical-clock advancement generated under the pinned timer policy, with stream/epoch/sequence, monotonic deadline evidence, and intended freshness/status scope | Authoritative only for deterministic run logical-time progression and policies that explicitly consume it; it has no market meaning | Yes, on the registered run-timer stream | New ordered timer fact or epoch transition; accepted timer history is never rewritten |
| **Execution fact** | Intent, approval, order, acknowledgement, submission, cancellation, fill, correction, bust, or reconciliation-related execution history | Authoritative only for the state owned by the emitting execution authority | Not as a market/state run input unless a later explicitly versioned workflow declares it; never silently mixed into market order | State-machine transition or linked correction/reconciliation fact |
| **Accounting fact** | Ledger transaction, posting, cash, fee, funding, settlement, mark, valuation, or accounting projection history | Ledger/posting facts are authoritative economic records; projections are derived as declared | No for market/strategy run input; accounting replay consumes its own ordered inputs | Compensating/reversal transaction or linked correction; no in-place mutation |
| **Audit fact** | Immutable evidence about material actors, commands, security decisions, policy outcomes, or evidence gaps | Authoritative evidence surface, but not a replacement for the owning domain fact | No | Linked amendment/correction that preserves the original audit record |
| **Telemetry fact** | Log, metric, trace, health, profile, loss, or alert signal under Phase 02 contracts | Non-authoritative operational evidence, except that an explicitly named audit record remains audit rather than telemetry | No | Phase 02 profile/reset/loss semantics; never domain correction |

External observations, features, strategy evaluations, recommendations, portfolio targets, and risk facts are domain events using the stable event envelope and their Phase 01 namespaces. Their leaf schemas activate in their owning phases. They are not source, telemetry, or audit facts merely because they contain provenance or diagnostics.

## Logical envelopes

### Envelope versioning

The envelope has an independent `envelope_version`. Payload schemas have independent `schema_version` values. A payload version change does not require an envelope change unless routing, identity, ordering, or provenance semantics change.

Every encoded representation identifies:

- envelope kind: `command`, `event`, or `batch`;
- envelope version;
- contract-registry snapshot identity or a resolvable immutable reference;
- payload type and schema version;
- integrity information required by the selected representation.

An envelope version is supported only when its required fields and semantics can be interpreted. Unknown optional fields may be preserved. Missing or unknown required envelope semantics fail before authority admission.

### Command envelope

Every state-changing command contains:

| Field | Requirement |
|---|---|
| `command_id` | Stable identity for this request across transport retries. |
| `command_type` | Namespaced imperative type registered to exactly one target authority. |
| `envelope_version` | Logical command-envelope version. |
| `schema_version` | Command payload schema version. |
| `target_authority` | Authority permitted to decide the command. |
| `actor` | Authenticated actor/service identity and authentication context reference; never secret material. |
| `submitted_by` | Originating component/runtime identity when different from actor. |
| `run_id` | Required when the transition belongs to a run. |
| `scope_refs` | Typed invariant scope, such as run, strategy instance, portfolio, account, execution adapter, or observability profile. |
| `expected_precondition` | Optional expected state/version/epoch/cursor used for compare-and-set behavior. Absence must be explicitly permitted by the command type. |
| `requested_effective_position` | Optional requested run-input boundary or scheduling expression for behavior-changing commands. It is a request, not the accepted effective position. |
| `idempotency_key` | Required retry identity within the command type's declared idempotency scope. It may equal `command_id` where the registry says so. |
| `reason_code`, `reason_text` | Typed reason and bounded human context where required by safety or operator policy. |
| `issued_time` | Named wall-clock issuance time with clock-domain/precision metadata; never used alone to order run effect. |
| `admission_deadline` | Optional deadline for admission; expiry produces an admission outcome, never silent drop. |
| `correlation_refs` | Optional workflow references; not causation proof. |
| `payload` | Command-specific immutable request data. |
| `integrity` | Representation-level checksum/signature/authentication reference as required by trust boundary. |

Commands do not contain `event_id`, authoritative `stream_sequence`, accepted `effective_position`, or domain `record_time` as if the requested transition already occurred.

### Event envelope

Every accepted or observed domain/audit event contains directly, or reconstructably through a batch frame:

| Field | Requirement |
|---|---|
| `event_id` | Stable identity of this accepted or observed fact. |
| `event_type` | Registered namespaced past-tense semantic type. |
| `envelope_version` | Logical event-envelope version. |
| `schema_version` | Version required to decode and interpret the payload. |
| `semantic_owner` | Owning authority. It must match the registry entry. |
| `producer` | Producing component, implementation version, and runtime incarnation. |
| `acceptance_class` | `accepted_transition`, `accepted_observation`, `accepted_rejection`, `accepted_correction`, or another registered non-ambiguous class. |
| `run_id` | Required when accepted or observed in a run; reusable source capture also identifies its capture session. |
| `mode` | Required when mode affects interpretation or permitted consumers. |
| `stream_id`, `stream_epoch`, `stream_sequence` | Required for ordered facts. All three appear together. |
| `run_input_sequence` | Required only for an event selected as a run input or a dispatch fact about that selection. |
| `effective_position` | Required for accepted `ControlOutcome` and reference facts whose effect begins at a run-input boundary; absent from rejected outcomes. |
| `source_event_id` | Required exactly once for every non-synthetic, non-administrative normalized fact, including normalized corrections. |
| `causation_refs` | Typed direct causes. Required for derived transitions unless the event is a declared root observation. |
| `correlation_refs` | Optional workflow grouping; never substitutes for causation. |
| `subject_refs` | Typed canonical subjects. Display symbols or mutable names do not substitute for identities. |
| `source_event_time` | Source assertion when available, with source-clock quality. |
| `chronos_receive_time` | Required where Chronos ingress occurred. |
| `accept_time` | Time the owner accepted the fact, with clock-domain identity. |
| `recoverability_handoff_time` | Optional lifecycle metadata for the time a persistence/reconstruction boundary accepted responsibility or an attempt was made; it is not durable-record proof. |
| `record_time` | Present only when the fact is proven recorded and recoverable under the declared failure model. It is the durable/recoverable recording time, not enqueue, append-attempt, publication, or handoff time. |
| `quality` | Validation, completeness, freshness, uncertainty, correction, and fidelity status relevant to interpretation. |
| `payload` | Event-specific immutable facts. |
| `integrity` | Representation-level checksum/signature/authentication reference as required by trust boundary. |

Fields that do not apply are absent. Zero values, empty strings, epoch `0`, or synthetic timestamps may not be used to disguise missing applicability or unavailable evidence.

`record_time` and `recoverability_handoff_time` are lifecycle metadata, not mutable business payload. They may be carried by an enclosing persistence record/batch when they were unavailable at event acceptance. `record_time` is excluded from domain identity and the pre-recording semantic checksum, becomes immutable once assigned, and must be reproducibly associated with the same `event_id`. An in-memory or published pre-recoverability envelope omits it; it may not fill it with acceptance, enqueue, flush-request, or publication time.

### Command admission and `ControlOutcome` envelope

Admission and authority decisions are distinct:

```text
received
  -> admitted | admission_rejected | admission_expired | admission_unavailable
  -> ordered ControlOutcome(accepted, behavior change, effective position)
     | ordered ControlOutcome(rejected, no behavior effect)
  -> published outcome
  -> effective, for accepted outcomes only

retry of an already decided idempotency identity
  -> deduplicated response referencing the original ControlOutcome
```

Admission outcomes state whether the command entered the target authority's bounded decision path. They do not claim a domain decision. Every authenticated, structurally valid behavior-changing command that enters control admission is serialized to exactly one ordered accepted/rejected `ControlOutcome`; implementations may not discard an admissible policy rejection as a mere transport response. Only requests that fail before authority admission—such as malformed, unauthenticated, unsupported, expired-before-admission, or unavailable admission-path requests—end with a typed admission result and no `ControlOutcome`.

Every newly admitted behavior-changing command identity receives exactly one ordered terminal `ControlOutcome` event on the dedicated run-control stream:

- `accepted`: the authority accepted the behavior change; the outcome itself carries the complete change, prior state/epoch, new configuration/control epoch, and reserved `effective_position`;
- `rejected`: the authority evaluated the command and declined it with a typed reason; the outcome occupies its ordered control-stream position but carries no `effective_position`, changes no epoch, and has no run behavior effect.

The `ControlOutcome` is owned by the run/configuration authority for run behavior, while policy-specific validation may consult the authority that owns the affected invariant. It contains:

- `command_id`, `command_type`, and `idempotency_key`;
- actor and scope references;
- target/consulted authority references;
- accepted/rejected status;
- typed reason and precondition result;
- one run-control stream/epoch/sequence position;
- the complete behavior change, prior state/epoch, new state/configuration epoch, and chosen `effective_position` when accepted;
- no separate activation/control event reference for the same change;
- authority serialization position or version where the affected authority contract exposes one;
- causation to the command;
- no secret or raw authentication material.

A deduplicated retry response is a command-protocol response, not a second `ControlOutcome`. It contains the original outcome reference and disposition and may produce an audit fact under policy, but replay and domain state continue to reference the single original outcome.

A timeout waiting for an outcome produces an `unknown_to_caller` response, not a synthetic rejection. The command client must query or retry with the same idempotency identity. It must never issue a semantically equivalent command under a fresh identity merely because the prior outcome is unknown.

### Atomic control-admission/dispatch barrier

For every admitted behavior-changing command, one serialization transaction or equivalently atomic recoverable protocol must:

1. evaluate idempotency, authorization, preconditions, and conflict ordering;
2. allocate exactly one run-control stream sequence;
3. accept exactly one `ControlOutcome`;
4. for an accepted outcome, reserve the exact `effective_position` and resulting control/configuration epoch;
5. make the outcome, decision status, complete behavior change, and effective-position reservation visible to the stream/run-input authority;
6. advance the dispatch barrier so no `RunInputSelectionRecord` at or beyond that effective position can be accepted under the prior epoch.

The barrier invariant is:

```text
dispatch_frontier >= reserved effective_position
  implies accepted ControlOutcome and reservation were already visible
  and the selected record references the new epoch
```

The outcome and reservation may share a transaction, append group, or deterministic reconstruction protocol; the technology is deferred. They may not be split into independently losable facts. If acceptance succeeds but visibility or recoverability is uncertain, dispatch remains stopped at the barrier and recovery reconstructs the original outcome/reservation before continuing. It never re-decides the command, reallocates its control-stream position, or chooses a new effective position.

Rejected outcomes also receive exactly one ordered control-stream position and remain replayable/auditable. They reserve no effective position and do not block later dispatch after the ordered rejection is visible. Admission failures before authority admission are not `ControlOutcome` facts; they remain typed admission results and audit/telemetry evidence as policy requires.

### Telemetry envelope relationship

Telemetry records use the Phase 02 telemetry envelope, not the domain event envelope. They may reference `event_id`, `command_id`, cursors, run input, or batch identity through bounded correlation fields, but trace/span identity is not domain causation and telemetry loss cannot erase a domain transition.

An audit fact about telemetry profile activation, security-sensitive exporter changes, evidence loss, or unauthorized access remains an `audit.*` or owning-domain event. A metric or log about that audit fact remains telemetry.

## Taxonomy

### Type naming

Event types use:

```text
<namespace>.<subject>.<past_tense_fact>
```

Command types use:

```text
<authority_or_area>.<subject>.<imperative_action>
```

Names are lowercase ASCII segments with a fixed separator and registry-enforced syntax. A type name is semantic, not a class name, topic name, table name, endpoint, or file path. Renaming a field or moving code does not justify a new type; changing meaning does.

### Stable namespaces

| Namespace | Fact class and minimum meaning | Initial activation |
|---|---|---|
| `source.*` | Capture payload, framing, integrity, session, continuity, and capture-failure facts | Phase 03 |
| `reference.*` | Instrument/listing/mapping/rule/status definition and effective-version facts | Phase 03 contract; leaf payloads Phase 04 |
| `market.book.*` | Book snapshot, delta, synchronization, quality, and recovery facts | Phase 04 |
| `market.trade.*` | Public trade and trade-stream quality facts | Phase 04 |
| `market.control.*` | Heartbeat, stream status, gap, reconnect, epoch, and recovery facts | Phase 03 contract; source-specific leaves Phase 04 |
| `external.*` | External observation, quality, correction, and availability facts | Phase 11A |
| `run.control.*` | Run lifecycle, configuration activation, strategy enablement, pause, kill switch, reset, and recovery facts | Phase 03 |
| `run.timer.*` | Run/configuration-owned logical-clock advancement, timer-stream continuity, and missed/degraded timer facts | Phase 03 contract; active freshness use Phase 04 |
| `run.input.*` | Merge selection, dispatch, cursor checkpoint, and merge-policy application facts | Phase 03 |
| `feature.*` | Valid feature and diagnostic observation facts | Phase 05 |
| `strategy.*` | Evaluation, signal, abstention, expiration, and supersession facts | Phase 05 |
| `recommendation.*` | Actionable/hold recommendation, expiration, and supersession facts | Phase 05 |
| `opportunity.*` | Opportunity, candidate, resolution, and ranking facts | Phase 10B/11A as assigned |
| `portfolio.*` | Target, aggregation, and supersession facts | Phase 07 |
| `risk.*` | Projected exposure, risk outcome, reservation outcome/state, release, and expiry facts | Phase 07 |
| `execution.approval.*` | Human live approval, rejection, revocation, and expiry facts | Phase 11B |
| `execution.intent.*` | Executable intent and execution-group lifecycle facts | Phase 08; live extension Phase 11B |
| `execution.order.*` | Order submission outcome and order-state facts | Phase 08; live extension Phase 11B |
| `execution.fill.*` | Fill, correction, bust, fee, and allocation facts | Phase 08; live extension Phase 11B |
| `ledger.*` | Transaction, posting, valuation, and accounting projection facts | Phase 08 |
| `reconciliation.*` | Match, discrepancy, freeze, correction workflow, and resolution facts | Phase 08; live extension Phase 11B |
| `audit.*` | Security/operator evidence not already fully represented by the owning domain event | Phase 03 foundation; cumulative |

Telemetry names remain under the Phase 02 `chronos.<area>.<subject>.<measurement_or_event>` convention and are not added to the domain event taxonomy.

### Type registry entry

Each command or event type registration contains:

- stable type name and semantic description;
- semantic owner and authorized producer classes;
- envelope kind and minimum envelope version;
- payload schema identity/version;
- fact class;
- permitted modes and introducing phase;
- subject and causation requirements;
- ordering domain and partition-key rules;
- whether it is eligible for run-input merge;
- state-effect classification by consumer capability;
- idempotency and deduplication scope;
- correction, supersession, expiry, and tombstone policy;
- unknown-field, unknown-enum, unknown-version, and unknown-type behavior;
- compatibility window and deprecation state;
- data classification, size, rate, and security limits;
- canonical valid/invalid fixtures;
- required telemetry points and evidence owner.

No unregistered type may be accepted as an authoritative Chronos event. Source capture may retain an unsupported payload as opaque source evidence, but it must not assign unsupported domain meaning.

## Identity contract

### Identity properties

Canonical identities are opaque values with explicit type and scope. They:

- are stable for the lifetime of the logical fact;
- are unique within their declared scope with collision handling that fails closed;
- are not derived solely from mutable display names, timestamps, process-local counters, or payload serialization bytes;
- survive representation migration and batch reconstruction;
- do not embed secrets or sensitive payload fragments;
- have canonical textual and binary comparison forms where both are supported;
- are never silently reused for semantically different facts.

The exact ID generation algorithm remains an implementation choice. If deterministic IDs are used, the registry must define canonical inputs, collision behavior, version, and whether re-normalization creates a new identity. Random and deterministic identities may coexist only with explicit type rules.

### Required identities

| Identity | Meaning |
|---|---|
| `command_id` | One submitted command across retries. |
| `event_id` | One accepted or observed immutable fact. |
| `source_event_id` | One capture occurrence; duplicate source deliveries may have distinct IDs. |
| `deduplication_fingerprint` | Evidence that two deliveries may represent the same source/economic fact; never substitutes for event identity. |
| `capture_session_id` | One bounded source/file/generator capture session. |
| `stream_id` | One logical ordering domain. |
| `stream_epoch` | One interval in which continuity rules remain valid. |
| `run_id` | One bounded execution with immutable initialization manifest. |
| `batch_id` | One physical/logical batch frame; never substitutes for member event IDs. |
| `registry_snapshot_id` | Immutable set of contract/type/schema registrations used to interpret records. |
| `schema_id`, `schema_version` | Payload contract identity and version. |
| `merge_policy_id`, `merge_policy_version` | Deterministic run-input selection policy. |
| `configuration_epoch` | Effective interval for behavior-changing configuration. |

Derived domain identities introduced in later phases follow Phase 01 identity rules and are added to the registry without changing these meanings.

## Source lineage and capture contract

### Source fact requirements

A source fact records, directly or through a capture batch:

- source adapter and implementation version;
- source system, connection/file/generator identity, and declared trust class;
- `capture_session_id`;
- source channel/topic/file partition and framing context;
- source-provided sequence, message, transaction, and time values exactly as asserted, with their scope and quality;
- `chronos_receive_time` at the earliest practical ingress boundary;
- monotonic capture partition and sequence;
- raw payload bytes or a lossless source representation;
- payload length and integrity digest;
- decompression/deframing metadata where needed to reproduce bytes-to-message interpretation;
- validation status without discarding malformed or unsupported source evidence;
- reconnect, truncation, corruption, loss, or source-session status;
- data classification and redaction restrictions.

Capture does not canonicalize symbols, infer economic sides, repair prices, or claim venue-neutral meaning.

### Normalized lineage

Every normalized fact references exactly one `source_event_id`, with only these exceptions:

- a synthetic generator origin;
- an explicitly administrative origin that has no external/source message.

Both exceptions carry one typed synthetic/administrative origin identity, generation policy/version, actor or generator identity, and reason. They may not be used to hide missing source capture.

Normalized corrections remain exactly-one-`SourceEvent` facts: the correcting source message is captured as its own `SourceEvent`, and the resulting normalized correction references that one source plus the prior normalized fact it corrects through `causation_refs` or the registered correction relation. A correction inferred by Chronos from multiple accepted facts is a derived domain correction/reconciliation fact, not a normalized event.

Aggregations over multiple source or normalized events are feature observations, market-state views, quality assessments, or another explicitly derived fact. They are never represented as normalized events with multi-source lineage. A source message that contains multiple independent domain facts may still produce multiple normalized events, but each resulting normalized event references that same single `source_event_id`.

The normalized fact also identifies:

- normalizer implementation and policy version;
- source framing/schema version;
- reference/listing-definition version used;
- canonical mapping result;
- normalized stream and cursor;
- validation and uncertainty status;
- preserved source extensions required for audit or future re-normalization.

One source event may produce zero, one, or many normalized events. Zero produces a typed normalization rejection/quarantine fact. Many preserve stable member ordering and distinct normalized event identities, each with exactly that one source identity. Re-normalization uses a new normalizer/reference selection and creates a new dataset/lineage; it never overwrites the prior normalized facts.

## Stream, epoch, and sequence semantics

### Stream identity

A stream is one logical ordering domain with:

- owner;
- stream kind;
- partition key and subject scope;
- source or producer scope;
- sequence assignment rule;
- epoch transition rule;
- gap/duplicate/out-of-order policy;
- permitted event types;
- correction behavior;
- retention/reconstruction owner;
- declared consumers.

Examples include a source capture partition, normalized book stream, normalized public-trade stream, reference stream, market-control stream, dedicated run-control stream, execution-order stream, fill stream, ledger stream, and audit stream. They are not assumed to share a clock or sequence.

### Epoch

An epoch identifies a continuity interval. A new epoch is required when continuity cannot be proven, including:

- reconnect without a provable resume position;
- source sequence reset or ambiguous wrap;
- snapshot/reset contract establishing a new base;
- producer incarnation change where sequence continuation is unproven;
- journal recovery beyond the accepted loss bound;
- incompatible stream-policy or partitioning change;
- explicit administrative reset defined by the stream contract.

Epoch transition is an accepted fact. The first sequence, predecessor epoch, transition reason, continuity status, recovery basis, and any required snapshot/base cursor are recorded. Consumers never compare sequence numbers across epochs without the registered transition relation.

### Sequence

`stream_sequence` is a monotonic integer within `(stream_id, stream_epoch)`. The registry defines whether it is:

- inherited from a source sequence;
- assigned at capture;
- assigned at normalized acceptance;
- assigned by a domain authority at accepted transition.

A source sequence remains a source assertion and does not become the Chronos stream sequence unless the stream contract explicitly proves the equivalence. Sequence allocation is part of authority acceptance. Gaps in assigned Chronos sequences are prohibited unless the registry defines reserved/aborted positions and consumers can distinguish them from lost facts.

### Cursor

A `StreamCursor` is:

```text
(stream_id, stream_epoch, last_consumed_sequence)
```

It includes an explicit origin value before the first consumed event. A cursor asserts only accepted consumption through that position under the stream contract. It does not assert recoverability, wall-clock recency, or completeness beyond the current epoch's continuity status.

Cursor checkpoints contain registry, schema, and stream-policy references sufficient to interpret the position. Checkpoints are accelerators/evidence; the stream owner remains authoritative.

## Deterministic run-input merge

### Eligible inputs

Only registered normalized market, reference, market-control, run-control, and run-timer event types explicitly marked `run_input_eligible` may enter the canonical market/strategy run-input merge. Run-timer facts are owned by the existing run/configuration authority, advance the run's pinned logical-clock policy, and carry no market meaning by themselves. Source, execution, accounting, audit, and telemetry facts never enter that merge merely because they have timestamps.

Future workflows that consume execution or accounting events use separately named ordered processors and manifests. They may reuse envelope mechanics but cannot silently broaden the canonical market run-input set.

### Merge manifest

Each run initialization manifest references an immutable merge contract containing:

- `merge_policy_id` and version, owned by stream/run-input authority;
- ordered input stream identities and initial cursors;
- required versus optional streams;
- stream priority classes;
- eligibility predicates;
- tie-break keys and canonical comparison rules;
- watermark, lateness, buffering, and timeout policy where applicable;
- `ControlOutcome` priority and effective-position assignment rules;
- behavior for a missing, gapped, ended, stalled, or newly introduced stream;
- batch expansion order;
- correction policy and replay class;
- arithmetic/string/canonicalization rules used by comparisons;
- terminal conditions and checksum method.

The run/configuration authority selects and records the policy. The dataset/replay authority reconstructs and validates inputs under that selected policy. Neither may define a competing merge meaning.

### Selection and dispatch

For each selected input, the stream/run-input authority:

1. validates the candidate against its stream cursor, registry, and run manifest;
2. evaluates the pinned merge policy over all currently eligible heads;
3. selects exactly one logical event, expanding batches into canonical member order;
4. verifies the atomic control-admission/dispatch barrier and determines the control/configuration epoch that must apply;
5. atomically accepts one `RunInputSelectionRecord` that assigns the next monotonic `run_input_sequence`, binds the selected input, and advances the cursor vector;
6. makes that selection record recoverable, or atomically recoverable with publication, before publication can become externally effective;
7. publishes the already selected input to the declared consumer using the selection identity and assigned sequence;
8. accepts an append-only publication-state transition for that same selection.

Tie-breaking must be total for the eligible candidate set. Host scheduling, map iteration order, pointer value, thread race, arrival at an unspecified queue, or serializer field order may not decide the result.

### `RunInputSelectionRecord`

The logical selection record is the authoritative allocation of one run-input position. Its initial accepted form contains:

- stable `selection_id`;
- `run_id`;
- assigned `run_input_sequence`;
- selected input `event_id`, type, stream, epoch, and sequence;
- complete pre-selection `RunInputCursorVector`;
- complete post-selection `RunInputCursorVector`;
- active run-control cursor, accepted `ControlOutcome` references, configuration epoch, and last applied/reserved effective positions;
- merge-policy and registry snapshot identities;
- canonical tie-break/eligibility evidence sufficient to verify the choice;
- initial `publication_state = not_published`;
- intended consumer/publication boundary;
- semantic checksum and integrity metadata.

Allocation of `selection_id` and `run_input_sequence`, acceptance of the record, and transition from the pre-selection to post-selection cursor vector are one atomic state transition. It is invalid to allocate or expose a sequence without the record, advance a cursor without the selected input identity, or publish an input whose selection record is not recoverable or atomically becoming recoverable under the selected protocol.

The selection's publication state is a durably reconstructable append-only state machine:

```text
not_published
  -> publication_in_progress
  -> published_to_consumer_boundary
  -> consumer_accepted
  | publication_failed_retryable -> publication_in_progress
  | publication_failed_terminal
```

The initial selection record carries `not_published`. Later `run.input.publication_*` facts carry `selection_id`, the same `run_input_sequence`, attempt identity, consumer boundary, outcome, and acknowledgement evidence. They update the reconstructed publication state without mutating the original selection fact. `published_to_consumer_boundary` is the Phase 01 `Published` state; `consumer_accepted` is separate. A retryable failure may transition only through a new `publication_in_progress` attempt. A terminal failure is never automatically retried.

On replay or recovery:

- an existing selection is reconstructed by `selection_id` and `run_input_sequence`;
- if its reconstructed publication state is not `published`, the authority re-publishes the same selected input with the same selection identity, input identity, cursor vectors, active epoch, and sequence;
- the consumer deduplicates by selection identity/run-input position and cannot apply it twice;
- recovery never reevaluates the merge choice for that allocated position;
- recovery never allocates a replacement sequence for the same selection;
- if the record cannot be reconstructed exactly, the run stops as incomplete/non-faithful rather than guessing or selecting a new candidate;
- a publication acknowledgement lost after consumer application is resolved by idempotent re-publication and consumer acknowledgement of the original selection.

New merge evaluation begins only after the prior selection's publication policy permits the frontier to advance. Pipelining is allowed only if the implementation proves the same allocation, barrier, ordering, and idempotent recovery invariants for every in-flight selection.

### Run-input sequence and effective position

`run_input_sequence` is a total order only within one `run_id`. It starts from a declared origin and advances monotonically with no silent reuse.

For an accepted `ControlOutcome`:

- `effective_position` is the first run-input boundary whose domain processing observes the new behavior; the pinned merge/control policy defines whether control-dispatch bookkeeping has its own preceding sequence or shares an atomic boundary, and this choice cannot vary within the run;
- all inputs before it use the prior configuration/control epoch;
- the behavior carried by the `ControlOutcome` is applied atomically before the first affected input;
- multiple controls for the same boundary are ordered by the pinned control serialization and merge rules;
- conflicting commands produce their own exactly-one accepted/rejected ordered `ControlOutcome` rather than last-writer ambiguity;
- a requested wall/event time is resolved to a concrete position before effect;
- the admission/dispatch barrier prevents acceptance of a selection at or beyond the position under the prior epoch;
- replay uses the recorded accepted outcome and position, not a fresh interpretation of “now.”

A rejected `ControlOutcome` has an ordered run-control history position but no `effective_position`, no new control/configuration epoch, no behavior effect, and is never selected into the canonical run-input merge. It remains recoverable control history for audit, idempotent retry, and faithful reconstruction of command outcomes without consuming `run_input_sequence`.

### Cursor vector

The run-input authority maintains a complete canonically ordered `RunInputCursorVector` containing:

- one `StreamCursor` for every required or declared optional input stream;
- explicit origin cursors for streams not yet consumed;
- stream continuity/quality status;
- current `run_input_sequence`;
- last accepted `selection_id` and reconstructed publication frontier;
- consumed run-control cursor;
- active configuration epoch, last applied effective position, and any accepted reserved control boundary not yet crossed;
- merge-policy identity/version;
- registry snapshot identity;
- replay/live input class.

Omitting a declared stream is invalid. Adding or removing an optional stream is a manifest/configuration change with an explicit effective boundary or a new run, according to the registered policy.

The vector supports deterministic reconstruction and lineage; it is not a claim of external simultaneity. A digest may be used in telemetry or indexes only when the full vector remains recoverable and the digest algorithm/version is declared.

## Batch framing

### Batch semantics

A batch is a representation and handoff unit, not a new domain fact unless a registered batch-control event explicitly says otherwise. Each logical member retains its own identity, type, schema, ordering position, causation, and correction semantics.

A batch frame contains:

- `batch_id`, batch schema/version, producer, and runtime incarnation;
- registry snapshot and envelope version;
- member count and canonical member order;
- shared-field dictionary or base values;
- per-member deltas/overrides sufficient to reconstruct each full logical envelope;
- first/last stream cursor for single-stream batches, or explicit per-member cursors for mixed streams;
- integrity digest over canonical member envelopes or an equivalent verifiable construction;
- compression/framing metadata;
- maximum encoded and decoded sizes;
- acceptance mode: atomic batch or independently accepted members;
- partial-failure policy;
- creation and handoff lifecycle points.

### Batch rules

- Batch order never creates cross-stream order unless the merge policy explicitly uses that batch order.
- A mixed-stream batch cannot replace per-member stream cursors.
- Batch acceptance is atomic only when every affected authority and consumer contract declares it atomic.
- In independently accepted batches, each member has an explicit accepted/rejected/quarantined result; prefix acceptance and restart behavior are deterministic.
- A corrupt frame cannot produce partially trusted members unless member integrity and framing permit proven isolation.
- Retrying a batch preserves member command/event identities and idempotency keys.
- Rebatching, compression, and columnar conversion preserve canonical member semantics and fixture checksums.
- Oversized, deeply nested, decompression-amplifying, or count-exceeding batches fail bounded validation before resource exhaustion.

## Corrections, supersession, expiry, and tombstones

### Correction relation

A correction is a new event that identifies:

- corrected event or economic fact;
- correction type and reason;
- fields or interpretation affected;
- authoritative evidence causing the correction;
- effective economic/source time where relevant;
- acceptance time and ordering position of the correction;
- whether downstream recomputation, reconciliation, or compensating accounting is required;
- fidelity impact for faithful versus corrected replay.

A correction never reuses the original `event_id`. Consumers must be able to present original history and corrected current interpretation.

### Supersession relation

Supersession means a newer fact becomes current for a declared semantic scope while the earlier fact remains valid history for its original interval. The type registry defines:

- supersession key/scope;
- whether supersession is total or partial;
- precedence and tie-breaking;
- effective position/time;
- handling of concurrent or conflicting superseders;
- whether downstream work already caused by the old fact remains historical, expires, or requires compensating action.

Configuration, reference definitions, strategy signals, recommendations, targets, and projections use typed supersession or expiry rules owned by their phases.

### Tombstones

A tombstone states that a key or fact is no longer active or available under a declared scope. It is not physical deletion and cannot erase source, audit, execution, fill, or ledger history.

Tombstones are permitted only for types whose registry entry defines:

- tombstone semantic owner;
- target identity/key;
- reason and effective boundary;
- whether the target never existed, was removed, expired, revoked, or became inaccessible;
- consumer behavior;
- retention and audit obligations.

Unknown tombstone semantics fail closed. Physical compaction may discard superseded representations only after the retention/reconstruction contract proves that required historical and correction chains remain available.

### Replay treatment

- Faithful capture-order and normalized-fact replay preserve the original correction arrival/effective positions.
- Raw re-normalization may create new normalized correction lineage but does not mutate the original dataset.
- Corrected event-time research replay may apply a declared repair policy, but its manifest identifies every correction source and cannot claim faithful identity.

## Deduplication and idempotency

### Distinct concepts

- **Duplicate delivery**: the same command/event identity arrives again.
- **Source duplicate**: separate capture occurrences may represent the same source message.
- **Semantic/economic duplicate**: different messages may claim the same trade, fill, fee, or transition.
- **Retry**: a sender deliberately resubmits with the same command/idempotency identity.
- **Replay**: accepted history is deliberately consumed again in a new or recovered processing context.

These are not interchangeable.

### Command idempotency

Each command type declares:

- idempotency-key scope and retention/reconstruction source;
- request-equivalence fields;
- behavior when the same key arrives with different payload, actor, scope, or precondition;
- terminal outcome retrieval;
- in-flight duplicate behavior;
- expiry policy, if any, that cannot permit an unsafe repeated effect;
- crash/recovery handling.

Same key plus non-equivalent request is a typed conflict and never silently aliases the original command. A deduplicated outcome references the original accepted/rejected event. Safety-critical command identity remains queryable for at least the complete period in which replay or repeated effect is possible.

### Event idempotency

Consumers apply accepted events idempotently by `event_id` within the declared authority/stream scope. For economic facts, the consumer additionally enforces the phase-owned economic identity, such as venue execution ID or ledger transaction cause, because distinct deliveries may have distinct transport identities.

Deduplication state is itself recoverable or reconstructable to the failure model required by the consumer. Losing a local cache cannot make a duplicate order, fill, reservation, or ledger effect acceptable.

### Deduplication outcome

Duplicate source deliveries remain capturable. Their normalized/state effect follows the stream's deduplication policy and produces explicit evidence:

- duplicate detected and linked;
- duplicate accepted as distinct because source semantics require it;
- uncertain duplicate quarantined or marked degraded;
- collision/conflict requiring recovery.

Payload hash alone is insufficient where identical legitimate events can occur. Deduplication keys and collision policy are source/type-specific and versioned.

## Ordering, gaps, late events, and out-of-order behavior

### Validation before state effect

An ordered consumer validates:

- recognized envelope/type/schema under the pinned registry;
- authorized producer and owner;
- stream/epoch match;
- expected next sequence or permitted duplicate;
- source/normalized continuity evidence;
- correction relation;
- run manifest eligibility;
- bounded freshness/lateness policy;
- payload and resource limits.

Failure produces no undeclared state effect.

### Gaps

A forward gap:

1. creates an explicit gap fact with expected/observed positions and detection evidence;
2. marks the affected stream `gapped` or `recovering`;
3. prevents consumption by state/strategy paths that require continuity;
4. invokes the registered resume, backfill, snapshot, or epoch-transition procedure;
5. records whether continuity was restored, replaced by a new base, or remains unproven;
6. marks affected runs/datasets incomplete or non-faithful when policy bounds are exceeded.

Skipping a gap because later payloads appear plausible is prohibited.

### Out-of-order events

Out-of-order buffering is permitted only with:

- bounded item and byte capacity;
- bounded event-time/sequence distance and residence time;
- deterministic release order;
- overflow and expiry behavior;
- telemetry for depth, age, release, rejection, and recovery;
- manifest-pinned policy.

Beyond the bound, the event is quarantined, rejected, or triggers stream recovery according to type policy. It is never inserted into already published live state as if processed on time.

### Late events

Lateness is evaluated against a named policy: source sequence, capture order, watermark/event time, run-input position, or domain validity deadline. A generic `late=true` without policy and bound is invalid.

Late facts may:

- be retained as source evidence;
- produce a typed late/rejected/correction event;
- trigger recovery or a new epoch;
- be included in a new corrected research dataset;
- be ignored for a particular consumer only when the registry explicitly proves that doing so is safe and auditable.

Live outputs already accepted remain historical. Recomputing a corrected research result creates new run and lineage identities.

### Duplicates

An exact previously accepted event identity does not advance state twice. Whether it advances a transport offset or acknowledgement is representation-specific, but the logical consumer outcome is `duplicate_no_effect` linked to the first acceptance.

### Stream end and stall

Clean end-of-stream, temporary stall, source silence, and failed connection are distinct facts. Required-stream stall affects merge eligibility and run health according to the manifest. A timeout cannot be converted into end-of-stream unless the source contract says so.

## Reference, configuration, and control events

### Reference events

Reference changes that can affect normalization, market state, strategy validity, sizing, or execution:

- are immutable versioned facts;
- identify canonical instrument/listing and prior version;
- carry an explicit effective position for run consumption;
- state compatibility and migration impact;
- never retroactively reinterpret earlier accepted events in a faithful run;
- cause a new run/dataset or explicit corrected lineage when re-applied historically.

Static reference lookup outside the ordered run path may support validation or capture metadata, but behavior-affecting reference values must be pinned in the manifest or consumed through ordered events.

### Configuration events

Behavior-changing configuration uses:

```text
command
  -> one ordered ControlOutcome:
       rejected(no effect)
       or accepted(complete configuration change,
                   reserved effective_position,
                   new configuration_epoch)
  -> atomic dispatch barrier
  -> effective application at the reserved position
```

The accepted `ControlOutcome` is the activation fact. It contains a canonical redacted configuration identity, schema/version, prior epoch, changed fields or configuration reference, actor, reason, scope, and chosen effective position. No second `configuration_activated` event is emitted for the same command. Secret values are referenced through approved secret identities and never copied into the event.

Static process configuration remains versioned process-start evidence under Phase 01. It does not masquerade as an in-run control event unless explicitly reclassified by an approved contract change.

### Safety control events

Pause, kill-switch, execution freeze, strategy disablement, and similar safety controls:

- use reserved-capacity authenticated admission;
- emit exactly one ordered accepted/rejected `ControlOutcome`;
- carry the complete behavior change and exact effective position in that outcome when accepted;
- pass the atomic control-admission/dispatch barrier before dispatch may cross that position;
- have explicit scope and prior state;
- cannot be superseded by a lower-authority or stale command;
- remain recoverable as required before a trade-capable run can be represented as faithful;
- produce authoritative boundary acknowledgements in phases that implement those boundaries;
- are projected by observability but never inferred from telemetry silence.

## Run lifecycle events

### State machine

The canonical run lifecycle is:

```text
Created
  -> Initialized
  -> Running <-> Paused
Running | Paused -> Stopping -> Completed
Created | Initialized | Running | Paused | Stopping -> Aborted
Created | Initialized | Running | Paused | Stopping -> Failed
```

Implementation activities such as initializing, pausing, resuming, aborting, failing, and recovering are lifecycle transition facts, not additional authoritative run states. Unsupported transitions are rejected. A run may move directly from `Created`, `Initialized`, `Running`, or `Paused` to `Failed` where recovery cannot preserve its declared semantics. Terminal `Completed`, `Aborted`, and `Failed` states do not become active again.

### Lifecycle facts

Registered lifecycle facts include at least:

- run declared with initialization-manifest identity;
- initialization started/completed/failed;
- readiness established/withdrawn;
- start `ControlOutcome` accepted with its effective position and resulting running epoch;
- pause `ControlOutcome` accepted or rejected, with an accepted outcome carrying its effective position and resulting paused epoch;
- resume `ControlOutcome` accepted or rejected, with an accepted outcome carrying its effective position and resulting running epoch;
- stop `ControlOutcome` accepted or rejected, with an accepted outcome carrying its effective position and resulting stopping epoch;
- abort `ControlOutcome` accepted or rejected, with an accepted outcome carrying its effective position, reason, and resulting aborted terminal state;
- completion accepted with terminal-attestation reference;
- abort accepted with terminal-attestation reference;
- failure accepted with reason and last proven cursor vector;
- recovery started, continuity evaluated, and recovered/child-run-required/failed outcome;
- reset `ControlOutcome` accepted or rejected, with an accepted outcome carrying child-run creation semantics.

Command requests are commands, not lifecycle facts. Effective application is the deterministic consequence of dispatch reaching the position recorded in the single accepted `ControlOutcome`; it does not create a second activation event. The exact leaf names are fixed in the registry before implementation.

### Reset

Reset never deletes or rewrites a run. It:

1. emits exactly one ordered accepted/rejected reset `ControlOutcome`;
2. when accepted, carries reset initiation, the reserved effective position, and the child-run creation policy without a second reset-initiation event;
3. passes the atomic dispatch barrier and stops or seals the prior run according to its mode;
4. creates a new `run_id` and immutable initialization manifest;
5. records parent/reset causation and selected initial state;
6. preserves prior events, cursor vectors, ledger/accounting history, and terminal status.

Paper portfolio/account reset semantics are owned by the portfolio/accounting phases and must use new account/portfolio/accounting epochs or compensating facts, never historical deletion.

### Recovery

Same-run recovery is permitted only when:

- the initialization manifest is unchanged;
- accepted input/control history and cursor vector are recoverable;
- the active configuration epoch and effective position are proven;
- stream continuity remains within policy;
- replay from a trusted checkpoint produces the expected semantic checksum;
- no authority-specific ambiguity requires reconciliation or a child run.

Otherwise recovery creates a causally linked child run or marks the run incomplete/non-faithful/failed. Recovery events state which case applies.

## Unknown type and version behavior

### Unknown envelope version

If required envelope semantics cannot be interpreted, the message is not admitted to an authority or ordered stream. It is retained only as opaque source/quarantine evidence where safe and within resource/security policy.

### Unknown command type

An unknown command type is admission-rejected. It cannot be routed by naming convention, best-effort field matching, or fallback handler. The rejection records the caller-visible unsupported type without reflecting secret/payload content.

### Unknown event type

An event type may be skipped by a specific consumer only when all are true:

1. the consumer recognizes the envelope version;
2. the run pins a registry snapshot containing the event type even if the consumer lacks its payload decoder;
3. the registry explicitly marks the type `ignorable` for that consumer capability and version;
4. ordering advancement without state effect is explicitly permitted;
5. causation, correction, tombstone, and safety/economic classifications prove no hidden dependency;
6. the event is retained or forwarded losslessly as required;
7. a typed unsupported/ignored evidence record and telemetry signal are produced.

Otherwise the consumer stops or quarantines the affected stream/scope. Unknown control, reference, correction, tombstone, execution, fill, ledger, reconciliation, safety, or run-input-affecting types always fail closed.

### Unknown fields and enum values

- Unknown optional fields are preserved across read/write/migration where the representation supports it.
- Unknown required fields are impossible by definition; a producer adding required semantics needs a compatible version transition.
- Unknown enum values are accepted only when the field registry defines an `unknown-preservable` behavior that cannot alter state or safety. They are never mapped to a familiar default.
- Consumers may reject a newer schema even when decoding succeeds if semantic compatibility is not registered.

## Schema and compatibility model

### Version dimensions

The following evolve independently:

- command envelope;
- event envelope;
- batch frame;
- type taxonomy/registry;
- payload schema;
- stream policy;
- merge policy;
- correction/supersession policy;
- canonicalization and checksum method;
- source framing;
- run manifest and cursor-vector schema.

Every retained record carries or inherits enough immutable references to interpret all applicable dimensions.

### Compatibility classes

Each schema transition is classified:

| Class | Meaning | Consumer behavior |
|---|---|---|
| **Identical semantics** | Representation-only change with proven canonical equivalence | May migrate/re-encode without new domain identity |
| **Backward compatible** | New producer output remains semantically interpretable by declared older consumers | Allowed only within registered support window and fixtures |
| **Forward preservable** | Older consumer can preserve but not interpret added optional data, with no hidden effect | May relay/store; may not claim interpretation |
| **Transformable** | Explicit deterministic migration yields the target schema with provenance | New representation/lineage records migration identity and evidence |
| **Re-normalization required** | Source meaning must be interpreted again under new normalizer/reference rules | Creates new normalized lineage/dataset |
| **Incompatible** | Meaning or required behavior changed | Reject, isolate, or start a new run/dataset/version boundary |

Shape compatibility is insufficient. Unit, precision, rounding, default, ordering, optionality, enum, nullability, identity, correction, and authority changes are semantic.

### Evolution rules

1. Historical source facts are never rewritten to fit a new source or normalized schema.
2. Additive optional fields are compatible only when absence has a defined meaning and old consumers can safely preserve or ignore them.
3. Changing units, scale, sign, time basis, sequence scope, identity scope, effective-position semantics, or correction meaning is incompatible unless an explicit migration proves equivalence.
4. Removing or reusing a field number/name/type identity is prohibited within its compatibility lineage.
5. Defaults that affect behavior are materialized or pinned by schema/configuration version; hidden runtime defaults are prohibited.
6. Registry and schema changes are reviewed, versioned, signed/checksummed as appropriate, and produce immutable change records.
7. A run pins the registry/schema support set it uses. Mid-run semantic upgrades require an ordered configuration/reference event where allowed or a new run.
8. Deprecated types remain interpretable for their declared retention/support period or have a tested migration/rebuild path.

## Serialization and portability

### Canonical logical form

Chronos defines a serializer-independent canonical logical form for:

- field names/identities and types;
- integer ranges and signedness;
- decimal quantity/price representation;
- timestamps, precision, and clock-domain references;
- byte strings and text normalization;
- enums and unknown values;
- maps, sets, and canonical ordering;
- absent versus null versus empty values;
- identity textual/binary form;
- checksums and semantic digests.

Floating-point values are not used for canonical monetary, price, quantity, sequence, or identity semantics unless a later schema explicitly proves acceptable behavior. Non-finite numbers are rejected unless a diagnostic-only schema explicitly permits them.

### Round-trip requirements

Every supported representation proves:

- logical envelope round trip;
- payload round trip for supported versions;
- unknown optional-field preservation where promised;
- batch expansion/rebatching equivalence;
- stable identity, ordering, causation, correction, and subject references;
- canonical checksum equivalence;
- cross-language fixture equivalence where multiple runtimes exist;
- rejection of malformed, ambiguous, non-canonical, or resource-amplifying encodings.

Language-native object serialization, pointer/address identity, platform-dependent integer width, host-endian assumptions, locale-sensitive parsing, and undocumented time-zone behavior are prohibited at authority boundaries.

## Validation and security limits

### Validation stages

Validation is layered:

1. **Frame validation:** length, count, nesting, compression, integrity, and envelope-kind bounds.
2. **Envelope validation:** required fields, types, version support, identities, timestamp names, and applicability.
3. **Registry validation:** registered type, owner, producer authorization, mode, stream eligibility, and compatibility.
4. **Payload validation:** schema, units, ranges, precision, enumerations, and cross-field invariants.
5. **Ordering validation:** stream/epoch/sequence, cursor, duplicate, gap, lateness, and correction relation.
6. **Authority validation:** preconditions, state-machine transition, actor authorization, idempotency, and safety policy.

Passing an earlier stage does not imply acceptance by a later stage.

### Resource limits

Every envelope/type declares bounded:

- encoded and decoded bytes;
- field count, nesting depth, string/byte length, list/map member count;
- batch member count;
- decompression ratio and work;
- validation time/work budget;
- causation/correlation/subject reference count;
- unknown-field retained bytes;
- per-source/type rate and burst;
- quarantine volume and retry attempts.

Limits are derived and tested through the Phase 02 performance method. Exceeding a limit produces a typed rejection/quarantine/overload result and telemetry; it never causes unbounded allocation or silent truncation.

### Trust and authorization

- Producers authenticate at trust boundaries and may emit only registered types for their producer class.
- An event's declared `semantic_owner` must match the registry; a producer cannot claim another authority.
- Commands are authorized for actor, action, scope, mode, and freshness before admission/acceptance as defined by the command type.
- Replayed, expired, forged, cross-scope, or cross-run commands fail explicitly.
- External/source fields never choose an internal type, authority, file path, code class, schema location, or command target without allowlisted mapping.
- Registry/schema artifacts have integrity, provenance, and rollback protection.
- Error records avoid payload reflection and secret leakage.
- Secrets, credentials, authentication tokens, private keys, and secret-derived raw values are prohibited in commands, events, batches, fixtures, audit exports, and telemetry.
- Quarantine and raw source stores follow data classification and restricted access; retaining malformed source evidence does not make it safe to display or export.

## Contract registries and governance

### Registry artifacts

The registry is an immutable versioned artifact set containing:

- envelope and batch schemas;
- type registrations;
- payload schemas;
- producer/consumer capability declarations;
- compatibility graph;
- stream and merge-policy references;
- canonicalization/checksum versions;
- lifecycle/correction/idempotency metadata;
- security/data-classification limits;
- fixtures and conformance expectations;
- activation phase and deprecation status.

A `registry_snapshot_id` resolves to a complete immutable set. Mutable aliases such as `latest` may aid tooling but cannot appear as run provenance.

### Registration process

A new or changed type requires:

1. named semantic owner and reviewers;
2. problem and lifecycle placement;
3. envelope/payload schema;
4. ordering and state-effect analysis;
5. causation, subjects, correction, expiry, and idempotency rules;
6. unknown-type/version behavior;
7. compatibility classification and support window;
8. security/resource classification;
9. canonical positive/negative fixtures;
10. performance and telemetry impact;
11. migration/rebuild/rollback path;
12. cumulative review against earlier contracts.

Changes to authority, ordering, run-input eligibility, effective-position meaning, safety/economic classification, or canonical lifecycle require an architecture decision and re-review of affected prior phases.

### Capability negotiation

Producers and consumers declare supported envelope, registry, schema, and type ranges. Negotiation:

- occurs before ordered consumption or run start;
- selects an exact compatible set recorded in process/run evidence;
- never silently downgrades safety or drops required semantics;
- fails readiness when no compatible set exists;
- does not permit a running faithful replay to change interpretation mid-run.

## Performance and telemetry hooks

### Mandatory Phase 02 points

Event infrastructure emits or exposes the exact Phase 02 lifecycle points applicable to implemented facts:

- `source.ingress.local_received`;
- `source_event.accepted`;
- source-event publication and recoverability subpoints;
- normalized-event acceptance and `normalized_event.published.stream_authority`;
- `run_input.accepted`, defined as acceptance of the atomic `RunInputSelectionRecord`;
- `run_input.recoverability_accepted` and `run_input.recoverable`;
- `run_input.published.market_state`, tied to the selection's reconstructed `published` state;
- `<fact>.recoverability_accepted`;
- `<fact>.recoverable`;
- queue enqueue/dequeue/expiry/completion points;
- batch creation/acceptance/flush/recovery points where implemented.

The Phase 02 synthetic kill-switch path is adopted without renaming, omitting, or conflating any boundary:

1. `command_received`;
2. `command_admitted`, or its typed rejection point;
3. `control_outcome_accepted`, which is the single ordered accepted/rejected `ControlOutcome`;
4. `effective_position_assigned`, atomically coupled to an accepted outcome and dispatch-barrier reservation;
5. `effective_applied`;
6. `execution_boundary_ack.accepted` for every synthetic capable boundary;
7. `execution_fence_ack.accepted` for the authoritative synthetic aggregate;
8. `execution_fence_ack.published.observability`;
9. `operator_acknowledgement.published.command_caller_read_model`.

`control_outcome_accepted` and `effective_position_assigned` remain separate logical measurement points even though an accepted outcome and its reservation are one atomic transition; an implementation may record equal or tightly adjacent same-clock times but may not collapse the semantics. Rejected outcomes emit `control_outcome_accepted` and no `effective_position_assigned`.

Event infrastructure does not create real risk, reservation, intent, or adapter fencing before those phases exist. It does implement the real admission, `ControlOutcome`, effective-position reservation, dispatch barrier, application, publication, and recovery behavior, while retaining the Phase 02 synthetic downstream boundary/fence authorities. Observability only projects steps 6–8; missing, stale, contradictory, or wrong-epoch acknowledgements yield `unknown`, never `execution_fenced`.

The canonical Phase 01/02 segments remain unchanged:

- `ingress`;
- `normalization`;
- `dispatch_wait`;
- `recoverability_acceptance`;
- `recoverability_proof`.

Subsegments may refine but not redefine these endpoints. Acceptance and publication remain separate even if atomic in one implementation.

### Required metrics and health inputs

Schemas are registered for:

- accepted/rejected/quarantined/duplicate events and commands by bounded type class and reason;
- events/bytes/batches offered, accepted, published, recoverability-accepted, and recoverable;
- stream cursor, epoch transitions, gaps, out-of-order depth/age, late events, and recovery state;
- run-input offered/selected/dispatched rates and merge wait;
- queue capacity, depth, oldest age, rejection, expiry, and drain;
- batch size, encoded/decoded bytes, compression work, partial failure, and corruption;
- schema/registry/version incompatibility;
- deduplication hit/conflict and idempotency outcome;
- validation work and resource-limit rejection;
- journal/persistence handoff lag where the companion persistence leaf activates it;
- replay semantic checksum and deterministic mismatch;
- telemetry loss/profile epoch under Phase 02 rules.

High-cardinality identities remain in restricted logs/traces/audit/evidence, not general metric dimensions. Health projections distinguish stream healthy, degraded, gapped, recovering, incompatible, stalled, and unknown. Missing evidence never becomes healthy.

### Performance workloads

Phase 03 activates Phase 02 performance methods for:

- small and maximum-size individual messages;
- homogeneous and mixed batches;
- normal, burst, and overload arrival shapes;
- multiple independent streams and deterministic merge;
- gaps, duplicates, late/out-of-order buffering, and recovery;
- registry/schema validation cache cold/warm behavior;
- malformed and adversarial inputs;
- persistence handoff and recoverability paths introduced by the companion leaf;
- replay and rebuild throughput;
- shutdown/drain and restart continuation.

Every queue is bounded under `PS-E07`. Coordinated-omission protection, scheduled-versus-actual arrival evidence, SUT-induced pressure classification, accepted telemetry profile, and preregistered statistical rules remain mandatory.

Phase 03 also reruns the complete Phase 02 synthetic kill-switch command-to-fence campaign with the real event-infrastructure admission path, single `ControlOutcome`, atomic effective-position/dispatch barrier, run-input selection records, crash/recovery behavior, and the same synthetic execution-boundary/fence/operator-publication path. It adopts or tightens the accepted empirical budgets for admission, outcome acceptance, effective-position assignment, effective application, each boundary acknowledgement, aggregate fence acknowledgement, observability publication, and operator acknowledgement under the Phase 03 reference workloads. It may not drop a point, replace the aggregate fence with absence-of-work inference, reset a budget without evidence, or defer the campaign until Phase 07.

Event handling may not block on optional telemetry. Mandatory audit or recoverability fences use their own explicit authority/persistence contract and are not hidden inside a logging call.

## Event-infrastructure invariants

The following invariants are cumulative with Phases 01–02:

1. Every admitted behavior-changing command identity has exactly one ordered accepted/rejected `ControlOutcome`; accepted commands do not emit a second activation/control fact.
2. An accepted `ControlOutcome` contains the complete behavior change, new epoch, and effective-position reservation; a rejected outcome is ordered but has no effect or effective position.
3. Dispatch cannot accept or publish a selection at or beyond a reserved effective position until the accepted outcome/change/reservation is visible and the selection references the resulting epoch.
4. Every allocated `run_input_sequence` belongs to exactly one atomic, recoverable `RunInputSelectionRecord` containing selected input identity, complete pre/post cursors, and initial publication state.
5. Replay/recovery republishes an incomplete original selection idempotently with the same identity and sequence; it never reruns merge selection or allocates a substitute position.
6. Every non-synthetic/non-administrative normalized event, including a normalized correction, references exactly one `SourceEvent`; multi-source aggregation is a derived observation, not normalization.
7. `record_time` exists only after proven recoverable recording. Acceptance, publication, enqueue, append attempt, flush request, or recoverability handoff uses its own named evidence and cannot populate `record_time`.
8. Unknown control, correction, selection, publication-state, execution, accounting, or safety semantics cannot advance an authoritative cursor or state.
9. All nine Phase 02 synthetic kill-switch measurement points remain present and semantically distinct; observability cannot create boundary or aggregate fence acknowledgements.
10. Crash/restart cannot create a second `ControlOutcome`, effective position, selection identity, run-input sequence, normalized lineage, or economic/state effect.

## Testing strategy

### Fixture matrix

Canonical fixtures cover:

- every envelope kind and stable namespace;
- each fact class: source, normalized, reference, control, run-timer, execution, accounting, audit, and telemetry linkage;
- required/optional/absent fields;
- minimum/maximum values and resource limits;
- all compatibility classes;
- unknown fields, enum values, types, envelope versions, and schema versions;
- single-stream and mixed-stream batches;
- corrections, supersessions, expirations, reversals, and tombstones;
- exactly-one accepted/rejected `ControlOutcome`, deduplicated retry responses, timeout to caller, idempotency conflict, and control-barrier visibility;
- run-timer origin/progression, duplicate, gap, missed deadline, epoch transition, live generation, faithful replay without regeneration, and deterministic merge ordering;
- atomic selection records with pre/post cursors and every publication state;
- exactly-one-SourceEvent normalized lineage, normalized source corrections, explicit synthetic/admin origins, and rejected multi-source normalized aggregations;
- `recoverability_handoff_time` without `record_time`, and proven recoverable records with `record_time`;
- all Phase 02 synthetic kill-switch boundary/fence/operator points;
- run lifecycle and reset/recovery paths;
- each replay class's treatment of correction and ordering.

Fixtures are immutable, checksum-addressed, and language-neutral. Expected acceptance, rejection reason, canonical form, state effect, cursor effect, and emitted evidence are machine-readable.

### Contract conformance

Every producer/consumer implementation runs:

- envelope and payload validation;
- registry owner/producer authorization;
- serialization round trip;
- unknown-field preservation;
- type/version compatibility;
- batch reconstruction and rebatching;
- ordering/cursor advancement;
- atomic control-barrier and selection/publication-state reconstruction;
- deduplication/idempotency;
- correction chain;
- resource/security rejection;
- telemetry-point placement;
- semantic checksum tests.

A producer cannot be declared conformant using only its own decoder. At least one independent reference fixture runner or cross-implementation check validates canonical form.

### Property and model-based tests

Generated tests prove:

- accepted stream sequences never move backward or cross epochs implicitly;
- cursor advancement equals exactly the accepted consumed prefix;
- deterministic merge returns the same run-input order for identical manifests and candidate sets regardless of producer scheduling, batching, or map order;
- every selected input appears once and only once unless the manifest explicitly filters it;
- every allocated run-input sequence has exactly one reconstructable selection record containing selected input identity, complete pre/post cursors, and publication state;
- recovery republishes the same unpublished selection and cannot allocate a second sequence for it;
- complete cursor vectors never omit declared streams;
- an accepted `ControlOutcome` and effective-position reservation are visible before any selection can cross the boundary under the new epoch;
- accepted controls affect exactly the first position specified; rejected outcomes are ordered and have no effect;
- each admitted behavior-changing command produces exactly one accepted/rejected `ControlOutcome`; retries produce no second outcome or transition;
- each non-synthetic/non-administrative normalized event has exactly one `source_event_id`; generated multi-source normalized aggregations are rejected;
- `record_time` is absent until recoverability is proven and never equals a mere handoff time by convention;
- duplicate events do not duplicate state/economic effect;
- correction/supersession graphs are acyclic and owner-valid;
- batch expansion is semantically equivalent to individual delivery;
- unknown unsafe types cannot advance state;
- malformed inputs cannot cause unbounded work or allocation;
- reset creates a new run and never deletes prior history.

### Determinism and replay tests

Golden runs include:

- independent book, trade, reference, market-control, and run-control streams;
- same-time/tie candidates requiring every merge tie-break key;
- controls immediately before, at, and after effective positions;
- accepted and rejected `ControlOutcome` sequences with no duplicate activation facts;
- crash after outcome acceptance but before barrier visibility, after effective-position reservation, after selection allocation, and before/after consumer acknowledgement;
- idempotent re-publication of the same selection identity and `run_input_sequence`;
- batch boundaries changed without changing logical member order;
- duplicate, gap, out-of-order, late, epoch reset, correction, and stream-end cases;
- crash/restart from every fact lifecycle handoff implemented in Phase 03;
- faithful capture-order versus normalized-fact versus raw re-normalization versus corrected event-time manifests.

Repeated runs under compatible implementations produce identical accepted event order, `ControlOutcome` order, run-input order, selected logical input identities, pre/post cursor vectors, publication-state reconstruction, control epochs, semantic checksums, and terminal fidelity classification. Selection IDs must be identical within crash recovery/re-publication of one run. Across independently created equivalent runs, identity equality is required only if the selected ID policy is explicitly deterministic; otherwise semantic selection equivalence is tested independently of opaque IDs. Telemetry identities and host processing times are excluded as required by Phase 02.

### Negative security tests

Tests reject:

- forged owner/producer identity;
- unauthorized command actor/scope/mode;
- command replay under altered payload;
- registry rollback or untrusted schema substitution;
- unknown control/execution/accounting types;
- payload-driven type or code dispatch;
- oversized/deep/decompression-amplifying frames;
- malformed decimal/time/sequence values;
- secret canaries in every envelope and error path;
- cross-run/cross-epoch cursor injection;
- correction or tombstone from a non-owner;
- batch member count/integrity mismatch.

### Failure and recovery tests

Faults include:

- crash before acceptance, after acceptance, after publication, after recoverability acceptance, and after recoverability;
- crash during every control-admission/dispatch-barrier step;
- crash after atomic selection acceptance in every publication state, including lost consumer acknowledgement;
- partial/corrupt batch;
- deduplication-state loss;
- registry unavailable after run start;
- incompatible producer restart;
- queue full and poison input;
- cursor checkpoint corruption;
- gap recovery success/failure;
- control accepted but not yet effective;
- telemetry/collector unavailable;
- storage full/read-only/timeouts where activated by the persistence leaf.

No fault may become a plausible success. Tests verify the correct incomplete, failed, non-faithful, quarantined, recovering, or fail-closed status.

## Evidence and exit gates

Planning approval fixes the evidence contract below. Artifacts become mandatory during implementation of their owning phase and are rerun cumulatively when affected.

| Evidence ID and artifact | Owning phase | Required contents | Pass condition |
|---|---|---|---|
| **EC-E01 — Envelope and taxonomy registry** | 03 | Versioned command/event/batch envelopes, stable namespaces, type entries, owners, fact classes, modes, ordering, idempotency, correction, unknown behavior, resource/security limits | Commands cannot masquerade as events; every type has one owner; source/normalized/control/execution/accounting/audit/telemetry meanings remain distinct |
| **EC-E02 — Cross-representation conformance suite** | 03 | Canonical individual/batch fixtures through every implemented in-memory, journal, dataset, or IPC representation | Logical identities, order, causation, subjects, versions, unknown fields, and semantic checksums round-trip identically |
| **EC-E03 — Command admission, `ControlOutcome`, and barrier model** | 03 foundation; extend 07/09/11B | Admission fixtures, exactly-one accepted/rejected ordered outcomes, accepted outcome carrying the complete behavior change/effective position/new epoch, rejected outcome carrying no effect, timeout/unknown-to-caller, deduplicated retry, conflict ordering, crash between every barrier step | Accepted commands produce one `ControlOutcome`, not separate outcome and activation facts; rejected commands produce one ordered rejected outcome; dispatch cannot cross an accepted reserved position before the outcome/change/reservation is visible and reconstructable; retry/timeout causes no second decision |
| **EC-E04 — Effective-position and control-barrier fixture (`DM-E04`)** | 03 | Inputs and selection attempts immediately before, at, and after configuration, pause/resume, kill-switch, and reset boundaries; accepted/rejected outcomes; crash/recovery before visibility and application | The accepted `ControlOutcome` affects exactly its reserved first position; no selection crosses under the prior epoch; rejected outcome has no effect; faithful replay restores the same outcome, position, epoch, and barrier |
| **EC-E05 — Stream/epoch/sequence conformance** | 03 foundation; extend 04/08 | Origin, normal progression, duplicate, gap, reconnect, wrap/reset, new epoch, checkpoint, and invalid cursor fixtures | No bare/cross-epoch sequence comparison; no unproven continuity; invalid or duplicate input has no undeclared effect |
| **EC-E06 — Deterministic merge, atomic selection, and cursor-vector proof** | 03 | Multi-stream candidates, tie cases, required/optional streams, controls, stalls, batches, generated scheduling permutations, atomic selection records with pre/post cursors and initial publication state, publication acknowledgements, and crashes at every publication state | Every permutation produces identical run-input order, selected logical input identity, complete pre/post cursor vectors, active epochs, and semantic checksum; opaque selection IDs need not match across independent runs unless the registered policy makes them deterministic; recovery within one run re-publishes the same selection ID idempotently and never allocates a new sequence; omitted stream, unstable tie-break, or unreconstructable publication state fails |
| **EC-E07 — Batch framing suite** | 03 | Atomic and per-member modes, mixed streams, compression, rebatching, corruption, partial failure, retry, and size/depth limits | Every member reconstructs its logical envelope; batch order creates no undeclared cross-stream order; corruption/limits fail boundedly and visibly |
| **EC-E08 — Correction/supersession/tombstone graph suite** | 03 foundation; extend by domain phase | Original/correction chains, concurrent superseders, expiry, reversal, tombstone, compaction, and replay-class behavior | History remains append-only; graph is acyclic and owner-valid; faithful history is preserved; economic history cannot be deleted by tombstone |
| **EC-E09 — Deduplication and idempotency suite** | 03 foundation; extend 04/07/08/11B | Duplicate deliveries, distinct capture duplicates, fingerprint collision, command retry/conflict, crash/rebuild of dedup state, economic identity cases | No duplicate state/economic effect; distinct legitimate facts survive; lost cache cannot bypass idempotency; conflicts fail explicitly |
| **EC-E10 — Unknown and compatibility matrix (`DM-E06`)** | 03 | Unknown envelope/type/schema/field/enum cases across every stable namespace and consumer capability, plus migration/re-normalization fixtures | Unsafe unknown semantics fail closed; safe skips require pinned registry proof; commands cannot be routed heuristically; re-normalization creates new lineage |
| **EC-E11 — Run lifecycle/reset/recovery model** | 03 | Generated valid/invalid transitions, terminal states, reset child-run lineage, same-run recovery proof, incomplete/non-faithful outcomes | Unsupported transitions reject; reset never mutates/deletes history; same-run recovery occurs only with proven cursor/control/config continuity |
| **EC-E12 — Exactly-one SourceEvent lineage suite** | 03 contract; extend 04 | Zero/one/many normalization outputs from one source, malformed/unsupported source, explicit synthetic/admin origin, normalized source correction, multi-source aggregation attempt, derived observation alternative, reference version, source extensions, re-normalization | Every non-synthetic/non-administrative normalized fact, including a normalized correction, has exactly one `source_event_id`; multi-source aggregation cannot validate as normalized; source truth remains immutable; historical facts are not overwritten |
| **EC-E13 — Validation and security suite** | 03 | Layered validation, producer/owner authorization, registry integrity/rollback, limits, adversarial framing, secret canaries, quarantine access | Forged/cross-scope/oversized/secret-bearing inputs fail before authority effect; validation remains bounded; errors do not leak payload/secrets |
| **EC-E14 — Event-infrastructure telemetry, synthetic fence, and performance campaign** | 03 | Exact Phase 02 points/segments; all nine `command_received` through `operator_acknowledgement.published.command_caller_read_model` points; real admission/outcome/barrier/selection path with synthetic downstream execution boundaries; queue/batch/stream metrics; normal/burst/overload/adversarial/crash/recovery workloads; accepted empirical segment budgets and statistical artifacts | No point or endpoint is omitted/redefined; observability does not create fence facts; wrong/missing acknowledgements yield `unknown`; every queue passes `PS-E07`; Phase 02 budgets are adopted or tightened from evidence; the campaign is not deferred to Phase 07 |
| **EC-E15 — Fact lifecycle, record-time, and selection crash matrix** | 03 with persistence companion | Crash/fault injection at computed, accepted, published, recoverability-accepted, and recoverable boundaries; `recoverability_handoff_time` versus `record_time`; control barrier; selection allocation; every publication state | `record_time` appears only with proven recoverability; handoff is never called durable recording; recovery reports exact state, restores the original `ControlOutcome`/reservation/selection/sequence, and re-publishes idempotently; no accepted control effect is silently lost |
| **EC-E16 — Contract registry evolution drill** | 03 | Additive, forward-preservable, transformable, re-normalization-required, incompatible, deprecated, and rollback cases with capability negotiation | Compatibility follows registered semantics, not decoder success; pinned runs do not change interpretation; migration/rebuild provenance is complete |
| **EC-E17 — Independent critique and cumulative compatibility review** | Every phase gate | Critique disposition and comparison against all approved Phase 01–02 contracts plus affected later leaves | Zero unresolved material findings; no authority, lifecycle, ordering, clock, telemetry, mode, or recoverability contract is weakened |

### Phase 03 event-contract closure

The event-contract portion of Phase 03 is implementation-complete only when:

- the registry, logical envelopes, stable taxonomy, and canonical fixtures exist independently of any chosen serializer or storage product;
- command admission, exactly-one ordered accepted/rejected `ControlOutcome`, idempotency, unknown-to-caller behavior, and the atomic control-admission/dispatch barrier are machine tested;
- accepted outcomes carry the behavior change/effective-position reservation without a duplicate activation fact, rejected outcomes remain ordered with no effect, and dispatch cannot cross a reserved boundary under the prior epoch;
- deterministic multi-stream merge, atomic durably reconstructable selection records, complete pre/post cursor vectors, publication states, effective positions, and same-selection idempotent re-publication pass generated scheduling, batching, and crash permutations;
- every non-synthetic/non-administrative normalized event, including normalized corrections, has exactly one `SourceEvent`; aggregations are derived observations; source evidence remains immutable and re-normalization creates new lineage;
- corrections, supersessions, expirations, reversals, and tombstones preserve append-only history;
- unknown state-, safety-, execution-, accounting-, correction-, or run-input-affecting semantics fail closed;
- every implemented queue and batch path is bounded, observable, and performance-tested under Phase 02 methods;
- serialization round trips preserve canonical semantics across every implemented runtime;
- crash tests distinguish acceptance, publication, recoverability acceptance, and recoverability; `record_time` proves durable/recoverable recording while `recoverability_handoff_time` does not;
- all Phase 02 synthetic kill-switch boundary, aggregate fence, observability publication, and operator acknowledgement points are retained and the complete campaign is rerun with Phase 03's real control/selection path and adopted or tightened empirical budgets;
- Phase 01 `DM-E04`, `DM-E06`, applicable `DM-E12`, Phase 02 latency/telemetry/performance obligations, and `EC-E01` through `EC-E17` pass;
- independent critique and cumulative Phase 01–02 review have no unresolved material finding.

This leaf does not fail because later market, strategy, risk, execution, accounting, external-discovery, or live-execution leaf payloads are not implemented. It fails if their namespaces or safety requirements cannot be added without redefining the envelope, command/event distinction, ordering, correction, compatibility, or authority semantics.

## Deferred choices

The following remain for the companion persistence leaf, later domain phases, or evidence-driven architecture decisions:

1. Wire, in-memory, journal, and dataset serialization products/formats.
2. Schema-definition language, code generator, registry implementation, and artifact distribution mechanism.
3. ID generation algorithms and textual encodings.
4. Exact checksum, canonicalization, compression, and signing algorithms.
5. Journal segmentation, indexes, flush/commit protocol, retention, compaction, and replication.
6. Numeric batch, message, queue, lateness, buffering, timeout, rate, and recovery limits.
7. Exact initial merge policy and tie-break values for the selected venue/input set.
8. Venue-specific source sequence, snapshot, reconnect, heartbeat, and gap contracts.
9. Concrete normalized market-data, feature, strategy, risk, execution, fill, ledger, and reconciliation leaf payloads.
10. Cross-language runtime boundary and IPC, if evidence justifies more than one process/runtime.
11. Cryptographic authentication/signature requirements for purely local boundaries versus future remote boundaries.
12. Regulatory or contractual retention requirements for live capital.

Deferral does not permit a local component to invent incompatible behavior. Until a choice is made, it remains behind the logical contracts, registries, manifests, and conformance suites defined here.

## Cumulative review checklist

At this and every later phase gate:

1. Confirm every new command/event type has one Phase 01 authority and one registry entry.
2. Confirm source, normalized, reference, control, execution, accounting, audit, and telemetry facts remain distinct.
3. Confirm no representation-specific field is the sole carrier of identity, order, causation, mode, actor, version, or effective position.
4. Confirm all sequences retain stream and epoch scope and no new global-order assumption appears.
5. Confirm every non-synthetic/non-administrative normalized fact has exactly one `SourceEvent`, normalized corrections follow the same rule, and aggregation remains a derived observation.
6. Confirm run-input eligibility is explicit, every sequence allocation has one atomic reconstructable selection record with pre/post cursors and publication state, and the complete cursor vector includes every declared stream.
7. Confirm behavior-changing commands emit one ordered accepted/rejected `ControlOutcome`, accepted outcomes carry the change and reserved position, rejected outcomes have no effect, and no duplicate activation fact exists.
8. Confirm the control-admission/dispatch barrier prevents selection under a stale epoch and replay/recovery preserves the original outcome, effective position, selection identity, and sequence.
9. Confirm retries, duplicates, replay, corrections, and re-normalization remain different concepts.
10. Confirm unknown state/safety/economic semantics fail closed.
11. Confirm append-only history and authority-owned correction/reconciliation remain intact.
12. Confirm telemetry uses Phase 02 envelopes, all synthetic kill-switch/fence/operator points, clock rules, profile epochs, and cardinality policy without becoming domain authority.
13. Confirm every new queue/batch path is bounded and covered by Phase 02 performance evidence, including the rerun synthetic command-to-fence campaign.
14. Confirm recovery distinguishes accepted, published, recoverability-accepted, and recoverable states; `record_time` means proven recoverable recording and handoff uses a different name.
15. Confirm later schema activation does not emit misleading placeholder facts or zero metrics before capability exists.
16. Re-run affected prior fixtures, performance baselines, compatibility checks, and independent critique.

Approval of this document fixes the event semantic baseline. Later phases may add leaf types, stricter validation, new consumers, domain-specific idempotency, and implementation products, but they may not redefine commands as proof, create hidden ordering, mutate accepted history, infer unknown semantics, or weaken the authority and recovery contracts established in Phases 01–02.
