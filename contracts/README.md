# contracts/

> **Module owner stub (M0.1).** M1 establishes the first canonical contract surface.

- **Owner / plane:** Cross-cutting (canonical)
- **Language:** Generated/shared (C++ + Python bindings)
- **Purpose:** Canonical domain contracts: the minimum event envelope, value objects, commands, events, views, and conformance fixtures. The lowest, most stable layer.
- **Accepted dependencies:** Nothing. Canonical contracts depend on no adapter, database, web, UI, telemetry, or OS implementation.
- **Must not depend on:** Any infrastructure, adapter, or application code.
- **Public API:** C++ headers under `contracts/include/chronos/contracts/` and mirrored
  Python modules under `python/chronos/` define M1 fixed-point values, identities,
  lineage, the event envelope, and canonical serialization.
- **Failure semantics:** invalid values and messages fail explicitly before authority
  admission; C++ factories/codecs return `std::optional`, Python raises typed value errors,
  and the C boundary returns a stable status code.
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`domain/`** — Canonical envelopes, identities, and value-object semantics.
- **`commands/`** — Command contracts (intended transitions).
- **`events/`** — Event contracts (immutable accepted/observed facts).
- **`views/`** — Immutable view/snapshot contracts.
- **`conformance/`** — Fixtures and cross-boundary compatibility tests.
