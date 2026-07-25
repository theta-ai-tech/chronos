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
`AcceptedFeatureEvaluationCut`; the trusted `runtime/strategies` activation
owner binds one exact definition, instance, parameter, timer/control context,
deadline, and fuel class before issuing an immutable
`AcceptedStrategyInvocation`. `StrategyHost` accepts only that token and
evaluates a data-only, loop-free `StrategyProgram`. No strategy
callback or function pointer runs during evaluation, so a definition cannot
reach network, filesystem, host clocks, environment/secrets, persistence,
telemetry, or other ambient process capabilities.

Definition admission copies a valid definition into fixed owning storage and
rejects any shape or semantic identity except the V1 single-listing,
order-book-imbalance, one-feature, one-parameter, eight-instruction threshold
program with distinct factors and a bounded horizon. Invocation admission owns
the resolved parameter, binds the definition digest and activation checksum,
and validates the full bounded feature lineage, including the designated timer
and control cursors. The host charges a fixed admission budget before
revalidating that token,
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
target accepts exactly one strict data manifest and compiles only trusted
generated code against the SDK/options surface. CI rejects authored strategy
C++ and non-registration CMake commands; a linked-symbol check remains defense
in depth. Evaluation isolation does not depend on a source denylist because no
pack-authored code exists or is called by `StrategyHost`.
