# applications/replay_runner/

> **Submodule owner stub (M0.1).** Implemented in M6.7.

- **Owner:** Applications.
- **Plane:** Orchestration.
- **Language:** C++20.
- **Purpose:** Deterministically compose verified captured market data through
  the Chronos authorities and expose stable replay summaries.
- **Public API:** `run_market_replay`, `run_paper_replay`, and the
  `chronos_paper_replay` executable.
- **Accepted dependencies:** Authority APIs from capture/replay, market state,
  features, strategy, recommendation, portfolio, risk, execution, paper broker,
  ledger, and valuation modules.
- **Must not own:** Trading policy, risk approval, fill economics, ledger
  posting, or valuation rules. This module wires the owning authorities and
  stops when any required stage fails.

## Paper Replay CLI

Build and run a verified capture dataset:

```sh
cmake -S . -B build
cmake --build build -j 6
./build/applications/replay_runner/chronos_paper_replay DATASET_DIRECTORY
```

The command emits one stable JSON object. It reports the source counts, every
downstream authority count, final ledger-derived position and P&L, ledger
checksum, and a semantic checksum. It performs no network access.

For a local deterministic acceptance run, generate the explicitly synthetic
two-cut fixture and replay it twice:

```sh
fixture_dir="$(mktemp -d)/fixture"
./build/applications/replay_runner/chronos_paper_replay --generate-fixture "$fixture_dir"
./build/applications/replay_runner/chronos_paper_replay "$fixture_dir"
./build/applications/replay_runner/chronos_paper_replay "$fixture_dir"
```

The generated dataset is test evidence only. It must not be represented as a
live venue capture.
