# Chronos Planning

This directory defines the infrastructure-led plan for Chronos before implementation planning begins.

## Reading Order

1. `01-architecture/`
2. `02-observability/`
3. `03-event-infrastructure/`
4. `04-market-data/`
5. `05-strategy-runtime/`
6. `06-research-backtesting/`
7. `07-portfolio-risk/`
8. `08-paper-execution/`
9. `09-operations-console/`
10. `10-live-paper/`
11. `11-live-execution/`

Most phases contain two planning documents. A phase may contain more when the approved architecture splits the phase into distinct substages that need separate review; for example, Phase 11 contains external discovery, human-approved live execution, and optional guarded automation leaves. The documents define boundaries, invariants, decisions, failure behavior, observability, security, testing, deliverables, and phase exit gates. They deliberately stop before ticket-level implementation planning.

## Phase Gates

A phase is approved for planning purposes only when:

- Every leaf document has received an independent critique and all material findings are resolved.
- The phase has been reviewed as a coherent whole.
- The phase has been checked cumulatively against all earlier approved phases.
- Interfaces, terminology, ownership, and assumptions remain consistent across the planning tree.
- The exit criteria are evidence-based and can later be translated into implementation work.

## Infrastructure Standard

Every functional capability must define:

- Deterministic or explicitly bounded behavior.
- Structured telemetry and latency measurement.
- Failure detection, degradation, and recovery behavior.
- Durable auditability where decisions or state changes matter.
- Reproducible tests, replay fixtures, and performance evidence.
- Versioned contracts and migration expectations.
- Local-first operation without requiring distributed infrastructure prematurely.

## Scope Boundary

These documents define what Chronos should build and the contracts between its parts. They do not yet decompose work into epics, stories, tasks, estimates, or implementation commits. That decomposition starts only after this planning set is reviewed and approved.
