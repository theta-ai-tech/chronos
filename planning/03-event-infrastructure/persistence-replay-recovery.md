# Persistence, Replay, and Recovery

## Status and authority

This document defines the Phase 03 persistence, replay, and recovery contract for Chronos.

It builds on:

- the approved Phase 01 domain model and architecture;
- the approved Phase 02 telemetry and performance method;
- the Phase 03 event contracts, including the command/event distinction, exactly-one `ControlOutcome`, atomic control-admission/dispatch barrier, deterministic merge, `RunInputSelectionRecord`, append-only publication-state facts, replay classes, and run lifecycle.

Those documents remain authoritative for domain meaning, ownership, ordering, clocks, telemetry semantics, and modes. This document makes persistence and restart behavior concrete without selecting a database, journal product, object store, serializer, replication system, or deployment topology.

If a persistence implementation cannot preserve the identities, order, authority, lifecycle, and recovery classifications defined here, the implementation is incompatible even if it can store and retrieve bytes.

## Objectives

Phase 03 persistence must:

1. Preserve authoritative facts without turning storage into a second domain authority.
2. Distinguish `Computed`, `Accepted`, `Published`, `Recoverability-accepted`, and `Recoverable` states at every relevant boundary.
3. Make accepted controls, effective-position reservations, run-input selections, cursor transitions, and publication states reconstructable without re-deciding them.
4. Preserve immutable source evidence and enough normalized/run-input history to support the declared replay classes.
5. Recover deterministically from tested crashes, partial writes, lost acknowledgements, corruption, storage pressure, and process restart.
6. Prove when same-run recovery is valid and require a child run, incomplete status, non-faithful status, or failure otherwise.
7. Support snapshots and checkpoints as accelerators without making them an unverified source of truth.
8. Define RPO and RTO derivation methods from semantic risk and measured capability rather than inventing numbers during planning.
9. Keep optional telemetry, projections, and caches off authoritative durability fences.
10. Remain local-first while preserving migration paths through ports, manifests, checksums, and conformance tests.

## Non-goals

This phase does not:

- choose a storage product, filesystem layout, record format, compression algorithm, checksum algorithm, encryption product, or replication service;
- implement market-book, strategy, portfolio, risk, paper-broker, accounting, or live-execution persistence;
- create live orders, venue submissions, acknowledgements, fills, reconciliation facts, reservations, or human execution approvals;
- claim exactly-once transport;
- make every accepted fact synchronously durable;
- define fixed retention periods, byte capacities, flush intervals, checkpoint cadence, RPOs, or RTOs before workloads and evidence exist;
- allow snapshots, indexes, audit records, telemetry, or query models to replace authoritative records;
- allow corrected research replay to claim faithful-capture identity.

Later phases may add stricter durability fences and new record classes. They may not weaken the lifecycle, append-only history, deterministic reconstruction, or failure-classification rules established here.

## Persistence authority and ownership

Persistence adapters preserve records on behalf of their owning domain authority. They do not acquire semantic ownership by storing them.

| Concern | Authoritative owner | Persistence responsibility | Persistence must not do |
|---|---|---|---|
| Source capture | Source adapter/capture authority | Preserve source bytes/values, framing, capture identity, integrity, and session continuity | Normalize meaning or infer market truth |
| Normalized facts | Normalization authority | Preserve accepted normalized facts and source/reference lineage | Re-normalize during ordinary reads or mutate prior facts |
| Stream epochs and cursors | Stream/run-input authority | Preserve accepted epoch transitions, cursor evidence, and merge-policy references | Invent continuity or advance cursors |
| Control outcomes | Run/configuration authority | Preserve exactly one accepted/rejected `ControlOutcome`, command idempotency decision, and accepted change | Re-decide a command |
| Effective-position barrier | Run/configuration and stream/run-input authorities at their declared atomic boundary | Preserve the accepted outcome, reservation, epoch, and barrier visibility as one non-splittable recovery unit | Expose a crossed boundary without the accepted control state |
| Run-input selections | Stream/run-input authority | Preserve each atomic selection allocation, pre/post cursor vector, and publication-state history | Rerun merge selection for an allocated position |
| Run manifests and lifecycle | Run/configuration authority | Preserve immutable initialization manifests, lifecycle facts, parent/reset/recovery lineage, and terminal attestations | Mutate a manifest or revive a terminal run |
| Dataset/replay manifests | Dataset/replay authority | Preserve dataset identity, partitions, integrity, replay class, ordering/correction policy, and lineage | Define a competing merge policy |
| Snapshots/checkpoints | Authority whose state is snapshotted | Preserve accelerators plus consistency-cut and validation evidence | Treat an unverified snapshot as authoritative |
| Registry snapshots | Contract registry authority | Preserve immutable registry/type/schema sets pinned by runs and datasets | Resolve a pinned run through a mutable `latest` alias |
| Audit/provenance | Audit/provenance authority | Preserve actor, cause, security, and evidence references not already complete in domain facts | Become a shadow source of domain truth |
| Telemetry/evidence | Observability authority | Preserve operational measurements and phase-gate artifacts under its own loss policy | Prove a domain transition from telemetry alone |
| Quarantine | Owning ingest/validation authority | Isolate uninterpretable, corrupt, hostile, or unauthorized material with bounded metadata | Admit quarantined content into authoritative processing |

Recovery reads return through the owning authority. Consumers do not read another authority's journal or snapshot store as an integration API.

## Authoritative record classes

### Phase 03 active classes

The following record classes are active in Phase 03:

| Record class | Minimum authoritative contents | Ordering scope | Correction model |
|---|---|---|---|
| `SourceCaptureRecord` | Source payload or canonical opaque value, capture session, source identity, framing, capture sequence, receive clock metadata, integrity metadata | Capture session/source stream | New linked capture/correction evidence; original remains immutable |
| `NormalizationOutcomeRecord` | Accepted normalized fact or typed rejection/quarantine, one source-event lineage except explicit synthetic/administrative origin, normalizer/reference versions | Normalized stream epoch/sequence | New normalized correction or new re-normalized dataset lineage |
| `StreamEpochRecord` | Stream identity, old/new epoch, transition reason, continuity classification, base/snapshot cursor, registry/policy identity | Stream | New epoch transition; never rewrite prior epoch |
| `RunTimerRecord` | Run ID, timer stream/epoch/sequence, pinned timer-policy version, logical-clock position, local monotonic deadline evidence, scope, and integrity metadata | Run-timer stream | New ordered timer fact or timer-stream epoch transition; prior accepted timers remain immutable |
| `CursorCheckpointRecord` | Stream or complete run-input cursor vector, registry/schema/policy references, consistency cut, checksum | Authority-local checkpoint order | Superseded by a newer validated checkpoint |
| `ControlOutcomeRecord` | Exactly one accepted/rejected decision, command/idempotency identity, actor, reason, control-stream position; accepted outcome includes complete change, new epoch, and reserved effective position | Run-control stream | New superseding control outcome; prior outcome/effect remains history |
| `ControlBarrierRecord` | Outcome identity, effective-position reservation, prior/new epoch, dispatch frontier condition, visibility/commit-group evidence | Run and barrier generation | Append-only release/cross/failure facts; reservation is not rewritten |
| `RunInputSelectionRecord` | Selection identity, run input sequence, selected input, complete pre/post cursor vectors, active control epoch, merge/registry identity, tie-break evidence, initial `not_published` state | Run | Immutable allocation; append publication-state facts |
| `RunInputPublicationRecord` | Selection identity/sequence, attempt identity, consumer boundary, attempt/result/acknowledgement, resulting reconstructed publication state | Selection | Append-only transition; never mutate initial selection |
| `RunLifecycleRecord` | Valid lifecycle transition, manifest identity, reason, last proven position, recovery/reset lineage where applicable | Run lifecycle stream | New transition only |
| `RunInitializationManifest` | Immutable Phase 01 manifest contents and content identity | One per run | No mutation; replacement requires a new run |
| `RunTerminalAttestation` | Terminal status, final effective position and cursor lineage, final checksums, reason, initialization-manifest identity | One terminal attestation per terminal run outcome policy | No mutation; corrections are linked attestations without reviving the run |
| `CaptureManifest` | Capture session, partitions/segments, source identities, continuity status, integrity roots, time/sequence bounds | Capture/dataset | New manifest version referencing unchanged immutable parts |
| `DatasetManifest` | Dataset identity, replay class, partition order, source/normalized lineage, registry and policy versions, corrections, integrity roots | Dataset | New immutable manifest/version |
| `ReplayRunManifest` | Parent dataset, replay class, merge policy, initial cursors/snapshot, component versions, pacing and deterministic inputs | Replay run | New run/manifest |
| `SnapshotRecord` | Authority state image or content reference, exact consistency cut, schema/registry/component versions, checksum and validation metadata | Authority snapshot generation | New snapshot; no in-place update |
| `RecoveryRecord` | Recovery attempt identity, failure class, inspected evidence, selected checkpoint, tail bounds, continuity result, disposition, child-run relation | Recovery attempt | New attempt/result records |
| `RegistrySnapshotRecord` | Immutable set of envelope/type/schema registrations and compatibility rules | Registry version | New snapshot/version |
| `QuarantineRecord` | Original record reference or bounded opaque content, reason, parser/validator version, integrity, access class, disposition | Quarantine scope | Append-only disposition; never silent re-admission |
| `AuditEvidenceRecord` | Actor/cause references, security-sensitive action, evidence-manifest references, integrity metadata | Audit stream | Linked correction only |

### Reserved extension classes

The following names reserve persistence boundaries for later phases but are not Phase 03 active facts:

