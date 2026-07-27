# core/recommendation/

> **Submodule owner stub (M0.1).** Implemented beginning in M5.5.

- **Parent:** `core/`
- **Owner:** Inherits `core/` ownership.
- **Plane:** Inherits `core/` plane.
- **Language:** Inherits `core/` language policy.
- **Public API:**
  `chronos/core/recommendation/recommendation.hpp`
- **Purpose:** Exactly one actionable/hold TradeRecommendation per valid signal.
- **Accepted dependencies:** contracts plus the immutable public
  `StrategyEvaluation`/`StrategySignal` outcome API; no portfolio, risk,
  execution, venue, storage, or telemetry state.

The M5.5 authority consumes only an immutable signal-emitting
`StrategyEvaluation` plus a versioned portfolio-neutral recommendation policy.
It returns one deterministic actionable or explicit hold recommendation. An
abstained evaluation has no recommendation, and a hold has zero indicative
exposure and is not eligible for target construction.

M5.6 adds bounded acceptance keyed by `StrategySignalId`. Exact retries return
the previously accepted recommendation; a conflicting second recommendation
for one signal fails closed, and capacity exhaustion is explicit. Acceptance
does not claim cardinality until it is finalized against the exact emitted
valid-signal identities. Missing, duplicate, or mismatched signal identities,
conflicts, or capacity exhaustion make that proof terminally incomplete.
