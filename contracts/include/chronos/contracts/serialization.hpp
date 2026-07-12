#pragma once

#include "chronos/contracts/event_envelope.hpp"
#include "chronos/contracts/fixed_point.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace chronos::contracts {

inline constexpr std::size_t kMaxConformanceFrameBytes = 64 * 1024;

struct ConformanceFrame final {
  DecimalScale decimal_scale;
  Price price;
  Quantity quantity;
  Money money;
  EventTypeRegistration registration;
  EventEnvelope envelope;

  bool operator==(const ConformanceFrame &) const = default;
};

[[nodiscard]] std::vector<std::uint8_t>
encode_conformance_frame(const ConformanceFrame &frame);

[[nodiscard]] std::optional<ConformanceFrame>
decode_conformance_frame(std::span<const std::uint8_t> bytes) noexcept;

} // namespace chronos::contracts
