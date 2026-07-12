#include "chronos/adapters/market_data/bybit_websocket.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <utility>

namespace chronos::adapters::market_data {
namespace {

class JsonCursor final {
public:
  explicit JsonCursor(std::string_view input) : input_(input) {}

  void whitespace() noexcept {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\n' ||
            input_[position_] == '\r' || input_[position_] == '\t')) {
      ++position_;
    }
  }

  bool consume(char expected) noexcept {
    whitespace();
    if (position_ >= input_.size() || input_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  std::optional<std::string> string() {
    whitespace();
    if (position_ >= input_.size() || input_[position_++] != '"') {
      return std::nullopt;
    }
    std::string result;
    while (position_ < input_.size()) {
      const unsigned char character =
          static_cast<unsigned char>(input_[position_++]);
      if (character == '"') {
        return result;
      }
      if (character < 0x20U) {
        return std::nullopt;
      }
      if (character == '\\') {
        if (position_ >= input_.size()) {
          return std::nullopt;
        }
        const char escaped = input_[position_++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result.push_back(escaped);
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        case 'u':
          for (int index = 0; index < 4; ++index) {
            if (position_ >= input_.size() ||
                std::isxdigit(
                    static_cast<unsigned char>(input_[position_++])) == 0) {
              return std::nullopt;
            }
          }
          result.push_back('?');
          break;
        default:
          return std::nullopt;
        }
      } else {
        result.push_back(static_cast<char>(character));
      }
    }
    return std::nullopt;
  }

  std::optional<bool> boolean() noexcept {
    whitespace();
    if (input_.substr(position_, 4) == "true") {
      position_ += 4;
      return true;
    }
    if (input_.substr(position_, 5) == "false") {
      position_ += 5;
      return false;
    }
    return std::nullopt;
  }

  bool skip_value(std::size_t depth = 0) {
    whitespace();
    if (depth > 16 || position_ >= input_.size()) {
      return false;
    }
    if (input_[position_] == '"') {
      return string().has_value();
    }
    if (input_[position_] == '{') {
      ++position_;
      whitespace();
      if (consume('}')) {
        return true;
      }
      while (true) {
        if (!string().has_value() || !consume(':') || !skip_value(depth + 1)) {
          return false;
        }
        if (consume('}')) {
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (input_[position_] == '[') {
      ++position_;
      whitespace();
      if (consume(']')) {
        return true;
      }
      while (true) {
        if (!skip_value(depth + 1)) {
          return false;
        }
        if (consume(']')) {
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (input_.substr(position_, 4) == "true" ||
        input_.substr(position_, 4) == "null") {
      position_ += 4;
      return true;
    }
    if (input_.substr(position_, 5) == "false") {
      position_ += 5;
      return true;
    }
    return skip_number();
  }

  [[nodiscard]] bool finished() noexcept {
    whitespace();
    return position_ == input_.size();
  }

private:
  bool skip_number() noexcept {
    const auto start = position_;
    if (position_ < input_.size() && input_[position_] == '-') {
      ++position_;
    }
    if (position_ >= input_.size()) {
      position_ = start;
      return false;
    }
    if (input_[position_] == '0') {
      ++position_;
    } else if (input_[position_] >= '1' && input_[position_] <= '9') {
      while (position_ < input_.size() && input_[position_] >= '0' &&
             input_[position_] <= '9') {
        ++position_;
      }
    } else {
      position_ = start;
      return false;
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      const auto fraction_start = position_;
      while (position_ < input_.size() && input_[position_] >= '0' &&
             input_[position_] <= '9') {
        ++position_;
      }
      if (position_ == fraction_start) {
        position_ = start;
        return false;
      }
    }
    if (position_ < input_.size() &&
        (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() &&
          (input_[position_] == '+' || input_[position_] == '-')) {
        ++position_;
      }
      const auto exponent_start = position_;
      while (position_ < input_.size() && input_[position_] >= '0' &&
             input_[position_] <= '9') {
        ++position_;
      }
      if (position_ == exponent_start) {
        position_ = start;
        return false;
      }
    }
    return true;
  }

  std::string_view input_;
  std::size_t position_{};
};

std::string_view message_text(const WebSocketMessage &message) noexcept {
  return {reinterpret_cast<const char *>(message.payload.data()),
          message.payload.size()};
}
} // namespace

BybitControlResponse parse_bybit_control_response(std::string_view payload) {
  JsonCursor cursor(payload);
  if (!cursor.consume('{')) {
    return BybitControlResponse::Malformed;
  }
  std::optional<std::string> operation;
  std::optional<std::string> request_id;
  std::optional<bool> success;
  if (cursor.consume('}')) {
    return cursor.finished() ? BybitControlResponse::Other
                             : BybitControlResponse::Malformed;
  }
  while (true) {
    const auto key = cursor.string();
    if (!key.has_value() || !cursor.consume(':')) {
      return BybitControlResponse::Malformed;
    }
    if (*key == "op") {
      if (operation.has_value()) {
        return BybitControlResponse::Malformed;
      }
      operation = cursor.string();
      if (!operation.has_value()) {
        return BybitControlResponse::Malformed;
      }
    } else if (*key == "req_id") {
      if (request_id.has_value()) {
        return BybitControlResponse::Malformed;
      }
      request_id = cursor.string();
      if (!request_id.has_value()) {
        return BybitControlResponse::Malformed;
      }
    } else if (*key == "success") {
      if (success.has_value()) {
        return BybitControlResponse::Malformed;
      }
      success = cursor.boolean();
      if (!success.has_value()) {
        return BybitControlResponse::Malformed;
      }
    } else if (!cursor.skip_value()) {
      return BybitControlResponse::Malformed;
    }
    if (cursor.consume('}')) {
      break;
    }
    if (!cursor.consume(',')) {
      return BybitControlResponse::Malformed;
    }
  }
  if (!cursor.finished()) {
    return BybitControlResponse::Malformed;
  }
  if (!operation.has_value() || *operation != "subscribe") {
    return BybitControlResponse::Other;
  }
  if (!success.has_value() || !request_id.has_value()) {
    return BybitControlResponse::Malformed;
  }
  if (*request_id != "chronos-m2") {
    return BybitControlResponse::Other;
  }
  return *success ? BybitControlResponse::SubscriptionAccepted
                  : BybitControlResponse::SubscriptionRejected;
}

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
    std::unique_ptr<WebSocketTransport> transport, MessageObserver observer)
    : transport_(std::move(transport)), observer_(std::move(observer)) {}

TransportResult<bool> BybitWebSocketSession::connect_and_subscribe(
    const BybitSubscription &subscription, std::chrono::milliseconds timeout,
    std::size_t maximum_message_bytes, const std::function<bool()> &cancelled) {
  if (!transport_ || !valid_bybit_subscription(subscription) ||
      timeout.count() <= 0 || maximum_message_bytes == 0) {
    return {.failure = TransportFailure::InvalidConfiguration,
            .detail = "invalid Bybit session configuration"};
  }
  early_messages_.clear();
  early_message_overflowed_ = false;
  subscribed_ = false;
  const auto was_cancelled = [&cancelled] { return cancelled && cancelled(); };
  if (was_cancelled()) {
    return {.failure = TransportFailure::Closed,
            .detail = "Bybit session establishment cancelled"};
  }
  auto connected = transport_->connect(bybit_public_websocket_url(subscription),
                                       timeout, maximum_message_bytes);
  if (!connected.ok()) {
    return connected;
  }
  if (was_cancelled()) {
    transport_->close();
    return {.failure = TransportFailure::Closed,
            .detail = "Bybit session establishment cancelled"};
  }
  const auto payload = bybit_subscription_message(subscription);
  auto sent = transport_->send_text(payload, timeout);
  if (!sent.ok() || sent.value != payload.size()) {
    transport_->close();
    return {.failure = sent.ok() ? TransportFailure::Send : sent.failure,
            .detail = sent.ok() ? "partial subscription write" : sent.detail};
  }
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  const auto retain_early = [this](WebSocketMessage message) {
    if (early_messages_.size() == 8) {
      early_messages_.pop_front();
      early_message_overflowed_ = true;
    }
    early_messages_.push_back(std::move(message));
  };
  while (true) {
    if (was_cancelled()) {
      transport_->close();
      return {.failure = TransportFailure::Closed,
              .detail = "Bybit session establishment cancelled"};
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      transport_->close();
      return {.failure = TransportFailure::Timeout,
              .detail = "Bybit subscription acknowledgement timed out"};
    }
    auto response = transport_->receive(remaining);
    if (!response.ok()) {
      if (response.failure_evidence.has_value() &&
          !observe(*response.failure_evidence)) {
        transport_->close();
        return {.failure = TransportFailure::CaptureHandoff,
                .detail = "source capture rejected framing-failure evidence"};
      }
      transport_->close();
      return {.failure = response.failure,
              .detail = response.detail,
              .failure_evidence = std::move(response.failure_evidence)};
    }
    if (!observe(response.value)) {
      transport_->close();
      return {.failure = TransportFailure::CaptureHandoff,
              .detail = "source capture handoff rejected ingress message"};
    }
    if (response.value.kind != WebSocketMessageKind::Text) {
      retain_early(std::move(response.value));
      continue;
    }
    switch (parse_bybit_control_response(message_text(response.value))) {
    case BybitControlResponse::SubscriptionAccepted:
      subscribed_ = true;
      return {.value = true};
    case BybitControlResponse::SubscriptionRejected:
      transport_->close();
      return {.failure = TransportFailure::SubscriptionRejected,
              .detail = "Bybit rejected the subscription"};
    case BybitControlResponse::Malformed:
      transport_->close();
      return {.failure = TransportFailure::Protocol,
              .detail = "malformed Bybit control response"};
    case BybitControlResponse::Other:
      retain_early(std::move(response.value));
      break;
    }
  }
}

TransportResult<WebSocketMessage>
BybitWebSocketSession::receive(std::chrono::milliseconds timeout) {
  if (!transport_ || !subscribed_) {
    return {.failure = TransportFailure::InvalidConfiguration,
            .detail = "session is not subscribed"};
  }
  if (!early_messages_.empty()) {
    auto message = std::move(early_messages_.front());
    early_messages_.pop_front();
    return {.value = std::move(message)};
  }
  auto result = transport_->receive(timeout);
  if (!result.ok() && result.failure_evidence.has_value()) {
    if (!observe(*result.failure_evidence)) {
      transport_->close();
      return {.failure = TransportFailure::CaptureHandoff,
              .detail = "source capture rejected framing-failure evidence"};
    }
    transport_->close();
    return result;
  }
  if (result.ok() && !observe(result.value)) {
    transport_->close();
    return {.failure = TransportFailure::CaptureHandoff,
            .detail = "source capture handoff rejected ingress message"};
  }
  return result;
}

TransportResult<std::size_t>
BybitWebSocketSession::send_heartbeat(std::chrono::milliseconds timeout) {
  if (!transport_ || !subscribed_ || timeout.count() <= 0) {
    return {.failure = TransportFailure::InvalidConfiguration,
            .detail = "session is not subscribed"};
  }
  return transport_->send_text("{\"op\":\"ping\"}", timeout);
}

bool BybitWebSocketSession::capture_enabled() const noexcept {
  return static_cast<bool>(observer_);
}

bool BybitWebSocketSession::early_message_overflowed() const noexcept {
  return early_message_overflowed_;
}

bool BybitWebSocketSession::observe(const WebSocketMessage &message) const {
  return !observer_ || observer_(message);
}

void BybitWebSocketSession::close() noexcept {
  if (transport_) {
    transport_->close();
  }
  early_messages_.clear();
  subscribed_ = false;
}

} // namespace chronos::adapters::market_data
