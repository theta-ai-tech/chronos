#include "chronos/normalization/market_data/book_normalizer.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

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

BookNormalizationFailure
convert_snapshot_side(const std::vector<SourceBookLevel> &source,
                      const core::reference_data::ListingDefinition &listing,
                      bool descending, std::vector<BookLevel> &output) {
  output.reserve(source.size());
  for (const auto &level : source) {
    const auto price = listing.parse_price(level.price_decimal);
    const auto quantity = listing.parse_quantity(level.quantity_decimal);
    if (!price.has_value() || !quantity.has_value() || price->units() <= 0 ||
        quantity->units() <= 0) {
      return BookNormalizationFailure::InvalidNumeric;
    }
    output.push_back({.price = *price, .quantity = *quantity});
  }
  std::sort(output.begin(), output.end(),
            [descending](const auto &left, const auto &right) {
              return descending ? left.price.units() > right.price.units()
                                : left.price.units() < right.price.units();
            });
  for (std::size_t index = 1; index < output.size(); ++index) {
    if (output[index - 1].price.units() == output[index].price.units()) {
      return BookNormalizationFailure::AmbiguousDuplicate;
    }
  }
  return BookNormalizationFailure::None;
}

BookNormalizationFailure
convert_delta_side(const std::vector<SourceBookLevel> &source,
                   const core::reference_data::ListingDefinition &listing,
                   bool descending, std::vector<BookLevelChange> &output) {
  output.reserve(source.size());
  for (const auto &level : source) {
    const auto price = listing.parse_price(level.price_decimal);
    const auto quantity = listing.parse_quantity(level.quantity_decimal);
    if (!price.has_value() || !quantity.has_value() || price->units() <= 0 ||
        quantity->units() < 0) {
      return BookNormalizationFailure::InvalidNumeric;
    }
    output.push_back({
        .price = *price,
        .quantity = *quantity,
        .operation = quantity->units() == 0 ? BookLevelOperation::Delete
                                            : BookLevelOperation::SetAbsolute,
    });
  }
  std::sort(output.begin(), output.end(),
            [descending](const auto &left, const auto &right) {
              return descending ? left.price.units() > right.price.units()
                                : left.price.units() < right.price.units();
            });
  for (std::size_t index = 1; index < output.size(); ++index) {
    if (output[index - 1].price.units() == output[index].price.units()) {
      return BookNormalizationFailure::AmbiguousDuplicate;
    }
  }
  return BookNormalizationFailure::None;
}

} // namespace

BookNormalizationResult
normalize_book(const DecodedBookEnrichment &enrichment,
               const core::reference_data::ReferenceConfigurationLineage
                   &reference_lineage,
               contracts::ClockDomainId source_wall_clock_domain_id,
               const BookNormalizerVersions &versions) {
  const auto &message = enrichment.message();
  const auto &lineage = enrichment.source_lineage();
  const auto &decode_evidence = enrichment.decode_evidence();
  if (!valid_version(decode_evidence.decoder_version) ||
      !valid_version(decode_evidence.source_schema_version) ||
      !valid_version(decode_evidence.registry_version) ||
      !valid_version(decode_evidence.canonicalization_version) ||
      !valid_version(versions.normalizer_version)) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }

  const auto environment =
      reference_environment(message.assertions.environment);
  const auto product_class =
      reference_product_class(message.assertions.product_class);
  if (!environment.has_value() || !product_class.has_value()) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }

  const auto selected = reference_lineage.select(
      message.assertions.venue, *environment, *product_class,
      message.assertions.source_symbol, lineage.capture_partition_id,
      lineage.capture_sequence);
  if (selected.failure ==
      core::reference_data::ReferenceSelectionFailure::Ambiguous) {
    return {.failure = BookNormalizationFailure::ReferenceAmbiguous};
  }
  if (!selected.ok()) {
    return {.failure = BookNormalizationFailure::ReferenceUnavailable};
  }
  const auto &reference = *selected.snapshot;
  const auto *listing = selected.listing;
  const auto timestamp =
      source_time(message.assertions.matching_timestamp_milliseconds,
                  source_wall_clock_domain_id);
  if (!timestamp.has_value()) {
    return {.failure = BookNormalizationFailure::InvalidNumeric};
  }

  BookObservationPayload payload;
  if (message.assertions.kind == SourceBookKind::Snapshot) {
    BookSnapshotObservation snapshot;
    auto failure =
        convert_snapshot_side(message.bids, *listing, true, snapshot.bids);
    if (failure == BookNormalizationFailure::None) {
      failure =
          convert_snapshot_side(message.asks, *listing, false, snapshot.asks);
    }
    if (failure != BookNormalizationFailure::None) {
      return {.failure = failure};
    }
    payload = std::move(snapshot);
  } else if (message.assertions.kind == SourceBookKind::Delta) {
    BookDeltaObservation delta;
    auto failure =
        convert_delta_side(message.bids, *listing, true, delta.bid_changes);
    if (failure == BookNormalizationFailure::None) {
      failure =
          convert_delta_side(message.asks, *listing, false, delta.ask_changes);
    }
    if (failure != BookNormalizationFailure::None) {
      return {.failure = failure};
    }
    payload = std::move(delta);
  } else {
    return {.failure = BookNormalizationFailure::UnsupportedMessage};
  }

  return {
      .fact =
          NormalizedBookFact{
              .canonical_instrument_id = reference.instrument().instrument_id,
              .listing_id = listing->listing_id,
              .reference_snapshot_version = reference.version(),
              .instrument_version = reference.instrument().version,
              .listing_version = listing->version,
              .reference_selection =
                  {
                      .reference_configuration_lineage_version =
                          reference_lineage.version(),
                      .lineage_schema_version = std::string(
                          reference_lineage.lineage_schema_version()),
                      .semantic_key_policy_version = std::string(
                          reference_lineage.semantic_key_policy_version()),
                      .effective_basis_policy_version = std::string(
                          reference_lineage.effective_basis_policy_version()),
                      .selection_policy_version = std::string(
                          reference_lineage.selection_policy_version()),
                      .semantic_key =
                          {
                              .venue = message.assertions.venue,
                              .environment = message.assertions.environment,
                              .product_class = message.assertions.product_class,
                              .source_listing_key =
                                  message.assertions.source_symbol,
                          },
                      .effective_capture_partition_id =
                          lineage.capture_partition_id,
                      .effective_capture_sequence = lineage.capture_sequence,
                  },
              .source_lineage = lineage,
              .source_decode_evidence = decode_evidence,
              .source_assertions = message.assertions,
              .source_extensions = message.extensions,
              .source_event_time = *timestamp,
              .decoder_version = decode_evidence.decoder_version,
              .source_schema_version = decode_evidence.source_schema_version,
              .normalizer_version = versions.normalizer_version,
              .payload = std::move(payload),
          },
      .failure = BookNormalizationFailure::None,
  };
}

} // namespace chronos::normalization::market_data
