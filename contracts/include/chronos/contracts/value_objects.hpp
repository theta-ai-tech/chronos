#pragma once

#include "chronos/contracts/digest.hpp"

#include <array>
#include <charconv>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace chronos::contracts {

template <typename Tag> class OpaqueId final {
public:
  using bytes_type = std::array<std::uint8_t, 16>;

  [[nodiscard]] static constexpr std::optional<OpaqueId>
  from_bytes(bytes_type bytes) noexcept {
    bool nonzero = false;
    for (const auto byte : bytes) {
      nonzero = nonzero || byte != 0;
    }
    if (!nonzero) {
      return std::nullopt;
    }
    return OpaqueId(bytes);
  }

  [[nodiscard]] static constexpr OpaqueId
  from_sha256_digest(const Sha256Digest &digest) noexcept {
    bytes_type bytes{};
    bool nonzero = false;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
      bytes[index] = digest.bytes[index];
      nonzero = nonzero || bytes[index] != 0;
    }
    // The all-zero value is reserved as invalid; use one stable fallback ID.
    if (!nonzero)
      bytes.back() = 1;
    return OpaqueId(bytes);
  }

  [[nodiscard]] static constexpr std::optional<OpaqueId>
  parse(std::string_view value) noexcept {
    if (value.size() != 36 || value[8] != '-' || value[13] != '-' ||
        value[18] != '-' || value[23] != '-') {
      return std::nullopt;
    }

    bytes_type bytes{};
    std::size_t byte_index = 0;
    std::uint8_t high_nibble = 0;
    bool have_high_nibble = false;
    for (std::size_t input_index = 0; input_index < value.size();
         ++input_index) {
      const char character = value[input_index];
      const bool separator = input_index == 8 || input_index == 13 ||
                             input_index == 18 || input_index == 23;
      if (separator) {
        continue;
      }
      if (character == '-') {
        return std::nullopt;
      }
      const auto nibble = decode_hex(character);
      if (!nibble.has_value()) {
        return std::nullopt;
      }
      if (!have_high_nibble) {
        high_nibble = *nibble;
        have_high_nibble = true;
      } else {
        bytes[byte_index++] =
            static_cast<std::uint8_t>((high_nibble << 4U) | *nibble);
        have_high_nibble = false;
      }
    }
    return from_bytes(bytes);
  }

  [[nodiscard]] constexpr const bytes_type &bytes() const noexcept {
    return bytes_;
  }

  [[nodiscard]] std::string to_string() const {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(36, '-');
    std::size_t output_index = 0;
    for (const auto byte : bytes_) {
      while (output_index == 8 || output_index == 13 || output_index == 18 ||
             output_index == 23) {
        ++output_index;
      }
      result[output_index++] = kHex[byte >> 4U];
      result[output_index++] = kHex[byte & 0x0FU];
    }
    return result;
  }

  auto operator<=>(const OpaqueId &) const = default;

private:
  explicit constexpr OpaqueId(bytes_type bytes) noexcept : bytes_(bytes) {}

  [[nodiscard]] static constexpr std::optional<std::uint8_t>
  decode_hex(char character) noexcept {
    if (character >= '0' && character <= '9') {
      return static_cast<std::uint8_t>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
      return static_cast<std::uint8_t>(character - 'a' + 10);
    }
    if (character >= 'A' && character <= 'F') {
      return static_cast<std::uint8_t>(character - 'A' + 10);
    }
    return std::nullopt;
  }

  bytes_type bytes_;
};

struct EventIdTag;
struct CommandIdTag;
struct StateViewIdTag;
struct DecisionIdTag;
struct AuthorityIdTag;
struct RuntimeIdTag;
struct CorrelationIdTag;
struct IntegrityIdTag;
struct StreamIdTag;
struct RunIdTag;
struct SourceEventIdTag;
struct SourceDecodeEnrichmentIdTag;
struct RunInputSelectionIdTag;
struct FeatureEvaluationIdTag;
struct FeatureObservationIdTag;
struct FeatureUnavailableIdTag;
struct StrategyInstanceIdTag;
struct StrategyEvaluationIdTag;
struct StrategySignalIdTag;
struct TradeRecommendationIdTag;
struct PortfolioIdTag;
struct AccountIdTag;
struct PortfolioSnapshotIdTag;
struct TargetPositionIdTag;
struct PortfolioConstructionOutcomeIdTag;
struct PublicationAttemptIdTag;
struct ConsumerBoundaryIdTag;
struct CapturePartitionIdTag;
struct CanonicalInstrumentIdTag;
struct ListingIdTag;
struct ProducerIdTag;
struct DefinitionIdTag;
struct ClockDomainIdTag;

