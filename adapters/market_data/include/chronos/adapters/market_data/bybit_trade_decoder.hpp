#pragma once

#include "chronos/adapters/market_data/capture_dataset.hpp"
#include "chronos/normalization/market_data/trade.hpp"

#include <cstddef>
#include <optional>

namespace chronos::adapters::market_data {

struct BybitTradeDecodeLimits final {
  std::size_t maximum_payload_bytes{1U << 20U};
  std::size_t maximum_trades_per_message{1024};
  std::size_t maximum_json_depth{16};
  std::size_t maximum_json_nodes{16384};
  std::size_t maximum_object_members{64};
  std::size_t maximum_string_bytes{128};
  std::size_t maximum_number_bytes{32};
};

struct BybitTradeDecodeResult final {
  std::optional<normalization::market_data::DecodedTradeEnrichment> enrichment;
  normalization::market_data::TradeNormalizationFailure failure{
      normalization::market_data::TradeNormalizationFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return enrichment.has_value() &&
           failure ==
               normalization::market_data::TradeNormalizationFailure::None;
  }
};

[[nodiscard]] BybitTradeDecodeResult
decode_bybit_v5_trades(const DatasetReadResult &dataset,
                       std::size_t record_index,
                       const BybitTradeDecodeLimits &limits = {});

} // namespace chronos::adapters::market_data
