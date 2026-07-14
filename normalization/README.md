# normalization/

- **Owner / plane:** Market-data normalization / hot plane.
- **Language:** C++.
- **Purpose:** Bounded source decoding and deterministic venue-neutral market
  fact construction against pinned reference data.
- **Accepted dependencies:** `contracts/`, `adapters/sdk`, concrete source
  decoders, and immutable `core/reference_data` views.
- **Must not depend on:** Transport sessions, mutable reference aliases,
  stream/run-input allocation, market state, strategy, or telemetry.
- **Public API:** `chronos/normalization/market_data/book.hpp` and
  `book_normalizer.hpp`; Bybit decoding is exposed separately through
  `chronos/adapters/market_data/bybit_book_decoder.hpp`.
- **Failure semantics:** Malformed, integrity-ineligible, unsupported,
  ambiguous, inexact, resource-exhausting, or reference-ineligible source
  messages return typed failures and no normalized fact.
- **Reconstruction source:** One record selected from a cryptographically
  verified immutable capture dataset, pinned decoder/schema/normalizer
  versions, and one pinned reference/configuration lineage and snapshot.

M3.2 normalizes observations only. It does not mutate an L2 book or allocate
normalized stream, epoch, event, or run-input positions; those remain later
stream/dispatch and M4 authorities.

Book facts retain the source dataset identity, bound capture lineage, immutable
decode-enrichment identity and semantic checksum, source environment/product
class, JSON Pointer-addressed unknown-field extensions, selected reference
semantic key, effective capture-sequence evidence, and a reference snapshot
selected through one versioned configuration-lineage authority. Capture format
v2 includes market class in the manifest hash, so callers cannot relabel a
verified record as another product class.
