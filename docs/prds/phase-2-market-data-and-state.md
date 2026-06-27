# Phase 2 PRD: Market Data And State

## Goal
Turn raw venue feed data into a normalized event stream and maintain accurate local L2 market state for a small set of instruments.

## Scope
- Single-venue market-data adapter, tentatively Bybit
- Normalization into Chronos event types
- Local L2 order book maintenance
- Trade stream handling
- Support for replay and live read-only ingestion

## Requirements
- The system must support a few selected instruments on one venue.
- Market data must be normalized into a venue-agnostic event model.
- Local L2 state must be correct, queryable, and replayable.
- Feed freshness must be observable so stale-data guards can be enforced later.
- The design must preserve a clean path to future L3 support and additional venues.

## Deliverables
- First venue adapter
- Normalized market-data event definitions
- L2 market-state module
- Replay data import or recording workflow
- State validation hooks

## Out Of Scope
- Multi-venue arbitrage
- L3 reconstruction
- External signals such as tweets or news

## Exit Criteria
- Replay and live read-only market data can drive consistent L2 state.
- A small watchlist of instruments can be tracked locally.
- Market-state outputs are stable enough for strategy evaluation.
