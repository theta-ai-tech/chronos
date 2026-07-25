#pragma once

#include "chronos/strategies/sdk/strategy.hpp"

#include <optional>

namespace chronos::strategies::conformance {

[[nodiscard]] std::optional<sdk::AcceptedStrategyDefinition>
accepted_threshold_definition() noexcept;

} // namespace chronos::strategies::conformance
