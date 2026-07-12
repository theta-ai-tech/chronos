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

sdk::FramingStatus
framing_status(sdk::CaptureIntegrityStatus integrity_status) noexcept {
  switch (integrity_status) {
  case sdk::CaptureIntegrityStatus::Complete:
    return sdk::FramingStatus::Complete;
  case sdk::CaptureIntegrityStatus::Malformed:
  case sdk::CaptureIntegrityStatus::Corrupt:
    return sdk::FramingStatus::Invalid;
  case sdk::CaptureIntegrityStatus::Unsupported:
    return sdk::FramingStatus::Unsupported;
  case sdk::CaptureIntegrityStatus::Truncated:
  case sdk::CaptureIntegrityStatus::ResourceLimitExceeded:
    return sdk::FramingStatus::Incomplete;
  }
  return sdk::FramingStatus::Invalid;
}

sdk::CaptureIntegrityStatus
capture_integrity(WebSocketIngressIntegrity integrity) noexcept {
  switch (integrity) {
  case WebSocketIngressIntegrity::Complete:
    return sdk::CaptureIntegrityStatus::Complete;
  case WebSocketIngressIntegrity::Malformed:
    return sdk::CaptureIntegrityStatus::Malformed;
  case WebSocketIngressIntegrity::Unsupported:
    return sdk::CaptureIntegrityStatus::Unsupported;
  case WebSocketIngressIntegrity::Truncated:
    return sdk::CaptureIntegrityStatus::Truncated;
  case WebSocketIngressIntegrity::ResourceLimitExceeded:
    return sdk::CaptureIntegrityStatus::ResourceLimitExceeded;
  }
  return sdk::CaptureIntegrityStatus::Corrupt;
}
} // namespace

sdk::CaptureResult
capture_websocket_message(sdk::SourceCaptureRecorder &recorder,
                          contracts::SourceEventId source_event_id,
                          contracts::ClockDomainId monotonic_clock_domain_id,
                          const WebSocketMessage &message) {
  const auto receive_time = contracts::TimePoint::from(
      message.monotonic_receive_time_nanoseconds, monotonic_clock_domain_id,
      contracts::ClockClass::monotonic, 1);
  if (!receive_time.has_value()) {
    return {.failure = sdk::CaptureFailure::InvalidInput};
  }
  const auto integrity_status = capture_integrity(message.integrity);
  const auto original_size = message.original_payload_size == 0
                                 ? message.payload.size()
                                 : message.original_payload_size;
  return recorder.capture({
      .source_event_id = source_event_id,
      .chronos_receive_time = *receive_time,
      .raw_payload = message.payload,
      .original_payload_size = original_size,
      .complete_payload_available =
          message.integrity == WebSocketIngressIntegrity::Complete,
      .framing_protocol = sdk::FramingProtocol::WebSocket,
      .frame_kind = source_frame_kind(message.kind),
      .framing_status = framing_status(integrity_status),
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
  if (pending_event_.has_value()) {
    last_failure_ = sdk::CaptureFailure::ConsumerRejected;
    return false;
  }
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
  pending_event_.emplace(std::move(*result.event));
  return retry_pending();
}

bool WebSocketSourceCapture::retry_pending() {
  if (!pending_event_.has_value()) {
    return true;
  }
  if (!consumer_.accept(*pending_event_)) {
    last_failure_ = sdk::CaptureFailure::ConsumerRejected;
    return false;
  }
  pending_event_.reset();
  last_failure_ = sdk::CaptureFailure::None;
  return true;
}

bool WebSocketSourceCapture::has_pending() const noexcept {
  return pending_event_.has_value();
}

sdk::CaptureFailure WebSocketSourceCapture::last_failure() const noexcept {
  return last_failure_;
}

} // namespace chronos::adapters::market_data