- market-state snapshots and derived feature/strategy/recommendation rebuild artifacts;
- portfolio targets, risk decisions, reservation outcomes, reservations, and exposure checkpoints;
- paper intents, paper orders, simulated acknowledgements/fills, and accounting records;
- human execution approvals;
- live executable intents, external submissions, acknowledgements, fills, ambiguous outcomes, and reconciliation records;
- external-observation and opportunity datasets.

Phase 03 may define namespaces, ports, registry placeholders, and failure-test seams for those classes. It must not emit plausible instances, create venue credentials, or claim their durability requirements are satisfied. Live-execution records require their later phase's write-ahead, authorization, idempotency, ambiguity, reconciliation, and economic-loss policies.

## Fact lifecycle and persistence transitions

The canonical lifecycle is:

```text
Computed
  -> Accepted
  -> Published
  -> Recoverability-accepted
  -> Recoverable
```

The order shown is conceptual, not a mandatory wall-clock order between publication and persistence. A policy may require recoverability before publication, may perform acceptance and recoverability atomically, or may publish before an asynchronous recoverability handoff. The named meanings never collapse.

### Phase 03 baseline crash class

For accepted controls and run-input selections, Phase 03 defines a mandatory baseline crash class:

- loss of process memory, threads, queues, and in-flight IPC;
- abrupt process termination;
- process or operating-system restart against the same configured persistence generation;
- lost append/commit acknowledgements;
- interrupted, partial, or torn record/group writes that the persistence integrity contract can detect and exclude from the valid prefix.

Acceptance of a `ControlOutcome`, effective-position reservation, command-idempotency outcome, control barrier, or `RunInputSelectionRecord` is not complete until the same atomic transition establishes a validated recovery source that survives this baseline crash class. The recovery source may be a committed journal group or an equivalently proven mechanism, but it cannot be process memory, an asynchronous write request, an enqueue acknowledgement, optional telemetry, or a consumer-side observation.

For the declared baseline crash class, successful atomic acceptance therefore reaches `Recoverable` at the same logical boundary as `Accepted`; `recoverability_accepted` is also satisfied by that atomic operation. Telemetry emits all three named lifecycle points with the declared atomic relationship. Stronger host, device, backup, or disaster classes do not redefine these states: they are separate recovery guarantees and RPO evidence attached to the same fact class.

For these fenced classes, authority code may compute and prepare a candidate before the recovery source exists, but it must not report the fact as `Accepted`, advance the authoritative control/selection cursor, dispatch the selection, apply the selected input to state, or expose the transition as effective until the atomic acceptance/recovery-source operation succeeds.

The Phase 03 baseline does not by itself claim survival of:

- complete primary-device loss;
- destruction or permanent loss of the host;
- rollback or corruption outside the configured storage guarantee;
- simultaneous loss of every configured backup/export copy;
- a disaster class that requires replication or backup topology not yet selected.

Those stronger failure classes require an explicit storage/backup policy, independently measured recovery evidence, and a record-class RPO. If a failure exceeds the proven guarantee, Chronos does not silently resume the original run as faithful. It restores from independently verified evidence when possible and marks the affected run or interval `incomplete`, `non-faithful`, or `failed`; any continued processing uses an explicit child run when the same-run conditions no longer hold.

### Transition definitions

| Transition | Required proof | What it does not prove |
|---|---|---|
| `Computed -> Accepted` | Owning authority validated and serialized the transition, assigned identity/order, and committed to one immutable fact; controls and selections additionally established their atomic Phase 03 baseline recovery source | Publication or survival beyond the record class's declared failure model |
| `Accepted -> Published` | The fact crossed the declared consumer boundary and is available to that consumer | Consumer application, recoverability, or durable recording |
| `Accepted -> Recoverability-accepted` | The declared persistence/reconstruction boundary acknowledged responsibility under the record-class loss policy | The record already survives every tested crash |
| `Recoverability-accepted -> Recoverable` | The record or its deterministic reconstruction inputs satisfy the declared failure model, integrity checks, and retrieval/rebuild proof | Indefinite retention or survival outside the declared failure model |
| `Published -> consumer accepted` | The named consumer accepted the original identity/position idempotently | Persistence unless the consumer acknowledgement is itself within an atomic recoverability protocol |

`recoverability_handoff_time` records a named handoff attempt or acknowledgement. `record_time` is populated only when the fact is proven recoverable under its declared failure model. Neither time participates in domain identity or deterministic semantic checksums.

### Lifecycle rules

1. A `Computed` value may disappear after a crash without correction.
2. An `Accepted` fact may never be reconstructed as a different fact.
3. If an accepted fact is within a permitted loss window and is lost, recovery records the exact loss/fidelity consequence; it does not pretend the fact never existed.
4. A published fact that is not recoverable is not called durable.
5. A rebuildable projection is recoverable only when its complete authoritative inputs, versions, and deterministic rebuild procedure are recoverable.
6. `record_time` may be stored in an enclosing commit/batch record and associated reproducibly with each member.
7. Recovery must distinguish “not accepted,” “accepted but outcome unknown to caller,” “published,” “recoverability accepted,” and “recoverable.”
8. Mandatory safety fences are explicit persistence protocols, not logging or telemetry calls.
9. For controls and selections, `Accepted` includes survival of the Phase 03 baseline crash class. Later `Recoverability-accepted` and `Recoverable` evidence may cover a broader host/device/backup failure model, longer retention, or an independently verified copy; they may not weaken the baseline acceptance guarantee.

## Durability policy classes

Every record class is assigned a policy class in its phase-owned durability matrix.

| Policy class | Semantic rule | Phase 03 examples |
|---|---|---|
| **Fenced authoritative** | Acceptance atomically establishes a recovery source surviving the Phase 03 baseline crash class before any dispatch or state effect; stronger failure classes may add later recoverability fences | Accepted `ControlOutcome` plus effective-position/barrier reservation; `RunInputSelectionRecord` before dispatch/state application |
| **Bounded-loss authoritative** | A declared, measured loss window may exist; any loss beyond or ambiguity within it changes fidelity/readiness and is surfaced | Source capture segments where the venue/source cannot be replayed; selected normalized journals if their source evidence remains complete |
| **Reconstructable authoritative** | Direct materialization may be lost only when retained inputs and pinned deterministic versions reproduce identity/order/semantics | Normalized datasets from raw capture where the replay class explicitly permits re-normalization; cursor summaries from selection history |
| **Accelerator** | May be discarded and rebuilt; never required alone to establish truth | Snapshots, indexes, caches, query projections |
| **Optional evidence** | Loss follows the Phase 02 telemetry/evidence policy and cannot remove the authoritative causal chain | Optional traces, sampled logs, disposable dashboards |
| **Quarantine** | Preserved or disposed under security/retention policy but cannot affect domain state | Malformed, unsupported, corrupt, or hostile records |

Policy assignment is per record class and mode, not per storage product. Sharing one file or transaction mechanism does not make all contained records equivalent.

## Phase 03 durability matrix

Numeric windows and targets are evidence-derived later. The semantic bounds below are mandatory now.

| Authoritative record | Owner | Required recoverability boundary | Permitted loss policy | Reconstruction source | Failure disposition |
|---|---|---|---|---|---|
| Source capture | Capture | Before the capture buffer/segment is acknowledged as protected under the selected capture policy | Bounded only by an explicit capture RPO; unprovable continuity is never faithful | Retained source segments, upstream replay/resume if source contract proves equivalence | Stream/run `gapped`, `incomplete`, or `non-faithful`; capture may continue under a new epoch |
| Normalized fact | Normalization | Directly journaled or reconstructable from retained source plus pinned normalizer/reference/registry | May lose materialization only if exact class permits deterministic reconstruction | Source capture plus versions/policies, or normalized journal | Quarantine, re-normalization into new lineage, or run incomplete; never fabricate original normalized identity unless ID policy proves it |
| Stream epoch transition | Stream authority | Recoverable before inputs in the new epoch are accepted/published | No silent loss once new-epoch inputs exist | Epoch journal plus source/capture continuity evidence | Stop affected stream/run; do not compare or merge across unknown epoch |
| Run-timer fact | Run/configuration | Recoverable before a selection may apply the timer or any freshness transition caused by it | No silent loss for a timer referenced by accepted selection/state lineage | Timer journal plus pinned timer policy and timer-stream epoch history | Stop deterministic freshness progression; same-run recovery only from exact timer history, otherwise child run/incomplete |
| Accepted/rejected `ControlOutcome` | Run/configuration | Acceptance atomically establishes the control journal/commit-group recovery source for the Phase 03 baseline crash class; accepted outcomes cannot permit dispatch across their reservation before this succeeds | No silent loss after control-stream acceptance within the baseline class; stronger host/device loss follows the separately proven RPO | Control journal plus command idempotency record and barrier commit group | Dispatch stopped; outcome queried/reconstructed, never re-decided; beyond the proven stronger guarantee mark incomplete/non-faithful/failed |
| Effective-position/barrier reservation | Joint declared barrier protocol | Same atomic acceptance/recovery unit as outcome/change/new epoch before any dispatch crossing | No split loss within the baseline class; stronger host/device guarantees are explicit | Barrier commit group and control journal | Dispatch remains fenced; child run/failure if exact reservation cannot be proven |
| `RunInputSelectionRecord` | Stream/run-input | Acceptance atomically establishes the selection recovery source for the Phase 03 baseline crash class before dispatch to the state consumer or any state effect | No silent loss after sequence allocation within the baseline class; stronger host/device loss follows the separately proven RPO | Selection journal/commit group | Stop; reconstruct original selection or classify run incomplete/non-faithful/failed |
| Publication-state facts | Stream/run-input | Recoverability sufficient to reconstruct whether the original selection needs idempotent re-publication | Lost acknowledgement may cause re-publication, never duplicate application | Publication journal plus consumer dedup/acceptance evidence | Re-publish original identity; terminal failure only when consumer state cannot be proven |
| Complete cursor vector/checkpoint | Stream/run-input | Checkpoints are accelerators; authoritative cursor state reconstructs from selections and controls | Checkpoint may be lost | Selection, control, epoch, and publication histories | Rebuild from earlier trusted cut; stop if history is incomplete |
| Run initialization manifest | Run/configuration | Recoverable before run becomes `Initialized` | No loss for a run that has accepted any lifecycle/input fact | Manifest store plus immutable content identity and registry references | Run cannot start/recover; child run only with a new manifest and explicit lineage |
| Run lifecycle fact | Run/configuration | Recoverable according to transition criticality; terminal facts and reset lineage must be recoverable before presenting terminal/new-run state as authoritative | No silent loss of terminal/reset/recovery relationship | Lifecycle journal and referenced manifests | State `unknown`/`recovering`; no terminal revival or orphan child |
| Terminal attestation | Run/configuration | Recoverable before run is reported durably `Completed`, `Failed`, or `Aborted` | No silent loss after authoritative terminal publication | Final records/checksums plus lifecycle history | Terminal status remains unproven until rebuilt or explicitly marked incomplete; an `Aborted` run is never revived |
| Capture/dataset/replay manifest | Dataset/replay | Recoverable before dataset/run is admitted for replay | No replay from an unresolvable manifest | Immutable manifest copies and referenced content roots | Dataset/run rejected or quarantined |
| Snapshot | Snapshotted authority | Accelerator; recoverable only if both snapshot and consistency-cut proof validate | Snapshot may be discarded | Earlier snapshot plus complete tail, or origin replay | Fallback to earlier cut/origin; failure if tail is unavailable |
| Registry snapshot | Registry authority | Recoverable before any pinned record/run/dataset is interpreted | No substitution by mutable alias | Immutable registry artifact and trusted distribution copies | Run/dataset incompatible and unready |
| Command idempotency outcome | Command owner | Same recovery unit as terminal `ControlOutcome` for admitted behavior-changing commands | No expiry while repeated effect remains possible | Control outcome and canonical request-equivalence digest | Return original outcome or remain unknown/fenced; never accept a fresh duplicate |
| Quarantine disposition | Ingest/validation owner | According to security and diagnostic policy; authoritative processing must not depend on it becoming accepted | Bounded retention may apply | Original protected source where available | No domain effect; report evidence limitation |
| Mandatory audit evidence | Audit/provenance | Recoverable before the governed action is represented as fully auditable when policy requires it | No loss of required actor/cause chain | Authoritative domain facts plus audit journal | Action/run marked unauditable or unready according to policy |
| Optional telemetry/projections | Observability/query owner | Phase 02 policy | Loss permitted only within declared telemetry policy | Authoritative facts and new observations | Explicit telemetry/projection gap; no domain-state change |

