# Phase 0 PRD: Setup

## Goal
Create the local-first foundation for Chronos so later phases can be built and measured without reworking the project layout or tooling.

## Scope
- Establish repository structure for engine, adapters, UI, and docs.
- Define local development workflow.
- Configure build, test, and formatting tooling.
- Define baseline data and replay workflow.
- Define a stable event model boundary for future phases.

## Requirements
- The project must build locally with minimal setup friction.
- The codebase must separate latency-critical code from control-plane and UI code.
- Replay datasets and sample sessions must be easy to load and inspect.
- The repository must include a clear path for benchmarks and profiling scripts.
- The architecture must reserve clean boundaries for future venue adapters and execution adapters.

## Deliverables
- Initial directory structure
- Build configuration
- Development scripts
- Baseline docs for running the system locally
- Example replay dataset contract
- Initial event schema draft

## Out Of Scope
- Real strategy logic
- Full venue integration
- Production deployment

## Exit Criteria
- A new contributor can run the project locally.
- The project structure supports the planned phases without major restructuring.
- Replay inputs can be stored and referenced consistently.
