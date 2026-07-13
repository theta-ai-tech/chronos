# core/reference_data/

- **Owner:** Reference authority.
- **Plane:** Hot plane.
- **Language:** C++.
- **Purpose:** Immutable canonical-instrument and venue-listing definitions,
  effective-interval selection, and exact tick/step conversion.
- **Accepted dependencies:** `contracts/` only.
- **Must not depend on:** Adapters, mutable configuration aliases, storage, UI,
  or telemetry implementations.
- **Public API:** `chronos/core/reference_data/reference_data.hpp`.
- **Failure semantics:** Invalid definitions, ambiguous resolution, inactive or
  out-of-interval listings, and inexact numeric conversion fail explicitly.
- **Reconstruction source:** A run- or dataset-pinned immutable reference
  snapshot version.
