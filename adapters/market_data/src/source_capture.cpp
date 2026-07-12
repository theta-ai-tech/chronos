#include "chronos/adapters/market_data/source_capture.hpp"

#include <utility>

namespace chronos::adapters::market_data {
namespace {
sdk::SourceFrameKind source_frame_kind(WebSocketMessageKind kind) noexcept {
  switch (kind) {
  case WebSocketMessageKind::Text:
    return sdk::SourceFrameKind::Text;
  case WebSocketMessageKind::Binary:
    return sdk::SourceFrameKind::Binary;
  case WebSocketMessageKind::Ping:
    return sdk::SourceFrameKind::Ping;
  case WebSocketMessageKind::Pong:
    return sdk::SourceFrameKind::Pong;
  case WebSocketMessageKind::Close:
    return sdk::SourceFrameKind::Close;
  }
  return sdk::SourceFrameKind::Unknown;
}
} // namespace

sdk::CaptureResult
capture_websocket_message(sdk::SourceCaptureRecorder &recorder,
                          contracts::SourceEventId source_event_id,
                          contracts::ClockDomainId monotonic_clock_domain_id,
                          const WebSocketMessage &message,
                          sdk::CaptureIntegrityStatus integrity_status) {
  const auto receive_time = contracts::TimePoint::from(
      message.monotonic_receive_time_nanoseconds, monotonic_clock_domain_id,
      contracts::ClockClass::monotonic, 1);
  if (!receive_time.has_value()) {
    return {.failure = sdk::CaptureFailure::InvalidInput};
  }
  return recorder.capture({
      .source_event_id = source_event_id,
      .chronos_receive_time = *receive_time,
      .raw_payload = message.payload,
      .framing_protocol = sdk::FramingProtocol::WebSocket,
      .frame_kind = source_frame_kind(message.kind),
      .framing_status = sdk::FramingStatus::Complete,
      .integrity_status = integrity_status,
      .content_encoding = message.kind == WebSocketMessageKind::Text
                              ? sdk::ContentEncoding::Utf8Text
                              : sdk::ContentEncoding::OpaqueBinary,
      .compression_disposition = sdk::CompressionDisposition::NotCompressed,
      .fragmented = message.fragmented,
  });
}

WebSocketSourceCapture::WebSocketSourceCapture(
    sdk::SourceCaptureRecorder recorder,
    contracts::ClockDomainId monotonic_clock_domain_id,
    SourceEventIdentitySource &identity_source, SourceEventConsumer &consumer)
    : recorder_(std::move(recorder)),
      monotonic_clock_domain_id_(monotonic_clock_domain_id),
      identity_source_(identity_source), consumer_(consumer) {}

bool WebSocketSourceCapture::capture(const WebSocketMessage &message) {
  const auto event_id = identity_source_.next();
  if (!event_id.has_value()) {
    last_failure_ = sdk::CaptureFailure::InvalidInput;
    return false;
  }
  auto result = capture_websocket_message(recorder_, *event_id,
                                          monotonic_clock_domain_id_, message);
  last_failure_ = result.failure;
  if (!result.ok()) {
    return false;
  }
  return consumer_.accept(std::move(*result.event));
}

sdk::CaptureFailure WebSocketSourceCapture::last_failure() const noexcept {
  return last_failure_;
}

} // namespace chronos::adapters::market_data
