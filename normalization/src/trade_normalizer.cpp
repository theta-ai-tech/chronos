#include "chronos/normalization/market_data/trade_normalizer.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace chronos::normalization::market_data {
namespace {

bool valid_version(std::string_view value) {
  if (value.empty() || value.size() > 64) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') || character == '.' ||
           character == '-' || character == '_';
  });
}

std::optional<contracts::TimePoint>
source_time(std::uint64_t milliseconds,
            contracts::ClockDomainId source_wall_clock_domain_id) {
  constexpr std::int64_t nanoseconds_per_millisecond = 1'000'000;
  if (milliseconds >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() /
                                 nanoseconds_per_millisecond)) {
    return std::nullopt;
  }
  return contracts::TimePoint::from(
      static_cast<std::int64_t>(milliseconds) * nanoseconds_per_millisecond,
      source_wall_clock_domain_id, contracts::ClockClass::source_wall,
      nanoseconds_per_millisecond);
}

std::optional<AggressorSide> aggressor_side(std::string_view source_side) {
  if (source_side == "Buy") {
    return AggressorSide::Buy;
  }
  if (source_side == "Sell") {
    return AggressorSide::Sell;
  }
  return std::nullopt;
}

} // namespace

TradeNormalizationResult
normalize_trades(const BoundDecodedTradeMessage &decoded,
                 std::string_view source_venue,
                 const core::reference_data::ReferenceSnapshot &reference,
                 contracts::ClockDomainId source_wall_clock_domain_id,
                 const TradeNormalizerVersions &versions) {
  const auto &message = decoded.message;
  const auto &lineage = decoded.source_lineage;
  if (!valid_version(versions.decoder_version) ||
      !valid_version(versions.source_schema_version) ||
      !valid_version(versions.normalizer_version) || message.members.empty()) {
    return {.failure = TradeNormalizationFailure::SchemaViolation};
  }

  const auto &reference_listing = reference.listing();
  if (reference_listing.venue != source_venue) {
    return {.failure = TradeNormalizationFailure::WrongTopicOrSymbol};
  }

  TradeNormalizationResult result;
  result.facts.reserve(message.members.size());
  for (std::size_t index = 0; index < message.members.size(); ++index) {
    const auto &member = message.members[index];
    if (member.member_index != index) {
      return {.failure = TradeNormalizationFailure::AmbiguousDuplicate};
    }
    if (member.source_trade_id.empty()) {
      return {.failure = TradeNormalizationFailure::InvalidTradeId};
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (message.members[prior].source_trade_id == member.source_trade_id) {
        return {.failure = TradeNormalizationFailure::AmbiguousDuplicate};
      }
    }
    if (reference_listing.source_symbol != member.source_symbol) {
      return {.failure = TradeNormalizationFailure::WrongTopicOrSymbol};
    }
    const auto *listing = reference.resolve(source_venue, member.source_symbol,
                                            lineage.capture_partition_id,
                                            lineage.capture_sequence);
    if (listing == nullptr) {
      return {.failure = TradeNormalizationFailure::ReferenceUnavailable};
    }

    const auto side = aggressor_side(member.source_side);
    if (!side.has_value()) {
      return {.failure = TradeNormalizationFailure::InvalidSide};
    }
    const auto price = listing->parse_price(member.price_decimal);
    const auto quantity = listing->parse_quantity(member.quantity_decimal);
    if (!price.has_value() || !quantity.has_value() || price->units() <= 0 ||
        quantity->units() <= 0) {
      return {.failure = TradeNormalizationFailure::InvalidNumeric};
    }
    const auto system_timestamp = source_time(
        member.system_timestamp_milliseconds, source_wall_clock_domain_id);
    const auto timestamp = source_time(member.trade_timestamp_milliseconds,
                                       source_wall_clock_domain_id);
    if (!system_timestamp.has_value() || !timestamp.has_value()) {
      return {.failure = TradeNormalizationFailure::InvalidTime};
    }

    result.facts.push_back({
        .canonical_instrument_id = reference.instrument().instrument_id,
        .listing_id = listing->listing_id,
        .reference_snapshot_version = reference.version(),
        .instrument_version = reference.instrument().version,
        .listing_version = listing->version,
        .source_lineage = lineage,
        .source_message_extensions = message.envelope_extensions,
        .source_assertions = member,
        .source_event_time = *timestamp,
        .aggressor_side = *side,
        .price = *price,
        .quantity = *quantity,
        .decoder_version = versions.decoder_version,
        .source_schema_version = versions.source_schema_version,
        .normalizer_version = versions.normalizer_version,
    });
  }
  return result;
}

} // namespace chronos::normalization::market_data
