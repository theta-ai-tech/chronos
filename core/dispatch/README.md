# core/dispatch/

> **Submodule owner stub (M0.1).** Implemented beginning in M4.1.

M4.1 provides the single-stream form of the run-input merge authority. It
validates one contiguous input cursor, allocates one monotonic
`run_input_sequence`, persists a complete immutable selection before
publication, enforces reserved control boundaries, and retries uncertain
publication with the original selection identity and sequence.

- **Parent:** `core/`
- **Owner:** Inherits `core/` ownership.
- **Plane:** Inherits `core/` plane.
- **Language:** Inherits `core/` language policy.
- **Public API:** `chronos/core/dispatch/run_input_dispatcher.hpp`
- **Purpose:** Run-input dispatch: single monotonic run_input_sequence and merge policy.
- **Accepted dependencies:** inherits `core/` rules (see parent README).
