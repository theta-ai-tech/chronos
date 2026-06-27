# Chronos High-Level Product Requirements Document

## Summary
Chronos is a local-first, low-latency decision engine for short-horizon opportunities. It ingests time-sensitive crypto market events, maintains live market state, evaluates deterministic strategies under measurable latency constraints, and produces explainable paper-trade recommendations for a human operator.

Chronos is not primarily a trading bot. The core product is a fast, observable event-processing and strategy-evaluation platform. Venue-specific trading, including Polymarket, is a planned extension built on top of that core.

## Primary User
The primary v1 user is the project owner acting as researcher and operator. The product should still be shaped so it can later evolve into a solopreneur tool for traders or researchers who want fast signal evaluation and paper-trading workflows.

## Product Vision
Chronos should:
- Provide a measurable low-latency platform for experimenting with time-sensitive strategies.
- Turn live or replayed market events into explainable paper-trade decisions.
- Demonstrate benchmarking discipline and systems credibility, not just strategy logic.

## Product Principles
- Replay-first architecture, even when live monitoring is supported.
- Low-latency core stays small, deterministic, and measurable.
- Strategy evaluation is explainable and auditable.
- Venue integration is modular.
- The product is an engine for evaluating opportunities, not a hardcoded single strategy.
- Future execution adapters sit on top of a stable signal and intent layer.

## V1 Scope
Chronos v1 will:
- Run locally with a web UI and local services.
- Use a generic event interface, but support crypto market data only.
- Support a single venue first, tentatively Bybit.
- Monitor a small set of instruments rather than an entire venue.
- Support replay mode and live read-only monitoring mode.
- Start with L2 order book data and preserve a clean path to L3.
- Maintain internal market state for short-horizon strategies with a 1 to 5 minute holding window.
- Evaluate deterministic, mechanical strategies such as order-book imbalance, microprice or spread shifts, and short-horizon momentum.
- Emit both a directional or scored signal and a suggested paper trade.
- Explain each signal using ranked contributing factors.
- Expose observability and benchmarking data, especially latency and pipeline health.

## V1 Outputs
Each suggested paper trade should include:
- Side
- Confidence score
- Reference entry price
- Suggested size
- Expected holding window
- Exit or stop hint
- Explanation

## V1 User Experience
The initial UI should show:
- Top-of-book and recent depth
- Recent trades
- Active strategy signals
- Suggested paper positions
- Latency histogram
- Event throughput and queue health
- Replay controls
- Signal explanation panel

## V1 Controls
- Select venue and instruments
- Start or stop replay
- Start or stop live feed
- Enable or disable strategies
- Tune strategy thresholds
- Set paper capital
- Set max position size and exposure
- Pause signal generation
- Reset paper session

## V1 Risk Controls
- Max position per instrument
- Max total exposure
- Max concurrent positions
- Max trades per minute
- Cooldown after exit
- Stale-data guard that blocks signal generation when feed freshness degrades
- Kill switch

## Success Criteria
V1 success is measured primarily by technical quality, then by signal usefulness.

Primary success criteria:
- End-to-end signal latency is measurable under replay and live read-only modes.
- Latency distributions are stable, including tail behavior.
- Market-state maintenance and signal generation are correct and reproducible.
- The product is usable for live monitoring and replay analysis with low operational burden.

Secondary success criteria:
- Paper-trade P&L is directionally positive or at least informative.
- Hit rate is meaningful enough to justify deeper iteration.
- Strategy behavior remains stable under simple cost assumptions.

## Non-Goals For V1
- Real-money trading
- Full automated execution
- Tweet or news ingestion
- ML-based strategies
- Complex risk engines
- Broad multi-asset support
- Multi-venue arbitrage
- L3-first order-by-order reconstruction

## Planned Evolution
Chronos is expected to evolve in phases:
- Setup and architecture foundation
- Core low-latency engine
- Market-data normalization and market state
- Strategy signals and paper trading
- Live monitoring, UI, and observability
- Multi-venue support and L3 expansion
- External signals and Polymarket discovery
- Human-in-the-loop execution with later guarded automation

## Polymarket Positioning
Polymarket should be treated as a future venue and opportunity adapter, not as the defining core of Chronos. The intended future flow is:
- Detect a short-term opportunity from events
- Resolve that opportunity into relevant Polymarket markets
- Recommend or later place a position under human control
