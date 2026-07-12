# adapters/market_data/

> **Submodule owner stub (M0.1).** Implemented beginning in M2.1.

Contains concrete public market-data source adapters. The initial Bybit adapter
implements the shared SDK and a bounded public V5 WebSocket transport for L2
order-book and public-trade framing. Every framed ingress message can be handed
to the M2.3 immutable source-capture pipeline before control interpretation.
Semantic decoding remains an M3 concern.

M2.4 adds bounded reconnect coordination, unique capture-session identity per
attempt, capture-owned source-session epochs, scoped health, and explicit
gapped continuity after every unproven reconnect. It does not allocate the
normalized stream epochs owned by the later stream authority.

M2.5 adds a versioned immutable capture-dataset format. A writer accepts one
capture session/partition in contiguous capture order, synchronizes both files
and staging metadata before an atomic directory publish, synchronizes the
parent directory, and assigns the dataset a canonical SHA-256 identity. The
manifest retains the complete capture context and declares
`dataset_class=raw_source_capture` with `replay_admissible=false`; M3.4 owns the
replay-class manifest that can admit it to a run. The reader verifies file-size
bounds plus the manifest, file and payload
digests, record bounds, enum values, and sequence continuity before returning
records. Normalization and replay-class selection remain M3 concerns.

- **Parent:** `adapters/`
- **Owner:** Inherits `adapters/` ownership.
- **Plane:** Inherits `adapters/` plane.
- **Language:** Inherits `adapters/` language policy.
- **Public API:** `bybit_adapter.hpp`, `bybit_websocket.hpp`, and
  `websocket_transport.hpp`, `source_capture.hpp`, `reconnect.hpp`, and
  `capture_dataset.hpp` under
  `chronos/adapters/market_data/`
- **Purpose:** Venue market-data adapters (Bybit first) and capture.
- **Accepted dependencies:** inherits `adapters/` rules (see parent README).
