#include "chronos/adapters/sdk/source_event.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace chronos::adapters::sdk {
namespace {
constexpr std::size_t kAbsoluteCaptureBound = 64U * 1024U * 1024U;

bool valid_context(const SourceCaptureContext &context) noexcept {
  return detail::valid_token(context.adapter_id) &&
         detail::valid_token(context.adapter_version) &&
         detail::valid_token(context.build_version) &&
         detail::valid_token(context.venue) &&
         detail::known(context.environment) &&
         detail::known(context.endpoint) &&
         context.trust_class == SourceTrustClass::PublicUnauthenticated &&
         detail::valid_token(context.framing_version) &&
         detail::valid_token(context.static_configuration_version) &&
         detail::valid_token(context.capability_manifest_version) &&
         detail::valid_token(context.schema_policy_version) &&
         context.data_classification == DataClassification::PublicMarketData &&
         context.access_restriction == AccessRestriction::ChronosInternal &&
         context.maximum_retained_payload_bytes > 0 &&
         context.maximum_retained_payload_bytes <= kAbsoluteCaptureBound &&
         context.maximum_source_events > 0;
}

bool valid_input(const SourceCaptureInput &input) noexcept {
  const bool protocol_valid =
      input.framing_protocol == FramingProtocol::WebSocket;
  const bool frame_kind_valid = input.frame_kind >= SourceFrameKind::Text &&
                                input.frame_kind <= SourceFrameKind::Unknown;
  const bool framing_valid = input.framing_status >= FramingStatus::Complete &&
                             input.framing_status <= FramingStatus::Unsupported;
  const bool integrity_valid =
      input.integrity_status >= CaptureIntegrityStatus::Complete &&
      input.integrity_status <= CaptureIntegrityStatus::ResourceLimitExceeded;
  const bool clock_valid = input.chronos_receive_time.clock_class() ==
                           contracts::ClockClass::monotonic;
  const bool encoding_valid =
      input.content_encoding >= ContentEncoding::Utf8Text &&
      input.content_encoding <= ContentEncoding::OpaqueBinary;
  const bool compression_valid =
      input.compression_disposition >= CompressionDisposition::NotCompressed &&
      input.compression_disposition <= CompressionDisposition::CompressedOpaque;
  const bool complete_unknown =
      input.integrity_status == CaptureIntegrityStatus::Complete &&
      (input.framing_status != FramingStatus::Complete ||
       input.frame_kind == SourceFrameKind::Unknown);
  return protocol_valid && frame_kind_valid && framing_valid &&
         integrity_valid && clock_valid && encoding_valid &&
         compression_valid && !complete_unknown;
}
} // namespace

std::string PayloadDigest::hex() const {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result(bytes.size() * 2, '0');
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    result[index * 2] = digits[bytes[index] >> 4U];
    result[index * 2 + 1] = digits[bytes[index] & 0x0FU];
  }
  return result;
}

SourceEvent::SourceEvent(SourceCaptureContext context, SourceCaptureInput input,
                         std::uint64_t capture_sequence,
                         std::vector<std::byte> retained_payload,
                         PayloadDigest digest, FramingStatus effective_framing,
                         CaptureIntegrityStatus effective_integrity)
    : context_(std::move(context)), source_event_id_(input.source_event_id),
      chronos_receive_time_(input.chronos_receive_time),
      raw_payload_(std::move(retained_payload)),
      original_payload_size_(input.raw_payload.size()),
      payload_digest_(std::move(digest)), capture_sequence_(capture_sequence),
      framing_protocol_(input.framing_protocol), frame_kind_(input.frame_kind),
      framing_status_(effective_framing),
      integrity_status_(effective_integrity),
      content_encoding_(input.content_encoding),
      compression_disposition_(input.compression_disposition),
      fragmented_(input.fragmented) {}

