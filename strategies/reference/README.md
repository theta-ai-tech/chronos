# strategies/reference/

> **Submodule owner stub (M0.1).** Implemented beginning in M5.3.

- **Parent:** `strategies/`
- **Owner:** Inherits `strategies/` ownership.
- **Plane:** Inherits `strategies/` plane.
- **Language:** Inherits `strategies/` language policy.
- **Public API:**
  `chronos/strategies/reference/imbalance_threshold_strategy.hpp`
- **Purpose:** Reference strategies used as conformance examples.
- **Accepted dependencies:** inherits `strategies/` rules (see parent README).

M5.3 provides one mechanical conformance strategy. It consumes exactly one
valid top-book imbalance observation and one positive scale-6 threshold. An
imbalance whose absolute value meets the threshold emits a positive or negative
signal draft with magnitude as strength; a value inside the threshold produces
a typed `NoDirectionalSignal` abstention, never a neutral or zero-strength
signal. The strategy uses fixed operation fuel, logical cut/deadline facts, and
two deterministic explanation factors. It has no host or external capabilities.
