#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace chronos::adapters::sdk {

enum class EnvironmentClass : std::uint8_t { Test, Production };
enum class TransportClass : std::uint8_t { WebSocket };
enum class EndpointClass : std::uint8_t { PublicMarketData };
enum class AuthenticationMode : std::uint8_t { None, PublicCredential };
enum class FramingMode : std::uint8_t { TextMessage };
enum class CompressionMode : std::uint8_t { None, PerMessageDeflate };
enum class RecoveryMode : std::uint8_t {
  NewSession,
  ProvenResume,
  SourceReplay
};
enum class SourceSequenceScope : std::uint8_t {
  Connection,
  SourceSession,
  Listing,
  ListingChannel,
  VenueCrossSequence,
};
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
enum class HealthScope : std::uint8_t {
  Transport,
  SourceSession,
  Subscription,
  Capture,
  Continuity,
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

struct SourceSequenceCapability final {
  std::string field_name;
  ChannelFamily channel{ChannelFamily::OrderBookDelta};
  SourceSequenceScope scope{SourceSequenceScope::ListingChannel};
  bool monotonic{};
  bool duplicates_possible{};

  bool operator==(const SourceSequenceCapability &) const = default;
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
  std::vector<SourceSequenceCapability> source_sequences;
  std::vector<FramingMode> framing_modes;
  std::vector<CompressionMode> compression_modes;
  std::vector<RecoveryMode> recovery_modes;
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
  FramingMode framing{FramingMode::TextMessage};
  CompressionMode compression{CompressionMode::None};
  RecoveryMode recovery{RecoveryMode::NewSession};
  std::vector<SourceSequenceCapability> required_source_sequences;
  ResourceLimits required_limits;
};

enum class NegotiationFailure : std::uint8_t {
  InvalidManifest,
  InvalidRequest,
  LifecycleUnavailable,
  EnvironmentUnsupported,
  TransportUnsupported,
  EndpointUnsupported,
  ChannelUnsupported,
  MarketUnsupported,
  SchemaUnsupported,
  SubscriptionLimitExceeded,
  FramingUnsupported,
  CompressionUnsupported,
  SourceSequencesUnsupported,
  ResourceLimitUnsupported,
  RecoveryUnsupported,
};

[[nodiscard]] std::optional<NegotiationFailure>
negotiate(const CapabilityManifest &manifest,
          const CapabilityRequest &request) noexcept;

class SourceAdapter {
public:
  virtual ~SourceAdapter() = default;
  [[nodiscard]] virtual const CapabilityManifest &manifest() const noexcept = 0;
  [[nodiscard]] virtual ConnectionState connection_state() const noexcept = 0;
  [[nodiscard]] virtual HealthState
  health_state(HealthScope scope) const noexcept = 0;
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

template <typename T> [[nodiscard]] constexpr bool known(T value) noexcept {
  if constexpr (std::is_same_v<T, EnvironmentClass>) {
    return value == EnvironmentClass::Test ||
           value == EnvironmentClass::Production;
  } else if constexpr (std::is_same_v<T, TransportClass>) {
    return value == TransportClass::WebSocket;
  } else if constexpr (std::is_same_v<T, EndpointClass>) {
    return value == EndpointClass::PublicMarketData;
  } else if constexpr (std::is_same_v<T, AuthenticationMode>) {
    return value == AuthenticationMode::None ||
           value == AuthenticationMode::PublicCredential;
  } else if constexpr (std::is_same_v<T, ChannelFamily>) {
    return value >= ChannelFamily::OrderBookSnapshot &&
           value <= ChannelFamily::SourceError;
  } else if constexpr (std::is_same_v<T, MarketClass>) {
    return value >= MarketClass::Spot && value <= MarketClass::InversePerpetual;
  } else if constexpr (std::is_same_v<T, FramingMode>) {
    return value == FramingMode::TextMessage;
  } else if constexpr (std::is_same_v<T, CompressionMode>) {
    return value == CompressionMode::None ||
           value == CompressionMode::PerMessageDeflate;
  } else if constexpr (std::is_same_v<T, RecoveryMode>) {
    return value >= RecoveryMode::NewSession &&
           value <= RecoveryMode::SourceReplay;
  } else if constexpr (std::is_same_v<T, SourceSequenceScope>) {
    return value >= SourceSequenceScope::Connection &&
           value <= SourceSequenceScope::VenueCrossSequence;
  }
  return false;
}

template <typename T>
[[nodiscard]] bool all_known(std::span<const T> values) noexcept {
  return std::all_of(values.begin(), values.end(), known<T>);
}
} // namespace detail

inline bool CapabilityManifest::valid() const noexcept {
  if (!detail::valid_token(adapter_id) ||
      !detail::valid_token(implementation_version) ||
      !detail::valid_token(build_version) || !detail::valid_token(venue) ||
      !detail::known(environment) || !detail::known(public_authentication) ||
      transports.empty() || !detail::all_known<TransportClass>(transports) ||
      endpoints.empty() || !detail::all_known<EndpointClass>(endpoints) ||
      channels.empty() || !detail::all_known<ChannelFamily>(channels) ||
      markets.empty() || !detail::all_known<MarketClass>(markets) ||
      framing_modes.empty() || !detail::all_known<FramingMode>(framing_modes) ||
      compression_modes.empty() ||
      !detail::all_known<CompressionMode>(compression_modes) ||
      recovery_modes.empty() ||
      !detail::all_known<RecoveryMode>(recovery_modes) ||
      schema_versions.empty() || !limits.valid() ||
      !detail::valid_token(conformance_suite_version) ||
      !detail::valid_token(conformance_result_id)) {
    return false;
  }
  if (std::any_of(source_sequences.begin(), source_sequences.end(),
                  [](const SourceSequenceCapability &sequence) {
                    return !detail::valid_token(sequence.field_name) ||
                           !detail::known(sequence.channel) ||
                           !detail::known(sequence.scope);
                  })) {
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
  if (!detail::known(request.environment) ||
      !detail::known(request.transport) || !detail::known(request.endpoint) ||
      !detail::known(request.market) || !detail::known(request.framing) ||
      !detail::known(request.compression) || !detail::known(request.recovery) ||
      request.required_channels.empty() ||
      !detail::all_known<ChannelFamily>(request.required_channels) ||
      !detail::valid_token(request.schema_version) ||
      !request.required_limits.valid()) {
    return NegotiationFailure::InvalidRequest;
  }
  if (std::any_of(request.required_source_sequences.begin(),
                  request.required_source_sequences.end(),
                  [](const SourceSequenceCapability &sequence) {
                    return !detail::valid_token(sequence.field_name) ||
                           !detail::known(sequence.channel) ||
                           !detail::known(sequence.scope);
                  })) {
    return NegotiationFailure::InvalidRequest;
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
  if (!detail::contains<FramingMode>(manifest.framing_modes, request.framing)) {
    return NegotiationFailure::FramingUnsupported;
  }
  if (!detail::contains<CompressionMode>(manifest.compression_modes,
                                         request.compression)) {
    return NegotiationFailure::CompressionUnsupported;
  }
  for (const auto &required : request.required_source_sequences) {
    if (std::find(manifest.source_sequences.begin(),
                  manifest.source_sequences.end(),
                  required) == manifest.source_sequences.end()) {
      return NegotiationFailure::SourceSequencesUnsupported;
    }
  }
  if (request.required_limits.maximum_frame_bytes >
          manifest.limits.maximum_frame_bytes ||
      request.required_limits.maximum_message_bytes >
          manifest.limits.maximum_message_bytes ||
      request.required_limits.maximum_nesting_depth >
          manifest.limits.maximum_nesting_depth ||
      request.required_limits.maximum_expansion_ratio >
          manifest.limits.maximum_expansion_ratio ||
      request.required_limits.maximum_subscriptions >
          manifest.limits.maximum_subscriptions) {
    return NegotiationFailure::ResourceLimitUnsupported;
  }
  if (!detail::contains<RecoveryMode>(manifest.recovery_modes,
                                      request.recovery)) {
    return NegotiationFailure::RecoveryUnsupported;
  }
  return std::nullopt;
}

} // namespace chronos::adapters::sdk
