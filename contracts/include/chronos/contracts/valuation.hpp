#pragma once

#include "chronos/contracts/state_lineage.hpp"

namespace chronos::contracts {

// An application converts an accepted market view into this immutable mark.
// Accounting validates its scope, scales, quality and cut; admission of the
// view itself remains an application/market-state responsibility.
struct PositionMark final {
  RunId run_id;
  PortfolioId portfolio_id;
  AccountId account_id;
  CanonicalInstrumentId canonical_instrument_id;
  ListingId listing_id;
  VersionRef quote_currency;
  RunMode run_mode;
  StateViewId source_view_id;
  StateLineage source_lineage;
  VersionRef price_definition;
  AmountUnits price_units;
  DecimalScale price_scale;
  DecimalScale quantity_scale;
  DecimalScale money_scale;
  DataQuality quality;
  std::uint64_t run_input_sequence;
  std::int64_t logical_time_nanoseconds;
  VersionRef mark_policy_version;
};

} // namespace chronos::contracts
