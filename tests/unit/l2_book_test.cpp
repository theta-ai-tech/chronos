#include "chronos/core/market_state/l2_book.hpp"

#include "microtest.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace {
namespace contracts = chronos::contracts;
namespace market = chronos::core::market_state;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), 1)
      .value();
}

contracts::Price price(contracts::AmountUnits units,
                       contracts::VersionRef definition = version(2)) {
  return contracts::Price::from_units(units, definition).value();
}

contracts::Quantity quantity(contracts::AmountUnits units,
                             contracts::VersionRef definition = version(3)) {
  return contracts::Quantity::from_units(units, definition).value();
}

market::L2Level level(contracts::AmountUnits price_units,
                      contracts::AmountUnits quantity_units) {
  return {.price = price(price_units), .quantity = quantity(quantity_units)};
}

market::L2Change set(contracts::AmountUnits price_units,
                     contracts::AmountUnits quantity_units) {
  return {.price = price(price_units), .quantity = quantity(quantity_units)};
}

market::L2Change remove(contracts::AmountUnits price_units) {
  return {.price = price(price_units),
          .quantity = quantity(0),
          .operation = market::L2Operation::Delete};
}

market::L2BookConfig config(std::size_t maximum_levels = 8,
                            std::size_t maximum_changes = 8) {
  return {
      .listing_id = id<contracts::ListingId>(1),
      .price_definition = version(2),
      .quantity_definition = version(3),
      .maximum_levels_per_side = maximum_levels,
      .maximum_changes_per_delta = maximum_changes,
  };
}

std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>
units(std::span<const market::L2Level> levels) {
  std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>> result;
  result.reserve(levels.size());
  for (const auto &entry : levels)
    result.emplace_back(entry.price.units(), entry.quantity.units());
  return result;
}

} // namespace

TEST_CASE("snapshot atomically replaces and canonicalizes listing depth") {
  auto book = market::L2Book::create(config()).value();
  const market::L2Snapshot snapshot{
      .listing_id = id<contracts::ListingId>(1),
      .bids = {level(100, 5), level(98, 2), level(99, 0)},
      .asks = {level(103, 4), level(101, 3), level(102, 1)},
  };

  const auto result = book.apply_snapshot(snapshot);
  CHECK(result.ok());
  CHECK(result.content_changed);
  CHECK(result.transition_sequence == 1);
  CHECK(units(book.bids()) ==
        (std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>{
            {98, 2}, {100, 5}}));
  CHECK(units(book.asks()) ==
        (std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>{
            {101, 3}, {102, 1}, {103, 4}}));
}

TEST_CASE("absolute delta updates add and zero or delete removes") {
  auto book = market::L2Book::create(config()).value();
  CHECK(book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                             .bids = {level(98, 2), level(100, 5)},
                             .asks = {level(101, 3), level(103, 4)}})
            .ok());
  const market::L2Delta delta{
      .listing_id = id<contracts::ListingId>(1),
      .bid_changes = {set(100, 7), set(99, 6), remove(98)},
      .ask_changes = {set(101, 0), set(102, 8)},
  };

  const auto result = book.apply_delta(delta);
  CHECK(result.ok());
  CHECK(result.transition_sequence == 2);
  CHECK(units(book.bids()) ==
        (std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>{
            {99, 6}, {100, 7}}));
  CHECK(units(book.asks()) ==
        (std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>{
            {102, 8}, {103, 4}}));
}

TEST_CASE("invalid multi-member delta leaves the complete book untouched") {
  auto book = market::L2Book::create(config()).value();
  CHECK(book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                             .bids = {level(100, 5)},
                             .asks = {level(101, 3)}})
            .ok());
  const auto before_bids = units(book.bids());
  const auto before_asks = units(book.asks());
  market::L2Delta invalid{
      .listing_id = id<contracts::ListingId>(1),
      .bid_changes = {set(100, 7)},
      .ask_changes = {set(102, -1)},
  };

  const auto result = book.apply_delta(invalid);
  CHECK(result.failure == market::L2TransitionFailure::InvalidQuantity);
  CHECK(units(book.bids()) == before_bids);
  CHECK(units(book.asks()) == before_asks);
  CHECK(book.transition_sequence() == 1);
}

