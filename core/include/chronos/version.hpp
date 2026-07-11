#pragma once

#include <string_view>

namespace chronos {

// Returns the Chronos build version (from the CMake project version).
// Placeholder surface used by the M0.2 toolchain smoke test.
[[nodiscard]] std::string_view version() noexcept;

// Trivial liveness check: returns true. Exists so the unit-test target has a
// linked symbol from chronos_core to exercise.
[[nodiscard]] bool engine_alive() noexcept;

} // namespace chronos
