# core/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Hot plane
- **Language:** Optimized C++ (ADR-0001)
- **Purpose:** Deterministic, allocation-controlled domain authorities on the time-sensitive path.
- **Accepted dependencies:** contracts/ and narrow ports (clock, persistence handoff, event publication).
- **Must not depend on:** Concrete venue, storage, web, UI, or telemetry-exporter implementations.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`reference_data/`** — Versioned canonical-instrument and listing authority,
  effective selection, and exact tick/step conversion.
- **`dispatch/`** — Run-input dispatch: single monotonic run_input_sequence and merge policy.
- **`market_state/`** — Listing-scoped L2 state, StateLineage cuts, immutable view publication.
- **`features/`** — Deterministic, versioned feature observations.
- **`strategy_runtime/`** — Strategy-instance lifecycle and StrategyEvaluation outcomes.
- **`recommendation/`** — Exactly one actionable/hold TradeRecommendation per valid signal.
- **`portfolio/`** — Portfolio aggregation and absolute target construction.
- **`risk/`** — Risk-policy decisions and serialized exposure/reservation.
- **`execution_planning/`** — Approved targets + reservations -> mode-bound executable intents.
