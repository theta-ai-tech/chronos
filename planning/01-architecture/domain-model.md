# Chronos Domain Model

## Purpose

This document defines the canonical language, identities, lifecycle, and invariants of Chronos. It is the semantic contract shared by market-data adapters, replay, market state, feature calculation, strategies, portfolio construction, risk, paper execution, future live execution, accounting, observability, and the operations console.

Chronos is a local-first, replay-first trading research and decision platform. Its first useful operating modes are deterministic replay, live read-only monitoring, and live paper trading. Real-money execution is deliberately deferred, but the domain model must admit it without changing the meaning of upstream concepts.

This is a planning document. It defines what domain concepts mean and where responsibility belongs. It does not choose concrete classes, serialization libraries, storage engines, process boundaries, database tables, queue technologies, or ticket-level implementation.

## Modeling principles

1. **Facts, observations, decisions, and actions are different things.** A venue message is not a strategy signal; a signal is not a target position; a target position is not an order.
2. **The canonical path is explicit.** Every material transformation has a typed input, typed output, owner, version, and audit relationship.
3. **Replay is a first-class execution mode.** The same normalized inputs, configuration, code versions, and deterministic clock must produce the same decision outputs.
4. **Source truth is preserved.** Normalization never replaces or mutates the captured source event.
5. **State is derived.** Market state, positions, and P&L are reconstructable from authoritative ordered facts plus declared snapshots.
6. **Decisions are logically durable facts.** Signals, target positions, risk decisions, reservations, and executable order intents have retained, reconstructable histories even when rejected, superseded, or not executed. This does not require synchronous persistence on every hot-path transition.
7. **Live uncertainty is represented, not hidden.** Gaps, stale data, ambiguous ordering, unavailable dependencies, and reconciliation discrepancies have explicit domain status.
8. **Amounts are exact in domain logic.** Prices, quantities, fees, and money must not depend on binary floating-point equality.
9. **Venue-neutral does not mean least-common-denominator.** Canonical concepts are stable, while venue-specific facts remain available through explicit extensions and source references.
10. **The GUI is a consumer and control surface.** It does not own trading semantics or authoritative state.

## Ubiquitous language

The following terms are normative. Code, schemas, APIs, logs, metrics, tests, and planning documents should use them consistently.

### Context and reference terms

| Term | Canonical meaning |
|---|---|
| **Venue** | An external market or execution destination with its own instruments, protocols, clocks, sequencing, and trading rules. |
| **Canonical instrument** | A venue-independent economic instrument or contract identity used to relate equivalent or comparable tradeable representations. Equivalence is asserted by versioned reference data, not inferred from matching symbols. |
| **Listing** | A venue-specific tradeable representation of a canonical instrument, identified separately from its venue-native symbol. V1 market state is listing-scoped even when a one-to-one canonical mapping exists. |
| **Instrument definition** | Versioned reference data that defines the canonical instrument's economic semantics, assets, settlement, contract terms, and effective interval. |
| **Listing definition** | Versioned venue reference data that defines symbol, price tick, quantity step, minimums, status, venue rules, and effective interval. |
| **Market stream** | One logically sequenced feed partition for a venue, channel, and scope. Ordering guarantees are stated per stream, never assumed globally. |
| **Run** | One bounded execution of Chronos in replay, live read-only, paper, or future live-execution mode, with immutable provenance. |
| **Strategy instance** | A specific strategy definition plus immutable parameters and declared dependencies active within a run. |
| **Portfolio** | The accounting and risk scope within which target positions, exposure, cash, positions, and P&L are evaluated. |
| **Account** | A paper or venue account whose balances, orders, fills, and ledger are reconciled independently. |
| **External observation** | A timestamped, sourced fact from outside venue market data, such as news, social data, an oracle, or a prediction-market state. It is normalized and quality-scored separately from market events. It is not a V1 input. Any downstream influence must remain explicit in causal provenance and explanation factors. |
| **Opportunity** | A thesis derived from one or more valid strategy signals that may involve one or more economic legs and require resolution to tradeable listings or markets. The single-listing recommendation path remains the V1-specialized case. It carries explanation and provenance for every contributing signal, market observation, and external observation. |
| **Opportunity leg** | One desired economic exposure within a multi-leg opportunity, expressed before venue/listing resolution. |
| **Opportunity resolution** | The versioned decision that maps opportunity legs to eligible listings or markets under declared constraints. It is distinct from strategy assessment, portfolio sizing, risk, and execution. |
| **Candidate market** | One listing or external market considered during opportunity resolution, with eligibility evidence and source lineage. |
| **Candidate ranking** | A versioned ordering and scoring of candidate markets for an opportunity leg. Ranking does not authorize execution. |
| **Execution group** | A set of related executable order intents or orders whose economic purpose, coordination constraints, and partial-completion policy must be evaluated together. It does not imply atomic venue execution. |

### Lifecycle terms

| Term | Canonical meaning |
|---|---|
| **Source event** | The immutable payload and capture metadata received from a venue, file, synthetic generator, operator, or execution adapter before Chronos assigns domain meaning. |
| **Normalized event** | A versioned, venue-neutral statement of one market-domain fact derived from a source event. It retains source lineage and does not imply strategy meaning. |
| **Market state** | The listing-scoped state produced by applying an accepted ordered sequence of normalized market, reference, and control events, such as an L2 order book and recent public trades. |
| **Market-state view** | An immutable view exposed to downstream consumers at one complete lineage cut across every consumed book, trade, reference, and control stream. Consumers never observe a partially applied event. |
| **Feature observation** | A time- and state-bound measured value derived from a market-state view or an explicit window of views, with enough provenance to reproduce it. |
| **Diagnostic observation** | A calculation produced from degraded, stale, incomplete, or otherwise non-tradeable inputs for monitoring or investigation. It is explicitly labeled and cannot enter signal, target, risk, or execution flows. |
| **Strategy evaluation** | One immutable result of evaluating a strategy instance for one declared scope and input cut. Its outcome is exactly one of `signal_emitted` with one StrategySignal, or `abstained` with typed reasons and no StrategySignal. |
| **Strategy signal** | A strategy's immutable valid assessment of an opportunity. It expresses direction, strength or score, confidence, horizon, and explanation; it does not authorize a trade. An abstention is never a StrategySignal. |
| **Target position** | The desired absolute portfolio position for an instrument after considering one or more signals and current portfolio state, but before risk approval. |
| **Risk decision** | The immutable result of evaluating a target position and its context against risk policy: approved, modified, or rejected. |
| **Approved target** | The position and constraints authorized by a risk decision. It may equal the requested target or be a risk-reduced target. |
| **Projected exposure** | The authoritative risk view of positions plus worst-case economic effects of open orders, unknown orders, executable intents, reservations, and declared execution groups at a serialized risk position. |
| **Reservation** | An authoritative, time-bounded allocation of exposure, cash, position, or order capacity that prevents concurrent approved actions from consuming the same capacity. |
| **Reservation outcome** | The reservation authority's immutable result for one approved risk decision: `accepted` with a reservation, `rejected` because authorized capacity is unavailable, or `stale` because the decision/preconditions require a new risk-policy evaluation. It is not a risk decision. |
| **Human execution approval** | A human authorization fact required only in human-approved live execution, applied after risk approval/reservation and before an executable live intent exists. Live paper does not use or await human execution approval. |
| **Executable order intent** | A venue-independent, mode-specific instruction authorized to move from current position toward an approved target under stated execution constraints. It is executable only against its declared paper or live execution boundary and is not yet a venue order. |
| **Order** | A paper-broker or venue-bound instruction with an external or simulated lifecycle, including acknowledgement, rejection, cancellation, replacement, and terminal status. |
| **Fill** | An immutable execution fact stating that a quantity traded at a price under an order. A fill is never edited or deleted; corrections are compensating facts. |
| **Ledger entry** | An immutable balanced accounting fact caused by a fill, fee, funding event, cash movement, settlement, correction, or other declared accounting event. |
| **Position** | A derived quantity and cost-basis state for an instrument within an account or portfolio, calculated from the ledger under a declared accounting policy. |
| **P&L** | A derived valuation: realized P&L from closed economic activity and unrealized P&L from a position valued against an identified mark. |
| **Mark** | A named, time-bound price observation used for valuation. It is not necessarily executable and must identify its source and policy. |
| **Explanation factor** | A ranked, typed contribution to a strategy evaluation, signal, opportunity, or trade recommendation, including observed value, source type, interpretation, contribution where supported, and causal references. Every influential external observation appears explicitly as an explanation factor. |
| **Trade recommendation** | The single immutable, non-executable recommendation produced for each valid StrategySignal. It is `actionable` with a proposed non-zero target-position delta, or `hold` with an explicit zero target-position delta and reason. It is not a target position, risk decision, reservation, executable order intent, or risk preview. |
| **Command** | An imperative request to an authority to attempt a state transition. A command may be accepted, rejected, deduplicated, or fail; it is not evidence that the transition occurred. |
| **Domain event** | An immutable past-tense fact emitted after an authority accepts or observes a transition. Events, not commands, are replayed to reproduce accepted domain history. |
| **Control event** | An ordered domain event that changes run behavior or lifecycle, such as strategy enablement, parameter activation, pause, kill-switch activation, or reset initiation, at an explicit effective position. |
| **Audit record** | An immutable record tying an input, state version, decision, action, configuration, actor, and result into the causal history of a run. |

### Terms that must not be conflated

- **Event time** is when the source says a fact occurred; **receive time** is when Chronos received it; **process time** is when a Chronos stage handled it.
- **Signal** describes an opportunity; **target position** describes desired exposure.
- **Risk decision** authorizes or refuses exposure; it does not choose venue-specific order mechanics.
- **Strategy evaluation** either emits one valid **StrategySignal** or records **abstention**; abstention is not a signal.
- **Trade recommendation** is the one advisory action/hold interpretation of a valid signal; **target position** exists only for actionable recommendations; **risk decision** is authoritative policy evaluation; **executable order intent** is authorized work for one execution mode; **order** is an instruction accepted by a paper broker or external venue adapter.
- **Fill** is an execution fact; **position** and **P&L** are derived accounting views.
- **Market-data trade** reports market activity; an **execution fill** reports activity attributable to a Chronos account.
- **Replay** has an explicit replay class and ordering policy; **backtest** is an experiment that evaluates strategy and portfolio behavior over one such replay or a declared synthetic input.
- **Paper execution** simulates order outcomes; **live read-only** consumes live market data without sending orders.
- **Rejected** means a boundary made an explicit negative decision; **invalid** means an input violated its contract; **unavailable** means a required dependency or state could not support a decision.