## Journal and append-only semantics

### Logical journal contract

A logical journal is an ordered, append-only record history owned by one authority or one explicitly defined atomic protocol. It may be implemented by files, pages, tables, logs, or another mechanism, but it must provide:

- immutable logical records;
- stable record identity;
- authority and partition identity;
- explicit ordering or explicit absence of ordering;
- schema/registry identity;
- commit-group identity where atomicity spans records;
- integrity metadata sufficient to detect truncation, substitution, reordering, and partial groups;
- recovery scanning with a declared valid-prefix rule;
- idempotent append behavior for retried record identity;
- explicit conflict behavior for the same identity with non-equivalent content;
- segment/generation boundaries and predecessor relations;
- evidence distinguishing append attempt, append acceptance, recoverability acceptance, and recoverability proof.

An append-only journal does not prohibit correction. Correction, supersession, reversal, expiry, tombstone, publication transition, and migration are new linked records.

### Valid-prefix rule

After a crash, a journal reader determines the maximal valid committed prefix for each partition or commit domain. A record or group is part of the prefix only when:

1. framing and length are valid;
2. schema and registry identity are interpretable;
3. integrity checks pass;
4. commit-group completion rules pass;
5. predecessor/order constraints pass;
6. no earlier required record is missing;
7. the record owner and producer are authorized.

Bytes after the maximal valid prefix are not guessed into records. They are truncated only through a governed repair operation or preserved in quarantine evidence before repair.

### Atomic groups

An atomic group is used only where the semantic contract requires non-splittable recovery. It declares:

- group identity and owner/protocol;
- complete member set or deterministic member-count proof;
- ordering of members where meaningful;
- commit marker or equivalent atomic visibility proof;
- integrity root over members;
- idempotency/conflict rules;
- recovery behavior for absent, partial, duplicated, or contradictory members.

The following Phase 03 transitions require an atomic group or an equivalently proven atomic recoverable protocol:

- accepted `ControlOutcome`, resulting control/configuration epoch, effective-position reservation, command idempotency outcome, and dispatch-barrier state;
- `RunInputSelectionRecord`, sequence allocation, and complete pre/post cursor transition;
- publication with selection recoverability when the chosen protocol permits both to become externally effective atomically.

Atomic groups do not create cross-authority semantic ownership. Their protocol explicitly names each member's owner.

### No journal-as-database shortcut

Indexes and materialized views may answer queries, but consumers do not:

- mutate another authority's records;
- infer missing business state from journal offsets alone;
- treat physical location as domain identity;
- depend on undocumented partition order;
- bypass the owning authority's validation and recovery path.

## Flush and group-commit decision framework

The implementation selects flush, batching, and group-commit policy per record class using evidence. The decision record must consider:

1. semantic consequence of losing an accepted record;
2. whether downstream publication or a safety boundary must be fenced;
3. whether exact reconstruction exists and is independently testable;
4. maximum unacknowledged working set in records, bytes, and logical time;
5. crash classes covered: process, abrupt host loss, storage write loss, partial/torn write, filesystem/device error, and configured backup loss;
6. expected arrival rate, burst shape, and batch opportunity;
7. tail-latency and throughput cost;
8. storage write amplification and endurance;
9. memory pressure and maximum pending recoverability queue;
10. shutdown/drain behavior;
11. operator-visible fidelity state while records are pending;
12. how a policy change is versioned and represented in run/process evidence.

Candidate policies may include per-record fencing, bounded group commit, asynchronous handoff with a declared loss window, reconstruction-only materialization, or accelerator-only persistence. Product-specific names are deferred.

A policy is accepted only when:

- its semantic loss behavior matches the durability matrix;
- its pending queue is bounded;
- crash injection proves the claimed valid-prefix and reconstruction behavior;
- Phase 02 methodology quantifies latency, throughput, tail, saturation, recovery, and instrumentation overhead;
- the measured operating envelope supports an explicit RPO/RTO proposal;
- overload never silently widens the loss window or crosses a safety fence.

## Retention and record dependencies

Retention is dependency-based, not merely age-based.

### Source capture

Source capture is retained long enough to satisfy:

- the declared capture RPO and fidelity claims;
- every dataset and replay manifest that references it;
- any re-normalization, incident, audit, or evidence obligation;
- verification of normalized lineage and unsupported/malformed source handling.

Deleting source capture while a retained manifest requires raw re-normalization makes that manifest invalid and is prohibited.

### Control and selection history

For every retained run, retain or losslessly consolidate:

- all admitted command idempotency outcomes that can still prevent repeated effect;
- exactly-one ordered `ControlOutcome` history;
- effective-position and barrier reservations;
- run-input selections and complete pre/post cursor vectors;
- publication-state facts and consumer acceptance evidence needed for recovery;
- run lifecycle, reset, parent/child, recovery, and terminal evidence;
- merge-policy and registry snapshots.

Control or selection history may not be compacted into a snapshot alone unless the retained form still proves every control effect, selected input identity, cursor transition, and publication recovery obligation required by the run's fidelity/audit policy.

### Run and dataset manifests

Initialization manifests, terminal attestations, dataset manifests, replay manifests, registry snapshots, policy identities, and integrity roots are retained at least as long as any referenced result, snapshot, evidence artifact, child run, or audit obligation.

Retention deletion is itself:

- authorized;
- dry-run and dependency-checked;
- recorded in append-only audit evidence;
- blocked when a live reference exists;
- performed without rewriting retained manifests to hide missing dependencies.

If a permitted retention action makes full replay unavailable, affected artifacts explicitly transition to a documented archival capability class; they do not continue to claim replayability.

## Snapshots and checkpoints

### Snapshot role

A snapshot is a verified accelerator for one authority's state. It is never accepted merely because it deserializes.

Each snapshot contains or references:

- snapshot identity and owner;
- run/capture/dataset scope;
- state schema, registry snapshot, component version, and arithmetic/canonicalization policy;
- exact consistency cut;
- complete authority cursor or lineage vector;
- active configuration/control epoch and effective-position state where relevant;
- predecessor snapshot and tail origin;
- semantic checksum of reconstructed state;
- byte/content integrity metadata;
- creation mode: online coordinated, copy-on-write, quiesced, or replay-derived;
- validation status and evidence identity;
- confidentiality/access classification.

### Consistency cuts

A consistency cut states exactly which accepted facts are included.

For stream/run-input state, the cut includes:

- last included `run_input_sequence`;
- complete `RunInputCursorVector`;
- last accepted selection identity;
- reconstructed publication frontier;
- consumed run-control cursor;
- active control/configuration epoch;
- last applied effective position;
- any accepted reserved control boundary not yet crossed;
- merge-policy and registry identities.

The snapshot process must prove one of:

- state and cut were captured atomically;
- processing was quiesced at the cut;
- copy-on-write/versioning retained a consistent image;
- replay from an earlier trusted cut deterministically generated the snapshot.

Mixing state from different cuts is corruption even when each component is individually valid.

### Checkpoints

A checkpoint is a smaller recovery locator or cursor assertion. It may identify a journal position, selection frontier, dataset partition, or snapshot-tail relation. It does not replace the records it points to.

