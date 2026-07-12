#pragma once

#include "chronos/adapters/sdk/source_event.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace chronos::adapters::market_data {

struct CaptureDatasetManifest final {
  std::string format_version;
  std::string dataset_id;
  std::string records_sha256;
  sdk::CaptureSessionId capture_session_id;
  sdk::CapturePartitionId capture_partition_id;
  contracts::RuntimeId runtime_id;
  std::optional<sdk::SourceConnectionId> connection_id;
  std::optional<sdk::SourceSubscriptionId> subscription_id;
  std::string adapter_id;
  std::string adapter_version;
  std::string build_version;
  std::string venue;
  sdk::EnvironmentClass environment{sdk::EnvironmentClass::Test};
  sdk::EndpointClass endpoint{sdk::EndpointClass::PublicMarketData};
  sdk::SourceTrustClass trust_class{
      sdk::SourceTrustClass::PublicUnauthenticated};
  std::string framing_version;
  std::string static_configuration_version;
  std::string capability_manifest_version;
  std::string schema_policy_version;
  sdk::DataClassification data_classification{
      sdk::DataClassification::PublicMarketData};
  sdk::AccessRestriction access_restriction{
      sdk::AccessRestriction::ChronosInternal};
  std::string dataset_class{"raw_source_capture"};
  bool replay_admissible{};
  std::uint64_t records_bytes{};
  std::uint64_t maximum_retained_payload_bytes{};
  std::uint64_t maximum_source_events{};
  std::uint64_t record_count{};
  std::uint64_t first_capture_sequence{};
  std::uint64_t last_capture_sequence{};
};

struct CaptureDatasetRecord final {
  contracts::SourceEventId source_event_id;
  std::uint64_t capture_sequence{};
  contracts::TimePoint chronos_receive_time;
  std::vector<std::byte> raw_payload;
  std::uint64_t original_payload_size{};
  sdk::PayloadDigest payload_digest;
  sdk::FramingProtocol framing_protocol{sdk::FramingProtocol::WebSocket};
  sdk::SourceFrameKind frame_kind{sdk::SourceFrameKind::Unknown};
  sdk::FramingStatus framing_status{sdk::FramingStatus::Complete};
  sdk::CaptureIntegrityStatus integrity_status{
      sdk::CaptureIntegrityStatus::Complete};
  sdk::ParseStatus parse_status{sdk::ParseStatus::NotAttempted};
  sdk::ContentEncoding content_encoding{sdk::ContentEncoding::Utf8Text};
  sdk::CompressionDisposition compression_disposition{
      sdk::CompressionDisposition::NotCompressed};
  bool fragmented{};

  bool operator==(const CaptureDatasetRecord &) const = default;
};

enum class DatasetFailure {
  None,
  InvalidConfiguration,
  Io,
  InvalidManifest,
  InvalidRecord,
  IntegrityMismatch,
  SequenceMismatch,
  AlreadySealed,
  PublicationUnconfirmed,
};

struct DatasetSealResult final {
  std::optional<CaptureDatasetManifest> manifest;
  DatasetFailure failure{DatasetFailure::None};
};

struct DatasetReadResult final {
  std::optional<CaptureDatasetManifest> manifest;
  std::vector<CaptureDatasetRecord> records;
  DatasetFailure failure{DatasetFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return manifest.has_value() && failure == DatasetFailure::None;
  }
};

class DatasetPersistence {
public:
  virtual ~DatasetPersistence() = default;
  [[nodiscard]] virtual bool sync_file(const std::filesystem::path &path) = 0;
  [[nodiscard]] virtual bool
  sync_directory(const std::filesystem::path &path) = 0;
  [[nodiscard]] virtual bool
  publish_directory(const std::filesystem::path &staging,
                    const std::filesystem::path &destination) = 0;
};

class CaptureDatasetWriter final {
public:
  [[nodiscard]] static std::optional<CaptureDatasetWriter>
  create(std::filesystem::path destination,
         const sdk::SourceCaptureContext &context,
         DatasetPersistence *persistence = nullptr);
  CaptureDatasetWriter(CaptureDatasetWriter &&) noexcept;
  CaptureDatasetWriter &operator=(CaptureDatasetWriter &&) noexcept;
  ~CaptureDatasetWriter();

  [[nodiscard]] DatasetFailure append(const sdk::SourceEvent &event);
  [[nodiscard]] DatasetSealResult seal();

private:
  struct State;
  explicit CaptureDatasetWriter(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

[[nodiscard]] DatasetReadResult
read_capture_dataset(const std::filesystem::path &dataset_directory);

} // namespace chronos::adapters::market_data
