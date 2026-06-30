# runtime/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Control / data plane
- **Language:** Python (control) + storage adapters
- **Purpose:** Run orchestration, configuration, datasets, persistence, observability, audit.
- **Accepted dependencies:** contracts/ and the public APIs of owning authorities.
- **Must not depend on:** Direct mutation of another authority's internal state.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`run_control/`** — Run lifecycle, manifests, ordered control events, reset lineage.
- **`configuration/`** — Versioned config schemas, resolver, precedence, redaction.
- **`datasets/`** — Capture/dataset/replay manifests and ordered reconstruction.
- **`persistence/`** — Narrow persistence ports for owning authorities.
- **`observability/`** — Metric/trace/log/health schemas and evidence export.
- **`audit/`** — Causal-graph retention and actor/configuration attribution.
