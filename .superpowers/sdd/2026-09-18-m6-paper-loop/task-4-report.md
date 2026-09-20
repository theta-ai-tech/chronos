# Task 4 report: M6.6 immutable balanced ledger

Implemented `chronos_ledger` under `accounting/ledger`, linked publicly only to
contracts. `LedgerAuthority` is a noncopyable single-writer bound to one D0
backtest run, portfolio, account, instrument, listing, quote currency, ledger
stream/epoch, quantity scale, money scale and posting policy.

## API handoff

`post(const contracts::PaperFill &)` returns `LedgerResult` with a typed
failure, committed transaction identity, and `SettledExposureEvidence` for an
accepted fill. The evidence carries the complete scope, reservation, intent,
fill and transaction identities, committed cursor/checksum, signed posted
delta, ledger-derived post-fill position, scale, source quality, run-input
sequence and fill logical time.

`reverse(transaction_id, correction_id)` appends one compensation transaction;
it never edits the original. Exact retry returns the same transaction. Unknown
transactions, compensation-of-compensation, reuse of a correction identity for
a different source and a second correction of one source fail without mutation.

`transactions()` exposes immutable `LedgerTransaction` values for valuation.
Each retains the full `PaperFillFacts`, policy, signed fill quantity, exact
notional and fee, separately unit-tagged entries, cursor/checksum and optional
original/correction linkage. `cursor()`, `checksum()` and `position_units()`
expose committed ledger state. Position changes only through posted quantity
entries; no target value can overwrite it.

Fill postings use position/quantity-clearing legs for base quantity and
cash/trade-clearing/fee-expense legs for quote currency. Each dimension is
checked independently with widened sums. Notional and fee are recomputed using
the declared toward-positive fixed-point policy. Full fill semantics drive
deduplication; same fill identity with changed semantics conflicts. Transaction
IDs and the chained ledger checksum use canonical field serialization. All
arithmetic, entries, identity, cursor and checksum are staged before reserve and
the final no-throw move append.

The root build now registers accounting and the completed upstream market
replay target. Unit tests link both targets, and the Makefile formatting scope
includes `accounting/` and `applications/`.

## Evidence

TDD red was observed when `tests/unit/ledger_test.cpp` failed to compile because
`chronos/accounting/ledger.hpp` did not exist. Green and integration evidence:

- `cmake -S . -B build && cmake --build build --target chronos_unit_tests -j4`
  completed successfully.
- `./build/tests/chronos_unit_tests`: `RESULT OK: 278 case(s), 0 failed
  check(s)` including ledger and upstream market replay integration.
- clang-format dry-run passes for all ledger and replay-runner headers/sources
  and their tests.
- `make cpp-format` reaches the expanded scope and currently reports only an
  upstream-owned formatting change in `core/src/listing_view_publisher.cpp`;
  after ownership release, that formatting-only change was applied and the
  expanded check passes.
- M6 authority and risk authority checks pass. The legacy module-stub check
  still expects the already-implemented `core/execution_planning` README to be
  a scaffold; the ledger README was retained as a stub to avoid adding another
  stale-validator failure.
- `git diff --check` completed successfully.

Ledger tests cover genuine broker buy/sell fills with nonzero fees, separate
unit balance, complete broker identity/provenance, deterministic cursor and
checksum, exact retry and semantic conflict, scope/mode/scale rejection,
genuine broker cash overflow with unchanged state, broker model mismatch,
atomic compensation, reversal retry/conflict/unknown handling and deterministic
transaction replay.
