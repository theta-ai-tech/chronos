# applications/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Applications
- **Language:** Python / web
- **Purpose:** Operator- and developer-facing entrypoints.
- **Accepted dependencies:** The operations API schema and orchestration; never core internals.
- **Must not depend on:** Strategy, risk, execution, or accounting rules.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`local_api/`** — Local commands/queries/subscriptions for the console.
- **`console/`** — Operator web console (non-authoritative).
- **`cli/`** — Command-line entrypoints.
- **`replay_runner/`** — Deterministic replay entrypoint.
- **`benchmark_runner/`** — Benchmark profile runner (ADR-0002).
