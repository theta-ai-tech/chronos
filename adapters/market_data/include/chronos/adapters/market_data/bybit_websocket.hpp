#pragma once

#include "chronos/adapters/market_data/websocket_transport.hpp"
#include "chronos/adapters/sdk/adapter.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
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

class BybitSession {
public:
  virtual ~BybitSession() = default;
  [[nodiscard]] virtual TransportResult<bool>
  connect_and_subscribe(const BybitSubscription &subscription,
                        std::chrono::milliseconds timeout,
                        std::size_t maximum_message_bytes) = 0;
  [[nodiscard]] virtual TransportResult<WebSocketMessage>
  receive(std::chrono::milliseconds timeout) = 0;
  [[nodiscard]] virtual TransportResult<std::size_t>
  send_heartbeat(std::chrono::milliseconds timeout) = 0;
  [[nodiscard]] virtual bool capture_enabled() const noexcept = 0;
  virtual void close() noexcept = 0;
};

class BybitWebSocketSession final : public BybitSession {
public:
  using MessageObserver = std::function<bool(const WebSocketMessage &)>;

  explicit BybitWebSocketSession(std::unique_ptr<WebSocketTransport> transport =
                                     make_curl_websocket_transport(),
                                 MessageObserver observer = {});

  [[nodiscard]] TransportResult<bool>
  connect_and_subscribe(const BybitSubscription &subscription,
                        std::chrono::milliseconds timeout,
                        std::size_t maximum_message_bytes) override;
  [[nodiscard]] TransportResult<WebSocketMessage>
  receive(std::chrono::milliseconds timeout) override;
  [[nodiscard]] TransportResult<std::size_t>
  send_heartbeat(std::chrono::milliseconds timeout) override;
  [[nodiscard]] bool capture_enabled() const noexcept override;
  [[nodiscard]] bool early_message_overflowed() const noexcept;
  void close() noexcept override;

private:
  [[nodiscard]] bool observe(const WebSocketMessage &message) const;

  std::unique_ptr<WebSocketTransport> transport_;
  MessageObserver observer_;
  std::deque<WebSocketMessage> early_messages_;
  bool early_message_overflowed_{};
  bool subscribed_{};
};

} // namespace chronos::adapters::market_data
