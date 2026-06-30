# strategies/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Strategy (plugin-like)
- **Language:** C++ (strategy SDK)
- **Purpose:** Concrete strategies, living outside the strategy runtime.
- **Accepted dependencies:** strategies/sdk and approved deterministic utilities only.
- **Must not depend on:** Network, filesystem, wall-clock, secret, or order-submission access.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`sdk/`** — C++ strategy SDK interface (deterministic, resource/deadline hooks).
- **`reference/`** — Reference strategies used as conformance examples.
- **`fixtures/`** — Strategy test fixtures.
