# CLAUDE.md

Guidance for agents working in Chronos.

## What Chronos is

The decision and research engine of a three-system personal trading setup:

- **Chronos decides.** Capture, replay, features, strategies, sizing, economic risk, paper fills,
  front-office ledger. It never holds a venue order session.
- **Tachys acts** (`../tachys`). Order management, mechanical risk, kill switch, Bybit sessions.
- **Archeion checks** (`../archeion`). Books of record, reconciliation, the morning report.

No code connects Chronos to Tachys before April 2027. The component map is
`../trading-research/wiki/decisions/2026-10-15-component-map.md`.

## Current work

`docs/roadmap.md` is the plan of record. Each item gets a spec, then a plan, then small steps that
each end with something that runs and a check the owner can do by hand. The owner wants to
understand the code, not receive it: explain library behaviour with file and line references,
and prefer reviewing the owner's code to writing it unasked.

## Rules

- New programs go in `applications/<name>/` with `include/`, `src/`, `CMakeLists.txt`, following
  `applications/replay_runner/`. Unit tests go in `tests/unit/` and join `chronos_unit_tests`.
- Compose existing libraries. A change to `adapters/`, `core/`, `normalization/` or `runtime/`
  needs its own spec.
- Every module directory with a README stub is listed in
  `tools/development/validate_module_stubs.py`. Adding or removing one means editing that list.
- The `verify_m5/m6/m6_risk_*` scripts hard-code module paths and target names. Moving or
  renaming a built module means updating them.
- `make m0-check` is what CI runs. Run it before calling anything done.
- macOS needs Homebrew curl for wss. The system libcurl fails at runtime with
  InvalidConfiguration, not at configure time.
- Greek-origin names for new components. No emoji, no em dashes in prose.
