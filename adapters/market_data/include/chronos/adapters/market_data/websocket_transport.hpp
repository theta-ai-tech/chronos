#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::adapters::market_data {

enum class WebSocketMessageKind { Text, Binary, Ping, Pong, Close };
enum class WebSocketIngressIntegrity {
  Complete,
  Malformed,
  Unsupported,
  Truncated,
  ResourceLimitExceeded,
};

struct WebSocketMessage final {
  WebSocketMessageKind kind{WebSocketMessageKind::Text};
  std::vector<std::byte> payload;
  std::int64_t monotonic_receive_time_nanoseconds{};
  bool fragmented{};
  WebSocketIngressIntegrity integrity{WebSocketIngressIntegrity::Complete};
  std::size_t original_payload_size{};
};

enum class TransportFailure {
  None,
  InvalidConfiguration,
  Resolve,
  Connect,
  Tls,
  Upgrade,
  Send,
  Receive,
  Timeout,
  Closed,
  MessageTooLarge,
  UnsupportedFrame,
  Protocol,
  SubscriptionRejected,
  CaptureHandoff,
};

template <typename T> struct TransportResult final {
  T value{};
  TransportFailure failure{TransportFailure::None};
  std::string detail;
  std::optional<WebSocketMessage> failure_evidence;

  [[nodiscard]] bool ok() const noexcept {
    return failure == TransportFailure::None;
  }
};

class WebSocketTransport {
public:
  virtual ~WebSocketTransport() = default;
  [[nodiscard]] virtual TransportResult<bool>
  connect(std::string_view url, std::chrono::milliseconds timeout,
          std::size_t maximum_message_bytes) = 0;
  [[nodiscard]] virtual TransportResult<std::size_t>
  send_text(std::string_view payload, std::chrono::milliseconds timeout) = 0;
  [[nodiscard]] virtual TransportResult<WebSocketMessage>
  receive(std::chrono::milliseconds timeout) = 0;
  virtual void close() noexcept = 0;
};

[[nodiscard]] std::unique_ptr<WebSocketTransport>
make_curl_websocket_transport();

} // namespace chronos::adapters::market_data
