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

std::optional<core::reference_data::VenueEnvironment>
reference_environment(adapters::sdk::EnvironmentClass environment) {
  switch (environment) {
  case adapters::sdk::EnvironmentClass::Test:
    return core::reference_data::VenueEnvironment::Test;
  case adapters::sdk::EnvironmentClass::Production:
    return core::reference_data::VenueEnvironment::Production;
  }
  return std::nullopt;
}

std::optional<core::reference_data::ProductClass>
reference_product_class(SourceProductClass product_class) {
  switch (product_class) {
  case SourceProductClass::Spot:
    return core::reference_data::ProductClass::Spot;
  case SourceProductClass::LinearPerpetual:
    return core::reference_data::ProductClass::LinearPerpetual;
  }
  return std::nullopt;
}

} // namespace

TradeNormalizationResult
normalize_trades(const DecodedTradeEnrichment &enrichment,
                 const core::reference_data::ReferenceConfigurationLineage
                     &reference_lineage,
                 contracts::ClockDomainId source_wall_clock_domain_id,
                 const TradeNormalizerVersions &versions) {
  const auto &message = enrichment.message();
  const auto &lineage = enrichment.source_lineage();
  const auto &decode_evidence = enrichment.decode_evidence();
  if (!valid_version(versions.normalizer_version) || message.members.empty() ||
      decode_evidence.size() != message.members.size()) {
    return {.failure = TradeNormalizationFailure::SchemaViolation};
  }

  TradeNormalizationResult result;
  result.facts.reserve(message.members.size());
  for (std::size_t index = 0; index < message.members.size(); ++index) {
    const auto &member = message.members[index];
    const auto &evidence = decode_evidence[index];
    if (member.member_index != index || evidence.source_member_index != index ||
        !valid_version(evidence.decoder_version) ||
        !valid_version(evidence.source_schema_version) ||
        !valid_version(evidence.registry_version) ||
        !valid_version(evidence.canonicalization_version)) {
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
    const auto environment = reference_environment(member.environment);
    const auto product_class = reference_product_class(member.product_class);
    if (!environment.has_value() || !product_class.has_value()) {
      return {.failure = TradeNormalizationFailure::SchemaViolation};
    }
    const auto selected = reference_lineage.select(
        member.venue, *environment, *product_class, member.source_symbol,
        lineage.capture_partition_id, lineage.capture_sequence);
    if (selected.failure ==
        core::reference_data::ReferenceSelectionFailure::Ambiguous) {
      return {.failure = TradeNormalizationFailure::ReferenceAmbiguous};
    }
    if (!selected.ok()) {
      return {.failure = TradeNormalizationFailure::ReferenceUnavailable};
    }
    const auto &reference = *selected.snapshot;
    const auto *listing = selected.listing;

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
        .reference_selection =
            {
                .reference_configuration_lineage_version =
                    reference_lineage.version(),
                .lineage_schema_version =
                    std::string(reference_lineage.lineage_schema_version()),
                .semantic_key_policy_version = std::string(
                    reference_lineage.semantic_key_policy_version()),
                .effective_basis_policy_version = std::string(
                    reference_lineage.effective_basis_policy_version()),
                .selection_policy_version =
                    std::string(reference_lineage.selection_policy_version()),
                .semantic_key =
                    {
                        .venue = member.venue,
                        .environment = member.environment,
                        .product_class = member.product_class,
                        .source_listing_key = member.source_symbol,
                    },
                .effective_capture_partition_id = lineage.capture_partition_id,
                .effective_capture_sequence = lineage.capture_sequence,
            },
        .source_lineage = lineage,
        .source_decode_evidence = evidence,
        .source_message_extensions = message.envelope_extensions,
        .source_assertions = member,
        .source_event_time = *timestamp,
        .aggressor_side = *side,
        .price = *price,
        .quantity = *quantity,
        .decoder_version = evidence.decoder_version,
        .source_schema_version = evidence.source_schema_version,
        .normalizer_version = versions.normalizer_version,
    });
  }
  return result;
}

} // namespace chronos::normalization::market_data
