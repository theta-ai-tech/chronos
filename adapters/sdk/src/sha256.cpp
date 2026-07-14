#include "chronos/adapters/sdk/source_event.hpp"

#include "chronos/contracts/digest.hpp"

namespace chronos::adapters::sdk {

PayloadDigest sha256(std::span<const std::byte> payload,
                     DigestCoverage coverage) {
  const auto digest = contracts::sha256(payload);
  return {.bytes = digest.bytes, .coverage = coverage};
}

} // namespace chronos::adapters::sdk
