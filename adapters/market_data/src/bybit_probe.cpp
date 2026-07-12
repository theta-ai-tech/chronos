#include "chronos/adapters/market_data/bybit_websocket.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace {
std::string_view
as_text(const chronos::adapters::market_data::WebSocketMessage &message) {
  return {reinterpret_cast<const char *>(message.payload.data()),
          message.payload.size()};
}
} // namespace

int main(int argc, char **argv) {
  using namespace std::chrono_literals;
  namespace market_data = chronos::adapters::market_data;
  namespace sdk = chronos::adapters::sdk;

  const std::string symbol = argc > 1 ? argv[1] : "BTCUSDT";
  const bool production = argc > 2 && std::string_view(argv[2]) == "production";
  market_data::BybitSubscription subscription{
      .environment = production ? sdk::EnvironmentClass::Production
                                : sdk::EnvironmentClass::Test,
      .market = sdk::MarketClass::LinearPerpetual,
      .symbol = symbol,
      .order_book_depth = 50};

  market_data::BybitWebSocketSession session;
  const auto connected =
      session.connect_and_subscribe(subscription, 10s, 1U << 20U);
  if (!connected.ok()) {
    std::cerr << "connect/subscribe failed: " << connected.detail << '\n';
    return 1;
  }

  bool saw_book = false;
  bool saw_trade = false;
  for (int index = 0; index < 100 && !(saw_book && saw_trade); ++index) {
    const auto received = session.receive(10s);
    if (!received.ok()) {
      std::cerr << "receive failed: " << received.detail << '\n';
      return 1;
    }
    if (received.value.kind != market_data::WebSocketMessageKind::Text) {
      continue;
    }
    const auto text = as_text(received.value);
    saw_book = saw_book ||
               text.find("\"topic\":\"orderbook.") != std::string_view::npos;
    saw_trade = saw_trade ||
                text.find("\"topic\":\"publicTrade.") != std::string_view::npos;
    std::cout << "text frame bytes=" << text.size()
              << " book=" << (saw_book ? "yes" : "no")
              << " trade=" << (saw_trade ? "yes" : "no") << '\n';
  }
  session.close();
  if (!saw_book || !saw_trade) {
    std::cerr << "did not observe both required topics\n";
    return 1;
  }
  return 0;
}
