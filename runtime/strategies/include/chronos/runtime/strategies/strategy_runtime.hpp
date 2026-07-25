#pragma once

#include "chronos/strategies/sdk/strategy_host.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace chronos::runtime::strategies {

struct StrategyRuntimeConfig final {
  contracts::RunId run_id;
  contracts::StrategyInstanceId strategy_instance_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  chronos::strategies::sdk::AcceptedStrategyDefinition definition;
  std::optional<chronos::strategies::sdk::StrategyParameter> parameter;
  contracts::EventId activation_control_outcome_id;
  std::uint64_t active_configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  contracts::StreamId run_control_stream_id;
  std::uint64_t run_control_stream_epoch{};
  contracts::StreamId run_timer_stream_id;
  std::uint64_t run_timer_stream_epoch{};
  std::uint64_t maximum_operations{};
  std::optional<std::int64_t> logical_deadline_nanoseconds;
};

class StrategyRuntime final {
public:
  [[nodiscard]] static std::optional<StrategyRuntime>
  activate(StrategyRuntimeConfig config) noexcept;

  [[nodiscard]] std::optional<
      chronos::strategies::sdk::AcceptedStrategyInvocation>
  admit(const core::features::AcceptedFeatureEvaluationCut &feature_cut)
      const noexcept;

  [[nodiscard]] contracts::Sha256Digest activation_checksum() const noexcept {
    return activation_checksum_;
  }

private:
  StrategyRuntime(StrategyRuntimeConfig config,
                  contracts::Sha256Digest activation_checksum) noexcept
      : config_(std::move(config)), activation_checksum_(activation_checksum) {}

  StrategyRuntimeConfig config_;
  contracts::Sha256Digest activation_checksum_;
};

} // namespace chronos::runtime::strategies