using EventId = OpaqueId<EventIdTag>;
using CommandId = OpaqueId<CommandIdTag>;
using StateViewId = OpaqueId<StateViewIdTag>;
using DecisionId = OpaqueId<DecisionIdTag>;
using AuthorityId = OpaqueId<AuthorityIdTag>;
using RuntimeId = OpaqueId<RuntimeIdTag>;
using CorrelationId = OpaqueId<CorrelationIdTag>;
using IntegrityId = OpaqueId<IntegrityIdTag>;
using StreamId = OpaqueId<StreamIdTag>;
using RunId = OpaqueId<RunIdTag>;
using SourceEventId = OpaqueId<SourceEventIdTag>;
using SourceDecodeEnrichmentId = OpaqueId<SourceDecodeEnrichmentIdTag>;
using RunInputSelectionId = OpaqueId<RunInputSelectionIdTag>;
using FeatureEvaluationId = OpaqueId<FeatureEvaluationIdTag>;
using FeatureObservationId = OpaqueId<FeatureObservationIdTag>;
using FeatureUnavailableId = OpaqueId<FeatureUnavailableIdTag>;
using StrategyInstanceId = OpaqueId<StrategyInstanceIdTag>;
using StrategyEvaluationId = OpaqueId<StrategyEvaluationIdTag>;
using StrategySignalId = OpaqueId<StrategySignalIdTag>;
using TradeRecommendationId = OpaqueId<TradeRecommendationIdTag>;
using PortfolioId = OpaqueId<PortfolioIdTag>;
using AccountId = OpaqueId<AccountIdTag>;
using PortfolioSnapshotId = OpaqueId<PortfolioSnapshotIdTag>;
using TargetPositionId = OpaqueId<TargetPositionIdTag>;
using PortfolioConstructionOutcomeId =
    OpaqueId<PortfolioConstructionOutcomeIdTag>;
using PublicationAttemptId = OpaqueId<PublicationAttemptIdTag>;
using ConsumerBoundaryId = OpaqueId<ConsumerBoundaryIdTag>;
using CapturePartitionId = OpaqueId<CapturePartitionIdTag>;
using CanonicalInstrumentId = OpaqueId<CanonicalInstrumentIdTag>;
using ListingId = OpaqueId<ListingIdTag>;
using ProducerId = OpaqueId<ProducerIdTag>;
using DefinitionId = OpaqueId<DefinitionIdTag>;
using ClockDomainId = OpaqueId<ClockDomainIdTag>;

class StreamCursor final {
public:
  [[nodiscard]] static constexpr std::optional<StreamCursor>
  at_origin(StreamId stream_id, std::uint64_t stream_epoch) noexcept {
    if (stream_epoch == 0) {
      return std::nullopt;
    }
    return StreamCursor{stream_id, stream_epoch, std::nullopt};
  }

  [[nodiscard]] static constexpr std::optional<StreamCursor>
  at_sequence(StreamId stream_id, std::uint64_t stream_epoch,
              std::uint64_t sequence) noexcept {
    if (stream_epoch == 0) {
      return std::nullopt;
    }
    return StreamCursor{stream_id, stream_epoch, sequence};
  }

  [[nodiscard]] constexpr bool is_origin() const noexcept {
    return !last_consumed_sequence_.has_value();
  }

  [[nodiscard]] constexpr StreamId stream_id() const noexcept {
    return stream_id_;
  }
  [[nodiscard]] constexpr std::uint64_t stream_epoch() const noexcept {
    return stream_epoch_;
  }
  [[nodiscard]] constexpr std::optional<std::uint64_t>
  last_consumed_sequence() const noexcept {
    return last_consumed_sequence_;
  }

  bool operator==(const StreamCursor &) const = default;

private:
  constexpr StreamCursor(
      StreamId stream_id, std::uint64_t stream_epoch,
      std::optional<std::uint64_t> last_consumed_sequence) noexcept
      : stream_id_(stream_id), stream_epoch_(stream_epoch),
        last_consumed_sequence_(last_consumed_sequence) {}

  StreamId stream_id_;
  std::uint64_t stream_epoch_;
  std::optional<std::uint64_t> last_consumed_sequence_;
};

class VersionRef final {
public:
  [[nodiscard]] static constexpr std::optional<VersionRef>
  from(DefinitionId definition_id, std::uint64_t version) noexcept {
    if (version == 0) {
      return std::nullopt;
    }
    return VersionRef{definition_id, version};
  }

  [[nodiscard]] constexpr DefinitionId definition_id() const noexcept {
    return definition_id_;
  }
  [[nodiscard]] constexpr std::uint64_t version() const noexcept {
    return version_;
  }

