#pragma once

#include "chronos/adapters/sdk/source_event.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"
#include "chronos/normalization/market_data/source_lineage.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::normalization::market_data {

enum class TradeNormalizationFailure : std::uint8_t {
  None,
  IntegrityIneligible,
  ResourceLimitExceeded,
  MalformedPayload,
  SchemaViolation,
  UnsupportedMessage,
  WrongTopicOrSymbol,
  AmbiguousDuplicate,
  InvalidTradeId,
  InvalidSide,
  InvalidNumeric,
  InvalidTime,
  ReferenceUnavailable,
};

enum class AggressorSide : std::uint8_t { Buy, Sell };

struct SourceExtension final {
  std::string name;
  std::string canonical_json;

  bool operator==(const SourceExtension &) const = default;
};

struct SourceTradeAssertions final {
  std::string topic;
  std::string source_symbol;
  std::string source_trade_id;
  std::string source_side;
  std::string price_decimal;
  std::string quantity_decimal;
  std::string tick_direction;
  bool block_trade{};
  std::optional<bool> rpi_trade;
  std::optional<std::uint64_t> sequence;
  adapters::sdk::SourceSequenceScope sequence_scope{
      adapters::sdk::SourceSequenceScope::VenueCrossSequence};
  std::uint64_t system_timestamp_milliseconds{};
  std::uint64_t trade_timestamp_milliseconds{};
  SourceTimestampUnit timestamp_unit{SourceTimestampUnit::Milliseconds};
  std::uint32_t member_index{};
  std::vector<SourceExtension> extensions;

  bool operator==(const SourceTradeAssertions &) const = default;
};

struct DecodedTradeMessage final {
  std::vector<SourceExtension> envelope_extensions;
  std::vector<SourceTradeAssertions> members;

  bool operator==(const DecodedTradeMessage &) const = default;
};

struct BoundDecodedTradeMessage final {
  DecodedTradeMessage message;
  SourceCaptureLineage source_lineage;

  bool operator==(const BoundDecodedTradeMessage &) const = default;
};

struct NormalizedTradeFact final {
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::ListingId listing_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef instrument_version;
  contracts::VersionRef listing_version;
  SourceCaptureLineage source_lineage;
  std::vector<SourceExtension> source_message_extensions;
  SourceTradeAssertions source_assertions;
  contracts::TimePoint source_event_time;
  AggressorSide aggressor_side{AggressorSide::Buy};
  contracts::Price price;
  contracts::Quantity quantity;
  std::string decoder_version;
  std::string source_schema_version;
  std::string normalizer_version;

  [[nodiscard]] constexpr std::string_view event_type() const noexcept {
    return "market.trade.observed";
  }

  bool operator==(const NormalizedTradeFact &) const = default;
};

} // namespace chronos::normalization::market_data
