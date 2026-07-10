#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace chronos {

// M0.4 scaffold boundary: round-trip a compact, versioned "hello event" payload
// through C++ so Python can prove the native core is callable.
[[nodiscard]] std::string round_trip_hello_event(std::string_view payload);

}  // namespace chronos

extern "C" {

// Status codes for the C ABI. Kept as integers so Python can bind them through
// ctypes without sharing C++ runtime types across the boundary.
constexpr int CHRONOS_BOUNDARY_OK = 0;
constexpr int CHRONOS_BOUNDARY_INVALID_ARGUMENT = 1;
constexpr int CHRONOS_BOUNDARY_OUTPUT_TOO_SMALL = 2;

int chronos_boundary_hello_event_round_trip(
    const char* input, std::size_t input_len,
    char* output, std::size_t output_capacity,
    std::size_t* output_len) noexcept;

}
