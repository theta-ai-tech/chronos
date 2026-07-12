# adapters/sdk/

> **Submodule owner stub (M0.1).** Implemented beginning in M2.1.

The C++ adapter SDK defines venue-neutral lifecycle, health, capability
negotiation, resource bounds, and source-adapter contracts. Concrete adapters
must publish a valid immutable capability manifest and pass negotiation before
activation.

M2.3 adds the immutable `SourceEvent` capture contract, partition-local
sequencing, SHA-256 payload integrity, malformed/unsupported retention, and
bounded source-event handoff.

- **Parent:** `adapters/`
- **Owner:** Inherits `adapters/` ownership.
- **Plane:** Inherits `adapters/` plane.
- **Language:** Inherits `adapters/` language policy.
- **Public API:** `chronos/adapters/sdk/adapter.hpp` and
  `chronos/adapters/sdk/source_event.hpp`
- **Purpose:** Adapter contracts: lifecycle, health, capability, capture handoff.
- **Accepted dependencies:** inherits `adapters/` rules (see parent README).