The current PRDs use **suggested paper trade** and **trade intent** as compact product language. In this model, each valid StrategySignal produces exactly one non-executable TradeRecommendation containing action or hold, proposed delta/size, reference entry, horizon, exit/stop hints, and explanation. A zero delta is an explicit hold recommendation and stops there. Only an actionable recommendation may enter target construction. In live paper mode, target construction, a risk decision, and an accepted reservation may then produce an executable paper order intent without human approval. Live read-only mode cannot produce executable intents. Human approval is required only in human-approved live execution and occurs before an executable live intent exists. No component may bypass target construction, serialized exposure control, or risk by treating a recommendation as executable.

## Minimum stable message contract

Subsequent phases may choose different in-memory and durable encodings, but all cross-boundary domain messages must preserve one logical envelope. The envelope separates routing and provenance from a versioned payload.

### Event envelope

Every domain event has, directly or by an enclosing batch manifest:

| Field | Required meaning |
|---|---|
| `event_id` | Stable identity of this accepted or observed fact. |
| `event_type` | Namespaced semantic type from the minimum taxonomy below. |
| `schema_version` | Version required to decode and interpret the payload. |
| `producer` | Producing authority and implementation version. |
| `run_id` | Run in which the event was accepted or observed; source captures may additionally belong to a reusable capture session. |
| `stream_id`, `stream_epoch`, `stream_sequence` | Ordering scope and cursor for the event's authoritative stream. |
| `run_input_sequence` | Position in the ordered input/control sequence actually consumed by this run, when the event can affect run-domain state. |
| `source_event_id` | Captured-source lineage when applicable. |
| `causation_refs` | Typed references to commands, events, state cuts, or decisions that directly caused this fact. |
| `correlation_refs` | Optional workflow references; never a substitute for causation. |
| `subject_refs` | Canonical instrument, listing, account, portfolio, strategy, opportunity, order, or other typed subjects. |
| `source_event_time` | Source-asserted occurrence time when available. |
| `chronos_receive_time` | Chronos ingress time when applicable. |
| `effective_position` | Ordered run position at which a control/reference fact begins to affect behavior, when applicable. |
| `record_time` | Time this event was recorded by its authority, when recorded. |
| `quality` | Validation, freshness, completeness, uncertainty, and correction status relevant to interpretation. |
| `payload` | Event-type-specific immutable facts. |

Fields that do not apply are absent, not filled with invented values. Batching may compress repeated envelope fields but must reconstruct the same logical envelope for each event. Unknown event types are retainable and skippable only when their ordering and state-effect contract says doing so is safe.

### Minimum event taxonomy

The initial taxonomy must reserve stable namespaces and distinguish at least:

- `source.*`: captured payload, capture integrity, and source-session facts;
- `reference.*`: canonical instrument, listing, symbol mapping, trading-status, and effective-definition facts;
- `market.book.*`: L2 snapshot, delta, synchronization, and book-quality facts;
- `market.trade.*`: public market trades and trade-stream quality facts;
- `market.control.*`: heartbeat, stream status, gap, reconnect, epoch, and recovery facts;
- `external.*`: later external observations and their quality/correction facts;
- `run.control.*`: run lifecycle, configuration activation, strategy enablement, pause, kill switch, and reset facts;
- `feature.*`: valid feature and diagnostic-observation facts;
- `strategy.*`: evaluation, signal-emitted, abstained, signal expiration, and supersession facts;
- `recommendation.*`: actionable and hold recommendation facts, expiration, and supersession;
- `opportunity.*`: later opportunity, resolution, candidate, and ranking facts;
- `portfolio.*`: target and aggregation facts;
- `risk.*`: projected exposure, policy approval/modification/rejection, reservation accepted/rejected/stale, release, and expiry facts;
- `execution.approval.*`: human-approved-live approval, rejection, revocation, and expiry facts;
- `execution.intent.*`: executable-intent lifecycle and later execution-group facts;
- `execution.order.*`: order command outcome and order lifecycle facts;
- `execution.fill.*`: fill, correction, bust, fee, and allocation facts;
- `ledger.*`: transaction, posting, valuation, and projection facts;
- `reconciliation.*`: match, discrepancy, correction, and resolution facts;
- `audit.*`: security- and operator-relevant facts not already fully expressed by the owning domain event.

The exact leaf types are phase-owned. A leaf type may evolve only under the versioning rules in this document; it may not change namespace to hide a semantic change.

### Commands versus events

Commands and events use separate contracts:

- A command names an intended action, target authority, command identity, actor, expected precondition/version, requested effective position where applicable, and idempotency key.
- An authority serializes the command with competing commands for the same invariant scope, validates it, and emits accepted/rejected domain events.
- A command is never replayed as proof that its requested action occurred. Faithful replay consumes the resulting accepted/rejected event history.
- Operator and API commands may be retried, but deduplication returns the prior outcome and does not repeat the state transition.
- A command that changes run behavior must result in an ordered control event before the change becomes visible to domain processing.
- Queries and read-model refresh requests are not commands because they request no authoritative state transition.

## Canonical lifecycle

The canonical lifecycle is:

```text
Source Event
    |
    v
Normalized Event
    |
    v
Market State / immutable Market-State View
    |
    v
Feature Observation
    |
    v
Strategy Evaluation
    |
    +--> Abstained (terminal for this evaluation)
    |
    v
Strategy Signal (signal_emitted outcome)
    |
    v
Trade Recommendation
    |
    +--> Hold / zero delta (terminal for this recommendation)
    |
    v
Actionable Recommendation
    |
    v
Target Position
    |
    v
Risk Decision
    |
    v
Approved Target
    |
    v
Reservation
    |
    +--> Human-approved live: Human Execution Approval --+
    |                                                    |
    +--> Live paper / guarded automation policy ---------+
                                                         |
                                                         v
Executable Order Intent
    |
    v
Order
    |
    v
Fill
    |
    v
Ledger Entries
    |
    +--> Position
    |
    +--> Realized and Unrealized P&L
```

Not every input reaches the end of the lifecycle. A normalized event may update state without producing a feature observation. Every completed strategy evaluation either emits one valid signal or abstains with no signal. Every valid signal produces exactly one trade recommendation. A hold recommendation records a zero target delta and ends before target/risk processing; only actionable recommendations proceed. Risk may reject or reduce a target. Reservation may be rejected or become stale, and a human-approved-live reservation/workflow may await human approval. Executable order intents never await human approval because they do not exist until all required approval is present. An order may receive no fills. These are expected, auditable outcomes rather than missing data.

Later phases may add a branch before target construction:

```text
Market State and/or External Observations
    -> Strategy Evaluations and valid Signals
    -> exactly one TradeRecommendation per Signal
    -> Actionable Recommendations only
    -> Opportunity with one or more legs
    -> Opportunity Resolution
    -> Candidate Markets and Rankings
    -> Resolved Opportunity Legs
    -> Target Positions
```

This branch establishes forward-compatible language for multi-venue and Polymarket discovery. TradeRecommendation cardinality remains one per valid signal. An opportunity may compose one or more actionable recommendations, and resolution maps its legs to candidate listings without creating additional TradeRecommendation facts. A composite operator-facing opportunity recommendation is a query model over the opportunity, its source recommendations, and its resolution—not a second strategy recommendation. The branch is not required for V1, and candidate ranking or opportunity resolution never bypasses recommendation, portfolio construction, risk, reservations, or execution controls. Whenever external observations influence an opportunity, resolution, signal, or recommendation, their identities, source provenance, quality, and contribution must appear in the causal chain and explanation factors.

### 1. Capture source events

An adapter captures the source payload as received and adds capture metadata without altering the payload. An accepted SourceEvent is the earliest Chronos-owned domain fact. Whether it is merely accepted, published, recoverability-accepted, or proven recoverable is recorded under the architecture recoverability lifecycle; capture alone is never described as durable.

A source event must identify:

- its source type and venue where applicable;
- the feed, file, generator, operator command, or execution channel;
- the exact raw payload or a content-addressed reference to it;
- receive time and capture sequence;
- source-provided timestamps and sequence identifiers, if present;
- parser framing information needed to reproduce decoding;
- integrity status, including truncation or checksum failure.

Malformed or unsupported source events remain capturable. They may fail normalization, but their existence and failure reason must remain auditable.

### 2. Normalize source events

Normalization validates, decodes, and maps source events into one or more normalized events. Examples include book snapshot, book delta, market trade, instrument-status change, funding observation, and heartbeat or stream-status facts.

Each normalized event must:

- reference exactly one source event, except explicitly identified synthetic or administrative events;
- identify its schema version and normalizer version;
- identify venue, listing, and canonical instrument mapping where applicable;
- carry source, receive, and normalization timestamps under the timestamp rules below;
- carry source sequence and Chronos-assigned stream sequence where available;
- preserve venue-specific fields that cannot be represented canonically without loss;
- state validation status and any assumptions used in normalization.

Normalization must not silently invent ordering, prices, quantities, sides, or timestamps. If a required semantic value is ambiguous, normalization fails or emits an explicitly typed uncertainty fact according to the event contract.

### 3. Apply normalized events to market state

The market-state owner accepts normalized events only through the declared sequencing and validity rules for each consumed stream. A listing may consume separate book, public-trade, reference, and market-control streams plus the run-control stream. The owner applies one run input atomically and publishes a new immutable market-state view identified by a complete `StateLineage` vector, not by a single sequence.

The lineage vector contains one `StreamCursor(stream_id, stream_epoch, stream_sequence)` for every stream whose facts can affect the view. It must include distinct cursors for book, trade, reference/definition, market-control, and run-control streams when those streams exist. A stream not yet consumed is represented explicitly at its declared origin cursor; it is not omitted. The view also records the `run_input_sequence` at which the cut was published and the deterministic cross-stream dispatch policy.

For an L2 book, the state includes at minimum:

- bid and ask price levels with aggregate quantity;
- best bid, best ask, spread, and crossed/locked status;
- snapshot lineage and the complete vector of consumed stream cursors;
- freshness and synchronization status;
- recent market trades required by configured windows;
- canonical-instrument and listing-definition versions;
- effective run-control position and active configuration epoch.

