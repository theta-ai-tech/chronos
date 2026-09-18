#pragma once

#include <cstdint>
#include <optional>

namespace chronos::core::risk::detail {

struct QuantityOnlyArithmetic final {
  std::int64_t requested_delta{};
  std::int64_t requested_projected{};
  std::int64_t requested_absolute{};
  std::int64_t requested_delta_absolute{};
  std::int64_t expiry{};

  bool operator==(const QuantityOnlyArithmetic &) const = default;
};

[[nodiscard]] std::optional<QuantityOnlyArithmetic>
evaluate_quantity_only_arithmetic(std::int64_t desired_target,
                                  std::int64_t account_current,
                                  std::int64_t projected_before,
                                  std::int64_t cut_time,
                                  std::int64_t decision_duration,
                                  std::int64_t target_expiry);

} // namespace chronos::core::risk::detail
