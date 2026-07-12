#include "chronos/adapters/market_data/bybit_adapter.hpp"

#include <utility>

namespace chronos::adapters::market_data {

BybitAdapter::BybitAdapter(sdk::EnvironmentClass environment)
    : manifest_{.adapter_id = "chronos.bybit.public-market-data",
                .implementation_version = "m2.1",
                .build_version = "development",
                .venue = "bybit",
                .environment = environment,
                .transports = {sdk::TransportClass::WebSocket},
                .endpoints = {sdk::EndpointClass::PublicMarketData},
                .public_authentication = sdk::AuthenticationMode::None,
                .channels = {sdk::ChannelFamily::OrderBookSnapshot,
                             sdk::ChannelFamily::OrderBookDelta,
                             sdk::ChannelFamily::PublicTrade,
                             sdk::ChannelFamily::Heartbeat,
                             sdk::ChannelFamily::SourceError},
                .markets = {sdk::MarketClass::Spot,
                            sdk::MarketClass::LinearPerpetual,
                            sdk::MarketClass::InversePerpetual},
                .supports_source_sequences = true,
                .framing_modes = {sdk::FramingMode::TextMessage},
                .compression_modes = {sdk::CompressionMode::None},
                .recovery_modes = {sdk::RecoveryMode::NewSession},
                .supports_private_feeds = false,
                .supports_order_entry = false,
                .supports_level_three = false,
                .limits = {.maximum_frame_bytes = 1U << 20U,
                           .maximum_message_bytes = 1U << 20U,
                           .maximum_nesting_depth = 64,
                           .maximum_expansion_ratio = 1,
                           .maximum_subscriptions = 10},
                .schema_versions = {"bybit-v5-public-v1"},
                .conformance_suite_version = "adapter-sdk-v1",
                .conformance_result_id = "pending-m2.1"} {}

const sdk::CapabilityManifest &BybitAdapter::manifest() const noexcept {
  return manifest_;
}

sdk::ConnectionState BybitAdapter::connection_state() const noexcept {
  return connection_state_;
}

sdk::HealthState
BybitAdapter::health_state(sdk::HealthScope scope) const noexcept {
  const auto index = static_cast<std::size_t>(scope);
  return index < health_states_.size() ? health_states_[index]
                                       : sdk::HealthState::Unknown;
}

std::optional<sdk::NegotiationFailure>
BybitAdapter::configure(const sdk::CapabilityRequest &request) noexcept {
  if (connection_state_ != sdk::ConnectionState::Configured) {
    return sdk::NegotiationFailure::LifecycleUnavailable;
  }
  const auto failure = sdk::negotiate(manifest_, request);
  configured_ = !failure.has_value();
  health_states_.fill(configured_ ? sdk::HealthState::Starting
                                  : sdk::HealthState::Incompatible);
  return failure;
}

bool BybitAdapter::start() noexcept {
  if (!configured_ || connection_state_ != sdk::ConnectionState::Configured) {
    return false;
  }
  connection_state_ = sdk::ConnectionState::Connecting;
  health_states_.fill(sdk::HealthState::Starting);
  return true;
}

void BybitAdapter::stop() noexcept {
  connection_state_ = sdk::ConnectionState::Closed;
  health_states_.fill(sdk::HealthState::Unknown);
}

} // namespace chronos::adapters::market_data