  auto operator<=>(const VersionRef &) const = default;

private:
  constexpr VersionRef(DefinitionId definition_id,
                       std::uint64_t version) noexcept
      : definition_id_(definition_id), version_(version) {}

  DefinitionId definition_id_;
  std::uint64_t version_;
};

enum class ClockClass : std::uint8_t {
  source_wall,
  chronos_wall,
  monotonic,
  replay_logical,
};

[[nodiscard]] constexpr bool is_valid(ClockClass value) noexcept {
  switch (value) {
  case ClockClass::source_wall:
  case ClockClass::chronos_wall:
  case ClockClass::monotonic:
  case ClockClass::replay_logical:
    return true;
  }
  return false;
}

class TimePoint final {
public:
  [[nodiscard]] static constexpr std::optional<TimePoint>
  from(std::int64_t nanoseconds, ClockDomainId clock_domain_id,
       ClockClass clock_class, std::uint32_t precision_nanoseconds) noexcept {
    if (!is_valid(clock_class) || precision_nanoseconds == 0) {
      return std::nullopt;
    }
    return TimePoint{nanoseconds, clock_domain_id, clock_class,
                     precision_nanoseconds};
  }

  [[nodiscard]] constexpr std::int64_t nanoseconds() const noexcept {
    return nanoseconds_;
  }
  [[nodiscard]] constexpr ClockDomainId clock_domain_id() const noexcept {
    return clock_domain_id_;
  }
  [[nodiscard]] constexpr ClockClass clock_class() const noexcept {
    return clock_class_;
  }
  [[nodiscard]] constexpr std::uint32_t precision_nanoseconds() const noexcept {
    return precision_nanoseconds_;
  }

  bool operator==(const TimePoint &) const = default;

  [[nodiscard]] constexpr std::optional<std::strong_ordering>
  checked_compare(TimePoint other) const noexcept {
    if (clock_domain_id_ != other.clock_domain_id_ ||
        clock_class_ != other.clock_class_) {
      return std::nullopt;
    }
    return nanoseconds_ <=> other.nanoseconds_;
  }

private:
  constexpr TimePoint(std::int64_t nanoseconds, ClockDomainId clock_domain_id,
                      ClockClass clock_class,
                      std::uint32_t precision_nanoseconds) noexcept
      : nanoseconds_(nanoseconds), clock_domain_id_(clock_domain_id),
        clock_class_(clock_class),
        precision_nanoseconds_(precision_nanoseconds) {}

  std::int64_t nanoseconds_;
  ClockDomainId clock_domain_id_;
  ClockClass clock_class_;
  std::uint32_t precision_nanoseconds_;
};

enum class QualityStatus : std::uint8_t {
  valid,
  stale,
  gapped,
  recovering,
  invalid,
  unavailable,
};

[[nodiscard]] constexpr bool is_valid(QualityStatus value) noexcept {
  switch (value) {
  case QualityStatus::valid:
  case QualityStatus::stale:
  case QualityStatus::gapped:
  case QualityStatus::recovering:
  case QualityStatus::invalid:
  case QualityStatus::unavailable:
    return true;
  }
  return false;
}

class DataQuality final {
public:
  [[nodiscard]] static constexpr std::optional<DataQuality>
  from(QualityStatus status, std::uint32_t reason_code) noexcept {
    if (!is_valid(status) ||
        (status == QualityStatus::valid) != (reason_code == 0)) {
      return std::nullopt;
    }
    return DataQuality{status, reason_code};
  }

  [[nodiscard]] constexpr QualityStatus status() const noexcept {
    return status_;
  }
  [[nodiscard]] constexpr std::uint32_t reason_code() const noexcept {
    return reason_code_;
  }

  bool operator==(const DataQuality &) const = default;

private:
  constexpr DataQuality(QualityStatus status,
                        std::uint32_t reason_code) noexcept
      : status_(status), reason_code_(reason_code) {}

  QualityStatus status_;
  std::uint32_t reason_code_;
};

static_assert(sizeof(EventId) == 16);
static_assert(std::is_trivially_copyable_v<EventId>);
static_assert(std::is_trivially_copyable_v<StreamCursor>);
static_assert(std::is_trivially_copyable_v<VersionRef>);
static_assert(std::is_trivially_copyable_v<TimePoint>);
static_assert(std::is_trivially_copyable_v<DataQuality>);
static_assert(!std::is_aggregate_v<StreamCursor>);
static_assert(!std::is_aggregate_v<VersionRef>);
static_assert(!std::is_aggregate_v<TimePoint>);
static_assert(!std::is_aggregate_v<DataQuality>);

} // namespace chronos::contracts
