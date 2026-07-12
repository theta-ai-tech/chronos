#include "chronos/adapters/market_data/websocket_framer.hpp"
#include "chronos/adapters/market_data/websocket_transport.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <poll.h>
#include <string>
#include <utility>
#include <vector>

namespace chronos::adapters::market_data {
namespace {

class CurlGlobal final {
public:
  CurlGlobal() : result_(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
  ~CurlGlobal() { curl_global_cleanup(); }
  [[nodiscard]] CURLcode result() const noexcept { return result_; }

private:
  CURLcode result_;
};

CurlGlobal &curl_global() {
  static CurlGlobal instance;
  return instance;
}

TransportFailure map_curl_failure(CURLcode code) noexcept {
  switch (code) {
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_RESOLVE_PROXY:
    return TransportFailure::Resolve;
  case CURLE_COULDNT_CONNECT:
    return TransportFailure::Connect;
  case CURLE_PEER_FAILED_VERIFICATION:
  case CURLE_SSL_CONNECT_ERROR:
  case CURLE_SSL_CERTPROBLEM:
    return TransportFailure::Tls;
  case CURLE_OPERATION_TIMEDOUT:
    return TransportFailure::Timeout;
  case CURLE_GOT_NOTHING:
    return TransportFailure::Closed;
  case CURLE_TOO_LARGE:
    return TransportFailure::MessageTooLarge;
  default:
    return TransportFailure::Receive;
  }
}

std::string curl_detail(CURLcode code,
                        const std::array<char, CURL_ERROR_SIZE> &error) {
  return error.front() != '\0' ? std::string(error.data())
                               : std::string(curl_easy_strerror(code));
}

bool curl_supports_wss() noexcept {
  const auto *version = curl_version_info(CURLVERSION_NOW);
  if (version == nullptr || version->protocols == nullptr) {
    return false;
  }
  for (const char *const *protocol = version->protocols; *protocol != nullptr;
       ++protocol) {
    if (std::string_view(*protocol) == "wss") {
      return true;
    }
  }
  return false;
}

class CurlWebSocketTransport final : public WebSocketTransport {
public:
  ~CurlWebSocketTransport() override { close(); }

  TransportResult<bool> connect(std::string_view url,
                                std::chrono::milliseconds timeout,
                                std::size_t maximum_message_bytes) override {
    close();
    if (url.empty() || timeout.count() <= 0 || maximum_message_bytes == 0 ||
        timeout.count() > std::numeric_limits<long>::max() ||
        maximum_message_bytes >
            static_cast<std::size_t>(std::numeric_limits<curl_off_t>::max()) ||
        curl_global().result() != CURLE_OK || !curl_supports_wss()) {
      return {.failure = TransportFailure::InvalidConfiguration,
              .detail = "invalid configuration or libcurl lacks wss support"};
    }
    handle_ = curl_easy_init();
    if (handle_ == nullptr) {
      return {.failure = TransportFailure::Connect,
              .detail = "curl_easy_init failed"};
    }
    url_ = std::string(url);
    error_.fill('\0');
    assembler_.emplace(maximum_message_bytes);
    maximum_message_bytes_ = maximum_message_bytes;
    curl_easy_setopt(handle_, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(handle_, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS,
                     static_cast<long>(timeout.count()));
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS,
                     static_cast<long>(timeout.count()));
    curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle_, CURLOPT_PROTOCOLS_STR, "wss");
    curl_easy_setopt(handle_, CURLOPT_ERRORBUFFER, error_.data());
    const auto result = curl_easy_perform(handle_);
    if (result != CURLE_OK) {
      const auto failure = map_curl_failure(result);
      const auto detail = curl_detail(result, error_);
      close();
      return {.failure = failure, .detail = detail};
    }
    connected_ = true;
    return {.value = true};
  }

  TransportResult<std::size_t>
  send_text(std::string_view payload,
            std::chrono::milliseconds timeout) override {
    if (!connected_ || handle_ == nullptr || payload.empty() ||
        timeout.count() <= 0) {
      return {.failure = TransportFailure::InvalidConfiguration,
              .detail = "WebSocket is not connected"};
    }
    std::size_t offset = 0;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (offset < payload.size()) {
      std::size_t sent = 0;
      const auto result =
          curl_ws_send(handle_, payload.data() + offset,
                       payload.size() - offset, &sent, 0, CURLWS_TEXT);
      offset += sent;
      if (result == CURLE_AGAIN) {
        if (!wait_for_socket(POLLOUT, deadline)) {
          return {.value = offset,
                  .failure = TransportFailure::Timeout,
                  .detail = "subscription send timed out"};
        }
        continue;
      }
      if (result != CURLE_OK) {
        return {.value = offset,
                .failure = TransportFailure::Send,
                .detail = curl_detail(result, error_)};
      }
    }
    return {.value = offset};
  }