Checkpoints:

- are append-only;
- identify the evidence used to validate them;
- are ignored when their target, checksum, schema, or predecessor relation is invalid;
- may be regenerated;
- cannot advance an authoritative cursor without the corresponding accepted facts.

### Snapshot validation

Before use, recovery verifies:

1. manifest and registry availability;
2. owner and scope;
3. integrity and completeness;
4. compatibility or an approved migration;
5. consistency-cut fields;
6. predecessor/tail continuity;
7. semantic checksum;
8. replay of a validation sample or complete tail as required by policy;
9. absence of a later invalidation/quarantine record.

Failure falls back to an earlier trusted snapshot or replay from origin. If complete tail inputs are unavailable, the run cannot recover faithfully.

## Replay classes

Chronos preserves four distinct replay classes.

### Faithful capture-order replay

Consumes original captured facts in their recorded capture/order policy, including original control outcomes and correction arrival positions.

It:

- uses the original capture and run/control evidence;
- consumes the original accepted run-timer facts and pinned timer policy/history; it never regenerates timers from current host time;
- pins the original normalizer, reference-data version, envelope/schema registry snapshot, canonicalization rules, correction policy, and merge-policy version used by the captured run;
- runs normalization only with those pinned original versions when normalized facts are reconstructed from source capture;
- verifies reconstructed normalized semantic checksums against the immutable expected checksum set recorded by the original capture/run/dataset evidence before downstream replay is called faithful;
- does not insert late facts retroactively;
- reproduces accepted controls at recorded effective positions;
- preserves original gaps and fidelity limitations;
- may reproduce original run-input selections directly when validating a historical run, or rerun the pinned merge policy and compare against them;
- cannot become faithful if required capture/control/selection evidence, original normalizer/reference/schema/merge versions, or expected normalized semantic checksums are missing or fail validation.

Running retained source capture through current code, a replacement reference dataset, a different schema interpretation, a changed correction policy, or another merge version is not faithful capture-order replay even if the resulting market values appear equivalent. It is raw re-normalization replay with new lineage and a new dataset/run identity.

### Normalized-fact replay

Consumes an immutable accepted normalized dataset under its recorded stream, registry, merge, correction, and control policies.

It:

- does not re-run normalization;
- preserves accepted normalized identity and ordering;
- identifies the source-capture lineage when retained;
- requires a manifest that distinguishes it from capture-order replay.

### Raw re-normalization replay

Consumes retained source capture through explicitly selected normalizer, reference, registry, and policy versions.

It:

- creates new normalized lineage and a new dataset/run identity;
- never overwrites original normalized facts;
- records comparison relationships to prior normalization;
- cannot claim to reproduce original normalized identity unless the registered deterministic ID policy and identical versions prove it.

### Corrected event-time research replay

Consumes an immutable research dataset transformed by a declared correction, ordering, lateness, and event-time policy.

It:

- records every correction source and transform;
- may reorder or include information unavailable to the original live run only when the manifest declares it;
- is non-faithful to original live decision order by definition;
- cannot be used as evidence that the original live run made the same decisions;
- remains deterministic for an identical manifest.

## Dataset manifests

Every capture, normalized, re-normalized, or corrected dataset has an immutable manifest containing:

- dataset identity and replay class;
- parent capture/dataset identities;
- owner and creation authority;
- partition/segment list in canonical order;
- content identities, byte sizes, record counts, and integrity roots;
- source, normalized, reference, market-control, run-control, and run-timer stream identities and epoch bounds;
- gaps, quarantines, duplicates, corrections, exclusions, and fidelity classification;
- registry snapshot, schema, canonicalization, checksum, and compression identities;
- normalizer/reference versions where applicable;
- for faithful capture-order reconstruction, the original normalizer, reference-data, envelope/schema registry, canonicalization, correction, and merge versions plus the expected normalized semantic checksum set;
- merge-policy identity/version and initial cursors where run-input reconstruction is supported;
- pinned run-timer policy, logical-clock origin, timer-stream continuity/fidelity, and exact accepted timer partitions for faithful replay;
- correction, lateness, watermark, tie-break, and filtering policies;
- time domains and their uncertainty/meaning;
- snapshot/checkpoint references and exact tail origins;
- security/access classification and redaction/export status;
- creation tool/build/environment identity relevant to reproducibility;
- predecessor/superseding manifest relationships;
- semantic checksum set and expected invariants.

Manifests are content-identified or otherwise immutable. Mutable catalog aliases may point to them but cannot appear as pinned run provenance.

Dataset validation produces a typed result:

- `valid`;
- `valid_with_declared_gaps`;
- `incompatible`;
- `corrupt`;
- `incomplete`;
- `quarantined`;
- `non_faithful_for_requested_class`.

No replay begins while required validation is `unknown`.

## Atomic control-barrier persistence

### Acceptance protocol

For each newly admitted behavior-changing command, one atomic recoverable protocol:

1. validates canonical request equivalence and command idempotency identity;
2. evaluates authorization, preconditions, and conflict ordering;
3. allocates one control-stream sequence;
4. accepts exactly one `ControlOutcome`;
5. if accepted, records the complete change, prior/new epoch, and exact effective-position reservation;
6. records the command's terminal idempotency outcome;
7. installs a barrier preventing selection at or beyond the reserved position under the prior epoch;
8. atomically establishes the outcome/change/reservation/idempotency/barrier recovery source that survives the Phase 03 baseline crash class;
9. only then completes `Accepted`, advances the authoritative control cursor, and makes the recovery unit visible to dispatch;
10. publishes typed accepted/rejected and later effective/fence projections without creating a second activation fact.

The candidate remains computed/in-flight until step 8 succeeds. It is invalid to return an accepted domain outcome, advance the control cursor, or expose an effective reservation while its only recovery source is volatile. Broader `Recoverability-accepted`/`Recoverable` states may later prove survival of stronger host/device/backup failure classes, but dispatch cannot cross on the strength of those future operations. Caller timeout yields `unknown_to_caller`; it never triggers a new command identity or re-decision.

### Barrier recovery

At restart:

1. scan the control/barrier commit domain to its maximal valid prefix;
2. rebuild the command-idempotency map and ordered `ControlOutcome` history;
3. reconstruct every accepted reservation and its prior/new epoch;
4. compare barrier reservations with selection history and dispatch frontier;
5. for a reservation not yet crossed, reinstall the barrier;
6. for a crossed reservation, prove that the first affected selection references the new epoch and that the control became effective exactly at the reserved position;
7. for an accepted outcome whose recovery group is partial or contradictory, stop dispatch and quarantine the group;
8. never re-run policy evaluation, allocate a new control sequence, choose a new effective position, or emit a second activation fact.

If the exact accepted outcome and reservation cannot be reconstructed, same-run faithful recovery fails. A child run may start only after recording the parent as incomplete/failed/non-faithful and selecting a new initialization manifest.

## Run-input selection persistence and recovery

### Selection acceptance protocol

One atomic recoverable transition:

1. validates the current complete cursor vector and barrier state;
2. evaluates the pinned merge policy;
3. chooses one logical input;
4. allocates one `selection_id` and next `run_input_sequence`;
5. records selected input identity and canonical tie-break evidence;
6. records complete pre/post cursor vectors;
7. records active control epoch and effective-position state;
8. records initial `publication_state = not_published`;
9. establishes a committed selection recovery source surviving the Phase 03 baseline crash class;
10. only then completes `Accepted` and advances the authoritative selection/cursor frontier;
11. dispatches the selected input to the state consumer only after acceptance; no state effect may occur before the recovery source exists.

An implementation may combine selection acceptance and consumer publication in one wider atomic protocol only when the selection recovery source is established before or atomically with the consumer's state effect and crash tests prove that no state effect can survive without the original selection record. A volatile allocation followed by state application is prohibited.

### Publication protocol

Publication appends state facts for the original selection:

```text
not_published
  -> publication_in_progress
  -> published_to_consumer_boundary
  -> consumer_accepted
  | publication_failed_retryable -> publication_in_progress
  | publication_failed_terminal
```

`published_to_consumer_boundary` means the fact crossed the declared boundary and is available to the consumer. `consumer_accepted` is a distinct idempotent acknowledgement of acceptance/application. An enqueue, socket write, callback invocation, or send attempt counts as publication only when it is the declared boundary; it never proves consumer acceptance.

### Selection recovery

Recovery reconstructs selections in `run_input_sequence` order:

1. validate sequence origin, monotonicity, and no reuse;
2. validate each selected input and pre/post cursor continuity;
3. validate active epoch against control reservations;
4. fold append-only publication-state facts for each selection;
5. compare the consumer's recoverable dedup/application frontier where available;
6. retry `not_published` and `publication_failed_retryable` selections through a new `publication_in_progress` attempt using the same identity, sequence, input, cursors, epoch, and boundary;
7. reconcile `publication_in_progress` or `published_to_consumer_boundary` against the consumer frontier and re-publish idempotently only when acceptance is unproven;
8. never automatically retry `publication_failed_terminal`; stop for explicit recovery or child-run disposition;
9. require the consumer to deduplicate and acknowledge the original selection;
10. never rerun merge selection for an allocated sequence;
11. begin new merge evaluation only after the publication frontier policy permits it.

If an acknowledgement was lost after consumer application, re-publication resolves the uncertainty. If consumer application state and selection history cannot prove idempotency, recovery stops rather than risking a duplicate state effect.

Pipelined in-flight selections are allowed only after model and crash tests prove equivalent recovery for every prefix, publication permutation, and barrier interaction.

## Recovery model

### Recovery phases

Every authority follows a typed recovery state machine:

```text
uninitialized
  -> inspecting
  -> validating
  -> reconstructing
  -> verifying
  -> ready
  | degraded
  | child_run_required
  | quarantined
  | failed
```

