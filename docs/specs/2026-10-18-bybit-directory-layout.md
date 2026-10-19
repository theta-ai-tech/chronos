# Bybit directory layout

## Scope

Group existing Bybit public headers under
`adapters/market_data/include/chronos/adapters/market_data/bybit/` and their
implementations under `adapters/market_data/src/bybit/`. Remove the redundant
`bybit_` filename prefix inside those directories. Include the Bybit reconnect
coordinator, probes, and private decoder support header.

Shared transport, framing, capture, and dataset files remain at their current
locations. Preserve C++ symbols, namespaces, target names, dependency ownership,
and runtime behavior. In particular, normalization continues to compile the
Bybit decoder sources. This change does not introduce a common subscription
contract, split library targets, or add another exchange.

## Execution and acceptance

1. Move the files and update includes and CMake source paths.
2. Update the market-data README and check for stale active references.
3. Run `make m0-check`, including formatting, native tests, authority checks,
   and Python checks. Existing tests cover behavior; no new behavior is added.

Consumers include, for example,
`chronos/adapters/market_data/bybit/websocket.hpp`. Existing includes using the
old paths must migrate; no forwarding headers are retained.
