# ADR-0003: Hot-path arithmetic representation

- Status: Accepted
- Date: 2026-06-27

## Context

The domain model requires exact arithmetic: "Prices, quantities, fees, and money must
not depend on binary floating-point equality." ADR-0001 commits the hot path to
optimized, allocation-controlled C++. These two requirements collide directly on the one
path the project is built to show off.

The naive way to satisfy "exact" is an arbitrary-precision decimal type (a `Decimal`
class / bignum). On a low-latency core this is the wrong tool: such types typically
allocate, are not trivially copyable, blow cache locality, and add branch-heavy work to
the hottest loop. Choosing the representation is therefore an architectural decision, not
a tuning detail — it shapes the value objects, the message envelope payloads, and the
order book's memory layout. Deferring it means rewriting the core later.

## Decision

1. **Hot-path amounts are represented as fixed-point integers in venue-defined units.**
   Prices are integer multiples of the listing's price tick; quantities are integer
   multiples of the quantity step. These are stored as fixed-width integers (e.g.
   `int64_t`, widening to `int128` only where a product/accumulation provably needs it),
   are trivially copyable, and require no heap allocation.

2. **Scale and unit are carried by versioned reference data, not by the number.** The
   listing definition's tick/step and their scale exponents are the single source of
   truth for converting between integer ticks and human/display decimals. A bare integer
   is meaningless without its listing-definition version; that version travels in the
   canonical envelope's reference references.

3. **Exactness is preserved by construction.** Addition, subtraction, and comparison are
   exact in integer space. Multiplications and divisions (fees, notional, cost basis,
   conversions) use explicit fixed-point scaling with a declared rounding mode and are
   checked for overflow; rounding direction is part of the policy, never incidental. No
   hot-path computation relies on float equality, and no `==` is performed on a value
   that originated as a float.

4. **Floating point is confined to non-authoritative use.** Floats may appear in
   diagnostic observations, illustrative UI display, and analytics that are explicitly
   non-authoritative. They may never carry an authoritative price, quantity, fee, cash,
   position, or P&L value, and may never be the basis of a domain decision.

5. **Accounting may widen, not change kind.** The ledger and valuation may use wider
   fixed-point integers or an explicit rational/scaled representation for cost basis and
   conversions where a tick-multiple is insufficient, but it remains exact integer-based
   arithmetic with declared scale and rounding — not binary floating point. Any such
   widening is specified where the accounting phase introduces it.

6. **Overflow and out-of-range are explicit failures.** A value that cannot be
   represented in the chosen fixed-width type, or a scaling that would overflow, is a
   typed invalid/contract failure, not a silent wrap or saturation. Parsers enforce range
   limits on untrusted venue input before it reaches the integer domain.

## Consequences

- The hot path keeps exactness with primitive, cache-friendly, allocation-free integers —
  satisfying both the correctness and the latency theses instead of trading one for the
  other.
- Value objects, order-book levels, and message payloads are designed around integer
  ticks from the first implementation; there is no later "swap Decimal for int" rewrite.
- Every amount is inseparable from its listing-definition version. Reference-data
  versioning and migration must be solid early, because reinterpreting an integer under a
  changed tick/step is a semantic change requiring a new version.
- Rounding policy becomes an explicit, tested, versioned concern (the test pyramid's
  Level 1 "exact arithmetic, units, rounding, and boundaries" applies directly).
- Cross-venue or multi-asset work later must define scale alignment explicitly; there is
  no implicit common float to fall back on.

## Related

- `planning/01-architecture/domain-model.md` — exact-amount principle, ubiquitous
  language for listing definitions, tick/step, marks.
- `planning/04-market-data/source-adapter-and-normalization.md` — where untrusted venue
  numbers are range-checked and mapped into the integer domain.
- ADR-0001 — the allocation-controlled C++ core this representation serves.