Readiness is authority- and mode-specific. Capture may become ready while replay remains unavailable; query projections may rebuild while run processing remains fenced.

### Generic recovery algorithm

1. Load static configuration, security policy, process evidence, and storage locations.
2. Verify store identity, access mode, ownership, registry compatibility, and expected generations.
3. Discover journal segments, manifests, snapshots, checkpoints, quarantine records, and backup/import provenance.
4. Scan each required journal to its maximal valid prefix.
5. Detect missing, duplicate, overlapping, reordered, partial, or contradictory records/groups.
6. Select the newest trusted snapshot/checkpoint whose consistency cut and dependencies validate.
7. Reconstruct authoritative state from the snapshot plus complete tail, or from origin.
8. Rebuild deduplication and idempotency indexes from authoritative records.
9. Reconstruct control barriers before enabling selection.
10. Reconstruct selections and publication states before publishing new work.
11. Verify semantic checksums, cursor vectors, epochs, manifests, and terminal/lifecycle state.
12. Classify continuity and fidelity against the record-class RPOs.
13. Emit authoritative recovery records and operational telemetry.
14. Enable only the authorities and modes whose readiness predicates pass.

Recovery output includes:

- exact evidence positions used;
- records ignored/quarantined and why;
- declared loss window and observed loss/ambiguity;
- last proven cursor/effective position;
- fidelity classification;
- same-run/child-run/fail disposition;
- measured recovery duration and work performed.

### Authority-specific reconstruction

#### Capture

- restore capture session/segment generation and last valid sequence;
- compare with source resume/snapshot capability;
- open a new epoch when continuity cannot be proven;
- never bridge an unknown gap by sequence arithmetic alone.

#### Normalization

- restore accepted normalized journal or regenerate only under the declared replay class;
- preserve original source lineage and versions;
- quarantine incompatible records rather than best-effort decode;
- create new lineage for re-normalization.

#### Run/configuration

- restore immutable initialization manifest first;
- rebuild lifecycle and exactly-one control outcomes;
- restore the exact run-timer policy, stream epochs, accepted timer history, logical-clock origin, and continuity/fidelity before any timer-dependent state is resumed;
- restore active epoch, pending reservations, effective positions, and command idempotency;
- never resume a terminal run.

#### Stream/run-input

- rebuild stream epochs, cursor vectors, selections, and publication frontiers;
- reconcile control barriers before selecting;
- re-publish original incomplete selections idempotently;
- verify selection semantic checksums against the manifest.

#### Dataset/replay

- validate manifest dependency closure and integrity;
- validate replay class and requested fidelity;
- reconstruct ordered partitions and initial cursors;
- reject incompatible or incomplete datasets before run initialization.

#### Snapshotted later authorities

Later phases define their own authoritative inputs and semantic checksums. They adopt the same consistency-cut, snapshot-plus-tail, and idempotent rebuild rules. Phase 03 does not invent their state.

## Same-run recovery and child-run rules

### Same-run recovery

Same-run recovery is allowed only when all applicable conditions hold:

- the original immutable initialization manifest is available and unchanged;
- registry, merge, component, configuration, and replay-class versions remain compatible;
- accepted source/normalized/control/selection histories are complete within their declared policies;
- accepted run-timer history and timer-stream epochs are complete whenever timer facts can affect authoritative state or freshness;
- exact control outcomes, effective positions, barriers, and active epoch are proven;
- the complete cursor vector is reconstructable;
- every allocated selection and its publication state is reconstructable;
- snapshot plus tail or origin replay reaches the expected semantic checksum;
- no unknown external/economic effect exists;
- the observed loss is within the run's declared RPO and does not invalidate its fidelity claim;
- no authority-specific contract requires a child run.

The recovered process appends recovery lifecycle facts. It does not mutate the initialization manifest or erase the interruption.

### Child run

A child run is required when:

- recovery changes the initialization manifest, dataset, replay class, correction policy, registry, merge policy, component semantics, or initial state;
- accepted control/barrier/selection identity or ordering cannot be reconstructed;
- continuity is beyond policy;
- a corrected or re-normalized dataset replaces the original;
- a reset is accepted;
- an operator elects a new start after a failed/non-faithful parent;
- later execution/accounting phases identify ambiguity that cannot safely resume in place.

The child run:

- has a new `run_id` and immutable initialization manifest;
- references the parent and exact recovery/reset cause;
- records the parent's last proven cursor/effective position and terminal/fidelity status;
- declares its selected opening snapshot/state and any excluded interval;
- never claims to be a seamless continuation when continuity was not proven.

### Incomplete, non-faithful, and failed

- `incomplete`: required evidence is missing or retention/loss prevents full reconstruction.
- `non-faithful`: processing may continue for analysis, but original capture/control/order semantics cannot be claimed.
- `failed`: an invariant, security, corruption, or recovery condition prevents safe continuation.

These classifications are explicit domain/recovery facts, not inferred only from logs.

## RPO derivation

RPO is defined per authoritative record class and failure model. One system-wide number is insufficient.

For each class, derive the maximum permitted loss window from:

1. whether loss changes domain decisions or only rebuild cost;
2. whether upstream replay/resume is available and semantically equivalent;
3. whether exact reconstruction retains identity, order, causation, versions, and timing class;
4. whether loss invalidates faithful replay, audit, control state, or safety;
5. maximum event/byte/logical-time exposure while recoverability is pending;
6. mode: replay analysis, backtest/paper replay, live read-only, live paper, or later execution-capable mode;
7. incident investigation and retention obligations;
8. measured persistence throughput, flush distribution, saturation behavior, and pending-queue bounds;
9. local backup/export cadence and failure independence;
10. operator tolerance for declaring a run incomplete and restarting as a child.

The proposal records both:

- semantic RPO: the largest loss consistent with the stated fidelity/safety claim;
- operational RPO: the measured bound the implementation can meet under accepted workloads and faults.

The accepted RPO is no weaker than the semantic requirement and is supported by measured operational evidence. If evidence cannot support it, change the implementation, operating envelope, or product claim.

Controls and selections have semantic no-silent-loss fences at their crossing boundaries. That does not invent a time number; it defines a causal boundary.

## RTO derivation

RTO is derived per authority and readiness mode from:

1. maximum retained journal tail and snapshot cadence;
2. scan, integrity-check, migration, and rebuild throughput;
3. worst accepted corruption/quarantine fallback depth;
4. deduplication and index rebuild cost;
5. dataset size and partition parallelism consistent with deterministic order;
6. source resubscription/snapshot availability;
7. hardware/resource envelope and competing operational work;
8. required semantic verification before readiness;
9. backlog accumulated during recovery and safe drain rate;
10. whether capture may continue while downstream authorities recover;
11. operator diagnosis or approval steps;
12. required sustained post-recovery observation.

The RTO clock starts at the actual failure occurrence when that occurrence is observable from authoritative or validated environment evidence. When the occurrence cannot be established, the clock starts at the earliest authoritative detection of the failure. In that case:

- the detection delay is reported separately as a bounded value, measured distribution, or `unknown`;
- RTO results may not hide detection delay by starting at operator action, restart initiation, or recovery-worker startup;
- any upper-bound claim includes the detection policy and its measurement uncertainty.

Every reported recovery duration decomposes, where applicable, into:

1. failure-to-detection delay;
2. detection and classification;
3. containment/fencing and affected-scope isolation;
4. alert delivery and operator response/approval;
5. process or host restart/bootstrap;
6. storage inspection and valid-prefix establishment;
7. snapshot/checkpoint selection and reconstruction;
8. tail replay, backlog drain, and source resynchronization;
9. semantic verification and readiness publication;
10. sustained post-recovery observation.

Automated and operator-mediated recoveries report the same components; inapplicable components are explicitly zero/not-applicable rather than silently omitted.

Each RTO names its endpoint, such as:

- capture continuity re-established;
- replay dataset validated;
- control/barrier state reconstructed;
- run-input dispatch ready;
- complete run recovered and verified;
- query projections caught up.

RTO evidence uses Phase 02 workload/environment manifests and statistical method. “Process started” is not a recovery endpoint.

## Crash and ambiguity matrix

| Failure point | Expected reconstruction | Required behavior |
|---|---|---|
| Before authority acceptance | No authoritative fact | Retry may recompute under normal rules |
| After acceptance, before publication, no recoverability handoff | Accepted fact may be within declared loss policy unless fenced | Report exact fidelity consequence; fenced classes must not cross downstream boundary |
| After publication, before recoverability acceptance | Consumer may have seen a fact that can be lost | Recover/rebuild or mark incomplete; publication never proves durability |
| After recoverability acceptance, before proof | Persistence boundary owns pending work but survival is unproven | Inspect commit state; do not populate `record_time` or claim recoverable without proof |
| After recoverable proof, before caller acknowledgement | Fact survives; caller may see unknown | Query/retry same identity and return original outcome |
| Mid-record write | Invalid suffix/record | Stop at valid prefix; quarantine tail |
| Mid-atomic-group write | Group not committed | Exclude whole group; fenced transition remains closed |
| Commit persisted, acknowledgement lost | Group is recoverable | Return/reconstruct original identity; do not append duplicate transition |
| `ControlOutcome` accepted, barrier not yet crossed | Outcome/reservation restored | Reinstall barrier and continue from original position |
| Control effective at boundary, application acknowledgement lost | Selection and state may have applied | Replay original boundary idempotently; prove epoch/application, never emit activation duplicate |
| Selection accepted, never published | Original selection restored | Publish same identity and sequence |
| Selection publication attempted, consumer status unknown | Possible consumer application | Re-publish same identity; consumer deduplicates and acknowledges |
| Selection published, publication record lost | Consumer frontier may be ahead | Reconcile using original selection identity; append recovered acknowledgement evidence |
| Cursor checkpoint lost | Authoritative records remain | Rebuild checkpoint |
| Snapshot partial/corrupt | Snapshot unusable | Fall back to earlier trusted cut/origin |
| Journal segment missing | Continuity unproven | Restore from verified backup or classify incomplete/non-faithful/failed |
| Registry unavailable/incompatible | Records uninterpretable | Remain unready; never use mutable fallback semantics |
| Storage full/read-only during bounded-loss capture | Pending buffer reaches policy bound | Apply declared backpressure/degradation and mark gaps explicitly |
| Storage full during fenced control/selection commit | Fence cannot complete | Reject before acceptance where possible or keep dispatch stopped; no crossing |
| Complete host/device loss within an explicitly configured stronger guarantee | Primary recovery source unavailable | Restore from the independently verified guaranteed copy and prove original identities/order before same-run recovery |
| Host/device loss beyond the proven guarantee | Required evidence unavailable or outside RPO | Mark affected scope incomplete/non-faithful/failed; do not claim faithful same-run continuation; create a child run if processing continues |
| Quarantine capacity exhausted for one source/partition | Unsafe material cannot be retained in the normal quarantine path | Preserve protected metadata/hash evidence, isolate the source/partition, stop or explicitly degrade affected ingestion, and alert; never overwrite existing evidence |
| Clean shutdown during pending handoff | Drain within policy or record incomplete shutdown | Do not claim clean recoverability if drain proof is absent |
| Host restart during migration | Old/new generations inspected | Resume or roll back according to migration manifest; never mix generations silently |

