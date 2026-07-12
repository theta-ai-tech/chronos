#pragma once

#include "chronos/adapters/sdk/adapter.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::adapters::sdk {

struct CaptureSessionIdTag;
struct CapturePartitionIdTag;
struct SourceConnectionIdTag;
struct SourceSubscriptionIdTag;
using CaptureSessionId = contracts::OpaqueId<CaptureSessionIdTag>;
using CapturePartitionId = contracts::OpaqueId<CapturePartitionIdTag>;
using SourceConnectionId = contracts::OpaqueId<SourceConnectionIdTag>;
using SourceSubscriptionId = contracts::OpaqueId<SourceSubscriptionIdTag>;

enum class SourceTrustClass : std::uint8_t { PublicUnauthenticated };
enum class FramingProtocol : std::uint8_t { WebSocket };
enum class SourceFrameKind : std::uint8_t {
  Text,
  Binary,
  Ping,
  Pong,
  Close,
  Unknown
};
enum class FramingStatus : std::uint8_t {
  Complete,
  Incomplete,
  Invalid,
  Unsupported
};
enum class CaptureIntegrityStatus : std::uint8_t {
  Complete,
  Malformed,
  Unsupported,
  Truncated,
  Corrupt,
  ResourceLimitExceeded,
};
enum class ParseStatus : std::uint8_t { NotAttempted, Unavailable };
enum class DigestCoverage : std::uint8_t { CompletePayload, RetainedPrefix };
enum class ContentEncoding : std::uint8_t { Utf8Text, OpaqueBinary };
enum class CompressionDisposition : std::uint8_t {
  NotCompressed,
  DecompressedLosslessly,
  CompressedOpaque,
};
enum class DataClassification : std::uint8_t { PublicMarketData };
enum class AccessRestriction : std::uint8_t { ChronosInternal };

struct PayloadDigest final {
  std::array<std::uint8_t, 32> bytes{};
  DigestCoverage coverage{DigestCoverage::CompletePayload};

  bool operator==(const PayloadDigest &) const = default;
  [[nodiscard]] std::string hex() const;
};

struct SourceCaptureContext final {
  std::string adapter_id;
  std::string adapter_version;
  std::string build_version;
  std::string venue;
  EnvironmentClass environment{EnvironmentClass::Test};
  EndpointClass endpoint{EndpointClass::PublicMarketData};
  SourceTrustClass trust_class{SourceTrustClass::PublicUnauthenticated};
  CaptureSessionId capture_session_id;
  contracts::RuntimeId runtime_id;
  std::optional<SourceConnectionId> connection_id;
  std::optional<SourceSubscriptionId> subscription_id;
  CapturePartitionId capture_partition_id;
  std::string framing_version;
  std::string static_configuration_version;
  std::string capability_manifest_version;
  std::string schema_policy_version;
  DataClassification data_classification{DataClassification::PublicMarketData};
  AccessRestriction access_restriction{AccessRestriction::ChronosInternal};
  std::size_t maximum_retained_payload_bytes{};
  std::size_t maximum_source_events{};
};

struct SourceCaptureInput final {
  contracts::SourceEventId source_event_id;
  contracts::TimePoint chronos_receive_time;
  std::span<const std::byte> raw_payload;
  FramingProtocol framing_protocol{FramingProtocol::WebSocket};
  SourceFrameKind frame_kind{SourceFrameKind::Unknown};
  FramingStatus framing_status{FramingStatus::Complete};
  CaptureIntegrityStatus integrity_status{CaptureIntegrityStatus::Complete};
  ContentEncoding content_encoding{ContentEncoding::Utf8Text};
  CompressionDisposition compression_disposition{
      CompressionDisposition::NotCompressed};
  bool fragmented{};
};

class SourceEvent final {
public:
  SourceEvent(const SourceEvent &) = default;
  SourceEvent(SourceEvent &&) noexcept = default;
  SourceEvent &operator=(const SourceEvent &) = delete;
  SourceEvent &operator=(SourceEvent &&) = delete;

