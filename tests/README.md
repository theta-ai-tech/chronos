# tests/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Verification
- **Language:** Mixed
- **Purpose:** Layered test pyramid with replay and recovery as first-class layers.
- **Accepted dependencies:** All modules under test (may use privileged inspection helpers).
- **Must not depend on:** Being imported by production code.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`contract/`** — Producer/consumer conformance tests.
- **`integration/`** — Module integration tests.
- **`replay/`** — Deterministic replay and golden tests.
- **`recovery/`** — Failure, crash, and recovery tests.
- **`performance/`** — Benchmark correctness and soak tests.
- **`end_to_end/`** — Operator scenario tests.
