# contracts/domain/

> **Submodule owner stub (M0.1).** M1.1 introduces the first public value objects.

- **Parent:** `contracts/`
- **Owner:** Inherits `contracts/` ownership.
- **Plane:** Inherits `contracts/` plane.
- **Language:** Inherits `contracts/` language policy.
- **Public API:** `chronos/contracts/fixed_point.hpp` and `chronos.contracts` expose
  signed 64-bit `Price`, `Quantity`, and `Money` units, bounded `DecimalScale`,
  declared `RoundingMode`, and checked arithmetic.
- **Failure semantics:** Invalid scales, division, and overflow fail explicitly
  (`std::nullopt` in C++, `ContractValueError` in Python); values never saturate or wrap.
- **Purpose:** Canonical envelopes, identities, and value-object semantics.
- **Accepted dependencies:** inherits `contracts/` rules (see parent README).
