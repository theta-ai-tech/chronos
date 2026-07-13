#pragma once

#include "chronos/adapters/sdk/source_event.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"
#include "chronos/normalization/market_data/source_lineage.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace chronos::adapters::market_data {
class BybitBookDecoderAccess;
}

namespace chronos::normalization::market_data {

enum class SourceBookKind : std::uint8_t { Snapshot, Delta };
enum class SourceProductClass : std::uint8_t { Spot, LinearPerpetual };
enum class BookLevelOperation : std::uint8_t { SetAbsolute, Delete };
enum class BookNormalizationFailure : std::uint8_t {
  None,
  IntegrityIneligible,
  ResourceLimitExceeded,
  MalformedPayload,
  SchemaViolation,
  UnsupportedMessage,
  WrongTopicOrSymbol,
  AmbiguousDuplicate,
  InvalidNumeric,
  ReferenceUnavailable,
  ReferenceAmbiguous,
};

struct SourceBookLevel final {
  std::string price_decimal;
  std::string quantity_decimal;

  bool operator==(const SourceBookLevel &) const = default;
};

struct SourceBookAssertions final {
  SourceBookKind kind{SourceBookKind::Snapshot};
  std::string venue;
  adapters::sdk::EnvironmentClass environment{
      adapters::sdk::EnvironmentClass::Test};
  SourceProductClass product_class{SourceProductClass::Spot};
  std::string topic;
  std::string source_symbol;
  std::uint32_t depth{};
  std::uint64_t sequence{};
  adapters::sdk::SourceSequenceScope sequence_scope{
      adapters::sdk::SourceSequenceScope::VenueCrossSequence};
  std::uint64_t update_id{};
  adapters::sdk::SourceSequenceScope update_id_scope{
      adapters::sdk::SourceSequenceScope::ListingChannel};
  std::uint64_t system_timestamp_milliseconds{};
  std::uint64_t matching_timestamp_milliseconds{};
  SourceTimestampUnit timestamp_unit{SourceTimestampUnit::Milliseconds};

  bool operator==(const SourceBookAssertions &) const = default;
};

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

struct DecodedBookMessage final {
  SourceBookAssertions assertions;
  std::vector<SourceBookLevel> bids;
  std::vector<SourceBookLevel> asks;
  std::vector<SourceExtensionField> extensions;

  bool operator==(const DecodedBookMessage &) const = default;
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

class DecodedBookEnrichment final {
public:
  [[nodiscard]] const DecodedBookMessage &message() const noexcept {
    return message_;
  }
  [[nodiscard]] const SourceCaptureLineage &source_lineage() const noexcept {
    return source_lineage_;
  }
  [[nodiscard]] const SourceDecodeEvidence &decode_evidence() const noexcept {
    return decode_evidence_;
  }

  bool operator==(const DecodedBookEnrichment &) const = default;

private:
  DecodedBookEnrichment(DecodedBookMessage message,
                        SourceCaptureLineage source_lineage,
                        SourceDecodeEvidence decode_evidence)
      : message_(std::move(message)),
        source_lineage_(std::move(source_lineage)),
        decode_evidence_(std::move(decode_evidence)) {}

  DecodedBookMessage message_;
  SourceCaptureLineage source_lineage_;
  SourceDecodeEvidence decode_evidence_;

  friend class chronos::adapters::market_data::BybitBookDecoderAccess;
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

struct BookLevel final {
  contracts::Price price;
  contracts::Quantity quantity;

  bool operator==(const BookLevel &) const = default;
};

struct BookLevelChange final {
  contracts::Price price;
  contracts::Quantity quantity;
  BookLevelOperation operation{BookLevelOperation::SetAbsolute};

  bool operator==(const BookLevelChange &) const = default;
};

struct BookSnapshotObservation final {
  std::vector<BookLevel> bids;
  std::vector<BookLevel> asks;

  bool operator==(const BookSnapshotObservation &) const = default;
};

struct BookDeltaObservation final {
  std::vector<BookLevelChange> bid_changes;
  std::vector<BookLevelChange> ask_changes;

  bool operator==(const BookDeltaObservation &) const = default;
};

using BookObservationPayload =
    std::variant<BookSnapshotObservation, BookDeltaObservation>;

struct NormalizedBookFact final {
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::ListingId listing_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef instrument_version;
  contracts::VersionRef listing_version;
  ReferenceSelectionEvidence reference_selection;
  SourceCaptureLineage source_lineage;
  SourceDecodeEvidence source_decode_evidence;
  SourceBookAssertions source_assertions;
  std::vector<SourceExtensionField> source_extensions;
  contracts::TimePoint source_event_time;
  std::string decoder_version;
  std::string source_schema_version;
  std::string normalizer_version;
  BookObservationPayload payload;

  [[nodiscard]] std::string_view event_type() const noexcept {
    return std::holds_alternative<BookSnapshotObservation>(payload)
               ? "market.book.snapshot_observed"
               : "market.book.delta_observed";
  }

  bool operator==(const NormalizedBookFact &) const = default;
};

} // namespace chronos::normalization::market_data