  TransportResult<WebSocketMessage>
  receive(std::chrono::milliseconds timeout) override {
    if (!connected_ || handle_ == nullptr || timeout.count() <= 0) {
      return {.failure = TransportFailure::InvalidConfiguration,
              .detail = "WebSocket is not connected"};
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::array<std::byte, 64U * 1024U> buffer{};

    while (true) {
      std::size_t received = 0;
      const curl_ws_frame *metadata = nullptr;
      const auto result = curl_ws_recv(handle_, buffer.data(), buffer.size(),
                                       &received, &metadata);
      if (result == CURLE_AGAIN) {
        if (!wait_for_socket(POLLIN, deadline)) {
          return {.failure = TransportFailure::Timeout,
                  .detail = "WebSocket receive timed out"};
        }
        continue;
      }
      if (result != CURLE_OK || metadata == nullptr) {
        return {.failure = map_curl_failure(result),
                .detail = curl_detail(result, error_)};
      }
      const auto receive_time =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count();
      const auto bytes_left =
          metadata->bytesleft > 0
              ? static_cast<std::size_t>(metadata->bytesleft)
              : 0;
      std::optional<WebSocketMessageKind> kind;
      if ((metadata->flags & CURLWS_TEXT) != 0) {
        kind = WebSocketMessageKind::Text;
      } else if ((metadata->flags & CURLWS_BINARY) != 0) {
        kind = WebSocketMessageKind::Binary;
      } else if ((metadata->flags & CURLWS_PING) != 0) {
        kind = WebSocketMessageKind::Ping;
      } else if ((metadata->flags & CURLWS_PONG) != 0) {
        kind = WebSocketMessageKind::Pong;
      } else if ((metadata->flags & CURLWS_CLOSE) != 0) {
        kind = WebSocketMessageKind::Close;
      }
      if (!kind.has_value()) {
        const auto retained_size = std::min(received, maximum_message_bytes_);
        const auto retained_view =
            std::span<const std::byte>(buffer).first(retained_size);
        std::vector<std::byte> retained(retained_view.begin(),
                                        retained_view.end());
        WebSocketMessage evidence{
            .kind = WebSocketMessageKind::Unknown,
            .payload = std::move(retained),
            .monotonic_receive_time_nanoseconds = receive_time,
            .fragmented = (metadata->flags & CURLWS_CONT) != 0,
            .integrity = WebSocketIngressIntegrity::Unsupported,
            .original_payload_size = received + bytes_left};
        close();
        return {.failure = TransportFailure::UnsupportedFrame,
                .detail = "unsupported WebSocket frame kind",
                .failure_evidence = std::move(evidence)};
      }

      const auto assembled = assembler_->feed(
          {.kind = *kind,
           .payload = std::span<const std::byte>(buffer.data(), received),
           .monotonic_receive_time_nanoseconds = receive_time,
           .remaining_frame_bytes = bytes_left,
           .frame_complete = metadata->bytesleft == 0,
           .message_continues = (metadata->flags & CURLWS_CONT) != 0});
      if (assembled.failure != FrameAssemblyFailure::None) {
        const auto failure =
            assembled.failure == FrameAssemblyFailure::MessageTooLarge
                ? TransportFailure::MessageTooLarge
                : TransportFailure::UnsupportedFrame;
        auto evidence = std::move(assembled.failure_evidence);
        close();
        return {.failure = failure,
                .detail = "terminal WebSocket frame assembly failure",
                .failure_evidence = std::move(evidence)};
      }
      if (assembled.message.has_value()) {
        if (assembled.terminal) {
          connected_ = false;
        }
        return {.value = std::move(*assembled.message)};
      }
    }
  }

  void close() noexcept override {
    if (handle_ != nullptr) {
      if (connected_) {
        std::size_t sent = 0;
        (void)curl_ws_send(handle_, "", 0, &sent, 0, CURLWS_CLOSE);
      }
      curl_easy_cleanup(handle_);
    }
    handle_ = nullptr;
    connected_ = false;
    url_.clear();
    maximum_message_bytes_ = 0;
    assembler_.reset();
  }

private:
  bool wait_for_socket(short events,
                       std::chrono::steady_clock::time_point deadline) {
    curl_socket_t socket = CURL_SOCKET_BAD;
    if (curl_easy_getinfo(handle_, CURLINFO_ACTIVESOCKET, &socket) !=
            CURLE_OK ||
        socket == CURL_SOCKET_BAD) {
      return false;
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      return false;
    }
    pollfd descriptor{.fd = socket, .events = events, .revents = 0};
    const auto poll_timeout = static_cast<int>(std::min<std::int64_t>(
        remaining.count(), std::numeric_limits<int>::max()));
    return poll(&descriptor, 1, poll_timeout) > 0 &&
           (descriptor.revents & events) != 0;
  }

  CURL *handle_{};
  bool connected_{};
  std::size_t maximum_message_bytes_{};
  std::string url_;
  std::array<char, CURL_ERROR_SIZE> error_{};
  std::optional<WebSocketMessageAssembler> assembler_;
};

} // namespace

std::unique_ptr<WebSocketTransport> make_curl_websocket_transport() {
  return std::make_unique<CurlWebSocketTransport>();
}

} // namespace chronos::adapters::market_data
