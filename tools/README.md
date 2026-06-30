# tools/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Tooling
- **Language:** Mixed
- **Purpose:** Development, schema, profiling, and release tooling.
- **Accepted dependencies:** Build and developer tooling needs.
- **Must not depend on:** Becoming a runtime dependency of production modules.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`development/`** — Local development helpers.
- **`schema/`** — Schema/code generation and drift detection.
- **`profiling/`** — Profiling helpers.
- **`release/`** — Release and packaging tooling.
