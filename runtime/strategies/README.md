# runtime/strategies/

M5.2 introduces the trusted strategy-admission boundary. It owns immutable
strategy activations and derives accepted invocations from feature-authority
provenance. Strategy implementations cannot include this API; they receive only
the restricted data-only SDK definition surface.

Activation requires an opaque control outcome issued only after the dispatcher
consumer accepts the persisted selection. The canonical control payload binds
the complete runtime configuration, and admission verifies its epoch, effective
position, control order, and activation-cut checksum. Logical deadline offsets
are non-negative and are resolved independently against each admitted cut.
Market-state and feature cuts retain the opaque accepted control token and hash
its outcome ID and accepted-selection checksum into provenance, so later cuts
cannot substitute a numerically similar control lineage.

- **Parent:** `runtime/`
- **Owner / plane:** Strategy runtime / hot data plane
- **Language:** C++
- **Public API:** `chronos/runtime/strategies/strategy_runtime.hpp`
- **Accepted dependencies:** strategy SDK, feature authority, contracts
- **Failure semantics:** Invalid activations or mismatched cuts are rejected
  before an evaluation obligation is admitted.

External strategy observations are not activated in Deliverable 0. Evaluation
inputs are limited to the admitted feature/control/timer cut, and the checked
M5 dependency gate prevents imports from portfolio, risk, execution, adapters,
or accounting.