TEST_CASE("duplicate prices and changes fail without partial replacement") {
  auto book = market::L2Book::create(config()).value();
  CHECK(book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                             .bids = {level(100, 5)},
                             .asks = {level(101, 3)}})
            .ok());
  const auto duplicate_snapshot =
      book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                           .bids = {level(99, 1), level(99, 2)},
                           .asks = {level(102, 1)}});
  CHECK(duplicate_snapshot.failure ==
        market::L2TransitionFailure::DuplicateLevel);
  CHECK(units(book.bids()) ==
        (std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>{
            {100, 5}}));

  const auto duplicate_delta =
      book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                        .bid_changes = {set(100, 7), set(100, 8)}});
  CHECK(duplicate_delta.failure ==
        market::L2TransitionFailure::DuplicateChange);
  CHECK(units(book.bids()) ==
        (std::vector<std::pair<contracts::AmountUnits, contracts::AmountUnits>>{
            {100, 5}}));
  CHECK(book.transition_sequence() == 1);
}

TEST_CASE("definition listing and capacity violations fail closed") {
  auto book = market::L2Book::create(config(2, 2)).value();
  CHECK(book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                             .bids = {level(99, 1), level(100, 2)}})
            .ok());
  const auto original = units(book.bids());

  CHECK(book.apply_delta({.listing_id = id<contracts::ListingId>(9),
                          .bid_changes = {set(101, 1)}})
            .failure == market::L2TransitionFailure::WrongListing);
  CHECK(book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                          .bid_changes = {set(101, 1)}})
            .failure == market::L2TransitionFailure::ResourceLimitExceeded);
  auto wrong_definition = set(100, 3);
  wrong_definition.quantity = quantity(3, version(8));
  CHECK(book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                          .bid_changes = {wrong_definition}})
            .failure == market::L2TransitionFailure::DefinitionMismatch);
  auto unknown_operation = set(100, 3);
  unknown_operation.operation = static_cast<market::L2Operation>(99);
  CHECK(book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                          .bid_changes = {unknown_operation}})
            .failure == market::L2TransitionFailure::InvalidOperation);
  CHECK(units(book.bids()) == original);
  CHECK(book.transition_sequence() == 1);
}

TEST_CASE("delta operation limit applies across both sides") {
  auto book = market::L2Book::create(config(4, 2)).value();
  CHECK(book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                             .bids = {level(99, 1), level(100, 2)},
                             .asks = {level(101, 3)}})
            .ok());
  const auto before_bids = units(book.bids());
  const auto before_asks = units(book.asks());

  const auto result =
      book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                        .bid_changes = {set(99, 4), set(100, 5)},
                        .ask_changes = {set(101, 6)}});

  CHECK(result.failure == market::L2TransitionFailure::ResourceLimitExceeded);
  CHECK(units(book.bids()) == before_bids);
  CHECK(units(book.asks()) == before_asks);
  CHECK(book.transition_sequence() == 1);
}

TEST_CASE("transitions reuse fixed-capacity internal storage") {
  auto book = market::L2Book::create(config(4, 3)).value();
  const auto initial_storage = book.storage_profile();
  CHECK(initial_storage.bid_capacity >= 4);
  CHECK(initial_storage.ask_capacity >= 4);
  CHECK(initial_storage.scratch_bid_capacity >= 4);
  CHECK(initial_storage.scratch_ask_capacity >= 4);
  CHECK(initial_storage.validation_capacity >= 3);

  CHECK(book.apply_snapshot({.listing_id = id<contracts::ListingId>(1),
                             .bids = {level(99, 1), level(100, 2)},
                             .asks = {level(101, 3)}})
            .ok());
  CHECK(book.storage_profile() == initial_storage);
  CHECK(book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                          .bid_changes = {set(98, 4), remove(99)},
                          .ask_changes = {set(102, 5)}})
            .ok());
  CHECK(book.storage_profile() == initial_storage);
  CHECK(book.apply_delta({.listing_id = id<contracts::ListingId>(1),
                          .bid_changes = {set(100, 6), set(98, 7)},
                          .ask_changes = {set(101, 8), set(102, 9)}})
            .failure == market::L2TransitionFailure::ResourceLimitExceeded);
  CHECK(book.storage_profile() == initial_storage);
}
