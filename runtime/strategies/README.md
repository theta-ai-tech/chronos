# runtime/strategies/

M5.2 introduces the trusted strategy-admission boundary. It owns immutable
strategy activations and derives accepted invocations from feature-authority
provenance. Strategy implementations cannot include this API; they receive only
the restricted data-only SDK definition surface.

- **Parent:** `runtime/`
- **Owner / plane:** Strategy runtime / hot data plane
- **Language:** C++
- **Public API:** `chronos/runtime/strategies/strategy_runtime.hpp`
- **Accepted dependencies:** strategy SDK, feature authority, contracts
- **Failure semantics:** Invalid activations or mismatched cuts are rejected
  before an evaluation obligation is admitted.
