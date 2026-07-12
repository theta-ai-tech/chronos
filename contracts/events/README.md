# contracts/events/

> **Submodule owner stub (M0.1).** M1.4 introduces the minimum logical event envelope.

- **Parent:** `contracts/`
- **Owner:** Inherits `contracts/` ownership.
- **Plane:** Inherits `contracts/` plane.
- **Language:** Inherits `contracts/` language policy.
- **Public API:** `chronos/contracts/event_envelope.hpp` and `chronos.event_envelope`
  expose the validated logical envelope and reserved event namespaces.
- **Failure semantics:** invalid type/version/reference/lineage combinations fail before
  authority admission; non-applicable fields remain explicitly absent.
- **Purpose:** Event contracts (immutable accepted/observed facts).
- **Accepted dependencies:** inherits `contracts/` rules (see parent README).
