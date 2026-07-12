#pragma once

#include "chronos/adapters/sdk/adapter.hpp"

#include <array>
#include <optional>

namespace chronos::adapters::market_data {

class BybitAdapter final : public sdk::SourceAdapter {
public:
  explicit BybitAdapter(sdk::EnvironmentClass environment);

  [[nodiscard]] const sdk::CapabilityManifest &
  manifest() const noexcept override;
  [[nodiscard]] sdk::ConnectionState connection_state() const noexcept override;
  [[nodiscard]] sdk::HealthState
  health_state(sdk::HealthScope scope) const noexcept override;
  [[nodiscard]] std::optional<sdk::NegotiationFailure>
  configure(const sdk::CapabilityRequest &request) noexcept override;
  bool start() noexcept override;
  void stop() noexcept override;

private:
  sdk::CapabilityManifest manifest_;
  sdk::ConnectionState connection_state_{sdk::ConnectionState::Configured};
  std::array<sdk::HealthState, 5> health_states_{};
  bool configured_{};
};

} // namespace chronos::adapters::market_data
