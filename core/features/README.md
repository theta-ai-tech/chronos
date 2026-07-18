# core/features/

> **Submodule owner stub (M0.1).** Implemented beginning in M5.1.

- **Parent:** `core/`
- **Owner:** Inherits `core/` ownership.
- **Plane:** Inherits `core/` plane.
- **Language:** Inherits `core/` language policy.
- **Public API:** `chronos/core/features/feature_runtime.hpp`
- **Purpose:** Deterministic, versioned feature observations.
- **Accepted dependencies:** inherits `core/` rules (see parent README).

M5.1 introduces three concrete, immutable v1 single-listing definitions over
one exact accepted M4 bundle/member cut:

- top-level quantity imbalance is `(bid_qty - ask_qty) / (bid_qty + ask_qty)`
  at decimal scale 6, rounded to nearest with ties to even;
- microprice is `(ask_price * bid_qty + bid_price * ask_qty) /
  (bid_qty + ask_qty)`, in the input price definition and rounded to nearest
  with ties to even;
- spread is the exact `best_ask - best_bid` in the input price definition.

The definitions require a synchronized, fresh, proven two-sided book whose
shape is Normal or Locked. Every other quality, shape, completeness,
definition, quantity, or arithmetic condition produces a typed `unavailable`
evaluation containing no feature value. Valid and unavailable outcomes bind
the exact bundle, listing view, complete `StateLineage`, reference versions,
input checksums, and feature/implementation/arithmetic/identity versions.
