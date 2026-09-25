# accounting/

- **Owner / plane:** Hot/data plane
- **Language:** C++20
- **Purpose:** Immutable balanced ledger, derived positions and P&L, marks, and reconciliation.
- **Accepted dependencies:** contracts/ and narrow persistence ports.
- **Must not depend on:** Market-state, strategy, or UI logic; manual mutation of economic facts.
- **Public API:** `LedgerAuthority`, `derive_position`, and `value_position`.
- **Failure semantics:** Posting and valuation fail atomically with typed
  outcomes; unavailable evidence never becomes an invented economic value.
- **Reconstruction source:** Immutable balanced ledger transactions.

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`ledger/`** — Immutable balanced, idempotent postings.
- **`valuation/`** — Mark policy and unrealized valuation, separate from the ledger.
- **`reconciliation/`** — Comparison with paper/venue evidence and discrepancy lifecycle.
