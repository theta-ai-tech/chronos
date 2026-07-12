#include "chronos/boundary.hpp"
#include "chronos/contracts/serialization.hpp"

#include <cstring>
#include <new>
#include <string_view>

std::size_t chronos_boundary_max_hello_payload_bytes() noexcept {
  return chronos::kMaxHelloPayloadBytes;
}

std::size_t chronos_boundary_max_contract_frame_bytes() noexcept {
  return chronos::contracts::kMaxConformanceFrameBytes;
}

int chronos_boundary_contract_round_trip(const std::uint8_t *input,
                                         std::size_t input_len,
                                         std::uint8_t *output,
                                         std::size_t output_capacity,
                                         std::size_t *output_len) noexcept {
  if (output_len == nullptr) {
    return CHRONOS_BOUNDARY_INVALID_ARGUMENT;
  }
  *output_len = 0;
  if (input == nullptr && input_len > 0) {
    return CHRONOS_BOUNDARY_INVALID_ARGUMENT;
  }
  if (input_len > chronos::contracts::kMaxConformanceFrameBytes) {
    return CHRONOS_BOUNDARY_INPUT_TOO_LARGE;
  }
  try {
    const auto bytes = std::span<const std::uint8_t>{input, input_len};
    const auto frame = chronos::contracts::decode_conformance_frame(bytes);
    if (!frame.has_value()) {
      return CHRONOS_BOUNDARY_INVALID_CONTRACT;
    }
    const auto encoded = chronos::contracts::encode_conformance_frame(*frame);
    *output_len = encoded.size();
    if (output == nullptr || output_capacity < encoded.size()) {
      return CHRONOS_BOUNDARY_OUTPUT_TOO_SMALL;
    }
    std::memcpy(output, encoded.data(), encoded.size());
    return CHRONOS_BOUNDARY_OK;
  } catch (const std::bad_alloc &) {
    *output_len = 0;
    return CHRONOS_BOUNDARY_RESOURCE_EXHAUSTED;
  } catch (...) {
    *output_len = 0;
    return CHRONOS_BOUNDARY_INTERNAL_ERROR;
  }
}

int chronos_boundary_hello_event_round_trip(const char *input,
                                            std::size_t input_len, char *output,
                                            std::size_t output_capacity,
                                            std::size_t *output_len) noexcept {
  if (output_len == nullptr) {
    return CHRONOS_BOUNDARY_INVALID_ARGUMENT;
  }
  *output_len = 0;
  if (input == nullptr && input_len > 0) {
    return CHRONOS_BOUNDARY_INVALID_ARGUMENT;
  }
  if (input_len > chronos::kMaxHelloPayloadBytes) {
    return CHRONOS_BOUNDARY_INPUT_TOO_LARGE;
  }

  try {
    const std::string_view payload{input == nullptr ? "" : input, input_len};
    const std::string response = chronos::round_trip_hello_event(payload);
    *output_len = response.size();

    if (output == nullptr || output_capacity < response.size()) {
      return CHRONOS_BOUNDARY_OUTPUT_TOO_SMALL;
    }

    std::memcpy(output, response.data(), response.size());
    if (output_capacity > response.size()) {
      output[response.size()] = '\0';
    }
    return CHRONOS_BOUNDARY_OK;
  } catch (const std::bad_alloc &) {
    *output_len = 0;
    return CHRONOS_BOUNDARY_RESOURCE_EXHAUSTED;
  } catch (...) {
    *output_len = 0;
    return CHRONOS_BOUNDARY_INTERNAL_ERROR;
  }
}
