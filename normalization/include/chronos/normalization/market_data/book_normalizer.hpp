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

struct ReferenceSelectionPolicy final {
  contracts::DefinitionId reference_configuration_lineage_id;
  std::string lineage_schema_version;
  std::string semantic_key_policy_version;
  std::string effective_basis_policy_version;
  std::string selection_policy_version;
};

struct BookNormalizationResult final {
  std::optional<NormalizedBookFact> fact;
  BookNormalizationFailure failure{BookNormalizationFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return fact.has_value() && failure == BookNormalizationFailure::None;
  }
};

[[nodiscard]] BookNormalizationResult
normalize_book(const DecodedBookEnrichment &enrichment,
               const core::reference_data::ReferenceSnapshot &reference,
               contracts::ClockDomainId source_wall_clock_domain_id,
               const ReferenceSelectionPolicy &selection_policy,
               const BookNormalizerVersions &versions);

} // namespace chronos::normalization::market_data
