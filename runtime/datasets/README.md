# runtime/datasets/

> **Submodule owner stub (M0.1).** Implemented beginning in M3.4.

M3.4 provides content-identified replay run manifests and two ordered input
providers. Faithful capture-order replay accepts only a cryptographically
verified capture dataset and preserves its capture sequence. Normalized-fact
replay accepts an immutable content-identified fact dataset and never reruns
normalization. Both feed the same synchronous dispatch seam, stop visibly on
rejection, and require a manifest naming exactly one replay class.

- **Parent:** `runtime/`
- **Owner:** Inherits `runtime/` ownership.
- **Plane:** Inherits `runtime/` plane.
- **Language:** Inherits `runtime/` language policy.
- **Public API:** `chronos/runtime/datasets/replay.hpp`
- **Purpose:** Capture/dataset/replay manifests and ordered reconstruction.
- **Accepted dependencies:** inherits `runtime/` rules (see parent README).
