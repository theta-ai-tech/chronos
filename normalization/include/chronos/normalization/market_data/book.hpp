#pragma once

#include "chronos/adapters/sdk/source_event.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace chronos::normalization::market_data {

enum class SourceBookKind : std::uint8_t { Snapshot, Delta };
enum class BookLevelOperation : std::uint8_t { SetAbsolute, Delete };
enum class SourceTimestampUnit : std::uint8_t { Milliseconds };
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
};

struct SourceBookLevel final {
  std::string price_decimal;
  std::string quantity_decimal;

  bool operator==(const SourceBookLevel &) const = default;
};

struct SourceBookAssertions final {
  SourceBookKind kind{SourceBookKind::Snapshot};
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

struct DecodedBookMessage final {
  SourceBookAssertions assertions;
  std::vector<SourceBookLevel> bids;
  std::vector<SourceBookLevel> asks;

  bool operator==(const DecodedBookMessage &) const = default;
};

// Capture IDs remain adapter-SDK types until the reviewed M3.1 canonical-ID
// change lands. Keeping that dependency here isolates the later type swap.
struct SourceCaptureLineage final {
  contracts::SourceEventId source_event_id;
  adapters::sdk::CaptureSessionId capture_session_id;
  contracts::RuntimeId runtime_id;
  std::optional<adapters::sdk::SourceConnectionId> connection_id;
  std::optional<adapters::sdk::SourceSubscriptionId> subscription_id;
  adapters::sdk::CapturePartitionId capture_partition_id;
  std::uint64_t capture_sequence{};
  contracts::TimePoint chronos_receive_time;
  adapters::sdk::PayloadDigest payload_digest;

  bool operator==(const SourceCaptureLineage &) const = default;
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
  SourceCaptureLineage source_lineage;
  SourceBookAssertions source_assertions;
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
