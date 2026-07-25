# strategies/reference/

> **Submodule owner stub (M0.1).** Implemented beginning in M5.3.

- **Parent:** `strategies/`
- **Owner:** Inherits `strategies/` ownership.
- **Plane:** Inherits `strategies/` plane.
- **Language:** Inherits `strategies/` language policy.
- **Public API:** generated
  `chronos/strategies/generated/chronos_reference_strategies.hpp`
- **Purpose:** Reference strategies used as conformance examples.
- **Accepted dependencies:** inherits `strategies/` rules (see parent README).

M5.3 provides one strict JSON data manifest for the SDK's fixed
imbalance-threshold program. `StrategyHost` consumes exactly one valid top-book
imbalance observation and one positive scale-6 threshold under the accepted
definition. An absolute imbalance at or above the threshold emits a positive or
negative signal draft with magnitude as strength; a value inside the threshold
produces a typed `NoDirectionalSignal` abstention, never a neutral or
zero-strength signal. Trusted build generation supplies the fixed program from
stable versions, horizon, and two ranked factor identities. No reference-pack
authored C++ exists or runs during evaluation.
