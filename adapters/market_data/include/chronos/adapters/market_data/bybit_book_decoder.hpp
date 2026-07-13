#pragma once

#include "chronos/adapters/market_data/capture_dataset.hpp"
#include "chronos/normalization/market_data/book.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace chronos::adapters::market_data {

struct BybitBookDecodeLimits final {
  std::uint32_t expected_depth{50};
  std::size_t maximum_payload_bytes{1U << 20U};
  std::size_t maximum_levels_per_side{1000};
  std::size_t maximum_json_depth{16};
  std::size_t maximum_json_nodes{8192};
  std::size_t maximum_object_members{64};
  std::size_t maximum_string_bytes{128};
  std::size_t maximum_number_bytes{32};
  std::size_t maximum_extension_bytes{1U << 20U};
};

struct BybitBookDecodePolicy final {
  normalization::market_data::SourceProductClass product_class{
      normalization::market_data::SourceProductClass::Spot};
};

struct BybitBookDecodeResult final {
  std::optional<normalization::market_data::DecodedBookEnrichment> enrichment;
  normalization::market_data::BookNormalizationFailure failure{
      normalization::market_data::BookNormalizationFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return enrichment.has_value() &&
           failure ==
               normalization::market_data::BookNormalizationFailure::None;
  }
};

[[nodiscard]] BybitBookDecodeResult
decode_bybit_v5_book(const DatasetReadResult &dataset, std::size_t record_index,
                     const BybitBookDecodePolicy &policy,
                     const BybitBookDecodeLimits &limits = {});

} // namespace chronos::adapters::market_data
