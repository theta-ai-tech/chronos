#pragma once

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
    for (const char character : value) {
      if (character == '-') {
        continue;
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
struct StreamIdTag;
struct RunIdTag;
struct SourceEventIdTag;
struct CanonicalInstrumentIdTag;
struct ListingIdTag;
struct ProducerIdTag;
struct DefinitionIdTag;

using EventId = OpaqueId<EventIdTag>;
using StreamId = OpaqueId<StreamIdTag>;
using RunId = OpaqueId<RunIdTag>;
using SourceEventId = OpaqueId<SourceEventIdTag>;
using CanonicalInstrumentId = OpaqueId<CanonicalInstrumentIdTag>;
using ListingId = OpaqueId<ListingIdTag>;
using ProducerId = OpaqueId<ProducerIdTag>;
using DefinitionId = OpaqueId<DefinitionIdTag>;

struct StreamCursor final {
  StreamId stream_id;
  std::uint64_t stream_epoch;
  std::optional<std::uint64_t> last_consumed_sequence;

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
    return !last_consumed_sequence.has_value();
  }

  bool operator==(const StreamCursor &) const = default;
};

struct VersionRef final {
  DefinitionId definition_id;
  std::uint64_t version;

  [[nodiscard]] static constexpr std::optional<VersionRef>
  from(DefinitionId definition_id, std::uint64_t version) noexcept {
    if (version == 0) {
      return std::nullopt;
    }
    return VersionRef{definition_id, version};
  }

  auto operator<=>(const VersionRef &) const = default;
};

enum class ClockDomain : std::uint8_t {
  source_wall,
  chronos_wall,
  monotonic,
  replay_logical,
};

struct TimePoint final {
  std::int64_t nanoseconds;
  ClockDomain clock_domain;
  std::uint32_t precision_nanoseconds;

  [[nodiscard]] static constexpr std::optional<TimePoint>
  from(std::int64_t nanoseconds, ClockDomain clock_domain,
       std::uint32_t precision_nanoseconds) noexcept {
    if (precision_nanoseconds == 0) {
      return std::nullopt;
    }
    return TimePoint{nanoseconds, clock_domain, precision_nanoseconds};
  }

  bool operator==(const TimePoint &) const = default;

  [[nodiscard]] constexpr std::optional<std::strong_ordering>
  checked_compare(TimePoint other) const noexcept {
    if (clock_domain != other.clock_domain) {
      return std::nullopt;
    }
    return nanoseconds <=> other.nanoseconds;
  }
};

enum class QualityStatus : std::uint8_t {
  valid,
  stale,
  gapped,
  recovering,
  invalid,
  unavailable,
};

struct DataQuality final {
  QualityStatus status;
  std::uint32_t reason_code;

  [[nodiscard]] static constexpr std::optional<DataQuality>
  from(QualityStatus status, std::uint32_t reason_code) noexcept {
    if ((status == QualityStatus::valid) != (reason_code == 0)) {
      return std::nullopt;
    }
    return DataQuality{status, reason_code};
  }

  bool operator==(const DataQuality &) const = default;
};

static_assert(sizeof(EventId) == 16);
static_assert(std::is_trivially_copyable_v<EventId>);
static_assert(std::is_trivially_copyable_v<StreamCursor>);
static_assert(std::is_trivially_copyable_v<VersionRef>);
static_assert(std::is_trivially_copyable_v<TimePoint>);
static_assert(std::is_trivially_copyable_v<DataQuality>);

} // namespace chronos::contracts
