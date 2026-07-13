# contracts/domain/

> **Submodule owner stub (M0.1).** M1.1 introduces the first public value objects.

- **Parent:** `contracts/`
- **Owner:** Inherits `contracts/` ownership.
- **Plane:** Inherits `contracts/` plane.
- **Language:** Inherits `contracts/` language policy.
- **Public API:** `chronos/contracts/fixed_point.hpp` and `chronos.contracts` expose
  signed 64-bit `Price`, `Quantity`, and `Money` units bound to complete
  `VersionRef` unit definitions, bounded `DecimalScale`, declared `RoundingMode`,
  and checked arithmetic.
- **M1.2 value objects:** `chronos/contracts/value_objects.hpp` and
  `chronos.value_objects` expose strongly typed opaque IDs, `StreamCursor`, `VersionRef`,
  clock-qualified `TimePoint`, and `DataQuality`.
- **M1.3 lineage:** `chronos/contracts/state_lineage.hpp` and `chronos.state_lineage`
  expose complete, canonically ordered cursor vectors and vector-clock comparison.
- **Failure semantics:** Invalid scales, division, and overflow fail explicitly
  (`std::nullopt` in C++, `ContractValueError` in Python); values never saturate or wrap.
- **Purpose:** Canonical envelopes, identities, and value-object semantics.
- **Accepted dependencies:** inherits `contracts/` rules (see parent README).
