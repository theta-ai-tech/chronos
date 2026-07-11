#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace chronos {

inline constexpr std::size_t kMaxHelloPayloadBytes = 4096;

// M0.4 scaffold boundary: round-trip a compact, versioned "hello event" payload
// through C++ so Python can prove the native core is callable.
[[nodiscard]] std::string round_trip_hello_event(std::string_view payload);

} // namespace chronos

extern "C" {

#if defined(_WIN32)
#if defined(CHRONOS_BOUNDARY_BUILD)
#define CHRONOS_BOUNDARY_EXPORT __declspec(dllexport)
#else
#define CHRONOS_BOUNDARY_EXPORT __declspec(dllimport)
#endif
#else
#define CHRONOS_BOUNDARY_EXPORT __attribute__((visibility("default")))
#endif

// Status codes for the C ABI. Kept as integers so Python can bind them through
// ctypes without sharing C++ runtime types across the boundary.
constexpr int CHRONOS_BOUNDARY_OK = 0;
constexpr int CHRONOS_BOUNDARY_INVALID_ARGUMENT = 1;
constexpr int CHRONOS_BOUNDARY_OUTPUT_TOO_SMALL = 2;
constexpr int CHRONOS_BOUNDARY_INPUT_TOO_LARGE = 3;
constexpr int CHRONOS_BOUNDARY_RESOURCE_EXHAUSTED = 4;
constexpr int CHRONOS_BOUNDARY_INTERNAL_ERROR = 5;

CHRONOS_BOUNDARY_EXPORT std::size_t
chronos_boundary_max_hello_payload_bytes() noexcept;

CHRONOS_BOUNDARY_EXPORT int chronos_boundary_hello_event_round_trip(
    const char *input, std::size_t input_len, char *output,
    std::size_t output_capacity, std::size_t *output_len) noexcept;
}