## Corruption detection, quarantine, and repair

### Detection

Detect at least:

- malformed framing and impossible lengths;
- checksum/integrity-root mismatch;
- partial/torn record or commit group;
- segment predecessor mismatch;
- duplicated or overlapping sequence ranges;
- missing required sequence/group member;
- identity collision with non-equivalent content;
- unauthorized producer/owner;
- registry/schema substitution or rollback;
- cursor/checkpoint inconsistency;
- snapshot consistency-cut mismatch;
- manifest dependency or content-root mismatch;
- impossible lifecycle, control, barrier, or publication transition;
- secret/canary policy violation.

### Quarantine

Quarantine:

- isolates content from authoritative reads;
- preserves bounded forensic evidence and original location/generation;
- records detection tool/version, reason, time/clock, actor or automated policy, and affected dependency closure;
- applies strict access control;
- prevents automatic “skip and continue” for state-, control-, selection-, safety-, or ordering-relevant records;
- emits health and alert state without leaking payloads.

### Quarantine exhaustion

Quarantine storage and metadata queues are bounded. Exhaustion produces a typed `quarantine_capacity_exhausted` condition for the affected source, partition, stream, dataset, or ingest boundary.

On exhaustion, Chronos must:

1. preserve a minimal evidence record containing the offending record's content hash or integrity identity, source/partition/location, observed size, reason class, parser/validator version, detection time/clock, and first/aggregate occurrence counts without retaining unsafe payload content when capacity forbids it;
2. isolate the offending source or partition so it cannot continue consuming shared quarantine or authoritative capacity;
3. stop affected ingestion, or continue only in an explicitly declared degraded/gapped mode whose source contract and RPO permit it;
4. mark continuity, fidelity, readiness, and operator action requirements explicitly;
5. retain the exhaustion transition and evidence metadata in a protected audit/security path;
6. require governed capacity recovery, export, retention action, or source remediation before re-admission.

It must never silently drop or overwrite an existing quarantine record, recycle an unacknowledged slot, admit the offending content, skip a state-affecting record and report continuity, or allow one hostile source to exhaust every source's quarantine allocation. If even the protected minimal evidence/audit path cannot accept the exhaustion record, the affected ingest scope stops and becomes unready/failed; the system does not continue with an unrecorded evidence gap.

### Repair

Permitted repair operations are explicit and append evidence:

- restore an identical verified segment from backup/export;
- truncate an uncommitted invalid suffix after preserving evidence;
- rebuild an accelerator/index/checkpoint from authoritative history;
- regenerate a derived dataset into new lineage;
- migrate records into a new immutable generation;
- mark a gap and start a new stream epoch;
- create a child run from the last proven cut.

Repair never:

- edits an accepted record in place;
- invents a missing control outcome, effective position, selection, or cursor;
- resequences retained facts to make continuity appear valid;
- maps unknown schema values to familiar defaults;
- deletes evidence of corruption or fidelity change.

Every repair produces a repair manifest with before/after content identities, authority, method version, operator/policy authorization, validation results, and resulting fidelity classification.

## Compaction, retention, and migrations

### Compaction

Compaction may:

- remove obsolete physical encodings after a verified lossless rewrite;
- consolidate append-only publication states, cursor history, or indexes when the retained representation still proves original identities and transitions;
- discard expired accelerator generations;
- tier immutable source/dataset segments;
- retain integrity roots and dependency references.

Compaction may not:

- erase accepted control outcomes or their effect history;
- collapse an accepted/rejected decision into only current configuration;
- replace selection history with only the final cursor;
- remove correction/reversal chains required for replay or audit;
- convert a non-faithful dataset into a faithful one;
- remove command idempotency evidence while repeated effect remains possible.

Each compaction run uses an immutable manifest, reads a stable input generation, writes a new generation, validates semantic equivalence, atomically switches a catalog/reference pointer, and retains rollback evidence until the new generation is accepted.

### Migrations

Migration classes are:

- representation-only and semantically identical;
- additive/forward-preservable;
- transformable with explicit versioned semantics;
- re-normalization required;
- incompatible/new run or dataset required.

Migration:

1. pins source generation and registry;
2. writes a new immutable generation;
3. preserves old identity/lineage references;
4. verifies record counts, ordering, identities, semantic checksums, manifests, snapshots, and replay outputs as applicable;
5. runs recovery against the new generation;
6. switches only after validation;
7. records rollback and retention policy.

In-place destructive schema migration is prohibited for authoritative history.

## Idempotent rebuild

All rebuildable artifacts declare:

- authoritative input set and exact bounds;
- component/algorithm/schema/registry versions;
- deterministic ordering;
- canonical output identity or semantic equivalence rule;
- random seed/policy where allowed;
- correction and filtering policy;
- expected semantic checksum/invariants;
- output publication and generation-switch protocol.

Rebuilds write a new generation. They do not update the active generation incrementally unless the same crash/idempotency invariants are proven.

At minimum, Phase 03 proves idempotent rebuild for:

- stream/cursor indexes;
- command idempotency lookup from `ControlOutcome` history;
- control barrier state;
- selection and publication frontiers;
- capture/dataset catalogs from manifests;
- checkpoint indexes;
- quarantine indexes;
- disposable query/read projections used by event infrastructure.

Repeated rebuilds from identical inputs produce identical semantic output. Physical layout, timestamps, and opaque generated IDs may differ only when excluded by the registered equivalence contract.

## Storage pressure, backpressure, and safe degradation

### Resource model

Every persistence path declares bounded:

- ingress queue records/bytes;
- recoverability-pending records/bytes/age;
- current segment/group size;
- retry count and elapsed age;
- quarantine capacity;
- snapshot/rebuild working set;
- backup/export staging;
- disk-space reserve and inode/file-count reserve where applicable.

Pressure states are typed:

```text
normal -> elevated -> degraded -> critical -> exhausted
```

Transitions have hysteresis and operator guidance. Capacity thresholds are evidence-derived and configuration-versioned.

### Priority

Pressure handling preserves, in order:

1. execution safety and economic-integrity facts when activated: kill-switch/control fences, fill ingestion, unknown-order evidence, reservation integrity, and ledger correctness;
2. authoritative audit and security evidence required to attribute or contain those actions and failures;
3. other safety controls and their admission/outcome/barrier evidence;
4. required source-capture evidence and immutable run/registry/dataset manifests;
5. run-input selections, publication recovery, cursor/epoch history, and other ordering-critical facts;
6. other bounded-loss or reconstructable authoritative facts according to their declared RPO;
7. accelerators and rebuildable projections;
8. optional telemetry and diagnostics.

Priority does not permit accepting a fenced transition without its recoverability contract.

### Safe behavior

Under pressure, Chronos may:

- stop admitting new runs or commands;
- keep a control/selection barrier closed;
- backpressure source adapters where their protocol permits;
- shed optional telemetry under Phase 02 rules;
- suspend snapshot, compaction, research replay, or export work;
- rotate to a prevalidated reserve;
- mark capture gaps and open a new epoch when loss is unavoidable and declared;
- isolate a source/partition whose quarantine allocation is exhausted and stop or explicitly degrade only that affected ingest scope;
- pause downstream strategy progression while capture continues;
- become unready or fail closed.

Chronos must not:

- silently drop accepted controls or selections;
- silently widen a configured RPO;
- overwrite unconsumed authoritative history;
- block a kill-switch admission path behind ordinary bulk persistence work;
- report healthy/faithful when continuity is unknown;
- silently drop, overwrite, or recycle quarantine evidence when quarantine capacity is exhausted;
- delete quarantine or audit evidence automatically merely to preserve optional work;
- continue live-capable behavior in a later phase when required authorization/execution persistence is unavailable.

## Local backup and export

Phase 03 supports local-first backup/export without assuming remote infrastructure.

An export is a consistent immutable bundle containing applicable:

- manifests and registry snapshots;
- journal segments or selected record ranges;
- snapshots/checkpoints and exact cuts;
- integrity roots and dependency graph;
- replay class and fidelity status;
- configuration/build evidence needed for interpretation;
- redacted security metadata;
- export tool/version and source generation;
- encrypted secret references only, never secret values.

Export procedure:

1. select a stable consistency cut or sealed generations;
2. resolve dependency closure;
3. validate before copy;
4. write to a new staging generation;
5. verify copied content and manifest roots independently;
6. seal the bundle;
7. record export evidence and destination classification.

