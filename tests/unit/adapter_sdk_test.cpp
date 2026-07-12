#include "chronos/adapters/market_data/bybit_adapter.hpp"
#include "chronos/adapters/sdk/adapter.hpp"

#include "microtest.hpp"

#include <type_traits>

namespace {
using chronos::adapters::market_data::BybitAdapter;
namespace sdk = chronos::adapters::sdk;

sdk::CapabilityRequest request() {
  return {.environment = sdk::EnvironmentClass::Test,
          .transport = sdk::TransportClass::WebSocket,
          .endpoint = sdk::EndpointClass::PublicMarketData,
          .required_channels = {sdk::ChannelFamily::OrderBookSnapshot,
                                sdk::ChannelFamily::OrderBookDelta,
                                sdk::ChannelFamily::PublicTrade},
          .market = sdk::MarketClass::LinearPerpetual,
          .schema_version = "bybit-v5-public-v1",
          .subscription_count = 3,
          .framing = sdk::FramingMode::TextMessage,
          .compression = sdk::CompressionMode::None,
          .recovery = sdk::RecoveryMode::NewSession,
          .required_source_sequences =
              {{.field_name = "seq",
                .scope = sdk::SourceSequenceScope::VenueCrossSequence,
                .monotonic = true,
                .duplicates_possible = true}},
          .required_limits = {.maximum_frame_bytes = 1U << 20U,
                              .maximum_message_bytes = 1U << 20U,
                              .maximum_nesting_depth = 64,
                              .maximum_expansion_ratio = 1,
                              .maximum_subscriptions = 3}};
}
} // namespace

TEST_CASE("Bybit implements the venue-neutral source adapter SDK") {
  static_assert(std::is_base_of_v<sdk::SourceAdapter, BybitAdapter>);
  BybitAdapter adapter(sdk::EnvironmentClass::Test);
  CHECK(adapter.manifest().valid());
  CHECK(!adapter.manifest().supports_private_feeds);
  CHECK(!adapter.manifest().supports_order_entry);
  CHECK(!adapter.manifest().supports_level_three);
  CHECK(!adapter.configure(request()).has_value());
  CHECK(adapter.start());
  CHECK(adapter.connection_state() == sdk::ConnectionState::Connecting);
  adapter.stop();
  CHECK(adapter.connection_state() == sdk::ConnectionState::Closed);
}

TEST_CASE("capability negotiation fails closed before activation") {
  BybitAdapter adapter(sdk::EnvironmentClass::Test);
  auto unsupported = request();
  unsupported.recovery = sdk::RecoveryMode::ProvenResume;
  CHECK(adapter.configure(unsupported) ==
        sdk::NegotiationFailure::RecoveryUnsupported);
  CHECK(!adapter.start());
  CHECK(adapter.health_state(sdk::HealthScope::Transport) ==
        sdk::HealthState::Incompatible);

  unsupported = request();
  unsupported.subscription_count = 100;
  CHECK(adapter.configure(unsupported) ==
        sdk::NegotiationFailure::SubscriptionLimitExceeded);
}

TEST_CASE("unknown capabilities and active reconfiguration fail closed") {
  BybitAdapter adapter(sdk::EnvironmentClass::Test);
  auto invalid = request();
  invalid.required_channels = {static_cast<sdk::ChannelFamily>(255)};
  CHECK(adapter.configure(invalid) == sdk::NegotiationFailure::InvalidRequest);

  CHECK(!adapter.configure(request()).has_value());
  CHECK(adapter.start());
  CHECK(adapter.configure(request()) ==
        sdk::NegotiationFailure::LifecycleUnavailable);
}

TEST_CASE("sequence negotiation requires matching field semantics and scope") {
  BybitAdapter adapter(sdk::EnvironmentClass::Test);
  auto incompatible = request();
  incompatible.required_source_sequences.front().scope =
      sdk::SourceSequenceScope::ListingChannel;
  CHECK(adapter.configure(incompatible) ==
        sdk::NegotiationFailure::SourceSequencesUnsupported);
}
