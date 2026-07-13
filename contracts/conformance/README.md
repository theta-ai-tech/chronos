# contracts/conformance/

> **Submodule owner stub (M0.1).** M1.5 introduces canonical cross-runtime fixtures.

- **Parent:** `contracts/`
- **Owner:** Inherits `contracts/` ownership.
- **Plane:** Inherits `contracts/` plane.
- **Language:** Inherits `contracts/` language policy.
- **Public API:** `chronos/contracts/serialization.hpp`, `chronos.serialization`, and
  `fixtures/m1-full-frame.hex` define the bounded canonical conformance frame.
  Frame version 2 carries complete amount-definition `VersionRef` identities.
- **Failure semantics:** malformed, oversized, noncanonical, or semantically invalid frames
  fail before authority admission; trailing and truncated bytes are rejected.
- **Purpose:** Fixtures and cross-boundary compatibility tests.
- **Accepted dependencies:** inherits `contracts/` rules (see parent README).