Import/restore never makes an export authoritative merely because checksums match. It also verifies owner, scope, registry compatibility, provenance, replay/fidelity class, and rollback/substitution policy.

Backup policy distinguishes:

- same-device copy, which does not protect against device loss;
- independent local-device copy;
- operator-managed offline or remote copy when later selected.

The supported failure model states which class is required. Restore drills, not successful copy commands, prove backup usefulness.

## Security and integrity

Persistence security requires:

- least-privilege read/write/repair/export roles by authority and record class;
- separation of normal append, recovery read, quarantine access, repair, migration, retention deletion, and export permissions;
- authenticated producer/owner identity at persistence boundaries;
- no payload-driven code or type dispatch;
- immutable registry and manifest verification;
- path/key validation preventing traversal or cross-run/cross-authority injection;
- encryption requirements derived from data classification and deployment threat model;
- secret values excluded from journals, snapshots, manifests, telemetry, quarantine diagnostics, crash reports, and exports;
- credential references represented by redacted identity/version only;
- integrity metadata covering records, groups, segments, manifests, and exports;
- rollback/substitution detection for registry, manifest, journal generation, and snapshot;
- bounded parsing, decompression, nesting, allocation, and repair work;
- audit of privileged reads, repairs, exports, retention deletion, and policy changes;
- secure disposal policy for retired physical media/generations where applicable.

An integrity checksum detects accidental corruption but is not automatically proof against a malicious writer. Cryptographic authentication/signing requirements remain a threat-model-driven implementation decision.

No live-execution credentials or live venue records exist in Phase 03 stores.

## Observability contract

Persistence emits Phase 02 telemetry without making telemetry authoritative.

### Required lifecycle points

Implement exact points for applicable facts:

- `<fact>.accepted`;
- `<fact>.published.<consumer>`;
- `<fact>.recoverability_accepted`;
- `<fact>.recoverable`;
- journal append attempted/accepted;
- group commit started/completed/failed;
- flush requested/completed/failed;
- segment opened/sealed/validated/quarantined;
- snapshot/checkpoint started/completed/validated/rejected;
- recovery inspection/reconstruction/verification/readiness transitions;
- backup/export/import/restore started/completed/validated/failed;
- compaction/migration generation created/validated/switched/rolled_back;
- storage-pressure state transition.

`record_time` is associated only with the recoverable proof. Telemetry timestamps do not populate it.

### Metrics and health

At minimum:

- offered, accepted, appended, published, recoverability-accepted, recoverable, rejected, quarantined, and retried records/bytes;
- pending recoverability count/bytes/oldest age;
- group size and commit/flush distributions;
- journal segment/generation count, valid-prefix position, and tail distance;
- snapshot age, creation duration, validation result, and tail length;
- recovery scanned/replayed records/bytes, phase duration, fallback count, and final disposition;
- source/control/selection continuity and fidelity status;
- storage capacity/reserve, write/read error, latency, saturation, and pressure state;
- corruption type, affected scope, quarantine state, and repair disposition;
- replay class, dataset validation, semantic checksum, and deterministic mismatch;
- backup/export age, dependency completeness, restore validation, and failure;
- compaction/migration input/output generations and equivalence status.

High-cardinality identities belong in restricted logs/traces/evidence, not general metric dimensions.

Health states distinguish:

- persistence ready;
- degraded within declared policy;
- recoverability lagging;
- recovery active;
- continuity unknown;
- storage pressure;
- corrupt/quarantined;
- RPO breached;
- RTO at risk/breached;
- unready/failed.

Missing evidence is `unknown`, not healthy.

### Alerts

Use the Phase 02 alert schema for:

- fenced commit failure or barrier recovery ambiguity;
- pending recoverability age/bytes approaching policy;
- storage pressure and reserve breach;
- journal/snapshot/manifest corruption;
- invalid prefix or missing segment;
- snapshot validation failure/fallback;
- RPO breach or unprovable fidelity;
- recovery failure or RTO breach;
- backup/export/restore validation failure;
- unauthorized repair/export/delete access;
- semantic checksum mismatch;
- command/selection duplicate or identity conflict.

Alerts never infer a control or execution fence from lack of traffic.

## Performance method

Phase 03 persistence adopts the Phase 02 workload, environment, instrumentation, statistical, and coordinated-omission rules.

Reference campaigns include:

- steady source capture and normalized journal load;
- mixed source, control, and selection load;
- bounded group-commit sweeps;
- bursts and producer stalls;
- storage latency/error injection;
- storage-full and read-only transitions;
- quarantine exhaustion by one source/partition and concurrent healthy-source behavior;
- snapshot creation with foreground processing;
- restore from snapshot plus short, medium, and accepted worst-case tails;
- rebuild from origin;
- corruption scan and fallback;
- compaction/migration with foreground load;
- backup/export and restore validation;
- control-barrier and selection crash permutations;
- clean and abrupt shutdown;
- telemetry-profile A/B.

Every campaign records:

- offered, accepted, published, and recoverable populations separately;
- scheduled versus actual arrivals and generator lag;
- queue and pending age/bytes;
- exact durability policy and group/flush configuration;
- semantic checksum and fidelity result;
- environment/workload/build/profile manifests;
- fault timing and failure class;
- actual failure occurrence where observable, earliest authoritative detection, detection delay, containment, operator response, restart/bootstrap, reconstruction, verification, recovery endpoint, and sustained post-recovery validation.

Performance acceptance cannot trade correctness for speed. A faster result that loses a required record, widens an undeclared RPO, changes selection order, or weakens integrity fails.

## Testing and failure injection

### Contract tests

- round-trip every active record class through each implemented representation;
- preserve identity, order, causation, versions, unknown fields, and semantic checksum;
- verify append idempotency and non-equivalent identity conflicts;
- verify `recoverability_handoff_time` versus `record_time`;
- verify valid-prefix and atomic-group behavior;
- verify dependency closure for manifests, snapshots, and exports.

### Model/property tests

- accepted journal order never regresses or silently crosses generations;
- committed atomic groups are all-or-none after every crash point;
- each admitted command has exactly one recoverable terminal `ControlOutcome`;
- dispatch never crosses an accepted effective-position reservation under the prior epoch;
- each allocated run-input sequence has exactly one recoverable selection;
- recovery never changes selected input, cursor vectors, epoch, or sequence;
- re-publication produces at most one consumer state effect;
- snapshot plus complete tail equals origin replay semantically;
- compaction/migration preserves semantic history;
- retention never deletes a live dependency;
- rebuild output is semantically identical across scheduling/batching permutations;
- corrected/re-normalized replay never acquires faithful identity.
- faithful capture-order reconstruction uses the original normalizer/reference/schema/canonicalization/correction/merge versions and matches the recorded normalized semantic checksum set;
- current-code or changed-version normalization is classified as raw re-normalization with new lineage;
- quarantine exhaustion isolates only the declared affected scope, preserves protected evidence metadata/hash, and cannot overwrite or silently discard earlier evidence.

### Crash injection

Inject crashes:

- before and after each lifecycle transition;
- at every record framing boundary;
- before, during, and after atomic-group member writes and commit proof;
- after accepted control outcome but before recoverability;
- after barrier installation and before dispatch visibility;
- immediately before/at/after effective position;
- after selection allocation and at every publication state;
- before and after consumer application/acknowledgement;
- during segment rotation, snapshot, checkpoint, compaction, migration, export, import, and retention deletion;
- during recovery itself;
- during clean shutdown drain.

### Storage fault injection

- delayed, failed, partial, reordered, and duplicated writes where the chosen failure model permits;
- lost acknowledgement;
- stale reads or generation pointer;
- full volume, reserve exhaustion, read-only transition, and permission loss;
- missing/corrupt segment, manifest, snapshot, registry, index, and checkpoint;
- checksum collision simulation through forced conflicting content identity;
- decompression bomb, oversized record, deep nesting, and malformed length;
- backup destination failure and incomplete restore.
- quarantine payload and metadata capacity exhaustion, including a hostile-source flood while unrelated sources remain active where isolation policy permits.

### Recovery oracles

Tests verify:

- maximal valid prefix;
- exact record/fact lifecycle classification;
- original control and selection identities;
- complete cursor/barrier/configuration reconstruction;
- semantic checksum;
- fidelity/incomplete/failure status;
- no duplicate state effect;
- measured RPO/RTO against the registered campaign;
- RTO start at observable failure occurrence or earliest authoritative detection, with detection delay and containment/operator/restart/reconstruction components reported separately;
- correct readiness and alerts;
- no secret leakage.

No fault may become plausible success.

## Evidence gates

Planning approval fixes the evidence contract. Artifacts become mandatory during Phase 03 implementation and extend cumulatively as later authorities add record classes.

