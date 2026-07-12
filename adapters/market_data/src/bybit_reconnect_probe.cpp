#include "chronos/adapters/market_data/reconnect.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

namespace {
namespace market_data = chronos::adapters::market_data;

std::string_view as_text(const market_data::WebSocketMessage &message) {
  return {reinterpret_cast<const char *>(message.payload.data()),
          message.payload.size()};
}

bool observe_required_topics(market_data::BybitSession &session) {
  using namespace std::chrono_literals;
  if (!session.send_heartbeat(5s).ok()) {
    return false;
  }
  bool book = false;
  bool trade = false;
  for (int index = 0; index < 200 && !(book && trade); ++index) {
    auto result = session.receive(10s);
    if (!result.ok()) {
      return false;
    }
    if (result.value.kind != market_data::WebSocketMessageKind::Text) {
      continue;
    }
    const auto payload = as_text(result.value);
    book = book ||
           payload.find("\"topic\":\"orderbook.") != std::string_view::npos;
    trade = trade ||
            payload.find("\"topic\":\"publicTrade.") != std::string_view::npos;
  }
  return book && trade;
}
} // namespace

int main(int argc, char **argv) {
  using namespace std::chrono_literals;
  namespace sdk = chronos::adapters::sdk;

  const std::string symbol = argc > 1 ? argv[1] : "BTCUSDT";
  const bool production = argc > 2 && std::string_view(argv[2]) == "production";
  const market_data::BybitSubscription subscription{
      .environment = production ? sdk::EnvironmentClass::Production
                                : sdk::EnvironmentClass::Test,
      .market = sdk::MarketClass::LinearPerpetual,
      .symbol = symbol,
      .order_book_depth = 50};
  const market_data::RecoveryPolicy policy{.retry = {.maximum_attempts = 3,
                                                     .initial_delay = 500ms,
                                                     .maximum_delay = 2s},
                                           .maximum_elapsed = 15s,
                                           .connection_timeout = 10s,
                                           .maximum_message_bytes = 1U << 20U};

  market_data::BybitWebSocketSessionFactory factory;
  market_data::SteadyRecoveryScheduler scheduler;
  market_data::RandomJitterSource jitter;
  market_data::RandomCaptureSessionIdentitySource capture_sessions;
  auto controller = market_data::BybitReconnectController::create(
      subscription, policy, factory, scheduler, jitter, capture_sessions);
  if (!controller.has_value()) {
    std::cerr << "invalid reconnect probe configuration\n";
    return 1;
  }

  const auto initial = controller->start();
  if (initial.disposition !=
          market_data::RecoveryDisposition::ConnectedInitialOrigin ||
      controller->session() == nullptr ||
      !observe_required_topics(*controller->session())) {
    std::cerr << "initial source epoch failed\n";
    return 1;
  }
  const auto first_capture_session = controller->health().capture_session_id;

  const auto recovered =
      controller->recover(market_data::RecoveryCause::TransportClosed);
  if (recovered.disposition !=
          market_data::RecoveryDisposition::RecoveredNewSourceEpoch ||
      recovered.previous_source_session_epoch != 1 ||
      recovered.new_source_session_epoch != 2 ||
      controller->session() == nullptr ||
      !observe_required_topics(*controller->session()) ||
      !first_capture_session.has_value() ||
      controller->health().capture_session_id == first_capture_session) {
    std::cerr << "recovered source epoch failed\n";
    return 1;
  }

  std::cout << "reconnected source_epoch=1->2 capture_session_changed=yes "
               "continuity=gapped\n";
  controller->stop();
  return 0;
}
