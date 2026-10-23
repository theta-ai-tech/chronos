# Chronos planning notes

The original design corpus, kept as reference. It is not the plan of record: the roadmap is
`docs/roadmap.md`, and behaviour that shipped is specified in `docs/specs/`. Where a note and the
code disagree, the code and its tests win.

| Folder | Still useful for |
|---|---|
| `01-architecture/` | Module boundaries, dependency direction, the domain model and its vocabulary |
| `03-event-infrastructure/` | Event contracts, persistence and replay semantics |
| `04-market-data/` | Source adapters, normalization, market state and data quality |
| `05-strategy-runtime/` | Feature runtime, strategy and recommendation runtime |
| `06-research-backtesting/` | Datasets, experiments, backtest validation. Input to the strategy library and day runner |

Observability, portfolio and risk, paper execution, operations console, live paper and live
execution notes were removed on 2026-10-15. The first three were superseded by the shipped M6
specs; the rest are outside the plan. They remain in git history.
