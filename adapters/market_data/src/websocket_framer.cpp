#include "chronos/adapters/market_data/websocket_framer.hpp"

#include <utility>

namespace chronos::adapters::market_data {

WebSocketMessageAssembler::WebSocketMessageAssembler(
    std::size_t maximum_message_bytes)
    : maximum_message_bytes_(maximum_message_bytes) {}

FrameAssemblyResult
WebSocketMessageAssembler::feed(const WebSocketFrameChunk &chunk) {
  if (terminal_ || maximum_message_bytes_ == 0) {
    return {.failure = FrameAssemblyFailure::ProtocolViolation,
            .terminal = true};
  }
  const bool control = chunk.kind == WebSocketMessageKind::Ping ||
                       chunk.kind == WebSocketMessageKind::Pong ||
                       chunk.kind == WebSocketMessageKind::Close;
  return control ? append(chunk, pending_control_, pending_control_kind_, true)
                 : append(chunk, pending_data_, pending_data_kind_, false);
}

FrameAssemblyResult WebSocketMessageAssembler::append(
    const WebSocketFrameChunk &chunk, std::vector<std::byte> &pending_payload,
    std::optional<WebSocketMessageKind> &pending_kind, bool control) {
  if (pending_kind.has_value() && *pending_kind != chunk.kind) {
    return terminal(FrameAssemblyFailure::ProtocolViolation);
  }
  if (control && chunk.message_continues) {
    return terminal(FrameAssemblyFailure::ProtocolViolation);
  }
  pending_kind = chunk.kind;
  if (chunk.payload.size() > maximum_message_bytes_ ||
      pending_payload.size() > maximum_message_bytes_ - chunk.payload.size()) {
    return terminal(FrameAssemblyFailure::MessageTooLarge);
  }
  pending_payload.insert(pending_payload.end(), chunk.payload.begin(),
                         chunk.payload.end());
  if (!chunk.frame_complete || (!control && chunk.message_continues)) {
    return {};
  }

  WebSocketMessage message{.kind = chunk.kind,
                           .payload = std::move(pending_payload)};
  pending_payload.clear();
  pending_kind.reset();
  const bool closed = message.kind == WebSocketMessageKind::Close;
  if (closed) {
    terminal_ = true;
  }
  return {.message = std::move(message), .terminal = closed};
}

FrameAssemblyResult
WebSocketMessageAssembler::terminal(FrameAssemblyFailure failure) {
  reset();
  terminal_ = true;
  return {.failure = failure, .terminal = true};
}

void WebSocketMessageAssembler::reset() noexcept {
  pending_data_.clear();
  pending_data_kind_.reset();
  pending_control_.clear();
  pending_control_kind_.reset();
  terminal_ = false;
}

} // namespace chronos::adapters::market_data
