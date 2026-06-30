# config/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Configuration
- **Language:** Schemas / data
- **Purpose:** Versioned configuration schemas, defaults, and examples.
- **Accepted dependencies:** Configuration-schema definitions.
- **Must not depend on:** Holding secret values.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`schemas/`** — Versioned configuration schemas.
- **`defaults/`** — Explicit versioned defaults.
- **`examples/`** — Example configurations (no secrets).
