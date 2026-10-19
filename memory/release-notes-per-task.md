---
name: release-notes-per-task
description: Chronos convention — add a newcomer-friendly release note for every completed task
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 4170dc78-9af0-4b25-bcc5-87816c0437eb
---

For the Chronos project, every completed task (issue/PR) gets a release note in `release-notes/`, added **in the same PR as the work**.

**Why:** the user wants a per-task changelog readable by someone completely new to the project — plain language, not just a technical diff.

**How to apply:** file name `<milestone-id>-<slug>.md` (e.g. `m0.2-cpp-toolchain.md`). Structure: link issue+PR at top, then **What I did** (numbered, newcomer-friendly, explains context and the "why") and **How to test and confirm** (copy-pasteable commands + what success looks like). Update `release-notes/README.md` index. Convention doc lives at `release-notes/README.md`. Seeded with M0.1 and M0.2. See [[chronos-project]].
