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
          .require_resume = false,
          .require_replay = false};
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
  unsupported.require_resume = true;
  CHECK(adapter.configure(unsupported) ==
        sdk::NegotiationFailure::ResumeUnsupported);
  CHECK(!adapter.start());
  CHECK(adapter.health_state() == sdk::HealthState::Incompatible);

  unsupported = request();
  unsupported.subscription_count = 100;
  CHECK(adapter.configure(unsupported) ==
        sdk::NegotiationFailure::SubscriptionLimitExceeded);
}
