---
name: chronos-project
description: "What the Chronos project is, its current state, and the GitHub remote"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4170dc78-9af0-4b25-bcc5-87816c0437eb
---

Chronos is a local-first, replay-first crypto market decision/research engine. Stated v1 goal: a fast, observable, explainable paper-trading tool for the owner as researcher/operator, holding window 1–5 min, single venue (tentatively Bybit), L2 with a path to L3. Originally conceived (see docs/deep-research-report.md + the v1 spec PDF) as a portfolio project to demonstrate low-latency C++ credibility with a Python/web surface.

As of 2026-06-27 the project is **planning only** — no code. `docs/prds/` holds 8 short phase PRDs (phase 0–7) + a high-level PRD. `planning/` holds 11 numbered phase dirs (01–11), ~165k words total, very rigorous: domain model + architecture are authoritative (~14k & ~11k words). Tech/language choices deliberately deferred throughout.

GitHub remote: git@github.com:theta-ai-tech/chronos.git (empty as of linking). I ran `git init` + added remote locally on 2026-06-27 but did NOT commit or push (not requested).

Owner background: ~15y banking, Bloomberg + JPMorgan; wants to keep C++ sharp. Give peer-level, candid feedback.

**Committed direction (2026-06-27, recorded as ADRs in docs/adr/):** The explicit purpose is to be sellable to a hedge fund as a low-latency C++ demonstration. Decisions made:
- ADR-0001: hot plane = optimized C++ (incl. strategy evaluation via a C++ strategy SDK); control/research plane = Python; compact versioned boundary between them. "Optimize to C++ later" is rejected for hot-plane modules. Architecture.md amended to promote C++ from deferred to primary goal.
- ADR-0002: hosting = bare-metal benchmark host of record (the only source of latency numbers) + cheap always-on cloud VM for the demo console, labeled non-authoritative for perf.
- ADR-0003: hot-path arithmetic = fixed-point integers (price=tick multiples, qty=step multiples), scale from versioned reference data; floats only non-authoritative.
Product framing: configurable "factory of algos" run through the whole flow (replay/backtest/live-paper) — already well-modeled by the strategy runtime + experiment registry in planning. Next suggested step: thin vertical slice (Bybit one instrument: capture→normalize→L2 state→one strategy→paper fill→latency histogram) shaped to receive the C++ core.
