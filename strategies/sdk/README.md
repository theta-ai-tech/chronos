# strategies/sdk/

> **Submodule owner stub (M0.1).** Implemented in M5.2.

- **Parent:** `strategies/`
- **Owner:** Inherits `strategies/` ownership.
- **Plane:** Inherits `strategies/` plane.
- **Language:** Inherits `strategies/` language policy.
- **Public API:** `chronos/strategies/sdk/strategy.hpp`
- **Purpose:** C++ strategy SDK interface (deterministic, resource/deadline hooks).
- **Accepted dependencies:** inherits `strategies/` rules (see parent README).

The SDK is a capability boundary. `FeatureRuntime` issues an immutable
`AcceptedFeatureEvaluationCut`; `StrategyHost` admits that authority token and
maps only declared features into a non-constructible
`AcceptedStrategyInvocation`. Concrete strategies therefore cannot substitute
raw feature aggregates or import core runtime APIs.

The host charges the descriptor's fixed deterministic operation cost before
entering strategy code, checks the recorded logical deadline, zeroes and bounds
the supplied workspace, bounds explanation storage, and validates exactly one
terminal output against the descriptor. Strategy code cannot ignore these
controls because it receives neither the fuel counter nor unbounded storage.

Logical deadlines are part of the recorded cut and are safe for semantic
decisions. Host deadlines and cancellation remain external operational
enforcement and cannot be observed by strategy code. Explanation factors must
be appended in deterministic rank order before exactly one terminal signal or
abstention draft is written; no output allocation is required by the SDK.

Native strategy targets must be created with `chronos_add_strategy`. The target
links only the SDK/options surface and runs source plus linked-symbol capability
checks that reject host clock, network, filesystem, environment/secret,
nondeterministic randomness, process output, and direct core/host access.
