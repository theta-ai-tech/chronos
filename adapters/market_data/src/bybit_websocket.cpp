#include "chronos/adapters/market_data/bybit_websocket.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace chronos::adapters::market_data {

bool valid_bybit_subscription(const BybitSubscription &value) noexcept {
  if (value.symbol.empty() || value.symbol.size() > 32 ||
      !std::all_of(value.symbol.begin(), value.symbol.end(),
                   [](unsigned char character) {
                     return std::isupper(character) != 0 ||
                            std::isdigit(character) != 0;
                   })) {
    return false;
  }
  const bool supported_depth =
      value.order_book_depth == 1 || value.order_book_depth == 50 ||
      value.order_book_depth == 200 || value.order_book_depth == 1000;
  return supported_depth &&
         (value.market == sdk::MarketClass::Spot ||
          value.market == sdk::MarketClass::LinearPerpetual ||
          value.market == sdk::MarketClass::InversePerpetual) &&
         (value.environment == sdk::EnvironmentClass::Test ||
          value.environment == sdk::EnvironmentClass::Production);
}

std::string bybit_public_websocket_url(const BybitSubscription &value) {
  if (!valid_bybit_subscription(value)) {
    return {};
  }
  const std::string_view host =
      value.environment == sdk::EnvironmentClass::Production
          ? "stream.bybit.com"
          : "stream-testnet.bybit.com";
  std::string_view market;
  switch (value.market) {
  case sdk::MarketClass::Spot:
    market = "spot";
    break;
  case sdk::MarketClass::LinearPerpetual:
    market = "linear";
    break;
  case sdk::MarketClass::InversePerpetual:
    market = "inverse";
    break;
  }
  return "wss://" + std::string(host) + "/v5/public/" + std::string(market);
}

std::string bybit_subscription_message(const BybitSubscription &value) {
  if (!valid_bybit_subscription(value)) {
    return {};
  }
  return "{\"req_id\":\"chronos-m2\",\"op\":\"subscribe\",\"args\":["
         "\"orderbook." +
         std::to_string(value.order_book_depth) + "." + value.symbol +
         "\",\"publicTrade." + value.symbol + "\"]}";
}

BybitWebSocketSession::BybitWebSocketSession(
    std::unique_ptr<WebSocketTransport> transport)
    : transport_(std::move(transport)) {}

TransportResult<bool> BybitWebSocketSession::connect_and_subscribe(
    const BybitSubscription &subscription, std::chrono::milliseconds timeout,
    std::size_t maximum_message_bytes) {
  if (!transport_ || !valid_bybit_subscription(subscription) ||
      timeout.count() <= 0 || maximum_message_bytes == 0) {
    return {.failure = TransportFailure::InvalidConfiguration,
            .detail = "invalid Bybit session configuration"};
  }
  auto connected = transport_->connect(bybit_public_websocket_url(subscription),
                                       timeout, maximum_message_bytes);
  if (!connected.ok()) {
    return connected;
  }
  const auto payload = bybit_subscription_message(subscription);
  auto sent = transport_->send_text(payload, timeout);
  if (!sent.ok() || sent.value != payload.size()) {
    transport_->close();
    return {.failure = sent.ok() ? TransportFailure::Send : sent.failure,
            .detail = sent.ok() ? "partial subscription write" : sent.detail};
  }
  subscribed_ = true;
  return {.value = true};
}

TransportResult<WebSocketMessage>
BybitWebSocketSession::receive(std::chrono::milliseconds timeout) {
  if (!transport_ || !subscribed_) {
    return {.failure = TransportFailure::InvalidConfiguration,
            .detail = "session is not subscribed"};
  }
  return transport_->receive(timeout);
}

void BybitWebSocketSession::close() noexcept {
  if (transport_) {
    transport_->close();
  }
  subscribed_ = false;
}

} // namespace chronos::adapters::market_data
