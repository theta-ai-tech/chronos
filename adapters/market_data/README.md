# adapters/market_data/

Contains concrete public market-data source adapters. The initial Bybit adapter
implements the shared SDK and currently exposes its capability/lifecycle shell;
transport connectivity lands in M2.2.

- **Parent:** `adapters/`
- **Owner:** Inherits `adapters/` ownership.
- **Plane:** Inherits `adapters/` plane.
- **Language:** Inherits `adapters/` language policy.
- **Public API:** `chronos/adapters/market_data/bybit_adapter.hpp`
- **Purpose:** Venue market-data adapters (Bybit first) and capture.
- **Accepted dependencies:** inherits `adapters/` rules (see parent README).
