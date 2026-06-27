# Phase 6 PRD: External Signals And Polymarket Discovery

## Goal
Extend Chronos beyond pure exchange microstructure by incorporating additional event sources and mapping detected opportunities to tradable markets such as Polymarket.

## Scope
- External event source interfaces
- News or tweet ingestion
- Opportunity resolution layer
- Polymarket market search and ranking
- Human-facing recommendations based on external plus market signals

## Requirements
- The event model must support external signals without distorting the low-latency core.
- External event handling must remain optional and modular.
- The product must support turning an abstract short-term signal into a search for relevant markets.
- Polymarket integration must start with discovery, ranking, and recommendation rather than automated execution.
- Explanations must show how external signals influenced the recommendation.

## Deliverables
- External signal adapter contract
- Opportunity-resolution layer
- Polymarket discovery module
- Ranking logic for candidate markets
- Recommendation outputs that link signal to market opportunity

## Out Of Scope
- Full automated Polymarket trading
- Broad alternative data platform support

## Exit Criteria
- Chronos can ingest at least one external signal source.
- The system can map a detected opportunity to candidate Polymarket markets for operator review.
