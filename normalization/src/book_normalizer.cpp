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
normalize_book(const DecodedBookMessage &message,
               const SourceCaptureLineage &lineage,
               std::string_view source_venue,
               const core::reference_data::ReferenceSnapshot &reference,
               core::reference_data::EffectiveDomainId effective_domain_id,
               contracts::ClockDomainId source_wall_clock_domain_id,
               const BookNormalizerVersions &versions) {
  if (!valid_version(versions.decoder_version) ||
      !valid_version(versions.source_schema_version) ||
      !valid_version(versions.normalizer_version)) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }

  const auto &reference_listing = reference.listing();
  if (reference_listing.venue != source_venue ||
      reference_listing.source_symbol != message.assertions.source_symbol) {
    return {.failure = BookNormalizationFailure::WrongTopicOrSymbol};
  }
  const auto *listing =
      reference.resolve(source_venue, message.assertions.source_symbol,
                        effective_domain_id, lineage.capture_sequence);
  if (listing == nullptr) {
    return {.failure = BookNormalizationFailure::ReferenceUnavailable};
  }
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
              .source_lineage = lineage,
              .source_assertions = message.assertions,
              .source_event_time = *timestamp,
              .decoder_version = versions.decoder_version,
              .source_schema_version = versions.source_schema_version,
              .normalizer_version = versions.normalizer_version,
              .payload = std::move(payload),
          },
      .failure = BookNormalizationFailure::None,
  };
}

} // namespace chronos::normalization::market_data
