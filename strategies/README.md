# strategies/

> **Module owner stub (M0.1).** Implemented beginning in M5.2.

- **Owner / plane:** Strategy (plugin-like)
- **Language:** C++ (strategy SDK)
- **Purpose:** Concrete strategies, living outside the strategy runtime.
- **Accepted dependencies:** strategies/sdk and approved deterministic utilities only.
- **Must not depend on:** Network, filesystem, wall-clock, secret, or order-submission access.
- **Public API:** `chronos/strategies/sdk/strategy.hpp`
- **Failure semantics:** typed SDK execution status plus one bounded terminal
  signal draft or abstention draft; host interruption is external.
- **Reconstruction source:** declared feature outcomes, immutable parameters,
  logical cut, and deterministic operation budget.

Concrete native definition packs are registered with the CMake
`chronos_add_strategy` function. They supply immutable descriptor/program data;
the SDK host, not pack code, performs evaluation. The restricted target also
applies source and linked-symbol defense-in-depth checks on every build.

See `planning/01-architecture/architecture.md` (Repository and module structure,
Dependency direction) and `planning/01-architecture/domain-model.md` for the
authoritative ownership and dependency rules.

## Submodules

- **`sdk/`** — C++ strategy SDK interface (deterministic, resource/deadline hooks).
- **`reference/`** — Reference strategies used as conformance examples.
- **`fixtures/`** — Strategy test fixtures.
