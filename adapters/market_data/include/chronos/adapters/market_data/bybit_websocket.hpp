#pragma once

#include "chronos/adapters/market_data/websocket_transport.hpp"
#include "chronos/adapters/sdk/adapter.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <string_view>

namespace chronos::adapters::market_data {

struct BybitSubscription final {
  sdk::EnvironmentClass environment{sdk::EnvironmentClass::Test};
  sdk::MarketClass market{sdk::MarketClass::LinearPerpetual};
  std::string symbol;
  std::size_t order_book_depth{50};
};

[[nodiscard]] bool
valid_bybit_subscription(const BybitSubscription &value) noexcept;
[[nodiscard]] std::string
bybit_public_websocket_url(const BybitSubscription &value);
[[nodiscard]] std::string
bybit_subscription_message(const BybitSubscription &value);

enum class BybitControlResponse {
  SubscriptionAccepted,
  SubscriptionRejected,
  Other,
  Malformed,
};
[[nodiscard]] BybitControlResponse
parse_bybit_control_response(std::string_view payload);

class BybitWebSocketSession final {
public:
  explicit BybitWebSocketSession(std::unique_ptr<WebSocketTransport> transport =
                                     make_curl_websocket_transport());

  [[nodiscard]] TransportResult<bool>
  connect_and_subscribe(const BybitSubscription &subscription,
                        std::chrono::milliseconds timeout,
                        std::size_t maximum_message_bytes);
  [[nodiscard]] TransportResult<WebSocketMessage>
  receive(std::chrono::milliseconds timeout);
  [[nodiscard]] TransportResult<std::size_t>
  send_heartbeat(std::chrono::milliseconds timeout);
  void close() noexcept;

private:
  std::unique_ptr<WebSocketTransport> transport_;
  std::deque<WebSocketMessage> early_messages_;
  bool subscribed_{};
};

} // namespace chronos::adapters::market_data
