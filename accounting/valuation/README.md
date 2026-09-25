# accounting/valuation/

> **Submodule owner stub (M0.1).** Implemented in M6.7.

- **Owner:** Accounting.
- **Plane:** Hot-data projection.
- **Language:** C++20.
- **Purpose:** Rebuild position, realized P&L, fees, and FIFO lot basis solely
  from immutable ledger transactions; apply an explicit mark to derive
  unrealized and total net P&L.
- **Public API:** `derive_position` and `value_position` in
  `chronos/accounting/valuation.hpp`.
- **Accepted dependencies:** `contracts/` and the immutable ledger read model.
- **Must not depend on:** Market-state, strategy, portfolio, risk, execution,
  or adapter implementations.

Compensated transactions are excluded according to the ledger reversal
relationship. Partial FIFO closes allocate the stored lot basis
deterministically and retain the final remainder, so quote units are not lost.
Marks are listing-, scale-, lineage-, quality-, and logical-cut-bound. Missing,
future, stale, or mismatched marks produce typed unavailability rather than a
numeric zero.
