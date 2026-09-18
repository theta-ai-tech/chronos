#pragma once
#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/event_envelope.hpp"
#include "chronos/contracts/fixed_point.hpp"
namespace chronos::contracts {
// Neutral ledger-posted evidence. Every field must be supplied by the caller;
// consumers validate scope, full-fill linkage and monotonically advancing
// cursor.
struct SettledExposureEvidence final {
  RunId run_id;
  PortfolioId portfolio_id;
  AccountId account_id;
  CanonicalInstrumentId canonical_instrument_id;
  ListingId listing_id;
  VersionRef quote_currency;
  RunMode run_mode;
  ReservationId reservation_id;
  ExecutableOrderIntentId intent_id;
  PaperFillId fill_id;
  LedgerTransactionId transaction_id;
  StreamCursor ledger_cursor;
  Sha256Digest ledger_checksum;
  AmountUnits posted_delta_units;
  AmountUnits position_units;
  DecimalScale exposure_scale;
  DataQuality quality;
  std::uint64_t run_input_sequence;
  std::int64_t logical_time_nanoseconds;
  bool operator==(const SettledExposureEvidence &) const = default;
};
} // namespace chronos::contracts
