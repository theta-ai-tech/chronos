# Phase 7 PRD: Human-In-The-Loop Execution And Automation Guards

## Goal
Introduce controlled execution workflows after the signal and recommendation system is proven useful, starting with human approval and later guarded automation.

## Scope
- Human approval workflow
- Venue-specific execution adapters
- Execution audit trail
- Safety checks for later automation

## Requirements
- Execution must begin as human-in-the-loop.
- The system must preserve a clear distinction between signal generation, opportunity resolution, and execution.
- Every execution action must be logged with context and rationale.
- Safety controls must be strong enough to support later guarded automation.
- Automation, if added, must remain explicitly optional and tightly constrained.

## Deliverables
- Approval workflow
- Execution-intent lifecycle
- Venue execution adapter contracts
- Audit and safety model
- Automation guardrail design

## Out Of Scope
- Unconstrained full automation
- Production capital deployment without safety review

## Exit Criteria
- An operator can review and approve recommended actions.
- The system is ready for tightly controlled execution experiments without changing the product model.
