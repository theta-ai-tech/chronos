#include "chronos/adapters/market_data/bybit_websocket.hpp"
#include "chronos/adapters/market_data/websocket_framer.hpp"

#include "microtest.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
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
    if (messages_.empty()) {
      return {.failure = market_data::TransportFailure::Timeout,
              .detail = "fake receive queue empty"};
    }
    auto message = std::move(messages_.front());
    messages_.pop_front();
    return {.value = std::move(message)};
  }

  void close() noexcept override { closed_ = true; }

  std::string url_;
  std::string sent_;
  std::size_t maximum_message_bytes_{};
  std::deque<market_data::WebSocketMessage> messages_;
  bool closed_{};
};

market_data::BybitSubscription subscription() {
  return {.environment = sdk::EnvironmentClass::Test,
          .market = sdk::MarketClass::LinearPerpetual,
          .symbol = "BTCUSDT",
          .order_book_depth = 50};
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

market_data::WebSocketMessage text_message(std::string_view value) {
  return {.kind = market_data::WebSocketMessageKind::Text,
          .payload = bytes(value)};
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
  observer->messages_.push_back(text_message(
      R"({"success":true,"ret_msg":"","op":"subscribe","req_id":"chronos-m2","conn_id":"c"})"));
  market_data::BybitWebSocketSession session(std::move(transport));
  const auto result = session.connect_and_subscribe(subscription(), 2s, 4096);
  CHECK(result.ok());
  CHECK(observer->url_ == "wss://stream-testnet.bybit.com/v5/public/linear");
  CHECK(observer->sent_.find("orderbook.50.BTCUSDT") != std::string::npos);
  CHECK(observer->sent_.find("publicTrade.BTCUSDT") != std::string::npos);
  CHECK(observer->maximum_message_bytes_ == 4096);
  const auto heartbeat = session.send_heartbeat(2s);
  CHECK(heartbeat.ok());
  CHECK(observer->sent_ == R"({"op":"ping"})");
  session.close();
  CHECK(observer->closed_);
}

TEST_CASE("Bybit control responses are parsed structurally and fail closed") {
  CHECK(
      market_data::parse_bybit_control_response(
          R"({"op":"subscribe","success":true,"req_id":"chronos-m2","data":{"x":[1,2]}})") ==
      market_data::BybitControlResponse::SubscriptionAccepted);
  CHECK(
      market_data::parse_bybit_control_response(
          R"({"success":false,"op":"subscribe","req_id":"chronos-m2","ret_msg":"bad"})") ==
      market_data::BybitControlResponse::SubscriptionRejected);
  CHECK(market_data::parse_bybit_control_response(
            R"({"success":true,"op":"ping"})") ==
        market_data::BybitControlResponse::Other);
  CHECK(
      market_data::parse_bybit_control_response(
          R"({"success":true,"ret_msg":"\u03b1","op":"subscribe","req_id":"chronos-m2"})") ==
      market_data::BybitControlResponse::SubscriptionAccepted);
  CHECK(
      market_data::parse_bybit_control_response(
          R"({"success":true,"success":false,"op":"subscribe","req_id":"chronos-m2"})") ==
      market_data::BybitControlResponse::Malformed);
  CHECK(market_data::parse_bybit_control_response(
            R"({"success":true,"op":"subscribe","req_id":"stale"})") ==
        market_data::BybitControlResponse::Other);
  CHECK(
      market_data::parse_bybit_control_response(
          R"({"success":true,"op":"subscribe","req_id":"chronos-m2","extra":garbage})") ==
      market_data::BybitControlResponse::Malformed);
}

TEST_CASE("session does not become ready when Bybit rejects subscription") {
  auto transport = std::make_unique<FakeTransport>();
  auto *observer = transport.get();
  observer->messages_.push_back(text_message(
      R"({"success":false,"op":"subscribe","req_id":"chronos-m2"})"));
  market_data::BybitWebSocketSession session(std::move(transport));
  const auto result = session.connect_and_subscribe(subscription(), 2s, 4096);
  CHECK(result.failure == market_data::TransportFailure::SubscriptionRejected);
  CHECK(observer->closed_);
}

TEST_CASE("pre-ack market pressure stays bounded without rejecting readiness") {
  auto transport = std::make_unique<FakeTransport>();
  auto *observer = transport.get();
  for (int index = 0; index < 10; ++index) {
    observer->messages_.push_back(
        text_message(R"({"topic":"orderbook.50.BTCUSDT","data":[]})"));
  }
  observer->messages_.push_back(text_message(
      R"({"success":true,"op":"subscribe","req_id":"chronos-m2"})"));
  market_data::BybitWebSocketSession session(std::move(transport));
  const auto result = session.connect_and_subscribe(subscription(), 2s, 4096);
  CHECK(result.ok());
  CHECK(session.early_message_overflowed());
  for (int index = 0; index < 8; ++index) {
    CHECK(session.receive(2s).ok());
  }
}

TEST_CASE("frame assembler preserves data around interleaved control frames") {
  market_data::WebSocketMessageAssembler assembler(32);
  const auto first = bytes("hel");
  const auto ping = bytes("p");
  const auto second = bytes("lo");
  auto result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                                .payload = first,
                                .frame_complete = true,
                                .message_continues = true});
  CHECK(!result.message.has_value());
  result = assembler.feed({.kind = market_data::WebSocketMessageKind::Ping,
                           .payload = ping,
                           .frame_complete = true,
                           .message_continues = false});
  CHECK(result.message.has_value());
  CHECK(result.message->kind == market_data::WebSocketMessageKind::Ping);
  result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                           .payload = second,
                           .frame_complete = true,
                           .message_continues = false});
  CHECK(result.message.has_value());
  CHECK(result.message->payload == bytes("hello"));
}

TEST_CASE(
    "frame assembler retains partial chunks and isolates oversize input") {
  market_data::WebSocketMessageAssembler assembler(4);
  const auto first = bytes("ab");
  const auto second = bytes("cd");
  auto result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                                .payload = first,
                                .frame_complete = false,
                                .message_continues = false});
  CHECK(!result.message.has_value());
  result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                           .payload = second,
                           .frame_complete = true,
                           .message_continues = false});
  CHECK(result.message.has_value());
  CHECK(result.message->payload == bytes("abcd"));

  const auto hostile = bytes("abcde");
  result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                           .payload = hostile,
                           .frame_complete = true,
                           .message_continues = false});
  CHECK(result.failure == market_data::FrameAssemblyFailure::MessageTooLarge);
  CHECK(result.terminal);
  result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                           .payload = first,
                           .frame_complete = true,
                           .message_continues = false});
  CHECK(result.terminal);
  CHECK(!result.message.has_value());
}

TEST_CASE("close frame terminally ends frame assembly") {
  market_data::WebSocketMessageAssembler assembler(16);
  const auto reason = bytes("bye");
  auto result =
      assembler.feed({.kind = market_data::WebSocketMessageKind::Close,
                      .payload = reason,
                      .frame_complete = true,
                      .message_continues = false});
  CHECK(result.message.has_value());
  CHECK(result.terminal);
  const auto later = bytes("later");
  result = assembler.feed({.kind = market_data::WebSocketMessageKind::Text,
                           .payload = later,
                           .frame_complete = true,
                           .message_continues = false});
  CHECK(result.terminal);
  CHECK(!result.message.has_value());
}
