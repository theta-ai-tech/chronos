#include "chronos/adapters/market_data/bybit_websocket.hpp"

#include "microtest.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace market_data = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
using namespace std::chrono_literals;

class FakeTransport final : public market_data::WebSocketTransport {
public:
  market_data::TransportResult<bool>
  connect(std::string_view url, std::chrono::milliseconds,
          std::size_t maximum_message_bytes) override {
    url_ = url;
    maximum_message_bytes_ = maximum_message_bytes;
    return {.value = true};
  }

  market_data::TransportResult<std::size_t>
  send_text(std::string_view payload, std::chrono::milliseconds) override {
    sent_ = payload;
    return {.value = payload.size()};
  }

  market_data::TransportResult<market_data::WebSocketMessage>
  receive(std::chrono::milliseconds) override {
    return {.value = {.kind = market_data::WebSocketMessageKind::Text,
                      .payload = std::move(next_payload_)}};
  }

  void close() noexcept override { closed_ = true; }

  std::string url_;
  std::string sent_;
  std::size_t maximum_message_bytes_{};
  std::vector<std::byte> next_payload_;
  bool closed_{};
};

market_data::BybitSubscription subscription() {
  return {.environment = sdk::EnvironmentClass::Test,
          .market = sdk::MarketClass::LinearPerpetual,
          .symbol = "BTCUSDT",
          .order_book_depth = 50};
}
} // namespace

TEST_CASE("Bybit endpoint and subscription are exact public V5 contracts") {
  const auto value = subscription();
  CHECK(market_data::valid_bybit_subscription(value));
  CHECK(market_data::bybit_public_websocket_url(value) ==
        "wss://stream-testnet.bybit.com/v5/public/linear");
  CHECK(market_data::bybit_subscription_message(value) ==
        "{\"req_id\":\"chronos-m2\",\"op\":\"subscribe\",\"args\":["
        "\"orderbook.50.BTCUSDT\",\"publicTrade.BTCUSDT\"]}");
}

TEST_CASE(
    "Bybit rejects symbols and depths that could alter protocol framing") {
  auto value = subscription();
  value.symbol = "BTCUSDT\"],\"op\":\"unsubscribe";
  CHECK(!market_data::valid_bybit_subscription(value));
  CHECK(market_data::bybit_subscription_message(value).empty());
  value = subscription();
  value.order_book_depth = 13;
  CHECK(!market_data::valid_bybit_subscription(value));
}

TEST_CASE("session connects and subscribes through the transport seam") {
  auto transport = std::make_unique<FakeTransport>();
  auto *observer = transport.get();
  market_data::BybitWebSocketSession session(std::move(transport));
  const auto result = session.connect_and_subscribe(subscription(), 2s, 4096);
  CHECK(result.ok());
  CHECK(observer->url_ == "wss://stream-testnet.bybit.com/v5/public/linear");
  CHECK(observer->sent_.find("orderbook.50.BTCUSDT") != std::string::npos);
  CHECK(observer->sent_.find("publicTrade.BTCUSDT") != std::string::npos);
  CHECK(observer->maximum_message_bytes_ == 4096);
  session.close();
  CHECK(observer->closed_);
}
