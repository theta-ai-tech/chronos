# accounting/

> **Module owner stub (M0.1).** Public API, failure semantics, and reconstruction
> source are filled in as the owning issues land. Scaffold only — no logic yet.

- **Owner / plane:** Hot/data plane
- **Language:** C++ (hot postings) / shared
- **Purpose:** Immutable balanced ledger, derived positions and P&L, marks, and reconciliation.
- **Accepted dependencies:** contracts/ and narrow persistence ports.
- **Must not depend on:** Market-state, strategy, or UI logic; manual mutation of economic facts.
- **Public API:** _TBD_
- **Failure semantics:** _TBD_
- **Reconstruction source:** _TBD_

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`ledger/`** — Immutable balanced, idempotent postings.
- **`valuation/`** — Mark policy and unrealized valuation, separate from the ledger.
- **`reconciliation/`** — Comparison with paper/venue evidence and discrepancy lifecycle.