  [[nodiscard]] contracts::SourceEventId source_event_id() const noexcept;
  [[nodiscard]] CaptureSessionId capture_session_id() const noexcept;
  [[nodiscard]] contracts::RuntimeId runtime_id() const noexcept;
  [[nodiscard]] const std::optional<SourceConnectionId> &
  connection_id() const noexcept;
  [[nodiscard]] const std::optional<SourceSubscriptionId> &
  subscription_id() const noexcept;
  [[nodiscard]] CapturePartitionId capture_partition_id() const noexcept;
  [[nodiscard]] std::uint64_t capture_sequence() const noexcept;
  [[nodiscard]] contracts::TimePoint chronos_receive_time() const noexcept;
  [[nodiscard]] std::span<const std::byte> raw_payload() const noexcept;
  [[nodiscard]] std::size_t original_payload_size() const noexcept;
  [[nodiscard]] const PayloadDigest &payload_digest() const noexcept;
  [[nodiscard]] FramingProtocol framing_protocol() const noexcept;
  [[nodiscard]] SourceFrameKind frame_kind() const noexcept;
  [[nodiscard]] FramingStatus framing_status() const noexcept;
  [[nodiscard]] CaptureIntegrityStatus integrity_status() const noexcept;
  [[nodiscard]] ParseStatus parse_status() const noexcept;
  [[nodiscard]] ContentEncoding content_encoding() const noexcept;
  [[nodiscard]] CompressionDisposition compression_disposition() const noexcept;
  [[nodiscard]] bool fragmented() const noexcept;
  [[nodiscard]] bool compressed() const noexcept;
  [[nodiscard]] const SourceCaptureContext &context() const noexcept;

private:
  friend class SourceCaptureRecorder;
  SourceEvent(SourceCaptureContext context, SourceCaptureInput input,
              std::uint64_t capture_sequence,
              std::vector<std::byte> retained_payload, PayloadDigest digest,
              FramingStatus effective_framing,
              CaptureIntegrityStatus effective_integrity);

  SourceCaptureContext context_;
  contracts::SourceEventId source_event_id_;
  contracts::TimePoint chronos_receive_time_;
  std::vector<std::byte> raw_payload_;
  std::size_t original_payload_size_{};
  PayloadDigest payload_digest_;
  std::uint64_t capture_sequence_{};
  FramingProtocol framing_protocol_;
  SourceFrameKind frame_kind_;
  FramingStatus framing_status_;
  CaptureIntegrityStatus integrity_status_;
  ParseStatus parse_status_{ParseStatus::NotAttempted};
  ContentEncoding content_encoding_;
  CompressionDisposition compression_disposition_;
  bool fragmented_{};
};

enum class CaptureFailure : std::uint8_t {
  None,
  InvalidContext,
  InvalidInput,
  DuplicateSourceEventId,
  SequenceExhausted,
  CapacityExceeded,
};

struct CaptureResult final {
  std::optional<SourceEvent> event;
  CaptureFailure failure{CaptureFailure::None};

  [[nodiscard]] bool ok() const noexcept { return event.has_value(); }
};

class SourceCaptureRecorder final {
public:
  [[nodiscard]] static std::optional<SourceCaptureRecorder>
  create(SourceCaptureContext context);

  [[nodiscard]] CaptureResult capture(SourceCaptureInput input);
  [[nodiscard]] const SourceCaptureContext &context() const noexcept;
  [[nodiscard]] std::uint64_t next_capture_sequence() const noexcept;

private:
  explicit SourceCaptureRecorder(SourceCaptureContext context);

  SourceCaptureContext context_;
  std::uint64_t next_capture_sequence_{1};
  std::set<contracts::SourceEventId> accepted_event_ids_;
};

[[nodiscard]] PayloadDigest sha256(std::span<const std::byte> payload,
                                   DigestCoverage coverage);

} // namespace chronos::adapters::sdk