contracts::SourceEventId SourceEvent::source_event_id() const noexcept {
  return source_event_id_;
}
CaptureSessionId SourceEvent::capture_session_id() const noexcept {
  return context_.capture_session_id;
}
contracts::RuntimeId SourceEvent::runtime_id() const noexcept {
  return context_.runtime_id;
}
const std::optional<SourceConnectionId> &
SourceEvent::connection_id() const noexcept {
  return context_.connection_id;
}
const std::optional<SourceSubscriptionId> &
SourceEvent::subscription_id() const noexcept {
  return context_.subscription_id;
}
CapturePartitionId SourceEvent::capture_partition_id() const noexcept {
  return context_.capture_partition_id;
}
std::uint64_t SourceEvent::capture_sequence() const noexcept {
  return capture_sequence_;
}
contracts::TimePoint SourceEvent::chronos_receive_time() const noexcept {
  return chronos_receive_time_;
}
std::span<const std::byte> SourceEvent::raw_payload() const noexcept {
  return raw_payload_;
}
std::size_t SourceEvent::original_payload_size() const noexcept {
  return original_payload_size_;
}
const PayloadDigest &SourceEvent::payload_digest() const noexcept {
  return payload_digest_;
}
FramingProtocol SourceEvent::framing_protocol() const noexcept {
  return framing_protocol_;
}
SourceFrameKind SourceEvent::frame_kind() const noexcept { return frame_kind_; }
FramingStatus SourceEvent::framing_status() const noexcept {
  return framing_status_;
}
CaptureIntegrityStatus SourceEvent::integrity_status() const noexcept {
  return integrity_status_;
}
ParseStatus SourceEvent::parse_status() const noexcept { return parse_status_; }
ContentEncoding SourceEvent::content_encoding() const noexcept {
  return content_encoding_;
}
CompressionDisposition SourceEvent::compression_disposition() const noexcept {
  return compression_disposition_;
}
bool SourceEvent::fragmented() const noexcept { return fragmented_; }
bool SourceEvent::compressed() const noexcept {
  return compression_disposition_ != CompressionDisposition::NotCompressed;
}
const SourceCaptureContext &SourceEvent::context() const noexcept {
  return context_;
}

std::optional<SourceCaptureRecorder>
SourceCaptureRecorder::create(SourceCaptureContext context) {
  if (!valid_context(context)) {
    return std::nullopt;
  }
  return SourceCaptureRecorder(std::move(context));
}

SourceCaptureRecorder::SourceCaptureRecorder(SourceCaptureContext context)
    : context_(std::move(context)) {}

CaptureResult SourceCaptureRecorder::capture(SourceCaptureInput input) {
  if (!valid_input(input)) {
    return {.failure = CaptureFailure::InvalidInput};
  }
  if (accepted_event_ids_.contains(input.source_event_id)) {
    return {.failure = CaptureFailure::DuplicateSourceEventId};
  }
  if (next_capture_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return {.failure = CaptureFailure::SequenceExhausted};
  }
  if (accepted_event_ids_.size() >= context_.maximum_source_events) {
    return {.failure = CaptureFailure::CapacityExceeded};
  }

  const auto retained_size = std::min(input.raw_payload.size(),
                                      context_.maximum_retained_payload_bytes);
  const auto retained_view = input.raw_payload.first(retained_size);
  std::vector<std::byte> retained(retained_view.begin(), retained_view.end());
  const bool limited = retained_size != input.raw_payload.size();
  const auto effective_integrity =
      limited ? CaptureIntegrityStatus::ResourceLimitExceeded
              : input.integrity_status;
  const auto effective_framing =
      limited ? FramingStatus::Incomplete : input.framing_status;
  auto digest = sha256(retained, limited ? DigestCoverage::RetainedPrefix
                                         : DigestCoverage::CompletePayload);
  const auto sequence = next_capture_sequence_;
  SourceEvent event(context_, input, sequence, std::move(retained),
                    std::move(digest), effective_framing, effective_integrity);
  accepted_event_ids_.insert(input.source_event_id);
  ++next_capture_sequence_;
  return {.event = std::move(event)};
}

const SourceCaptureContext &SourceCaptureRecorder::context() const noexcept {
  return context_;
}
std::uint64_t SourceCaptureRecorder::next_capture_sequence() const noexcept {
  return next_capture_sequence_;
}

} // namespace chronos::adapters::sdk
