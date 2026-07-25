#pragma once

#include "chronos/core/dispatch/run_input_dispatcher.hpp"
#include "chronos/strategies/sdk/strategy_host.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace chronos::runtime::strategies {

struct StrategyRuntimeConfig final {
  contracts::StrategyInstanceId strategy_instance_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  chronos::strategies::sdk::AcceptedStrategyDefinition definition;
  std::optional<chronos::strategies::sdk::StrategyParameter> parameter;
  contracts::VersionRef recommendation_policy_version;
  contracts::Sha256Digest recommendation_policy_checksum;
  contracts::StreamId run_control_stream_id;
  std::uint64_t run_control_stream_epoch{};
  contracts::StreamId run_timer_stream_id;
  std::uint64_t run_timer_stream_epoch{};
  std::uint64_t maximum_operations{};
  std::optional<std::int64_t> logical_deadline_offset_nanoseconds;
};

[[nodiscard]] std::vector<std::byte>
encode_strategy_activation_control(const StrategyRuntimeConfig &config);

class StrategyRuntime final {
public:
  [[nodiscard]] static std::optional<StrategyRuntime>
  activate(StrategyRuntimeConfig config,
           const core::dispatch::AcceptedControlOutcome &control) noexcept;

  [[nodiscard]] std::optional<
      chronos::strategies::sdk::AcceptedStrategyInvocation>
  admit(const core::features::AcceptedFeatureEvaluationCut &feature_cut)
      const noexcept;

  [[nodiscard]] contracts::Sha256Digest activation_checksum() const noexcept {
    return activation_checksum_;
  }

private:
  StrategyRuntime(StrategyRuntimeConfig config,
                  core::dispatch::AcceptedControlOutcome control,
                  contracts::Sha256Digest activation_checksum) noexcept
      : config_(std::move(config)), control_(std::move(control)),
        activation_checksum_(activation_checksum) {}

  StrategyRuntimeConfig config_;
  core::dispatch::AcceptedControlOutcome control_;
  contracts::Sha256Digest activation_checksum_;
};

} // namespace chronos::runtime::strategies
