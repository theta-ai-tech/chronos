# Chronos

The decision and research engine. Chronos records the market, replays it, and decides: features,
strategies, sizing, economic risk, paper fills and the front-office ledger. It holds no venue order
session. Orders are Tachys's job (`../tachys`); independent books and reconciliation are Archeion's
(`../archeion`).

> **Status:** the M6 paper loop is built and tested: one dataset in, strategy, sizing, risk, paper
> fill, ledger out. Capture exists as libraries and probes, not as a service. Next work is in
> [`docs/roadmap.md`](docs/roadmap.md).

## Layout

| Path | What lives here | Built as |
|---|---|---|
| `contracts/` | Ids, timestamps, fixed point, event envelope. The lowest layer | `chronos_contracts` |
| `adapters/sdk/` | Venue-neutral source event and adapter contract | `chronos_adapter_sdk` |
| `adapters/market_data/` | Bybit websocket, reconnect, source capture, sealed capture datasets, three probes | `chronos_bybit_adapter` |
| `adapters/paper/` | Paper broker: fills against the recorded book | `chronos_paper_broker` |
| `normalization/` | Bybit book and trade decoding into venue-neutral facts | `chronos_market_data_normalization` |
| `runtime/datasets/` | Replay providers over capture datasets | `chronos_replay_providers` |
| `runtime/strategies/` | Strategy runtime | `chronos_strategy_runtime` |
| `core/` | Market state, features, portfolio, risk, recommendation, execution planning | `chronos_core`, `chronos_features`, `chronos_portfolio`, `chronos_risk`, ... |
| `strategies/` | Strategy SDK and the one reference rule | `chronos_strategy_sdk`, `chronos_reference_strategies` |
| `accounting/` | Ledger and valuation | `chronos_ledger`, `chronos_valuation` |
| `applications/replay_runner/` | `chronos_paper_replay`: one dataset through the whole paper loop | `chronos_paper_replay` |
| `tests/` | Unit, replay, contract, end-to-end (one `chronos_unit_tests` binary) and Python tests | |
| `tools/development/` | Build gates and architecture lints run by CI | |
| `python/chronos/` | Python mirror of the contracts, used only by the M0 boundary tests | |
| `docs/` | ADRs, specs of shipped behaviour, the roadmap | |
| `planning/` | Original design notes kept as reference | |

New programs go under `applications/<name>/` with `include/`, `src/` and a `CMakeLists.txt`, the
way `replay_runner` is laid out.

## Building

```sh
brew install curl            # macOS: the system libcurl cannot open wss:// connections
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
# presets debug, release, benchmark, sanitize build into build-<name>/:
# cmake --preset debug && cmake --build --preset debug && ctest --preset debug
uv sync --group dev && make m0-check   # everything CI runs
```

See [`BUILDING.md`](BUILDING.md) for profiles, sanitizers, and the live Bybit probe.

## Reference documents

- **Roadmap:** `docs/roadmap.md`
- **Shipped behaviour:** `docs/specs/` (target construction, risk decision, paper loop)
- **Decisions:** `docs/adr/` (C++ hot path, hosting, fixed point)
- **Architecture and domain model:** `planning/01-architecture/`
- **Component map across Chronos, Tachys and Archeion:** `../trading-research/wiki/decisions/2026-10-15-component-map.md`
