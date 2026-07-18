# tests/replay/

> **Submodule owner stub (M0.1).** Implemented beginning in M3.5.

M3.5 provides a committed normalized-stream golden. One verified captured
session containing a book snapshot, book delta, and multi-member trade frame
is normalized twice under identical pinned versions and reference lineage.
Both runs must produce equal complete fact values and byte-identical ordered
semantic records. A committed SHA-256 identity covers every normalized fact
field plus stream/source/version/acceptance provenance, and normalized-fact
replay must reproduce the same bytes and metadata exactly.

M4.6 extends that captured session through the real run-input dispatcher and
market-state authorities. It applies the normalized snapshot, delta, and
trades, inserts the required deterministic trade-continuity boundary, and
publishes five immutable listing views and bundles. Replaying the identical
manifest twice must reproduce every complete view and bundle exactly. The
committed M4 SHA-256 golden covers the semantic checksums of every intermediate
view and bundle, so drift in identity, content, ancestry, or lineage is visible.

- **Parent:** `tests/`
- **Owner:** Inherits `tests/` ownership.
- **Plane:** Inherits `tests/` plane.
- **Language:** Inherits `tests/` language policy.
- **Public API:** Test evidence only.
- **Purpose:** Deterministic replay and golden tests.
- **Accepted dependencies:** inherits `tests/` rules (see parent README).
