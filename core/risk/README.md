# core/risk/

> **Submodule owner stub (M0.1).** Implemented beginning in M6.2.

- **Parent:** `core/`
- **Owner:** Inherits `core/` ownership.
- **Plane:** Inherits `core/` plane.
- **Language:** Inherits `core/` language policy.
- **Public API:** `chronos/core/risk/risk_decision.hpp`
- **Purpose:** Deterministic risk-policy decisions over admitted portfolio targets.
- **Accepted dependencies:** contracts and the immutable portfolio target API;
  common build-policy targets are private.

`chronos_risk` is the M6.2 deterministic, fail-closed risk-policy authority.
It evaluates one M6.1 `portfolio::TargetPosition`; it does not reserve
exposure, create executable work, or mutate any input.

- **Target:** `chronos_risk`
- **Public dependencies:** `chronos_contracts`, `chronos_portfolio`
- **Private build dependencies:** `chronos_options`, `chronos_warnings`

The CMake boundary guard fixes that dependency set and rejects downstream
capabilities. In particular, this module cannot link execution, reservation,
storage, adapter, application, UI, ledger, or P&L targets.

## Evaluation Contract

Call `MinimalRiskAuthority::evaluate(target, policy, cut, admission, evidence)`
with immutable inputs:

- the candidate M6.1 absolute target;
- `MinimalRiskPolicy`, including the risk scope, target key, accepted target
  schema and run mode, versions, `QuantityOnlyV1` scale and limit, evidence
  ages, decision duration, and modification policy;
- `RiskEvaluationCut`, the selected run-input sequence and replay logical time;
- `TargetAdmissionEvidence`: publication, acknowledgement, and lifecycle facts;
- `RiskEvaluationEvidence`: run context, policy activation, account, market,
  projected-exposure, and kill-switch snapshots.

An invalid policy is a precondition failure:
`RiskEvaluationResult{RiskEvaluationFailure::InvalidPolicy, no terminal}`.
For a valid policy, evaluation returns exactly one terminal alternative:

1. `RiskDecision` with disposition `Approved`, `Modified`, or `Rejected`.
2. `RiskObligationUnavailable` when an admitted obligation lacks usable
   run/policy/safety evidence or arithmetic is unrepresentable.
3. `RiskAdmissionRejected` when the target never becomes an accepted risk
   obligation.

Unavailable and admission rejection are not `RiskDecision` dispositions. An
unavailable terminal has an obligation and outcome identity but no decision or
authorized target. An admission rejection has only an outcome identity: no
obligation, decision, or authorized target. Every terminal has
`executable() == false`.

## Fail-Closed Rules

Evaluation first validates the policy, then known target admission facts. It
next checks the run context, mode admission, policy activation, account, market,
projected exposure, and kill switch in that order. Missing, stale, future,
recovering, gapped, invalid, unavailable, incompatible, wrong-scope, or
wrong-scale required evidence returns the stable first unavailable reason; it
never authorizes the target.

Known target-side failures are admission rejections, including unpublished or
unacknowledged targets, bad lifecycle facts, superseded/invalidated targets,
target key/run/scale/schema mismatch, future cut, expired target, and validated
`capture` or `live_read_only` modes. A malformed policy does not fabricate a
domain terminal.

With complete valid evidence, the fixed semantic-rejection order is: enabled
kill switch, account trading disabled, market not tradeable, target already
satisfied, then projected-exposure limit. All triggered findings are retained;
the first is the binding reason.

## QuantityOnlyV1

`QuantityOnlyV1` uses checked signed integer arithmetic at one declared scale:

```text
requested_delta = requested_target - account_current_position
requested_projected_exposure =
    projected_exposure_before_target + requested_delta
```

The limit applies to the absolute requested projected exposure. Overflow,
including an unrepresentable absolute value, returns
`RiskObligationUnavailable::ArithmeticUnrepresentable`.

An in-limit result is approved. For an over-limit target with modification
enabled, M6.2 clamps projected exposure to the same-sign limit and solves:

```text
authorized_projected_exposure = clamp(requested_projected_exposure, -limit, +limit)
authorized_delta = authorized_projected_exposure - projected_exposure_before_target
authorized_target = account_current_position + authorized_delta
```

The modification is accepted only when its proof shows lower absolute projected
exposure, no greater same-direction movement, nonzero movement, compatible key
and scale, and representable arithmetic. Otherwise it is rejected. Notional,
leverage, concentration, liquidity, multi-leg, and conversion dimensions are
not `QuantityOnlyV1` inputs and cannot be silently admitted.

`RiskDecision::authorizes_target()` is true only for `Approved` and `Modified`.
It is a static semantic result, not a reservation grant. A decision expires at
`min(cut logical time + policy duration, source target validity)` and therefore
cannot outlive the source target.

## Deferred Authority

M6.2 records but does not advance `risk_sequence`. M6.3 owns current decision
eligibility, lifecycle rechecks, deduplication, serialized projected-exposure
mutation, capacity reservation, expiry/release, and sequence advancement. M6.4
owns executable paper intent. Orders, fills, positions, ledger, P&L, durable
recovery, live approval, and richer risk models remain outside this module.
