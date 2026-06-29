# research/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Research / presentation
- **Language:** Python
- **Purpose:** Experiment registry, experiments, reports, dataset tooling.
- **Accepted dependencies:** Production domain paths and contracts/.
- **Must not depend on:** Being imported by production runtime targets.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`registry/`** — Authoritative experiment identity and lifecycle.
- **`experiments/`** — Experiment manifests and runs.
- **`reports/`** — Non-authoritative reports and notebooks.
- **`dataset_tools/`** — Dataset preparation and inspection tools.
