# adapters/market_data/

> **Submodule owner stub (M0.1).** Implemented beginning in M2.1.

Contains concrete public market-data source adapters. The initial Bybit adapter
implements the shared SDK and a bounded public V5 WebSocket transport for L2
order-book and public-trade framing. Semantic decoding remains an M3 concern.

- **Parent:** `adapters/`
- **Owner:** Inherits `adapters/` ownership.
- **Plane:** Inherits `adapters/` plane.
- **Language:** Inherits `adapters/` language policy.
- **Public API:** `bybit_adapter.hpp`, `bybit_websocket.hpp`, and
  `websocket_transport.hpp` under `chronos/adapters/market_data/`
- **Purpose:** Venue market-data adapters (Bybit first) and capture.
- **Accepted dependencies:** inherits `adapters/` rules (see parent README).
