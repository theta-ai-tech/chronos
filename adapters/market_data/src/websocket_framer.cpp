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
  return control ? append(chunk, pending_control_, pending_control_kind_,
                          pending_control_receive_time_,
                          pending_control_fragmented_, true)
                 : append(chunk, pending_data_, pending_data_kind_,
                          pending_data_receive_time_, pending_data_fragmented_,
                          false);
}

FrameAssemblyResult WebSocketMessageAssembler::append(
    const WebSocketFrameChunk &chunk, std::vector<std::byte> &pending_payload,
    std::optional<WebSocketMessageKind> &pending_kind,
    std::optional<std::int64_t> &pending_receive_time, bool &pending_fragmented,
    bool control) {
  if (pending_kind.has_value() && *pending_kind != chunk.kind) {
    return terminal(FrameAssemblyFailure::ProtocolViolation);
  }
  if (control && chunk.message_continues) {
    return terminal(FrameAssemblyFailure::ProtocolViolation);
  }
  pending_kind = chunk.kind;
  if (!pending_receive_time.has_value()) {
    pending_receive_time = chunk.monotonic_receive_time_nanoseconds;
  }
  pending_fragmented = pending_fragmented || chunk.message_continues;
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
                           .payload = std::move(pending_payload),
                           .monotonic_receive_time_nanoseconds =
                               *pending_receive_time,
                           .fragmented = pending_fragmented};
  pending_payload.clear();
  pending_kind.reset();
  pending_receive_time.reset();
  pending_fragmented = false;
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
  pending_data_receive_time_.reset();
  pending_data_fragmented_ = false;
  pending_control_.clear();
  pending_control_kind_.reset();
  pending_control_receive_time_.reset();
  pending_control_fragmented_ = false;
  terminal_ = false;
}

} // namespace chronos::adapters::market_data
