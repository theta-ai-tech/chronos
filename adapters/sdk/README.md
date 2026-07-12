# adapters/sdk/

> **Submodule owner stub (M0.1).** Implemented beginning in M2.1.

The C++ adapter SDK defines venue-neutral lifecycle, health, capability
negotiation, resource bounds, and source-adapter contracts. Concrete adapters
must publish a valid immutable capability manifest and pass negotiation before
activation.

- **Parent:** `adapters/`
- **Owner:** Inherits `adapters/` ownership.
- **Plane:** Inherits `adapters/` plane.
- **Language:** Inherits `adapters/` language policy.
- **Public API:** `chronos/adapters/sdk/adapter.hpp`
- **Purpose:** Adapter contracts: lifecycle, health, capability, capture handoff.
- **Accepted dependencies:** inherits `adapters/` rules (see parent README).
