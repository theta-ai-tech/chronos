# ADR-0001: Hot-path language and the C++/control-plane boundary

- Status: Accepted
- Date: 2026-06-27

## Context

Chronos has two intertwined goals. The product goal is a local-first, replay-first
decision and research engine that runs a configurable "factory" of short-horizon
strategies through the full flow (replay, backtest, live read-only, live paper). The
credibility goal — the original motivation for the project — is to demonstrate
*measured* low-latency C++ on the time-sensitive path, to a standard a quantitative
trading firm would take seriously.

The planning corpus (`planning/`) is deliberately language-neutral and defers runtime
choices behind stable contracts. That is sound for replaceability, but it left the one
*differentiating* element — an optimized C++ hot path — unstated and therefore
implicitly demotable to a "later optimization." This ADR removes that ambiguity.

The architecture already separates a hot plane, a control plane, and a data plane, and
already requires that the control plane talk to the hot plane only through versioned
contracts. This ADR pins which language sits on each side and where strategy code runs.

## Decision

1. **The hot plane is implemented in optimized C++.** It covers accepted-input
   consumption, run-input dispatch, market-state transition and view publication,
   feature calculation, **strategy evaluation**, recommendation production,
   mode-appropriate portfolio/risk/reservation decisions where enabled, paper
   execution, fill acceptance, and latency-point capture. The core is single-threaded
   per run input, allocation-controlled, and benchmarkable without the UI or control
   plane (LMAX/Disruptor-style business-logic processor surrounded by bounded queues).

2. **Strategies run inside the C++ core through a C++ strategy SDK.** A strategy is a
   versioned C++ implementation of the strategy interface, instantiated with immutable
   parameters and declared dependencies. The "algo factory" is realized as
   configuration-driven strategy *instances* plus parameterized strategy *primitives*,
   not as Python callbacks on the hot path. Fast iteration comes from configuration and
   recompilation of the strategy module, never from injecting a higher-level runtime
   into the deterministic evaluation path.

3. **The control and research plane is implemented in Python.** It owns run/config
   authority, control commands, the operations API, experiment/backtest orchestration,
   the experiment registry surface, reporting, and the web console backend. It never
   executes hot-path domain logic.

4. **The boundary between Python and C++ is a compact, bounded, versioned message
   contract.** No runtime-native object (no `PyObject`, no language-specific
   serialization) crosses it. Identity, ordering, causation, mode, and version are
   carried by the canonical envelope, not by transport-specific fields. The boundary is
   valid in-process (e.g. an embedded/bound C++ core) and must remain replaceable by a
   local IPC transport without changing domain meaning. Serialization adapters prove
   round-trip semantic equivalence.

5. **"Optimize to C++ later" is not an accepted path for hot-plane modules.** A
   first implementation may stub or simplify behavior, but a hot-plane module is written
   in C++ from the start. Substituting the language of a hot-plane module requires a new
   ADR superseding this one, with measured justification.

## Consequences

- The benchmark deliverable (latency distributions, profiles, contention analysis) is
  produced against real C++ code, making the credibility claim defensible.
- Strategy authors work in C++. This raises the iteration cost relative to a Python
  strategy harness; it is accepted because in-hot-path strategy evaluation is part of
  the latency story being sold. A Python *prototyping* path may later be added for
  research only, but it must run through the same domain contracts and may not become a
  second strategy engine (see the domain model's prohibition on divergent backtest
  implementations).
- The Python/C++ boundary becomes a first-class contract with conformance tests, not an
  implementation detail.
- Build, CI, and the toolchain must support a mixed C++/Python repository with isolated
  module compilation, sanitizer/UB profiles for the native core, and a benchmark
  profile (see the build/CI standards in `architecture.md`).

## Related

- `planning/01-architecture/architecture.md` — runtime planes, dependency direction.
- ADR-0002 — hosting and benchmark environment (where the C++ numbers come from).
- ADR-0003 — hot-path arithmetic representation (fixed-point integers).
