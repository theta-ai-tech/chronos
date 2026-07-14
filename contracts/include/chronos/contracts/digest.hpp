#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace chronos::contracts {

struct Sha256Digest final {
  std::array<std::uint8_t, 32> bytes{};

  [[nodiscard]] std::string hex() const;
  bool operator==(const Sha256Digest &) const = default;
};

[[nodiscard]] Sha256Digest sha256(std::span<const std::byte> payload);

} // namespace chronos::contracts
