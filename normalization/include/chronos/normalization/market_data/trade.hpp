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
#include <vector>

namespace chronos::adapters::market_data {
class BybitTradeDecoderAccess;
}

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
  ReferenceAmbiguous,
};

enum class AggressorSide : std::uint8_t { Buy, Sell };

struct SourceTradeAssertions final {
  std::string venue;
  adapters::sdk::EnvironmentClass environment{
      adapters::sdk::EnvironmentClass::Test};
  SourceProductClass product_class{SourceProductClass::Spot};
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
  std::vector<SourceExtensionField> extensions;

  bool operator==(const SourceTradeAssertions &) const = default;
};

struct DecodedTradeMessage final {
  std::vector<SourceExtensionField> envelope_extensions;
  std::vector<SourceTradeAssertions> members;

  bool operator==(const DecodedTradeMessage &) const = default;
};

class DecodedTradeEnrichment final {
public:
  [[nodiscard]] const DecodedTradeMessage &message() const noexcept {
    return message_;
  }
  [[nodiscard]] const SourceCaptureLineage &source_lineage() const noexcept {
    return source_lineage_;
  }
  [[nodiscard]] const std::vector<SourceDecodeEvidence> &
  decode_evidence() const noexcept {
    return decode_evidence_;
  }

  bool operator==(const DecodedTradeEnrichment &) const = default;

private:
  DecodedTradeEnrichment(DecodedTradeMessage message,
                         SourceCaptureLineage source_lineage,
                         std::vector<SourceDecodeEvidence> decode_evidence)
      : message_(std::move(message)),
        source_lineage_(std::move(source_lineage)),
        decode_evidence_(std::move(decode_evidence)) {}

  DecodedTradeMessage message_;
  SourceCaptureLineage source_lineage_;
  std::vector<SourceDecodeEvidence> decode_evidence_;

  friend class chronos::adapters::market_data::BybitTradeDecoderAccess;
};

struct NormalizedTradeFact final {
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::ListingId listing_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef instrument_version;
  contracts::VersionRef listing_version;
  ReferenceSelectionEvidence reference_selection;
  SourceCaptureLineage source_lineage;
  SourceDecodeEvidence source_decode_evidence;
  std::vector<SourceExtensionField> source_message_extensions;
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
