# core/market_state/

> **Submodule owner stub (M0.1).** Implemented beginning in M4.2.

- **Parent:** `core/`
- **Owner:** Inherits `core/` ownership.
- **Plane:** Inherits `core/` plane.
- **Language:** Inherits `core/` language policy.
- **Public API:** `chronos/core/market_state/l2_book.hpp`,
  `chronos/core/market_state/listing_aux_state.hpp`
- **Purpose:** Listing-scoped L2 state, per-side completeness and exhaustion,
  derived top-of-book, StateLineage cuts, immutable view publication.
- **Auxiliary state:** Bounded recent trades plus orthogonal book synchronization,
  trade continuity/window completeness, and ordered logical-time freshness.
- **Accepted dependencies:** inherits `core/` rules (see parent README).
