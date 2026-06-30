# adapters/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Ingress / egress
- **Language:** C++ (capture hot path) / mixed
- **Purpose:** Concrete venue and source adapters plus the shared adapter SDK.
- **Accepted dependencies:** adapters/sdk and contracts/.
- **Must not depend on:** Strategy, portfolio, or risk logic.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`sdk/`** — Adapter contracts: lifecycle, health, capability, capture handoff.
- **`market_data/`** — Venue market-data adapters (Bybit first) and capture.
- **`external/`** — Deferred external-observation adapters (reserved).
- **`paper/`** — Paper broker: simulated order/fill outcomes.
- **`execution/`** — Deferred live execution adapters (reserved).
