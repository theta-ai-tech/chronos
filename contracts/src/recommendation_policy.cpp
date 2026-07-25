#include "chronos/contracts/recommendation_policy.hpp"

#include <array>
#include <string_view>
#include <type_traits>

namespace chronos::contracts {
namespace {

struct CanonicalPolicy final {
  std::array<std::byte, 128> bytes{};
  std::size_t size{};

  void append(std::string_view value) noexcept {
    for (const auto character : value)
      bytes[size++] = static_cast<std::byte>(character);
  }

  template <typename Id> void append_id(const Id &value) noexcept {
    for (const auto byte : value.bytes())
      bytes[size++] = static_cast<std::byte>(byte);
  }

  template <typename Integer> void append_integer(Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto converted = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
      const auto shift = (sizeof(Integer) - index - 1) * 8U;
      bytes[size++] =
          static_cast<std::byte>((converted >> shift) & Unsigned{0xFF});
    }
  }

  void append_version(const VersionRef &value) noexcept {
    append_id(value.definition_id());
    append_integer(value.version());
  }
};

} // namespace

bool valid_recommendation_policy(const RecommendationPolicy &policy) noexcept {
  return policy.minimum_actionable_strength > 0 &&
         policy.maximum_indicative_exposure > 0 &&
         policy.minimum_actionable_strength <=
             policy.maximum_indicative_exposure &&
         policy.maximum_indicative_exposure <= policy.scale.denominator();
}

Sha256Digest
recommendation_policy_checksum(const RecommendationPolicy &policy) noexcept {
  CanonicalPolicy canonical;
  canonical.append("chronos.recommendation-policy.v1");
  canonical.append_version(policy.policy_version);
  canonical.append_version(policy.schema_version);
  canonical.append_version(policy.authority_version);
  canonical.append_integer(policy.minimum_actionable_strength);
  canonical.append_integer(policy.maximum_indicative_exposure);
  canonical.append_integer(policy.scale.exponent());
  return sha256(
      std::span<const std::byte>(canonical.bytes).first(canonical.size));
}

} // namespace chronos::contracts
