# Phase 5 PRD: Multi-Venue And L3

## Goal
Expand Chronos beyond a single L2 venue by adding support for more venues and a path to finer market microstructure through L3.

## Scope
- Additional venue adapters
- Venue comparison and normalization improvements
- Cross-venue opportunity support
- Optional L3 data model and state maintenance

## Requirements
- The core event and adapter boundaries must support more than one venue without redesign.
- The system must allow strategy logic to work against normalized multi-venue inputs.
- L3 support must be additive rather than disruptive to existing L2 workflows.
- The product must support future cross-venue opportunity logic once multiple venues are available.

## Deliverables
- Additional venue adapter contracts
- Multi-venue state abstractions
- L3 event and state design
- Upgrade path documentation from L2 to L3

## Out Of Scope
- Full external event fusion
- Prediction-market resolution workflows

## Exit Criteria
- More than one venue can be supported through the same product model.
- The system design can accommodate L3 where needed without breaking L2 use cases.
