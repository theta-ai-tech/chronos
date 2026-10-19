# Roadmap

Plan of record from 2026-10-15. The goal and the six-month done state are in
`../trading-research/wiki/interviews/2026-10-07-final-review.md`: sixty consecutive days of
unattended paper trading on live Bybit data, one slow strategy. Each item below gets its own
spec, then a plan, then small steps that each end with something that runs.

| # | Item | Where | State |
|---|---|---|---|
| 1 | **Keryx**, live capture service | `applications/keryx/` | Spec v2.1 in review: `../trading-research/wiki/specs/2026-10-08-keryx-capture-service.md`. Steps 0 to 6 in `2026-10-12-keryx-delivery-steps.md` |
| 2 | **Day runner**, replays a day of datasets carrying book and ledger across them | `applications/replay_runner/` | Not specified. Needed because replay takes one dataset and needs a snapshot in it |
| 3 | **Anamnesis**, history backfill: bars, funding, Bybit trade archives | `applications/anamnesis/` | Not specified |
| 4 | **Bars and the bar replay path**, for a strategy on four-hour or daily bars | `core/features/`, `applications/replay_runner/` | Not specified |
| 5 | **Strategy library**, a way to express a second rule | `strategies/` | Not specified. One JSON reference rule today |
| 6 | **Spot reference profile** in replay | `applications/replay_runner/`, `core/` reference data | Needed only if year one is spot. Replay reads linear perpetual only |
| 7 | **Nightly paper run** over yesterday's capture, output to Archeion's morning report | ops | After 1, 2, 5 |

Known follow-ups, not scheduled: split the venue-neutral capture code out of
`chronos_bybit_adapter` when a second consumer appears; remove the Python contracts mirror and the
`chronos_boundary` library as one coordinated change; retire the three probes once Keryx lands.
