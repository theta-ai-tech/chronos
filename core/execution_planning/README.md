# D0 paper execution planning

`chronos_execution` consumes immutable risk decisions and the current reservation
writer. `PaperIntentAuthority::create` accepts an intent ID, decision, reservation
authority, reservation ID, optional `PaperExecutionEvidence`, and evaluation cut.
Only `RunMode::backtest` is eligible. Evidence must describe this exact current
cut, valid quality, permitted kill switch, tradeable market, positive ordered
bid/ask on tick, and risk quantity on step. Quote currency must match reservation
policy; quantity scale must match risk. Expiry is the earlier of risk and execution
expiry. All checks and allocating copies precede reservation consumption.

The neutral `contracts::PaperIntent` can only be minted by this authority. Its
`facts()` accessor returns a const full chain, scope, current market lineage,
policy/model versions, scales, cut, expiry and signed authorized quantity. The
broker receives this contract without including a risk or execution header.
`PaperSide::Buy` takes positive risk delta; `Sell` takes negative delta. Zero and
`INT64_MIN` cannot become a positive representable quantity.

Successful exact retries return the original immutable fact, including after the
reservation has advanced; this is an audit retry, not renewed eligibility.
Changed successful request reuse rejects. Failed validation does not consume or
cache an intent and may be corrected. Reservation consumption remains single-use
across authority instances. These are synchronous single-writer components;
reservation authorities must outlive the intent authority cache.

Evidence is an explicit upstream input contract, not independently fetched market
data. D0 requires equal input cut rather than a tunable staleness window. Model
versions are bound to the intent for the downstream broker to validate against its
configured policy. No network, routing, broker implementation or ledger dependency
belongs here. Shared fill facts will be added by the owning paper broker stage.

Tests use `tests/support/paper_intent_fixture.hpp` and genuine upstream target,
risk and reservation facts; no private constructors are exposed for fixtures.