A state view must be either valid for trade-capable strategy consumption or explicitly non-tradeable. Gap detection, stale status, invalid book shape, and recovery state are part of the domain, not merely log messages. When required inputs are stale beyond policy, a trade-capable strategy must not emit a strategy signal. Feature calculators and explicitly diagnostic strategy calculations may continue only by emitting diagnostic observations labeled with stale inputs, age, and non-tradeable status; those observations cannot reach target construction.

### 4. Produce feature observations

A feature calculator derives observations from an immutable market-state view and, where required, a declared historical window. Initial examples include order-book imbalance, microprice, spread, short-horizon return, trade imbalance, and volatility.

A valid feature observation identifies:

- feature definition and version;
- canonical instrument/listing and complete market-state lineage;
- exact observation time and window boundaries;
- numeric value, unit, and validity;
- input lineage sufficient to reproduce it;
- treatment of missing, stale, or insufficient history.

Features are observations, not recommendations. An unavailable feature is represented explicitly and cannot be silently replaced with zero or a previous value unless the feature definition declares that behavior. A value derived from stale or non-consumable required state is a diagnostic observation, not a valid feature observation.

### 5. Evaluate a strategy

A strategy instance evaluates a declared set of valid feature observations, external observations where configured, and contextual state for one declared evaluation scope. Every completed invocation emits exactly one StrategyEvaluation outcome:

- **`signal_emitted`:** the evaluation references exactly one valid StrategySignal.
- **`abstained`:** the evaluation records one or more typed abstention reasons and references no StrategySignal.

If any required market-state or external-observation dependency is stale, gapped, invalid, recovering, or unavailable beyond the strategy policy, a trade-capable strategy emits an `abstained` StrategyEvaluation. Abstention is not a neutral, zero-strength, degraded, or hidden StrategySignal. Diagnostic calculations remain separate and non-tradeable.

A strategy evaluation identifies:

- strategy definition, implementation version, parameters, instance, and evaluation scope;
- complete market-state lineage, feature observations, external observations, and contextual inputs;
- outcome, decision time, and typed abstention reasons where applicable;
- causal and version provenance sufficient to reproduce the outcome.

A strategy signal identifies:

- its `signal_emitted` StrategyEvaluation;
- strategy definition, implementation version, parameters, and instance;
- canonical instrument/listing, direction, score or strength, confidence, and expected horizon;
- reference price or mark used for interpretation;
- feature- and external-observation lineage;
- ranked explanation factors;
- issue time, validity interval, and supersession relationship.

A signal is immutable. Later evaluations may supersede it but do not mutate it. Confidence is a strategy-defined calibrated quantity only when the strategy supplies evidence for that interpretation; otherwise it must be named as a score rather than presented as probability. If an external observation influenced the evaluation or signal, each influential observation appears in the signal's causal lineage and as an explanation factor with observation ID, source/provenance, quality, observed/effective time, and contribution or qualitative role.

### 6. Produce one trade recommendation per valid signal

The recommendation authority produces exactly one portfolio-neutral TradeRecommendation for every valid StrategySignal. It does not produce recommendations for abstained StrategyEvaluations. The recommendation expresses the signal's suggested direction, reference terms, and strategy-defined indicative size before portfolio context. Portfolio construction may apply that one recommendation independently to any number of portfolios, using each portfolio's state to create zero or one portfolio-scoped target.

A trade recommendation identifies:

- exactly one StrategySignal;
- `actionable` or `hold`;
- proposed direction and non-risk-approved indicative size under the strategy's declared recommendation policy;
- reference entry, expected holding window, and exit/stop hint where defined;
- recommendation/sizing policy; it does not reference a portfolio snapshot;
- complete ranked explanation, including every influential external-observation contribution and provenance;
- issue time, validity, and supersession.

An `actionable` recommendation proposes non-zero indicative exposure. A `hold` recommendation proposes zero indicative exposure and an explicit strategy-level reason. Portfolio-specific reasons such as an already-satisfied position or portfolio minimum-size rounding are target-construction outcomes, not hold recommendations. Hold is a first-class recommendation, not an abstention and not a target position. It ends before target construction and risk.

### 7. Construct target positions

The portfolio-construction owner consumes only active actionable recommendations and current portfolio state to produce a desired absolute position per instrument. Using an absolute target makes recommendation aggregation, current exposure, and later reconciliation explicit.

A target position identifies:

- portfolio and instrument;
- current position used by the decision;
- desired signed quantity or exposure;
- contributing actionable recommendations, their signals, and aggregation policy;
- sizing policy and relevant capital basis;
- expected horizon and target validity;
- creation time and portfolio-state version.

Target positions are proposals. They do not reserve capital, authorize risk, or imply an order. If aggregation of actionable recommendations produces zero net delta, portfolio construction emits an explicit no-change aggregation outcome and no target is sent to risk.

### 8. Make a risk decision

The risk owner evaluates a target position against an authoritative projected exposure at one serialized risk position. Projected exposure includes posted positions and balances plus worst-case effects of open orders, unknown orders, already executable intents, active reservations, and execution-group completion policy. Read-model lag or a locally cached portfolio view is insufficient for approval.

The result is exactly one of:

- **Approved:** the requested target is authorized.
- **Modified:** a more restrictive target and/or constraints are authorized.
- **Rejected:** no movement toward the requested target is authorized.

A risk decision must record:

- requested and approved target, if any;
- policy and limit versions;
- projected-exposure identity, risk sequence, positions, balances, reservations, and worst-case open/unknown effects;
- data-freshness and system-health inputs;
- all triggered rules, including the binding rule;
- decision time and validity or expiry;
- execution mode context;
- deterministic reason codes plus human-readable context.

Failure to evaluate risk is fail-closed: it produces no approved target. “Risk service unavailable” is not equivalent to approval or strategy rejection.

### 9. Reserve capacity, obtain human approval for human-approved live, and create an executable order intent

Risk approval alone does not make work executable. The exposure/reservation authority serializes competing approved risk decisions for the same portfolio/account risk scope and validates only reservation preconditions: decision identity/version/expiry, projected-exposure position, currently available authorized capacity, and duplicate reservation status. It does not approve, modify, reject, or re-evaluate risk policy.

For each request it emits exactly one distinct ReservationOutcome:

- **`accepted`:** capacity is atomically reserved and the outcome references the new Reservation.
- **`rejected`:** the authorized capacity cannot be reserved without exceeding the approved scope; no reservation or risk decision is created.
- **`stale`:** the risk decision, exposure position, or required precondition is no longer valid; no reservation is created and the workflow returns to the risk-policy authority for a new RiskDecision.

The reservation authority may never issue a new or modified RiskDecision. A rejected reservation may be terminal or trigger a new target/risk request according to workflow policy; a stale outcome always requires a new risk-policy evaluation before another reservation attempt.

The serialization position is a monotonic `risk_sequence` within the declared risk scope. The projected exposure at that position is authoritative for subsequent decisions. An accepted reservation identifies its covered quantities or monetary limits, linked risk decision and prospective action, expiry, consumption, release conditions, worst-case exposure treatment, and executable intent once one is created.

Human approval applies only to **human-approved live execution**. In that mode, the accepted reservation/workflow may enter `awaiting_human_approval` until a HumanExecutionApproval is accepted, rejected, revoked, or expires. Live paper never enters a human-approval state and requires no HumanExecutionApproval. Guarded automated live execution uses its explicit automation policy, not an implied per-intent human approval. Only after all requirements for the selected mode are satisfied may execution planning create an executable order intent.

An executable order intent identifies:

- approved risk decision;
- reservation and risk sequence;
- current and desired position used to calculate the delta;
- side and maximum quantity;
- urgency, validity, price protection, and allowed execution modes;
- account and eligible venue scope;
- paper, human-approved live, or guarded-automation mode;
- deduplication key and causation chain;
- HumanExecutionApproval when and only when mode is human-approved live.

An executable order intent is bound to exactly one execution mode and exists only after the reservation is accepted and every requirement for that mode is satisfied. It never has an awaiting-human-approval state. A paper intent cannot route to a live adapter, and a live intent cannot be inferred from a recommendation or paper decision. It may be ready, held by an execution-health gate, expired, cancelled, partially satisfied, satisfied, or superseded. Its status changes are represented as immutable lifecycle facts.

Reservations are consumed by acknowledged/open orders and fills under a declared transfer rule, or released on rejection, cancellation, expiry, reconciliation, and terminal completion. Unknown orders retain worst-case reservation until reconciled. Serialization and reservation transitions must make it impossible for concurrent targets to each approve against the same available capacity.

### 10. Manage orders

An execution adapter or paper broker translates an authorized executable order intent into one or more orders. An order is identified independently from the intent because routing, retries, slicing, cancellation, and replacement can create multiple orders for one intent. Related multi-leg or sliced work may additionally belong to an execution group with an explicit partial-completion and unwind policy.

The order lifecycle distinguishes at minimum:

```text
Created -> Submitted -> Acknowledged -> Partially Filled -> Filled
                          |                  |
                          +-> Cancel Pending +-> Cancel Pending
                          |                  |
                          +-> Cancelled <----+
                          |
                          +-> Rejected

Submitted -> Unknown
Any non-terminal state -> Expired where supported
```

The exact transition set may expand by venue, but transitions must be monotonic under a declared state machine. “Unknown” is a first-class state used when the external outcome cannot yet be established; it must trigger reconciliation and must not be treated as rejected or cancelled.

Retries must not create unintended duplicate economic orders. Idempotency is required where the venue supports it; otherwise Chronos must detect ambiguity and stop unsafe retries pending reconciliation.

### 11. Record fills

A fill is accepted only when it can be associated with an account, instrument, and order, or placed into an explicit unmatched reconciliation state. It records executed side, quantity, price, venue execution identifier, fees known at that time, and execution/source timestamps.

Duplicate delivery of the same venue fill must not duplicate economic effect. A venue correction, bust, fee update, or late allocation is represented as a new compensating fact linked to the original fill.

### 12. Post ledger entries

Accepted fills and other accounting events post immutable ledger entries. Ledger posting is the authoritative boundary for positions, cash, fees, realized P&L, and later reconciliation.

Every journal transaction must:

- balance under the declared accounting model;
- identify its cause and account;
- use exact asset, quantity, and monetary units;
- carry effective time and recording time;
- be idempotent by economic event;
- use compensating entries for correction rather than mutation.

A fill being recorded and its economic ledger effect must be atomic from the perspective of downstream readers, or the temporary non-atomic state must be explicit and recoverable.

### 13. Derive position and P&L

