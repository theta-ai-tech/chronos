# strategies/sdk/

> **Submodule owner stub (M0.1).** Implemented in M5.2.

- **Parent:** `strategies/`
- **Owner:** Inherits `strategies/` ownership.
- **Plane:** Inherits `strategies/` plane.
- **Language:** Inherits `strategies/` language policy.
- **Public API:** `chronos/strategies/sdk/strategy.hpp`
- **Purpose:** C++ strategy SDK interface (deterministic, resource/deadline hooks).
- **Accepted dependencies:** inherits `strategies/` rules (see parent README).

The SDK is a capability boundary. `FeatureRuntime` issues an immutable
`AcceptedFeatureEvaluationCut`; `StrategyHost` admits that authority token and
evaluates a data-only, loop-free `StrategyProgram`. No strategy callback or
function pointer runs during evaluation, so a definition cannot reach network,
filesystem, host clocks, environment/secrets, persistence, telemetry, or other
ambient process capabilities.

Admission copies a valid definition into fixed owning storage and rejects any
shape except the V1 one-feature, one-parameter, eight-instruction threshold
program. The host charges a fixed admission budget before bounded validation,
then one deterministic fuel unit per instruction; rejects jumps and malformed
opcodes or operands; checks the recorded logical deadline; clears only fixed
workspace/output prefixes; and constructs bounded explanation/terminal output
itself. Definition packs cannot mutate accepted spans, ignore controls, or forge
explanation lineage because accepted feature IDs are attached by the host.

Logical deadlines are part of the recorded cut and are safe for semantic
decisions. Host deadlines and cancellation remain external operational
enforcement and cannot be observed by strategy code. Explanation factors must
be appended in deterministic rank order before exactly one terminal signal or
abstention draft is written; no output allocation is required by the SDK.

Native definition-pack targets must be created with `chronos_add_strategy`. The
target links only the SDK/options surface and runs source plus linked-symbol
defense-in-depth checks. CI also rejects unregistered strategy CMake targets;
evaluation isolation does not depend on those denylists because no pack code is
called by `StrategyHost`.
