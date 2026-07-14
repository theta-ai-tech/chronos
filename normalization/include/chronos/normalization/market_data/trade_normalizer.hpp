#pragma once

#include "chronos/core/reference_data/reference_data.hpp"
#include "chronos/normalization/market_data/trade.hpp"

#include <string>
#include <vector>

namespace chronos::normalization::market_data {

struct TradeNormalizerVersions final {
  std::string normalizer_version;
};

struct TradeNormalizationResult final {
  std::vector<NormalizedTradeFact> facts;
  TradeNormalizationFailure failure{TradeNormalizationFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return !facts.empty() && failure == TradeNormalizationFailure::None;
  }
};

[[nodiscard]] TradeNormalizationResult
normalize_trades(const DecodedTradeEnrichment &enrichment,
                 const core::reference_data::ReferenceConfigurationLineage
                     &reference_lineage,
                 contracts::ClockDomainId source_wall_clock_domain_id,
                 const TradeNormalizerVersions &versions);

} // namespace chronos::normalization::market_data
