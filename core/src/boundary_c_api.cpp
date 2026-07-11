#include "chronos/boundary.hpp"

#include <cstring>
#include <new>
#include <string_view>

std::size_t chronos_boundary_max_hello_payload_bytes() noexcept {
  return chronos::kMaxHelloPayloadBytes;
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
