#pragma once

#include "chronos/core/reference_data/reference_data.hpp"
#include "chronos/normalization/market_data/book.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace chronos::normalization::market_data {

struct BookNormalizerVersions final {
  std::string decoder_version;
  std::string source_schema_version;
  std::string normalizer_version;
};

struct BookNormalizationResult final {
  std::optional<NormalizedBookFact> fact;
  BookNormalizationFailure failure{BookNormalizationFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return fact.has_value() && failure == BookNormalizationFailure::None;
  }
};

[[nodiscard]] BookNormalizationResult
normalize_book(const DecodedBookMessage &message,
               const SourceCaptureLineage &lineage,
               std::string_view source_venue,
               const core::reference_data::ReferenceSnapshot &reference,
               core::reference_data::EffectiveDomainId effective_domain_id,
               contracts::ClockDomainId source_wall_clock_domain_id,
               const BookNormalizerVersions &versions);

} // namespace chronos::normalization::market_data
