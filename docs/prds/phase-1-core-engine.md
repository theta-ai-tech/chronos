# Phase 1 PRD: Core Engine

## Goal
Build the deterministic low-latency core that accepts normalized events, processes them predictably, and emits signals or intents within a measurable latency budget.

## Scope
- Core event ingestion interface
- Internal event pipeline
- Deterministic processing loop
- Low-allocation or zero-allocation hot-path design where practical
- Benchmark and timing hooks

## Requirements
- The engine must process normalized events through a stable, deterministic path.
- The hot path must avoid unnecessary allocations and hidden framework overhead.
- The engine must expose timing points that support end-to-end latency measurement.
- The core design must support replay-first execution and future live ingestion.
- The engine must be isolated from venue-specific logic.

## Deliverables
- Core engine module
- Event-processing pipeline
- Timing and instrumentation hooks
- Benchmarkable interfaces
- Deterministic replay entrypoint

## Out Of Scope
- Full market-state logic
- UI
- Real or paper execution workflows

## Exit Criteria
- The engine can accept replayed normalized events and produce deterministic outputs.
- Basic latency measurements can be captured consistently.
- The core interfaces are stable enough for market-state and strategy phases.