Position is derived from posted ledger entries using a versioned cost-basis policy. P&L is derived rather than directly edited.

Chronos distinguishes:

- **Realized P&L:** economic result recognized by the cost-basis and closing policy.
- **Unrealized P&L:** current position valued against an identified mark.
- **Fees and funding:** separately attributable components, even when included in net P&L.
- **Gross P&L:** before fees, funding, and other declared costs.
- **Net P&L:** after all included cost components.

Every displayed P&L value must identify portfolio/account scope, valuation time, mark source, accounting currency, and policy version. Missing or stale marks produce unavailable or stale valuation, never a silently current value.

## Identities and value objects

### Identity rules

- Domain identifiers are opaque and stable. Business meaning must not be encoded into IDs.
- IDs generated by Chronos are unique across runs unless the type is explicitly run-scoped.
- External identifiers are stored separately from Chronos IDs because venues may reuse identifiers across accounts, sessions, channels, or dates.
- Replay preserves captured and normalized event identities. Derived decision identities may be deterministic from run provenance and causal inputs when useful, but this is a design choice to be fixed before schema implementation.
- Human-readable names and symbols are attributes, not identity.
- Identity aliases, listing-symbol changes, and canonical-instrument mappings are handled through versioned reference mappings, not destructive renames.

### Entity identities

| Entity | Canonical identity | Scope and notes |
|---|---|---|
| Venue | `venue_id` | Stable Chronos identity; venue-native names are aliases. |
| Canonical instrument | `instrument_id` | Venue-independent economic identity. A materially different contract receives a new identity. |
| Listing | `listing_id` | Venue-specific tradeable identity; symbols and venue IDs are versioned attributes. |
| Market stream | `stream_id` | Identifies the ordering domain of source and normalized market events. |
| Source event | `source_event_id` | Assigned at capture; duplicate source payloads may have distinct capture identities and a shared deduplication fingerprint. |
| Normalized event | `normalized_event_id` | Stable fact identity; one source event may yield multiple normalized events. |
| Market-state view | `market_state_view_id` plus `StateLineage` | Identifies the listing state at a complete vector cut over every consumed stream and run input. A scalar sequence is insufficient. |
| Feature observation | `feature_observation_id` | Identifies one feature value over one declared state/window. |
| Diagnostic observation | `diagnostic_observation_id` | Non-tradeable output over degraded inputs. |
| Strategy definition | `strategy_id` | Stable conceptual strategy identity. |
| Strategy instance | `strategy_instance_id` | One immutable strategy version and parameter set in one run. |
| Strategy evaluation | `strategy_evaluation_id` | One immutable signal-emitted or abstained outcome for one scope/input cut. |
| Strategy signal | `signal_id` | One immutable emitted assessment. |
| Trade recommendation | `recommendation_id` | Exactly one actionable or hold recommendation for one valid signal. |
| External observation | `external_observation_id` | Later non-market-data fact with source, correction, quality, and time lineage. |
| Opportunity | `opportunity_id` | Later one- or multi-leg economic thesis; not required in V1. |
| Opportunity leg | `opportunity_leg_id` | One desired economic exposure within an opportunity. |
| Opportunity resolution | `opportunity_resolution_id` | One versioned mapping/ranking evaluation for an opportunity. |
| Candidate market | `candidate_market_id` | One listing/market candidate within one resolution. |
| Candidate ranking | `candidate_ranking_id` | One versioned scored ordering for one opportunity leg/resolution. |
| Execution group | `execution_group_id` | Related intents/orders with coordinated economic policy; not venue atomicity. |
| Portfolio | `portfolio_id` | Risk and aggregation boundary. |
| Account | `account_id` | Paper or external venue-account boundary. |
| Target position | `target_position_id` | One immutable desired-position proposal. |
| Risk decision | `risk_decision_id` | One evaluation of one target under one policy/context. |
| Projected exposure | `(risk_scope_id, risk_sequence)` | Authoritative serialized risk projection at a precise decision position. |
| Reservation outcome | `reservation_outcome_id` | One accepted, rejected, or stale result for one reservation request. |
| Reservation | `reservation_id` | One authoritative allocation of risk/execution capacity. |
| Human execution approval | `execution_approval_id` | One human-approved-live authorization for a reserved action. |
| Executable order intent | `order_intent_id` | One mode-bound, reserved execution objective. |
| Order | `order_id` | Chronos order identity; external client and venue order IDs are separate. |
| Fill | `fill_id` | Chronos identity; external execution ID participates in deduplication. |
| Ledger transaction | `ledger_transaction_id` | Groups balanced ledger entries caused by one economic fact. |
| Ledger entry | `ledger_entry_id` | One immutable posting line. |
| Run | `run_id` | Bounded execution and provenance scope. |
| Audit record | `audit_record_id` | One immutable causal or control-plane audit fact. |

Position and P&L are ordinarily projections, not independently mutable entities. A position snapshot may have a snapshot identity for recovery and comparison, but the snapshot is not the economic source of truth.

### Core value objects

| Value object | Required semantics |
|---|---|
| `Price` | Exact decimal or integer ticks plus listing-definition version; never an unqualified floating-point number. |
| `Quantity` | Exact signed or unsigned amount with unit and listing-definition version. |
| `Money` | Exact amount plus currency/asset. |
| `Side` | Buy or sell for executable actions; signal direction may additionally express neutral. Abstention is a StrategyEvaluation outcome, never a side. |
| `TimePoint` | Timestamp plus clock domain and precision. |
| `Sequence` | Integer plus stream, epoch, and sequence-kind context. |
| `StreamCursor` | Stream ID, epoch, and last consumed sequence, including an explicit origin value. |
| `StateLineage` | Canonically ordered complete map of all consumed stream cursors plus run-input sequence, dispatch-policy version, and active control/configuration position. |
| `EffectivePosition` | Run-input boundary at which an accepted control/reference event begins to affect processing. |
| `ValidityInterval` | Inclusive start and exclusive end unless a contract explicitly states otherwise. |
| `LatencyPoint` | Named timestamp observation with clock-domain ID, resolution, and uncertainty. |
| `LatencySegment` | Duration between named points measured on one monotonic clock or under a declared cross-clock comparability policy. |
| `Score` | Value plus defined range, interpretation, and strategy version. |
| `Confidence` | Value plus calibration definition; absent when not defensible. |
| `Horizon` | Expected holding or evaluation duration with units. |
| `DataQuality` | Typed status such as valid, stale, gapped, recovering, invalid, or unavailable, with reason. |
| `VersionRef` | Immutable reference to schema, code, strategy, policy, configuration, or dataset version. |
| `CausalRef` | Typed link such as caused-by, derived-from, supersedes, corrects, or reconciles. |

Value objects compare by normalized value and full semantic context. For example, two numeric prices are not equal if they refer to different instruments or incompatible instrument-definition versions.

## Time, clocks, and sequencing

### Timestamp taxonomy

Every stage uses named timestamps. A generic `timestamp` field is prohibited in canonical contracts.

| Timestamp | Meaning |
|---|---|
| `source_event_time` | Time asserted by the source for when the fact occurred. It may be absent, coarse, duplicated, or corrected. |
| `source_send_time` | Time asserted by the source for transmission, when provided. |
| `chronos_receive_time` | Time Chronos first received the source bytes, measured as near to the ingress boundary as practical. |
| `capture_time` | Time the source event became part of the capture stream. |
| `normalize_time` | Time normalization completed. |
| `state_apply_time` | Time the normalized event was atomically applied to market state. |
| `observation_time` | Logical time represented by a feature observation. |
| `decision_time` | Logical time at which a signal, target, or risk decision was made. |
| `submit_time` | Time an order left the execution boundary. |
| `ack_time` | Time acknowledgement was received. |
| `execution_time` | Source-asserted time of a fill. |
| `record_time` | Time a fact was durably recorded by Chronos. |
| `effective_time` | Time an accounting or reference-data fact takes economic effect. |

Wall-clock timestamps are represented in UTC with declared precision. Monotonic clocks are used for elapsed-time and latency measurements within a process. Durations must not be calculated by subtracting wall clocks unless clock comparability and synchronization error are known.

Clock source, resolution, synchronization method, and known uncertainty are run provenance. Replay uses a logical clock driven by ordered input, never the host wall clock for domain decisions.

### Latency points and segments

Latency is defined by named segments, not an unqualified `latency` field. The baseline segment vocabulary is:

- `ingress`: first Chronos receive point to source-event capture acceptance;
- `normalization`: capture acceptance to normalized-event availability;
- `dispatch_wait`: normalized/control-event availability to selected run-input dispatch;
- `state_apply`: dispatch to immutable market-state-view publication;
- `feature`: state-view publication to required feature availability;
- `strategy`: feature readiness to StrategyEvaluation outcome;
- `recommendation`: valid signal availability to actionable/hold TradeRecommendation;
- `portfolio_risk`: actionable recommendation availability through target and serialized risk outcome;
- `intent_ready`: approving risk outcome through ReservationOutcome, accepted reservation, any human-approved-live approval, and executable-intent readiness;
- `submit`: executable-intent release to adapter send;
- `acknowledgement`: adapter send to external/paper acknowledgement;
- `fill_ingest`: fill receive to accepted fill fact;
- `accounting`: accepted fill to ledger posting and projection update.

A latency measurement records both endpoint names, clock-domain IDs, measurement method, resolution, and uncertainty. A scalar duration is valid only when endpoints share a monotonic clock domain or a documented synchronization method bounds cross-clock error. Cross-process or source-to-Chronos measurements without bounded clock comparability are reported as apparent latency with uncertainty, not as precise processing latency.

End-to-end latency is tied to a causal path and is not necessarily the sum of all segment aggregates: branches may run concurrently, queue time may overlap, and percentile addition is invalid. Replay logical time is not performance time; benchmark wall/monotonic measurements are recorded separately and cannot influence replay decisions.

### Ordering model

Chronos does not claim one total order across all external facts. It establishes:

1. **Source order**, where the source provides a sequence within a documented scope.
2. **Capture order**, assigned monotonically within a Chronos capture partition.
3. **Normalized stream order**, assigned within a market stream and epoch after validation.
4. **Run input order**, a deterministic total order of market, reference, and control inputs actually dispatched within one run.
5. **State lineage order**, a partial order over complete vectors of consumed stream cursors.
6. **Causal order**, expressed by typed references between derived facts.

A sequence number is meaningless without its stream and epoch. An epoch changes when continuity cannot be proven, such as reconnect without resumable sequence, snapshot reset, or source-session reset.

