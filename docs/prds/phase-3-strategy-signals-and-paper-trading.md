# Phase 3 PRD: Strategy Signals And Paper Trading

## Goal
Produce explainable short-horizon trade ideas from market state and evaluate them through paper trading.

## Scope
- Deterministic strategy framework
- Initial strategy set
- Signal scoring
- Suggested paper trades
- Simple paper position tracking
- Ranked signal explanations

## Requirements
- The system must support mechanical, deterministic strategies suitable for a 1 to 5 minute holding window.
- Initial strategies should include order-book imbalance, microprice or spread shifts, and short-horizon momentum.
- Every signal must produce both a directional score and a suggested paper trade.
- Every signal must include ranked contributing factors.
- Paper trading must support simple fills and preserve a clean path to fees and slippage.
- Paper-risk controls must be enforceable.

## Deliverables
- Strategy framework
- Initial strategy implementations
- Signal and trade-intent model
- Paper position tracker
- Risk-control rules for paper trading
- Strategy evaluation outputs for replay analysis

## Out Of Scope
- Real-money execution
- ML strategies
- Full execution simulation realism

## Exit Criteria
- Signals can be generated from replayed or live-read-only state.
- Suggested paper trades are created with clear explanations.
- Paper trading can be evaluated using basic performance measures such as hit rate and P&L.
