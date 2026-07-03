# Chronos

A local-first, replay-first crypto market **decision and research engine**. Live or replayed
market events → maintained market state → deterministic short-horizon strategies → explainable
paper-trade recommendations, with a measured, optimized-C++ hot path.

> **Status:** scaffolding (Deliverable 0). Planning is complete and reviewed; implementation is
> beginning with the thin vertical slice. See `docs/roadmap/deliverable-0-thin-vertical.md`.

## Layout

| Path | What lives here |
|---|---|
| `contracts/` | Canonical domain contracts and value objects (lowest, most stable layer) |
| `core/` | Deterministic domain authorities on the hot path — **optimized C++** (ADR-0001) |
| `accounting/` | Immutable ledger, valuation, reconciliation |
| `adapters/` | Adapter SDK + venue/source/paper/execution adapters |
| `runtime/` | Run control, configuration, datasets, persistence, observability, audit |
| `applications/` | Local API, console, CLI, replay/benchmark runners — **Python / web** |
| `strategies/` | Strategy SDK and concrete strategies (live outside the runtime) |
| `research/` | Experiment registry, experiments, reports, dataset tooling |
| `tests/` | Layered tests: contract, integration, replay, recovery, performance, end-to-end |
| `tools/` | Development, schema, profiling, release tooling |
| `config/` | Versioned configuration schemas, defaults, examples |
| `docs/` | PRDs, ADRs, roadmap, runbooks |
| `planning/` | Authoritative planning corpus (architecture, domain model, phase plans) |

Each module has a README declaring its owner, plane, language, and accepted dependencies.

## Building

The C++ hot path builds with CMake + Ninja:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The Python application/research layer is managed with `uv`:

```sh
uv sync --group dev
make python-check
```

See [`BUILDING.md`](BUILDING.md) for C++ build profiles (Debug / Release / Benchmark),
sanitizers, Python tooling, and verification commands.

## Authoritative documents

- **Architecture:** `planning/01-architecture/architecture.md`
- **Domain model:** `planning/01-architecture/domain-model.md` (authoritative for language and invariants)
- **Decisions:** `docs/adr/` — hot path = C++ (0001), hosting (0002), fixed-point arithmetic (0003)
- **Current work:** `docs/roadmap/deliverable-0-thin-vertical.md` and GitHub Milestones M0–M8

## Dependency direction

Dependencies point inward toward stable contracts and outward toward replaceable
infrastructure: `applications → orchestration → domain authorities → canonical contracts`.
Infrastructure (adapters, storage, telemetry, UI) implements ports; domain modules never
import infrastructure implementations. See the architecture's *Dependency direction* section.