Cross-stream consumers must define their synchronization and deterministic merge policy. They may use capture order, receive order with a stable tie-breaker, event-time windows with watermarks, or another declared rule, but may not imply exact global simultaneity. The policy and its version are run provenance.

`StateLineage` is a vector clock-like cut, not a claim that source streams share a clock. For lineage A and B, A precedes or equals B only when every cursor in A is less than or equal to the corresponding cursor in B within the same stream epoch and A's run-input position is not later. Otherwise the cuts are concurrent or incomparable. A market-state snapshot is valid only with its full lineage vector.

### Ordered control events and effective positions

All accepted behavior-changing commands emit control events on a dedicated run-control stream. Control events include run start/pause/resume/stop, strategy enable/disable, parameter/configuration activation, risk-limit activation, kill-switch changes, mode-authorized approval changes, and reset initiation/completion.

The run-input dispatcher merges control events with normalized market/reference events and assigns each accepted input a monotonic `run_input_sequence`. A control event's `effective_position` is the first run-input sequence processed under the new behavior:

- inputs with `run_input_sequence < effective_position` use the prior configuration;
- the control event is applied atomically before the first input at `effective_position`;
- inputs at or after that position use the new configuration until superseded.

When a command requests “now,” the control authority chooses and records the next legal effective position after serialization; wall-clock receipt alone never determines effect. Scheduled event-time changes must first be resolved by the run's ordering/watermark policy into a concrete effective position. Every market-state lineage includes the consumed run-control cursor and active configuration epoch, making feature and decision replay faithful to the original control timing.

### Gaps, duplicates, and late events

- Duplicate source delivery is retained at capture but deduplicated before duplicate economic or state effect.
- A forward sequence gap moves the affected stream or instrument into a non-consumable `gapped` or `recovering` state until the contractually valid recovery procedure completes.
- An out-of-order event is buffered only within an explicitly bounded policy. Otherwise it is quarantined or triggers recovery.
- A late event cannot be inserted retroactively into already published live state as if it had arrived on time. It produces a correction/recovery path or is included only in a new replay run.
- Snapshot application establishes a new state lineage and must declare the sequence from which deltas are valid.
- Sequence wraparound or reset is handled only according to the venue contract and starts a new epoch when continuity cannot be established.

## Determinism and execution modes

### Replay classes

The term replay must name one of these input contracts:

| Replay class | Authoritative input and ordering | Intended use | Required caveat |
|---|---|---|---|
| **Faithful capture-order replay** | Captured source and control envelopes in recorded capture/run-input order, preserving gaps, duplicates, arrival timing metadata, and original normalizer/control versions | Incident reproduction and live-behavior fidelity | Reproduces what Chronos observed, not a corrected market chronology |
| **Normalized-fact replay** | Previously accepted normalized and control events with their original stream cursors, effective positions, and deterministic merge manifest | Fast deterministic engine, strategy, portfolio, risk, and paper tests | Does not retest source decoding or normalization |
| **Raw re-normalization replay** | Immutable source events decoded again by a selected normalizer/reference-data version, producing a new normalized lineage | Parser/schema upgrades and normalization comparison | Results are a new dataset/run and must not overwrite original facts |
| **Corrected event-time research replay** | A separately versioned research dataset reordered/corrected under an explicit event-time, watermark, deduplication, and repair policy | Research on an estimated market chronology | Is not faithful reproduction and must not be used to claim live-observed behavior |

Every run manifest names exactly one replay class or a live input mode. Comparisons across replay classes identify the class as an experimental variable. Corrected research data is never silently substituted for faithful capture.

### Deterministic scope

Given all of the following:

- the same ordered normalized event set and stream epochs;
- the same complete stream-cursor vectors, run-input ordering, control events, and effective positions;
- the same canonical-instrument, listing, and reference-data versions;
- the same strategy, feature, portfolio, risk, execution-simulation, and accounting versions;
- the same immutable configuration and random seed, if a seeded model is explicitly allowed;
- the same initial snapshots or empty initial state;
- the same declared arithmetic, rounding, and tie-breaking rules;
- the same logical-clock policy;

Chronos replay must produce semantically identical:

- market-state view identities, complete lineages, and content;
- feature observations;
- strategy evaluations, emitted signals/abstentions, and explanations;
- exactly one actionable/hold trade recommendation per valid signal;
- target positions;
- risk decisions;
- reservation outcomes, accepted reservations, executable paper order intents, orders, and fills;
- ledger entries, positions, and P&L.

Semantically identical means all domain values and causal relationships match. Host-specific processing timestamps, runtime metrics, and opaque IDs explicitly defined as random may differ and are excluded from semantic comparison. The preferred design minimizes such exclusions.

### Sources of nondeterminism

Implicit nondeterminism is prohibited in replay-domain logic. This includes:

- host wall-clock reads;
- unseeded randomness;
- iteration over unordered collections where order affects results;
- race-dependent decision ordering;
- locale- or host-dependent parsing;
- unspecified floating-point reduction order;
- hidden network or database reads;
- configuration that can change during a run without an effective event;
- dependence on current code or reference data rather than run-pinned versions.

If stochastic simulation is later introduced, its pseudorandom algorithm, seed, draw order, and model version become run provenance.

### Mode semantics

| Mode | Permitted outputs | Prohibited boundary |
|---|---|---|
| Replay analysis | State, features, strategy evaluations, signals/abstentions, recommendations, and proposed targets under the named replay class | No executable intent unless the run also declares paper simulation |
| Backtest/paper replay | Deterministic risk decisions, reservation outcomes, reservations, executable paper intents, paper orders/fills, and accounting | No live adapter or human-approval workflow |
| Live read-only | Live state, strategy evaluations, valid signals/abstentions, trade recommendations, and proposed targets from actionable recommendations | Hold recommendations stop before target; no authoritative risk decision, reservation, executable intent, paper/live order, or fill |
| Live paper | Authoritative paper risk decisions, reservation outcomes/reservations, executable paper intents, simulated orders/fills, and accounting | No live adapter and no human-approval workflow; recommendations remain distinct from intents |
| Human-approved live | Live risk decisions and reservation outcomes/reservations; executable live intent only after valid HumanExecutionApproval | No executable intent or order while the reservation/workflow awaits human approval |
| Guarded automated live | Live intents and orders only within an explicit automation policy and reservations | No action outside guardrails; mode remains explicitly optional |

The same domain terms retain the same meaning in every mode. Mode affects allowed transitions and evidence quality, not vocabulary.

### Run lifecycle and reset

A run has one immutable mode, input/replay class, manifest identity, and run ID. Its lifecycle is event-driven:

```text
Created -> Initialized -> Running <-> Paused -> Stopping -> Completed
                         |    |           |
                         |    +----------> Failed
                         +---------------> Aborted
```

- `Initialized` means versions, initial state, input manifests, and required authorities are resolved but no run input has been applied.
- `Paused` stops new strategy/portfolio progression at a recorded effective position; capture and safety-critical order/fill/accounting handling may continue according to mode policy.
- `Completed` is a clean terminal state with final lineage and projections.
- `Failed` records an invariant, dependency, or recovery failure; `Aborted` records an explicit operator/system termination. Neither is silently resumable.
- Resume within the same run is allowed only from a proven checkpoint plus complete tail inputs and emits ordered resume control events. Otherwise recovery creates a child run with explicit parent and recovery provenance.

Reset never erases or rewinds a run. A reset command closes or aborts the current run according to policy and creates a new run with a new ID, initial-state policy, parent-run reference, reset reason, and reset control events. Resetting a paper session creates a new paper account/ledger scope or explicit opening-balance transaction; it never deletes prior orders, fills, ledger entries, or P&L. Market-state, strategy, portfolio, and UI projections reset only through the new run's declared initial state. Run mode cannot change through reset-in-place.

## Ownership boundaries

Each domain fact has one authoritative owner. Other components consume facts or build projections; they do not mutate another owner's state.

| Boundary | Owns | Must not own |
|---|---|---|
| Canonical instrument and listing authority | Canonical identities, listing mappings, economic terms, venue rules, and effective reference-data versions | Market observations, strategy equivalence assumptions |
| Source adapter and capture | Connectivity, framing, raw payload capture, source metadata, ingress health | Venue-neutral interpretation, strategy logic |
| Stream sequencing and run-input authority | Stream epochs/cursors, deterministic merge policy, run-input sequence, control-event effective positions | Domain interpretation, strategy decisions |
| Normalization | Decoding, validation, canonical event mapping, source lineage | Market-state mutation, inferred trading decisions |
| Market state | Sequenced application, complete lineage vectors, L2 state, freshness, synchronization, immutable state views | Feature meaning, strategy thresholds |
| Feature calculation | Reproducible measured observations and windows | Trade recommendations, portfolio sizing |
| Strategy runtime | StrategyEvaluation outcomes, valid signals, abstention reasons, and signal explanations | Recommendations, capital allocation, risk authorization, order placement |
| External-observation authority | Later external-source normalization, quality, correction, and provenance | Market-book mutation, opportunity ranking |
| Opportunity and resolution authority | Later opportunities derived from valid signals, opportunity legs, candidate eligibility, ranking, and listing resolution | Strategy evaluation, portfolio sizing, risk approval, execution authorization |
| Portfolio construction | Signal aggregation, sizing, absolute target positions | Final risk approval, venue order mechanics |
| Risk-policy authority | Limits, health gates, approved/modified/rejected targets, kill switch policy | Alpha generation, fill invention, unsynchronized capacity allocation |
| Exposure and reservation authority | Serialized projected exposure, reservation requests/outcomes, reservations, release/consumption, and risk sequence | Issuing or modifying RiskDecisions, strategy assessment, venue fill invention |
| Execution planning | Translation of approved targets, reservations, and approvals into executable order intents and execution constraints | Risk-policy bypass, accounting mutation |
| Execution-group authority | Coordination and partial-completion policy for related intents/orders | Claiming unsupported venue atomicity, bypassing per-leg risk |
| Paper broker | Simulated order state, model-versioned acknowledgement/fill outcomes | Presenting simulation as venue truth |
| Venue execution adapter | Live submission, external identifiers, acknowledgement/cancellation/fill ingestion | Strategy or portfolio decisions, silent retry of ambiguous orders |
| Ledger and accounting | Balanced postings, positions, cost basis, cash, fees, and P&L | Market-state, strategy decisions, or reconciliation workflow state |
| Mark and valuation authority | Mark-source selection, mark quality/freshness, valuation policy and P&L projections | Editing ledger facts or inventing fills |
| Reconciliation authority | Comparison with paper/venue evidence, discrepancy state, correction workflow | Rewriting source, order, fill, or ledger history |
| Run and configuration authority | Run lifecycle, manifests, configuration epochs, ordered control events, reset lineage | Direct mutation of another authority's state |
| Dataset and replay authority | Capture/dataset manifests, replay class, correction policy, run-input reconstruction | Claiming corrected research replay is faithful capture |
| Recommendation authority | Exactly one actionable/hold TradeRecommendation per valid StrategySignal | Recommendations for abstentions, portfolio-specific projections, risk approval, reservation, executable intent, or state mutation |
| Query-model authority | Disposable operator-facing projections from authoritative facts, including source position, age, completeness, and mode | Treating projections as authoritative state or mutating domain facts |
| Operator identity and approval authority | Authenticated actor identity and HumanExecutionApproval/rejection/revocation/expiry facts for human-approved live execution | Live-paper approval workflow, strategy assessment, silent execution authorization |
| Observability authority | Latency-point/segment definitions, metrics, traces, health projections, and measurement uncertainty | Changing domain outcomes or presenting incomparable clocks as precise latency |
| Audit and provenance | Immutable causal history and actor/configuration attribution | Replacing authoritative domain stores |
| Operations console | Presentation, controls, operator commands, and human-approval command submission | Owning read models, approval facts, or direct mutation of engine, risk, order, or ledger state |

