#pragma once

#include "chronos/adapters/market_data/websocket_transport.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace chronos::adapters::market_data {

struct WebSocketFrameChunk final {
  WebSocketMessageKind kind{WebSocketMessageKind::Text};
  std::span<const std::byte> payload;
  bool frame_complete{};
  bool message_continues{};
};

enum class FrameAssemblyFailure { None, MessageTooLarge, ProtocolViolation };

struct FrameAssemblyResult final {
  std::optional<WebSocketMessage> message;
  FrameAssemblyFailure failure{FrameAssemblyFailure::None};
  bool terminal{};
};

class WebSocketMessageAssembler final {
public:
  explicit WebSocketMessageAssembler(std::size_t maximum_message_bytes);

  [[nodiscard]] FrameAssemblyResult feed(const WebSocketFrameChunk &chunk);
  void reset() noexcept;

private:
  [[nodiscard]] FrameAssemblyResult
  append(const WebSocketFrameChunk &chunk,
         std::vector<std::byte> &pending_payload,
         std::optional<WebSocketMessageKind> &pending_kind, bool control);
  [[nodiscard]] FrameAssemblyResult terminal(FrameAssemblyFailure failure);

  std::size_t maximum_message_bytes_{};
  std::vector<std::byte> pending_data_;
  std::optional<WebSocketMessageKind> pending_data_kind_;
  std::vector<std::byte> pending_control_;
  std::optional<WebSocketMessageKind> pending_control_kind_;
  bool terminal_{};
};

} // namespace chronos::adapters::market_data
