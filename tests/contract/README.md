# tests/contract/

This directory holds cross-boundary C++ contract checks. The adjacent M6.2
risk contract proof is implemented in `tests/unit/risk_decision_test.cpp`,
because it exercises the public C++ value contract directly; the structural
ownership proof is `tests/python/test_m6_risk_authority_boundaries.py`.

- **Parent:** `tests/`
- **Owner:** Inherits `tests/` ownership.
- **Plane:** Inherits `tests/` plane.
- **Language:** Inherits `tests/` language policy.
- **Purpose:** Producer/consumer conformance and authority-boundary evidence.
- **Accepted dependencies:** inherits `tests/` rules (see parent README).

## Evidence

`strategy_invariants_test.cpp` proves M5 producers do not carry direct order,
risk-decision, or reservation authority. The M5 and M6.1 Python/CMake gates
remain `tests/python/test_m5_authority_boundaries.py` and
`tests/python/test_m6_authority_boundaries.py`.

M6.2 adds a distinct risk-evaluation contract:

- `RiskEvaluationResult` is either the precondition failure
  `InvalidPolicy` with no terminal, or exactly one terminal variant:
  `RiskDecision`, `RiskObligationUnavailable`, or `RiskAdmissionRejected`.
- A `RiskDecision` is only `Approved`, `Modified`, or `Rejected`; unavailable
  evidence and failed admission are intentionally separate terminal categories.
- `tests/unit/risk_decision_test.cpp` proves target-admission reasons, fixed
  unavailable-evidence precedence, checked `QuantityOnlyV1` arithmetic,
  modification proof, safety-rejection ordering, deterministic identity
  sensitivity, target-expiry capping, non-authorization, and non-executability.
- `tests/python/test_m6_risk_authority_boundaries.py` and
  `tools/development/verify_m6_risk_authority_boundaries.py` prove that
  `chronos_risk` has only its declared contracts/portfolio/options/warnings
  dependencies and cannot gain downstream capabilities through CMake bypasses.

Run the structural proof with:

```bash
make m6-risk-authority-check
```

The risk result is not a reservation or executable intent. M6.3 owns
reservation and `risk_sequence` advancement; M6.4 owns executable paper intent.