Operator actions enter through typed commands and produce audit facts. A UI button never directly edits a position, order status, risk decision, or market-state value.

The latency-critical processing boundary may combine several owners in one process, and local-first deployment may be a modular monolith. Logical ownership does not require microservices.

## Logical durability and recovery ownership

Logical durability means an accepted fact or decision remains reconstructable and auditable according to its phase-owned loss policy. It does not mean every hot-path transition performs synchronous storage I/O before downstream processing. Persistence strategy, batching, replication, flush cadence, snapshots, and recovery-point objectives belong to event-infrastructure, execution, accounting, and observability planning.

The domain constraints are:

- Source capture loss is permitted only within a declared capture recovery-point policy. Any unprovable continuity marks the affected stream/run incomplete or gapped; Chronos may not present the result as faithful capture.
- Normalized events, market state, features, signals, targets, and read models may be reconstructed from retained authoritative inputs and versioned policies. Their materialization may be lost if rebuild evidence proves semantic equivalence.
- Accepted control events and their effective positions must be recoverable before a trade-capable run can be represented as faithfully replayable.
- In execution-capable modes, the accepted ReservationOutcome/reservation and, for human-approved live only, HumanExecutionApproval must be recoverable before or atomically with making an intent executable. The mechanism need not be synchronous disk I/O, but a crash must not permit unreserved or unapproved live execution or duplicate capacity use.
- External order submissions, ambiguous outcomes, fills, and accounting facts may not be silently lost. Each phase must define write-ahead, idempotency, reconciliation, and maximum permitted recovery window sufficient to preserve worst-case exposure and ledger correctness.
- Optional telemetry and UI projection loss may be tolerated under observability-owned limits, but loss must not remove the authoritative causal/audit chain or alter domain decisions.
- A run recovered beyond its declared loss bound is marked incomplete, failed, or non-faithful; it is not silently continued as equivalent.

Each later phase must publish a durability matrix naming its authoritative records, permitted loss window, reconstruction source, recovery procedure, and evidence test. This document intentionally does not prescribe storage technology or a universal synchronous-commit boundary.

## Cross-domain invariants

### Data and market-state invariants

1. Every normalized event is traceable to its source event or explicitly identified synthetic/administrative origin.
2. No invalid, duplicate, or unsequenced event has an undeclared effect on market state.
3. A published market-state view carries a complete lineage vector for all consumed book, trade, reference, market-control, and run-control streams; no omitted cursor may hide an input dependency.
4. An L2 book contains at most one aggregate quantity per side and price level; quantities are non-negative; zero quantity removes a level.
5. A consumable continuous-market book is not crossed unless the venue contract explicitly permits that state and the state status reflects it.
6. Listing tick, quantity, and minimum rules are evaluated against the listing-definition version effective for the event.
7. A gapped, stale beyond policy, invalid, or recovering required state produces an `abstained` StrategyEvaluation and no StrategySignal from a trade-capable strategy. Only explicitly non-tradeable diagnostic observations may continue.

### Observation and strategy invariants

8. Every feature observation identifies the exact complete market-state lineage or lineage-bounded input window from which it was derived.
9. No strategy evaluates future events relative to its decision point.
10. Every completed strategy invocation emits exactly one StrategyEvaluation whose outcome is either `signal_emitted` with exactly one valid StrategySignal or `abstained` with no StrategySignal.
11. Every signal identifies its StrategyEvaluation, strategy instance, input observations, validity, and explanation; abstention/degraded status is never encoded as a signal.
12. Missing or invalid required features cause an `abstained` StrategyEvaluation; they are not silently coerced into valid inputs.
12a. Diagnostic observations cannot be accepted as valid feature inputs, signals, recommendations, targets, or risk evidence.
12b. Every influential external observation is present in causal provenance and explanation factors for each affected StrategyEvaluation/Signal, Opportunity, OpportunityResolution where applicable, and TradeRecommendation.
12c. Every valid StrategySignal has exactly one TradeRecommendation; an abstained StrategyEvaluation has none.
12d. Every TradeRecommendation is either actionable with non-zero proposed delta or hold with exactly zero delta and an explicit reason. Hold recommendations cannot enter target or risk processing.
12e. A signal or recommendation does not directly create or mutate an order.

### Portfolio and risk invariants

13. A target position is absolute, portfolio-scoped, canonical-instrument/listing-scoped as declared, time-bound, and traceable to actionable recommendations and their signals. Operator-authored execution targets are outside the initial Chronos lifecycle; if introduced later, they require a separately planned typed proposal path with equivalent risk, reservation, approval, and audit gates.
14. At most one target is current for a portfolio/instrument/target-policy scope; later targets supersede rather than mutate earlier targets.
15. Every executable order intent references one unexpired approving risk decision, one `accepted` ReservationOutcome, and one active reservation created at an authoritative risk sequence.
15a. In human-approved live mode, no executable order intent exists until a valid HumanExecutionApproval is linked; pending human approval is represented only by the reservation/workflow. Live paper never requires or enters human approval.
16. A modified risk decision can only reduce or constrain the requested risk under the declared policy; any increase requires a new target and evaluation.
17. Kill switch, stale-data guard, exposure limits, and operational mode are evaluated at the risk boundary and rechecked at the execution boundary where time can invalidate approval.
18. Rejection or inability to evaluate risk produces no executable order intent.
18a. The reservation authority emits exactly one `accepted`, `rejected`, or `stale` ReservationOutcome and never issues, modifies, or replaces a RiskDecision. A stale outcome returns to the risk-policy authority for a new decision.
18b. Projected exposure includes worst-case open, unknown, reserved, and grouped execution effects; concurrent approvals are serialized so capacity cannot be double-consumed.
18c. Reservation consumption, transfer, expiry, and release are authoritative events and never inferred solely from a lagging UI or portfolio projection.

### Execution invariants

19. Every order is caused by one authorized executable order intent, except explicitly typed reconciliation/import records that can never initiate execution.
20. The cumulative filled quantity of an order cannot exceed its effective order quantity, accounting for valid corrections under venue rules.
21. Order lifecycle transitions follow the versioned state machine; terminal states do not become active without an explicit correction/reconciliation process.
22. Duplicate command or event delivery does not duplicate order submission, fill effect, or ledger effect.
23. Unknown external order outcome blocks unsafe retry and contributes to worst-case exposure until reconciled.
24. Live order submission is impossible in modes that do not permit it; human-approved live additionally requires valid HumanExecutionApproval before the executable intent exists.
24a. A recommendation, live read-only target, or executable paper intent can never be routed as an executable live intent.
24b. Each execution group declares whether partial completion is acceptable and how residual exposure is handled; grouping does not weaken per-order or aggregate risk limits.

### Accounting invariants

25. Every economically accepted fill posts exactly one idempotent balanced ledger transaction, with later corrections posted as linked compensating transactions.
26. Position, balances, and P&L are derived from ledger facts and declared marks; they are never authoritative manual fields.
27. Gross P&L, fees/funding, and net P&L remain separately explainable.
28. A valuation identifies its mark, mark time, accounting currency, and policy version.
29. Account projections reconcile to authoritative paper-broker or venue evidence within declared tolerances; discrepancies are explicit states.

### Audit and control invariants

30. Every material decision and state-changing command has actor, run, mode, configuration, version, time, cause, and outcome.
31. Historical facts are append-only. Corrections and supersession are explicit linked facts.
32. Configuration changes that affect domain behavior are versioned and take effect through a recorded boundary event.
33. No secret value is copied into domain events, audit records, logs, datasets, or explanations.
34. Every accepted behavior-changing command has exactly one ordered accepted/rejected control outcome; accepted changes take effect only at the recorded effective position.
35. A run reset creates a new run and preserves the prior run's complete history; no reset deletes or rewrites accepted facts.
36. Every event crossing an authority boundary preserves the minimum logical envelope, even when encoded or batched differently.

## Versioning and evolution

Chronos uses independent versions for concepts that evolve independently:

- source framing and capture format;
- logical event envelope and event taxonomy;
- normalized event schema;
- canonical-instrument, listing definitions, and symbol mappings;
- normalizer implementation;
- stream merge, control-event, and effective-position rules;
- market-state transition rules;
- feature definitions;
- strategy implementation and parameter schema;
- StrategyEvaluation outcome and TradeRecommendation/sizing policy;
- portfolio-construction policy;
- risk policy and limit set;
- projected-exposure, ReservationOutcome, and reservation state machines;
- order-intent and order state machines;
- opportunity-resolution and execution-group policy when introduced;
- paper fill model;
- ledger schema, accounting policy, and mark policy;
- run configuration and dataset manifest.

Every retained authoritative record, and every reconstructed artifact included in audit or comparison, carries the minimum version references required to interpret it without consulting mutable “current” configuration.

Version changes follow these rules:

1. Additive compatible changes may retain a major schema version when old consumers can preserve meaning.
2. Semantic changes require a new version even if field shape is unchanged.
3. Historical source events are never rewritten to fit a new schema.
4. Re-normalization creates a new normalized dataset or lineage; it does not overwrite prior normalized facts.
5. Strategy results from different strategy, feature, data, cost, or accounting versions are not directly comparable unless the experiment explicitly controls those differences.
6. Snapshots identify the complete stream-cursor vector, run-input position, active control/configuration position, and all schema/policy versions required to validate them.
7. Unknown fields may be preserved for forward compatibility, but unknown semantics cannot be treated as understood.
8. A migration must prove preservation of domain invariants and supply a rollback or rebuild path.

Version selection for a run is immutable after the run starts. A live configuration change closes or advances the affected configuration epoch and is represented as an effective event so the run remains replayable.

## Failure semantics

### Failure classes

| Class | Meaning | Required domain response |
|---|---|---|
| Invalid input | Payload or command violates its contract | Reject or quarantine; preserve evidence; no undeclared state effect |
| Duplicate | Fact or command was previously accepted | Return/record idempotent outcome; no duplicate economic effect |
| Gap or ordering failure | Continuity cannot be proven | Mark affected state non-consumable; recover from valid snapshot/resume point |
| Stale data | Freshness exceeds policy | Mark stale; emit abstained StrategyEvaluation and no StrategySignal for trade-capable evaluation; permit only labeled diagnostic observations; continue capture where safe |
| Dependency unavailable | Required component or external system cannot answer | Emit unavailable/degraded status; fail closed for risk and execution |
| Deadline or backpressure breach | Processing cannot meet bounded capacity/latency policy | Expose overload, shed only declared non-critical work, and prevent stale decisions |
| Domain rejection | Valid request is disallowed by policy or state | Emit immutable rejection with reason codes |
| Ambiguous external outcome | Submission may have succeeded but cannot be confirmed | Mark order unknown; reconcile; block unsafe retry |
| Accounting discrepancy | Internal projection differs from authoritative evidence | Freeze affected execution scope as policy requires; reconcile explicitly |
| Internal invariant violation | Chronos detects impossible state | Stop or isolate affected scope, preserve diagnostics, and require recovery from known-good facts |

### General failure rules

- No exception, timeout, parse error, queue overflow, reconnect, or process restart may be converted into a plausible domain success.
- Market-data failure isolates the affected stream/instruments where possible; it need not stop unrelated streams.
- Risk and live execution fail closed. Monitoring and capture may continue in degraded mode.
- Paper models expose assumptions and degraded confidence rather than presenting simulation as venue truth.
- Recovery resumes from a proven event position or authoritative reconciliation point. Guessing continuity is prohibited.
- Recovery restores the complete state-lineage vector and ordered control position; restoring only a book sequence is insufficient when trade/reference/control streams affect decisions.
- Restart recovery must distinguish durable accepted facts from in-flight work and safely replay idempotent transitions.
- Poison inputs are quarantined with bounded retry; they cannot permanently block capture of later source evidence without an explicit stream policy.
- Backpressure policy prioritizes source capture, safety, order/fill handling, and ledger integrity over UI updates or optional analytics.
- Operator overrides are typed, permissioned, reasoned, and audited. Overrides cannot rewrite historical facts or bypass hard safety controls that are declared non-overridable.

## Auditability and provenance

Chronos must answer, from retained evidence:

- What source fact entered the system, exactly as captured?
- How was it normalized, under which schema and code version?
- Which complete book/trade/reference/control cursor cut and control configuration did a feature observe?
- Why did a StrategyEvaluation emit one signal or abstain, and which inputs—including external observations—contributed?
- Which single actionable/hold TradeRecommendation was produced for each valid signal, and why?
- Which actionable recommendations and portfolio state produced a target?
- Which rules approved, reduced, or rejected that target?
- Which projected-exposure risk sequence and reservation prevented competing use of the same capacity?
- What accepted/rejected/stale ReservationOutcome occurred without changing the RiskDecision?
- In human-approved live mode, who supplied HumanExecutionApproval, and who or what created the executable order intent against which reservation?
- What orders were submitted, acknowledged, cancelled, rejected, or left unknown?
- Which fills and fees produced each ledger transaction?
- How were position and P&L calculated and marked?
- Which configuration, dataset, binaries, policies, and clock assumptions defined the run?
- Where did latency accrue along the causal path?

### Required causal chain

Material records carry typed causal references sufficient to traverse:

```text
source_event_id
  -> normalized_event_id
  -> market_state_view_id + complete StateLineage
  -> feature_observation_id
  -> strategy_evaluation_id
  -> signal_id
  -> recommendation_id
  -> target_position_id
  -> risk_decision_id
  -> projected_exposure(risk_scope_id, risk_sequence)
  -> reservation_outcome_id
  -> reservation_id
  -> execution_approval_id (human-approved live only)
  -> order_intent_id
  -> order_id
  -> fill_id
  -> ledger_transaction_id
  -> position/P&L projection version
```

One-to-many and many-to-one relationships are expected. The chain must not be forced into a single correlation ID that loses causal structure.

For an abstained StrategyEvaluation, the chain terminates at `strategy_evaluation_id`. For a hold TradeRecommendation, it terminates at `recommendation_id`. For rejected/stale reservation, it terminates at `reservation_outcome_id` unless a later workflow starts a new risk-policy evaluation with explicit causation.

When external observations influence any evaluation, signal, opportunity, resolution, or recommendation, their `external_observation_id` values join the causal chain at every affected record and remain visible in explanation factors; a downstream explanation cannot collapse them into an unattributed generic score.

### Run manifest and terminal attestation

Every run has an immutable initialization manifest created before processing, containing at minimum:

- run ID, mode, initial status, and parent run where applicable;
- run lifecycle/control stream, initial effective position, and reset/recovery relationship;
- dataset/capture identity, replay class, ordered partitions, deterministic merge policy, and correction policy where applicable;
- executable and component versions;
- event-envelope/taxonomy versions, event schemas, canonical-instrument/listing definitions, and complete initial stream cursors;
- strategy definitions, parameters, StrategyEvaluation/recommendation policies, feature versions, and external-observation datasets/schemas where used;
- portfolio, risk, execution, cost, accounting, and mark policies;
- initial state and snapshot references;
- random algorithm and seeds where allowed;
- clock policy and environment metadata relevant to reproducibility;
- latency-point/segment definitions and clock-domain uncertainty;
- operator identity and approved controls where applicable.

When a run terminates, a separate immutable terminal attestation records terminal status, final effective position, terminal lineage, completion/failure reason, final semantic checksums, and the initialization-manifest identity. Neither artifact is mutated after publication.

Audit retention may use tiered storage, but referential integrity and the ability to reconstruct material decisions must be preserved. Sensitive credentials are referenced by secret identity/version, never recorded as values.

## Testing expectations

The domain model is not considered implemented because types compile. Evidence must cover semantics across the complete lifecycle.

### Contract and unit evidence

- Schema tests for required fields, units, enum evolution, unknown fields, and version compatibility.
- Envelope/taxonomy tests proving commands cannot be consumed as accepted events and every event reconstructs its logical envelope after batching/serialization.
- Exact arithmetic, rounding, tick-size, quantity-step, and boundary tests.
- State-machine transition tests for run lifecycle, stream health, control effective positions, reservation outcomes/reservations, executable intent, human-approved-live approval, order, and reconciliation states.
- Cardinality tests proving every StrategyEvaluation has exactly one emitted-signal-or-abstention outcome and every valid StrategySignal has exactly one actionable-or-hold TradeRecommendation.
- Invariant tests for L2 books, target/risk relationships, fill quantities, balanced ledger transactions, and P&L decomposition.
- Clock and timestamp tests that prevent incompatible clock arithmetic.

### Replay and determinism evidence

- Golden fixtures for all four replay classes, with manifests that prove faithful capture-order and corrected event-time runs are distinguishable.
- Golden replay fixtures from raw source capture through normalized events and market-state checksums keyed by complete stream-cursor vectors.
- End-to-end golden runs through features, strategy evaluations, signals/abstentions, recommendations, actionable targets, risk, reservation outcomes, paper fills, ledger, positions, and P&L.
- Repeated runs with identical manifests producing semantically identical outputs.
- Runs under different thread scheduling and supported host environments to detect hidden nondeterminism.
- Snapshot-plus-tail replay producing the same state as replay from origin.
- Control-event boundary tests proving the event immediately before and at each effective position uses the intended configuration.
- Raw re-normalization tests proving a new lineage is created without overwriting original normalized facts.
- Proven absence of look-ahead in feature windows and strategy evaluation.

### Failure and recovery evidence

- Duplicate, gap, out-of-order, late-event, reconnect, snapshot-reset, malformed-payload, and stale-feed scenarios.
- Stale-feed tests proving trade-capable strategies emit an abstained StrategyEvaluation and no StrategySignal while labeled diagnostic observations may continue without reaching recommendations or targets.
- Crash/restart at each phase-declared durability boundary, including permitted-loss cases, with explicit faithful/incomplete status and no duplicate economic effect.
- Queue saturation and bounded-backpressure tests that demonstrate safety priorities.
- Unknown order outcome, duplicate acknowledgement, duplicate fill, partial fill, cancel/fill race, correction, and reconciliation scenarios.
- Fail-closed tests for unavailable risk inputs, stale market state, kill switch, and expired approval.
- Concurrent target tests proving serialized projected exposure and reservations prevent over-ordering, including unknown orders and execution groups; each reservation request emits accepted/rejected/stale without the reservation authority issuing a RiskDecision.
- Mode tests proving live paper creates no human-approval workflow, human-approved live creates no executable intent before HumanExecutionApproval, and executable intents never carry awaiting-approval state.
- Recommendation tests proving zero delta yields exactly one hold recommendation and no target/risk request, while only actionable recommendations can contribute to targets.
- External-observation tests proving every influential observation remains in causal provenance and explanations for signals, opportunities/resolutions, and recommendations.
- Run pause/resume/reset tests proving effective-position behavior and preservation of prior run/ledger history.
- Ledger rebuild and venue/paper-broker reconciliation from authoritative evidence.

### Property and model-based evidence

- Generated event sequences maintain book invariants or enter an explicit invalid/recovery state.
- Generated order lifecycle sequences permit only valid state transitions.
- Generated fills and corrections preserve quantity and accounting invariants.
- Ledger replay always produces balanced transactions and deterministic projections.
- Target and risk policies never authorize exposure beyond their declared limits.
- Generated StrategyEvaluation outcomes never encode abstention as a StrategySignal, and generated valid signals never have zero or multiple TradeRecommendations.
- Generated cross-stream cursor vectors never publish a state view with an omitted dependency or impossible cursor advance.
- Generated reservation/order/fill interleavings never consume capacity twice or release unknown-order exposure early.

