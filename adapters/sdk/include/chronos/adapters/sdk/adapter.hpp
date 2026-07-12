#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::adapters::sdk {

enum class EnvironmentClass : std::uint8_t { Test, Production };
enum class TransportClass : std::uint8_t { WebSocket };
enum class EndpointClass : std::uint8_t { PublicMarketData };
enum class AuthenticationMode : std::uint8_t { None, PublicCredential };
enum class ChannelFamily : std::uint8_t {
  OrderBookSnapshot,
  OrderBookDelta,
  PublicTrade,
  Heartbeat,
  SourceError,
};
enum class MarketClass : std::uint8_t {
  Spot,
  LinearPerpetual,
  InversePerpetual
};

enum class ConnectionState : std::uint8_t {
  Configured,
  Resolving,
  Connecting,
  TransportEstablished,
  ProtocolNegotiating,
  SessionReady,
  Subscribing,
  Active,
  Draining,
  Closed,
  Degraded,
  ReconnectWait,
  Recovering,
  Failed,
};

enum class HealthState : std::uint8_t {
  Unknown,
  Starting,
  Healthy,
  Degraded,
  Gapped,
  Recovering,
  Incompatible,
  Stalled,
  Failed,
};

struct ResourceLimits final {
  std::size_t maximum_frame_bytes{};
  std::size_t maximum_message_bytes{};
  std::size_t maximum_nesting_depth{};
  std::size_t maximum_expansion_ratio{};
  std::size_t maximum_subscriptions{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return maximum_frame_bytes > 0 && maximum_message_bytes > 0 &&
           maximum_frame_bytes <= maximum_message_bytes &&
           maximum_nesting_depth > 0 && maximum_expansion_ratio > 0 &&
           maximum_subscriptions > 0;
  }
};

struct RetryPolicy final {
  std::uint32_t maximum_attempts{};
  std::chrono::milliseconds initial_delay{};
  std::chrono::milliseconds maximum_delay{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return maximum_attempts > 0 && initial_delay.count() > 0 &&
           maximum_delay >= initial_delay;
  }
};

struct CapabilityManifest final {
  std::string adapter_id;
  std::string implementation_version;
  std::string build_version;
  std::string venue;
  EnvironmentClass environment{EnvironmentClass::Test};
  std::vector<TransportClass> transports;
  std::vector<EndpointClass> endpoints;
  AuthenticationMode public_authentication{AuthenticationMode::None};
  std::vector<ChannelFamily> channels;
  std::vector<MarketClass> markets;
  bool supports_source_sequences{};
  bool supports_resume{};
  bool supports_replay{};
  bool supports_compression{};
  bool supports_private_feeds{};
  bool supports_order_entry{};
  bool supports_level_three{};
  ResourceLimits limits;
  std::vector<std::string> schema_versions;
  std::string conformance_suite_version;
  std::string conformance_result_id;

  [[nodiscard]] bool valid() const noexcept;
};

struct CapabilityRequest final {
  EnvironmentClass environment{EnvironmentClass::Test};
  TransportClass transport{TransportClass::WebSocket};
  EndpointClass endpoint{EndpointClass::PublicMarketData};
  std::vector<ChannelFamily> required_channels;
  MarketClass market{MarketClass::Spot};
  std::string schema_version;
  std::size_t subscription_count{};
  bool require_resume{};
  bool require_replay{};
};

enum class NegotiationFailure : std::uint8_t {
  InvalidManifest,
  EnvironmentUnsupported,
  TransportUnsupported,
  EndpointUnsupported,
  ChannelUnsupported,
  MarketUnsupported,
  SchemaUnsupported,
  SubscriptionLimitExceeded,
  ResumeUnsupported,
  ReplayUnsupported,
};

[[nodiscard]] std::optional<NegotiationFailure>
negotiate(const CapabilityManifest &manifest,
          const CapabilityRequest &request) noexcept;

class SourceAdapter {
public:
  virtual ~SourceAdapter() = default;
  [[nodiscard]] virtual const CapabilityManifest &manifest() const noexcept = 0;
  [[nodiscard]] virtual ConnectionState connection_state() const noexcept = 0;
  [[nodiscard]] virtual HealthState health_state() const noexcept = 0;
  [[nodiscard]] virtual std::optional<NegotiationFailure>
  configure(const CapabilityRequest &request) noexcept = 0;
  virtual bool start() noexcept = 0;
  virtual void stop() noexcept = 0;
};

namespace detail {
template <typename T>
[[nodiscard]] bool contains(std::span<const T> values, T wanted) noexcept {
  return std::find(values.begin(), values.end(), wanted) != values.end();
}

[[nodiscard]] inline bool valid_token(std::string_view value) noexcept {
  if (value.empty()) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') || character == '.' ||
           character == '_' || character == '-';
  });
}
} // namespace detail

inline bool CapabilityManifest::valid() const noexcept {
  if (!detail::valid_token(adapter_id) ||
      !detail::valid_token(implementation_version) ||
      !detail::valid_token(build_version) || !detail::valid_token(venue) ||
      transports.empty() || endpoints.empty() || channels.empty() ||
      markets.empty() || schema_versions.empty() || !limits.valid() ||
      !detail::valid_token(conformance_suite_version) ||
      !detail::valid_token(conformance_result_id)) {
    return false;
  }
  return std::all_of(schema_versions.begin(), schema_versions.end(),
                     detail::valid_token);
}

inline std::optional<NegotiationFailure>
negotiate(const CapabilityManifest &manifest,
          const CapabilityRequest &request) noexcept {
  if (!manifest.valid()) {
    return NegotiationFailure::InvalidManifest;
  }
  if (manifest.environment != request.environment) {
    return NegotiationFailure::EnvironmentUnsupported;
  }
  if (!detail::contains<TransportClass>(manifest.transports,
                                        request.transport)) {
    return NegotiationFailure::TransportUnsupported;
  }
  if (!detail::contains<EndpointClass>(manifest.endpoints, request.endpoint)) {
    return NegotiationFailure::EndpointUnsupported;
  }
  for (const auto channel : request.required_channels) {
    if (!detail::contains<ChannelFamily>(manifest.channels, channel)) {
      return NegotiationFailure::ChannelUnsupported;
    }
  }
  if (!detail::contains<MarketClass>(manifest.markets, request.market)) {
    return NegotiationFailure::MarketUnsupported;
  }
  if (!detail::contains<std::string>(manifest.schema_versions,
                                     request.schema_version)) {
    return NegotiationFailure::SchemaUnsupported;
  }
  if (request.subscription_count == 0 ||
      request.subscription_count > manifest.limits.maximum_subscriptions) {
    return NegotiationFailure::SubscriptionLimitExceeded;
  }
  if (request.require_resume && !manifest.supports_resume) {
    return NegotiationFailure::ResumeUnsupported;
  }
  if (request.require_replay && !manifest.supports_replay) {
    return NegotiationFailure::ReplayUnsupported;
  }
  return std::nullopt;
}

} // namespace chronos::adapters::sdk
