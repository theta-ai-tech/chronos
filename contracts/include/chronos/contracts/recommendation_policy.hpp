#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"

namespace chronos::contracts {

struct RecommendationPolicy final {
  VersionRef policy_version;
  VersionRef schema_version;
  VersionRef authority_version;
  AmountUnits minimum_actionable_strength{};
  AmountUnits maximum_indicative_exposure{};
  DecimalScale scale;

  bool operator==(const RecommendationPolicy &) const = default;
};

[[nodiscard]] bool
valid_recommendation_policy(const RecommendationPolicy &policy) noexcept;

[[nodiscard]] Sha256Digest
recommendation_policy_checksum(const RecommendationPolicy &policy) noexcept;

} // namespace chronos::contracts