### Performance evidence

- Named latency points and segments from receive through StrategyEvaluation, recommendation, serialized risk/reservation outcome, order submission, fill processing, and accounting where applicable.
- Clock-domain tests reject precise duration calculation across incomparable clocks and retain uncertainty for synchronized cross-clock measurements.
- Throughput, queue depth, allocation, and tail-latency distributions under representative and overload workloads.
- Measurement overhead quantified and bounded.
- Correctness assertions remain enabled in benchmark validation runs even if expensive diagnostics are disabled in production hot paths.
- Performance regressions are evaluated against versioned datasets and manifests, not ad hoc traffic.

### Review evidence

- Domain terminology is checked across schemas, APIs, logs, metrics, UI labels, and later planning documents.
- Each owner demonstrates that it can reconstruct its state from authoritative inputs.
- Cross-boundary contract tests prove that consumers cannot mistake stale, invalid, rejected, unknown, or unavailable states for valid success.

## Explicit non-goals

This domain model does not:

- define a matching engine for operating an exchange;
- claim that Chronos market-data trades are fills attributable to Chronos;
- define ML training, model serving, or probabilistic strategy semantics;
- define broad multi-asset, OTC, derivatives-margin, options-greeks, or cross-venue-netting semantics for v1;
- require external observations, multi-leg opportunities, candidate-market ranking, or execution groups in V1;
- define unconstrained automated execution;
- define fund administration, investor reporting, tax accounting, regulatory reporting, or compliance case management;
- define GUI layout or make the GUI authoritative;
- prescribe microservices, Kubernetes, Kafka, a cloud platform, or any distributed topology;
- prescribe C++, Python, database, queue, or serialization implementation details;
- guarantee that paper fills are achievable live;
- treat backtest profitability as proof of executable alpha;
- permit manual editing of positions, fills, balances, or P&L as a shortcut;
- resolve ticket-level implementation scope, estimates, or build order.

## Decisions deferred

The following decisions are intentionally deferred to later planning leaves, but those decisions must preserve this model:

1. Concrete process and language boundaries between the latency-critical core, Python/control plane, and UI.
2. Wire and durable serialization formats and schema registry mechanics.
3. Append-only journal, snapshot store, dataset catalog, and retention technologies.
4. Exact Chronos ID format and whether derived replay IDs are content-addressed or namespaced deterministic IDs.
5. Initial venue and its precise stream, snapshot, resumption, and sequencing contracts.
6. Exact leaf event types beyond the minimum stable envelope and namespace taxonomy.
7. Cross-stream synchronization and watermark policy for multi-instrument or later multi-venue strategies.
8. Numeric representation, fixed-point scales, overflow policy, and rounding modes.
9. Feature materialization, caching, window-storage, and invalidation design.
10. Strategy plugin/runtime boundary, isolation, deployment, and latency budgets.
11. Signal aggregation and target-position construction policies.
12. Risk-limit hierarchy, reservation granularity/expiry, risk serialization partitioning, and operator-override matrix; authoritative projected exposure and serialization themselves are not deferred.
13. Paper fill model, queue-position assumptions, slippage, fees, latency, and market-impact policy.
14. Cost-basis, settlement, funding, mark, and reporting-currency policies.
15. Order types, routing, slicing, replacement semantics, and first live execution adapter.
16. Human approval authentication, authorization, expiry, and dual-control requirements.
17. Exact phase-specific recovery-point objectives, permitted-loss windows, retention periods, persistence mechanisms, and operational service-level objectives.
18. External-observation source contracts and detailed multi-leg opportunity, candidate-ranking, Polymarket-resolution, and execution-group semantics.

A deferred decision is not permission for local components to invent incompatible meanings. Until resolved, boundaries expose the uncertainty explicitly or remain unimplemented.

## Evidence-based exit criteria

The following table is the cumulative implementation evidence contract derived from this planning leaf. These artifacts are not expected to exist during planning approval. Each becomes mandatory at the owning implementation phase named below, and remains subject to rerun in later cumulative reviews when affected. Planning approval requires that the evidence contract be complete and internally testable.

| Evidence ID and artifact | Owning phase | Required contents | Pass condition |
|---|---|---|---|
| **DM-E01 — PRD terminology matrix** | 01 Architecture | Every domain-bearing term in the current high-level and phase PRDs mapped to a canonical term, marked identical, narrowed, broadened, deprecated, or unresolved | Zero unresolved terms; StrategyEvaluation, abstention, StrategySignal, suggested paper trade/TradeRecommendation, hold, target, RiskDecision, ReservationOutcome, reservation, executable intent, market trade, and fill map without conflation |
| **DM-E02 — Canonical lifecycle trace** | 01 Architecture | Complete worked traces for signal-emitted/actionable/paper-fill, signal-emitted/hold, abstained evaluation, rejected risk, stale reservation, and human-approved-live waiting/approval paths | Every valid signal has exactly one recommendation; hold has zero delta and no target/risk; abstention has no signal/recommendation; reservation authority never emits a RiskDecision; executable intent never awaits approval |
| **DM-E03 — State-lineage fixture** | 04 Market data | A fixture with independent book, trade, reference, market-control, and run-control streams, including a snapshot, delta, public trade, and configuration change | Every published view contains the complete cursor vector and run-input/control position; omitting or advancing any cursor incorrectly causes the check to fail |
| **DM-E04 — Control effective-position fixture** | 03 Event infrastructure | Ordered inputs immediately before, at, and after strategy/risk configuration changes, pause/resume, kill switch, and reset | Each input uses exactly the configuration defined by its effective position; faithful replay reproduces the same outcomes |
| **DM-E05 — Replay-class manifest set** | 06 Research/backtesting | One manifest and expected checksum set for faithful capture-order, normalized-fact, raw re-normalization, and corrected event-time research replay | Replay class is explicit; raw re-normalization creates new lineage; corrected research replay cannot match or claim faithful-capture identity; repeated identical runs match semantic checksums |
| **DM-E06 — Envelope and taxonomy contract suite** | 03 Event infrastructure | Valid/invalid examples for commands and events across each minimum namespace, including unknown-version/type behavior and batched envelope reconstruction | Commands cannot masquerade as accepted events; required envelope semantics survive serialization/batching; unsafe unknown events fail closed |
| **DM-E07 — Identity and authority registry** | 01 Architecture | Every entity/value object and event namespace mapped to identity scope, authoritative owner, correction/supersession rule, and permitted consumers | Zero entities or state transitions have missing or multiple authoritative owners; canonical instrument and listing identities remain distinct |
| **DM-E08 — Mode-boundary matrix** | 01 Architecture; rerun 08 and 11B | Allowed and prohibited outputs for replay analysis, backtest/paper replay, live read-only, live paper, human-approved live, and guarded automated live | Live paper has no human-approval workflow; human-approved live creates no executable intent before HumanExecutionApproval; executable intents never await approval; paper intents cannot route live |
| **DM-E09 — Exposure serialization model check** | 07 Portfolio/risk | Concurrent targets, RiskDecisions, reservation requests/outcomes, open/unknown orders, reservations, fills, releases, and one multi-leg execution-group scenario | Each request has one accepted/rejected/stale ReservationOutcome; reservation authority emits no RiskDecision; worst-case exposure stays within limits and capacity is never double-used |
| **DM-E10 — Strategy/recommendation and stale-data fixture** | 05 Strategy runtime; extend 11A | Fresh, stale, gapped, recovering, invalid, actionable, and zero-delta cases, including influential external observations | Every evaluation is signal-emitted or abstained; abstention is not a signal; every valid signal has exactly one actionable/hold recommendation; hold stops before target; external influence appears in provenance and explanations |
| **DM-E11 — Run lifecycle/reset fixture** | 03 Event infrastructure; rerun 10A | Created-to-terminal transitions, pause/resume from a proven checkpoint, failed recovery, child recovery run, and paper-session reset | Invalid transitions are rejected; reset creates a new run/accounting scope and preserves all prior facts; resume reproduces lineage from checkpoint plus tail |
| **DM-E12 — Durability and recovery matrix** | 03 Event infrastructure; extend cumulatively | For each authority: authoritative records, synchronous/asynchronous handoff boundary, permitted loss window, reconstruction source, recovery action, and incomplete/non-faithful status rule | Every record class has an explicit phase owner and bounded policy; no policy permits silent loss of control effect, executable authorization, ambiguous order, fill, or ledger effect |
| **DM-E13 — Latency semantics fixture** | 02 Observability | Named points/segments with same-clock, synchronized cross-clock, incomparable-clock, parallel-path, and replay-logical-time cases | Precise durations are computed only for compatible clocks; uncertainty is retained; incomparable clocks are rejected; percentile segments are not arithmetically summed |
| **DM-E14 — Audit reconstruction drill** | 08 Paper execution; extend 11B | Reconstruction of abstention, hold recommendation, actionable paper fill, stale reservation, rejected risk, and human-approved-live approval from retained evidence | Each drill reaches source/control/external inputs, full lineage and explanations, versions, actor, RiskDecision, ReservationOutcome/reservation, approval where applicable, execution facts, ledger, mark, and P&L without mutable-current lookups |
| **DM-E15 — Future-extension compatibility review** | 11A External discovery | Schema/authority walkthrough for external observation, multi-leg opportunity, opportunity resolution, candidate ranking, canonical instrument/listing mapping, and execution group | External contributors remain explicit in opportunity/signal/recommendation explanations; concepts extend the lifecycle without changing V1 evaluation, recommendation, target, risk, reservation, intent, order, fill, or ledger meanings |
| **DM-E16 — Independent critique disposition** | Every planning leaf | Critique from a reviewer other than the author, with each finding classified and linked to a resolution or explicit deferral | Zero open material findings; every deferral names the owning later planning leaf and does not contradict an invariant |
| **DM-E17 — Cumulative phase compatibility checklist** | Every phase gate | For each later planning phase, explicit confirmation of adopted terms, envelopes, authorities, lineage, mode boundaries, and invariants | Zero redefinitions or incompatible ownership claims; any required semantic change is handled by an architecture decision and re-review |

Approval of this document fixes the semantic baseline for later planning. A later phase may extend the model, but changing an established meaning requires an explicit architecture decision and cumulative review of all affected planning leaves.
