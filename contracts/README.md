# contracts/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Cross-cutting (canonical)
- **Language:** Generated/shared (C++ + Python bindings)
- **Purpose:** Canonical domain contracts: the minimum event envelope, value objects, commands, events, views, and conformance fixtures. The lowest, most stable layer.
- **Accepted dependencies:** Nothing. Canonical contracts depend on no adapter, database, web, UI, telemetry, or OS implementation.
- **Must not depend on:** Any infrastructure, adapter, or application code.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
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
