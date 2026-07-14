#pragma once

#include "chronos/adapters/sdk/source_event.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chronos::normalization::market_data {

enum class SourceTimestampUnit : std::uint8_t { Milliseconds };
enum class SourceProductClass : std::uint8_t { Spot, LinearPerpetual };

struct SourceExtensionField final {
  std::string json_pointer;
  std::string canonical_json;

  bool operator==(const SourceExtensionField &) const = default;
};

struct SourceDecodeEvidence final {
  contracts::SourceDecodeEnrichmentId source_decode_enrichment_id;
  std::uint32_t source_member_index{};
  std::string decoder_version;
  std::string source_schema_version;
  std::string registry_version;
  std::string canonicalization_version;
  adapters::sdk::PayloadDigest semantic_checksum;

  bool operator==(const SourceDecodeEvidence &) const = default;
};

struct SourceCaptureLineage final {
  std::string dataset_format_version;
  std::string dataset_id;
  std::string records_sha256;
  std::uint64_t dataset_record_index{};
  contracts::SourceEventId source_event_id;
  adapters::sdk::CaptureSessionId capture_session_id;
  contracts::RuntimeId runtime_id;
  std::optional<adapters::sdk::SourceConnectionId> connection_id;
  std::optional<adapters::sdk::SourceSubscriptionId> subscription_id;
  contracts::CapturePartitionId capture_partition_id;
  std::uint64_t capture_sequence{};
  contracts::TimePoint chronos_receive_time;
  adapters::sdk::PayloadDigest payload_digest;
  std::string adapter_version;
  std::string build_version;
  std::string framing_version;
  std::string static_configuration_version;
  std::string capability_manifest_version;
  std::string schema_policy_version;

  bool operator==(const SourceCaptureLineage &) const = default;
};

struct ReferenceSemanticKey final {
  std::string venue;
  adapters::sdk::EnvironmentClass environment{
      adapters::sdk::EnvironmentClass::Test};
  SourceProductClass product_class{SourceProductClass::Spot};
  std::string source_listing_key;

  bool operator==(const ReferenceSemanticKey &) const = default;
};

struct ReferenceSelectionEvidence final {
  contracts::VersionRef reference_configuration_lineage_version;
  std::string lineage_schema_version;
  std::string semantic_key_policy_version;
  std::string effective_basis_policy_version;
  std::string selection_policy_version;
  ReferenceSemanticKey semantic_key;
  contracts::CapturePartitionId effective_capture_partition_id;
  std::uint64_t effective_capture_sequence{};

  bool operator==(const ReferenceSelectionEvidence &) const = default;
};

} // namespace chronos::normalization::market_data