| Evidence ID and artifact | Owning phase | Required contents | Pass condition |
|---|---|---|---|
| **PRR-E01 — Record-class registry and durability matrix (`DM-E12`)** | 03; extend every phase | Every authoritative, reconstructable, accelerator, quarantine, audit, and optional class; owner; acceptance/publication/recoverability boundaries; Phase 03 baseline versus stronger host/device failure classes; RPO semantics; reconstruction; retention; failure disposition | Every implemented record has one owner and explicit policy; controls/selections establish a baseline crash-surviving recovery source at acceptance; failures beyond stronger proven guarantees become incomplete/non-faithful/failed rather than silent continuation |
| **PRR-E02 — Five-state lifecycle conformance** | 03; extend every phase | Fixtures for computed, accepted, published, recoverability-accepted, recoverable, caller-unknown, and lost-within-policy states, including fenced acceptance's baseline recovery source | Boundaries remain distinct; `record_time` appears only with recoverable proof; publication/enqueue never masquerades as durability; control/selection acceptance cannot exist only in volatile state |
| **PRR-E03 — Journal valid-prefix and atomic-group suite** | 03 | Individual/batch/group fixtures, partial writes, lost acknowledgements, duplicate/conflicting identities, segment rotation, abrupt process/OS restart on the same persistence generation, and scoped host/device-loss cases | Recovery selects one deterministic valid prefix; committed groups are all-or-none; retry creates no duplicate fact; the tested guarantee and beyond-guarantee disposition are explicit |
| **PRR-E04 — Atomic control-barrier recovery proof (`DM-E04`, `EC-E03`, `EC-E04`)** | 03; extend 07/09/11B | Crash at every admission/outcome/idempotency/epoch/effective-position/barrier/visibility/application point, including immediately before and after baseline recovery-source establishment | Exactly one outcome and position survive every baseline crash after acceptance; no accepted volatile-only outcome exists; dispatch never crosses stale epoch; recovery never re-decides or emits duplicate activation |
| **PRR-E05 — Selection/publication recovery proof (`EC-E06`, `EC-E15`)** | 03 | Crash immediately before/after atomic selection recovery-source establishment, allocation/publication states, state consumer applied/not applied/unknown, and lost acknowledgement | No dispatch/state effect occurs before accepted selection recovery; same identity/sequence/cursors/epoch is restored and republished; consumer state effect occurs once; merge is never reevaluated |
| **PRR-E06 — Snapshot/checkpoint consistency-cut suite** | 03 harness; extend stateful phases | Atomic/quiesced/versioned/replay-derived cuts, corrupt/partial/mixed cuts, fallback, snapshot-plus-tail and origin replay | Only validated cuts restore; snapshot plus tail matches origin semantic checksum; accelerator loss never invents state |
| **PRR-E07 — Replay-class and dataset-manifest set (`DM-E05`)** | 03 manifest foundation; complete 06 | Capture-order manifest pins original normalizer/reference/schema/canonicalization/correction/merge versions and expected normalized semantic checksums; normalized-fact, current-code raw re-normalization, and corrected event-time manifests include gaps/corrections/lineage/integrity | Faithful capture-order reconstruction matches the original normalized checksum set; changed/current-code normalization is raw re-normalization with new lineage; all classes remain distinguishable and invalid dependencies cannot start |
| **PRR-E08 — Run recovery and child-run matrix (`DM-E11`)** | 03; extend 10/11 | Same-run success, manifest/version change, lost control/selection, RPO breach, reset, corruption, parent/child lineage | Same-run recovery occurs only with complete proof; otherwise explicit child/incomplete/non-faithful/failed disposition |
| **PRR-E09 — RPO/RTO derivation and measured campaign** | 03; extend every persistence-owning phase | Per-class semantic derivation, measured implementation envelope, workloads/environments, statistical method, failure occurrence/detection evidence, detection delay, containment, operator response, restart, reconstruction, readiness endpoints | RTO starts at observable failure occurrence or earliest authoritative detection; detection delay and recovery components remain visible; no invented/unmeasured target; breach is detected and classified |
| **PRR-E10 — Crash/corruption/quarantine/repair campaign** | 03 | Full crash matrix, corruption classes, bounded quarantine and exhaustion, protected metadata/hash evidence, per-source/partition isolation, repair manifests, backup restore, fidelity outcomes | No corruption or quarantine exhaustion is silently skipped/dropped/overwritten; exhausted scope stops or explicitly degrades; unrelated scopes remain isolated where policy permits; repair preserves history and restore is independently validated |
| **PRR-E11 — Compaction/retention/migration equivalence** | 03 harness; rerun on policy/schema changes | Dependency graph, sealed generations, semantic checksums, rollback, destructive-negative tests | No live dependency is deleted; no in-place authoritative rewrite; new generation recovers and replays equivalently |
| **PRR-E12 — Idempotent rebuild suite** | 03; extend every derived store | Rebuild indexes, barriers, idempotency, selections, catalogs, checkpoints, projections under schedule/batch permutations | Repeated rebuilds are semantically identical and never advance authority without source facts |
| **PRR-E13 — Storage-pressure and safe-degradation campaign (`PS-E07`)** | 03 | Bounded queues/reserves, Phase 01 priority order, full/read-only/slow storage, capture gap, quarantine exhaustion, fenced commit failure, source isolation, recovery | Safety controls/kill switch and authoritative audit/security evidence outrank capture/manifests and other facts; no unbounded growth or silent widening/loss; fenced transitions remain closed; exhausted quarantine stops or explicitly degrades the affected scope |
| **PRR-E14 — Security and integrity suite** | 03 | Unauthorized writer/reader/repair/export/delete, registry/manifest rollback, path injection, hostile framing, secret canaries, integrity substitution | Unauthorized or hostile input has no authority effect; secrets do not leak; rollback/substitution is detected |
| **PRR-E15 — Backup/export and restore drill** | 03 | Stable cut, dependency closure, independent copy class, integrity verification, clean-room-style restore/replay | Restore, not copy, proves usability; manifest and semantic checksums pass; missing dependency fails explicitly |
| **PRR-E16 — Persistence observability/performance campaign (`EC-E14`)** | 03 | Exact lifecycle points, pressure/recovery metrics, alerts, normal/burst/overload/fault workloads, overhead A/B, semantic oracles | No endpoint is redefined; optional telemetry cannot block authority; accepted budgets and thresholds are evidence-based |
| **PRR-E17 — Reserved live-execution boundary review** | 03; activate 11B | Proof that no live records/credentials exist; reserved ports/classes documented; later fence requirements traced | Phase 03 cannot submit or persist plausible live execution; later extension can add records without weakening Phase 01–03 semantics |
| **PRR-E18 — Independent critique and cumulative compatibility review** | Every phase gate | Finding dispositions and comparison with all approved prior leaves and affected event contracts | Zero unresolved material findings; no authority, ordering, lifecycle, telemetry, replay, or recovery contract is weakened |

## Phase 03 persistence/replay/recovery closure

This leaf is implementation-complete only when:

- active authoritative record classes and their owners are registered;
- the five lifecycle states are machine-verifiable across every implemented persistence path;
- journal valid-prefix, append idempotency, atomic groups, and integrity behavior pass crash tests;
- control/effective-position and selection acceptance atomically establishes a recovery source surviving the declared Phase 03 baseline crash class before dispatch or state effect; the records cannot be split, lost silently, re-decided, or reallocated;
- stronger host/device/backup-loss guarantees are explicitly scoped and tested, and failures beyond them produce incomplete/non-faithful/failed status rather than faithful continuation;
- selection publication uncertainty is resolved through same-identity idempotent re-publication;
- source/control/selection/run-manifest retention dependencies are enforced;
- snapshot/checkpoint consistency cuts and snapshot-plus-tail equivalence are proven;
- all four replay classes have distinct valid manifests and fidelity labels; faithful capture-order replay pins original normalizer/reference/schema/canonicalization/correction/merge versions and verifies normalized semantic checksums, while current-code normalization is raw re-normalization;
- same-run versus child-run/incomplete/non-faithful/failed recovery is deterministic;
- RPO/RTO proposals are derived and measured per authority/record class without unsupported numbers; RTO starts at observable failure occurrence or earliest authoritative detection and reports detection, containment, operator, restart, reconstruction, and verification components;
- corruption, bounded quarantine/exhaustion, repair, compaction, retention, migration, rebuild, backup, and restore evidence passes;
- storage pressure follows the required safety-control, authoritative audit/security, capture/manifest, then other-fact priority; quarantine exhaustion preserves protected metadata/hash evidence, isolates the offender, and stops or explicitly degrades the affected ingest scope;
- Phase 02 telemetry/performance meanings and accepted synthetic control-fence campaign remain intact;
- applicable `DM-E04`, `DM-E05`, `DM-E11`, `DM-E12`, `EC-E03` through `EC-E06`, `EC-E11`, `EC-E14`, `EC-E15`, and `PRR-E01` through `PRR-E18` pass;
- cumulative Phase 01–02 and Phase 03 event-contract review has no unresolved material finding.

This leaf does not fail because later market-state, strategy, portfolio/risk, paper, accounting, external-discovery, or live-execution records do not yet exist. It fails if those phases cannot add their authoritative record classes, snapshots, reconstruction, or stronger fences without redefining the contracts here.

## Deferred choices

The following remain evidence-driven implementation or later-phase decisions:

1. Journal, database, filesystem, object-store, snapshot-store, and catalog products.
2. On-disk/wire serialization, schema language, code generation, and canonical encoding.
3. Physical partition, segment, page, index, and directory/key layout.
4. Checksum, content-identity, Merkle/integrity-tree, signature, and encryption algorithms.
5. Exact flush, group-commit, sync, buffering, and acknowledgement mechanisms.
6. Numeric queue capacities, group sizes, flush intervals, segment sizes, snapshot cadence, retention periods, RPOs, and RTOs.
7. Replication, independent-device backup, offline backup, and remote backup topology.
8. Exact snapshot algorithm for each later authority.
9. Compaction cadence, tiering policy, archival classes, and secure disposal mechanism.
10. Migration tooling and catalog pointer-switch mechanism.
11. Deterministic versus opaque ID algorithms where event contracts allow either.
12. Process boundaries and IPC persistence handoffs if measurements justify separation.
13. Venue/source-specific resume, historical backfill, and capture continuity capabilities.
14. Market-state, feature, strategy, portfolio, risk, paper, accounting, and external-observation record payloads and retention.
15. Live-execution write-ahead, authorization, submission, ambiguity, reconciliation, fill, ledger, and credential contracts, owned by later phases.

Deferral does not permit an implementation to invent local semantics. Every choice remains behind the authority, lifecycle, durability matrix, manifest, integrity, replay, recovery, telemetry, and evidence contracts defined here.
