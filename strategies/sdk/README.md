# strategies/sdk/

> **Submodule owner stub (M0.1).** Implemented in M5.2.

- **Parent:** `strategies/`
- **Owner:** Inherits `strategies/` ownership.
- **Plane:** Inherits `strategies/` plane.
- **Language:** Inherits `strategies/` language policy.
- **Public API:** `chronos/strategies/sdk/strategy.hpp`
- **Purpose:** C++ strategy SDK interface (deterministic, resource/deadline hooks).
- **Accepted dependencies:** inherits `strategies/` rules (see parent README).

The SDK is a capability boundary. An evaluation receives only immutable
declared feature outcomes and parameters, one logical cut, deterministic
operation fuel, and a caller-owned bounded output buffer. The interface exposes
no network, host clock, cancellation state, filesystem, environment, secret,
persistence, portfolio, risk, execution, telemetry, or random-device handle.

Logical deadlines are part of the recorded cut and are safe for semantic
decisions. Host deadlines and cancellation remain external operational
enforcement and cannot be observed by strategy code. Explanation factors must
be appended in deterministic rank order before exactly one terminal signal or
abstention draft is written; no output allocation is required by the SDK.
