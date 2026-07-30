# core/portfolio/

This module owns deterministic single-portfolio construction. Its public
authority consumes an immutable portfolio snapshot and a bounded set of
authority-created trade recommendations, then terminates with an absolute
target, explicit no-change, or typed construction rejection.

`TargetPosition` is advisory and non-executable. Portfolio construction does
not approve risk, reserve exposure, create an order intent, simulate a fill,
or update accounting. The target's delta is explanatory only.

- **Public API:**
  `chronos/core/portfolio/portfolio_construction.hpp`
- **Target:** `chronos_portfolio`
- **Allowed dependencies:** shared contracts, the immutable recommendation
  API, and common build-policy targets.
- **Forbidden authority:** adapters, storage, telemetry, applications, UI,
  risk, reservations, execution planning, order lifecycle, fills, and ledger
  or P&L state.

Deferred ownership remains explicit: M6.2 owns risk decisions, M6.3 owns
serialized exposure and reservations, M6.4-M6.5 own paper intent and order
lifecycle, and M6.6-M6.7 own ledger, positions, and P&L. Until those accounting
authorities exist, starting exposure is accepted only when the snapshot labels
it as a configured paper-transition assumption.
