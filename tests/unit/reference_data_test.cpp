#include "chronos/core/reference_data/reference_data.hpp"

#include "microtest.hpp"

#include <type_traits>

using namespace chronos::contracts;
using namespace chronos::core::reference_data;

namespace {
template <typename Id> Id id(std::string_view value) {
  return Id::parse(value).value();
}

VersionRef version(std::string_view value, std::uint64_t number) {
  return VersionRef::from(id<DefinitionId>(value), number).value();
}

constexpr auto kInstrumentId = "018f1f6e-7d3a-7c4b-8a91-0123456789ab";
constexpr auto kListingId = "018f1f6e-7d3a-7c4b-8a91-0123456789ac";
constexpr auto kInstrumentVersion = "018f1f6e-7d3a-7c4b-8a91-0123456789ad";
constexpr auto kListingVersion = "018f1f6e-7d3a-7c4b-8a91-0123456789ae";
constexpr auto kSnapshotVersion = "018f1f6e-7d3a-7c4b-8a91-0123456789af";

ReferenceSnapshot snapshot(ListingStatus status = ListingStatus::Active) {
  const auto interval =
      EffectiveInterval::from_capture_sequence(10, 20).value();
  return ReferenceSnapshot::create(
             version(kSnapshotVersion, 1),
             CanonicalInstrumentDefinition{
                 .instrument_id = id<CanonicalInstrumentId>(kInstrumentId),
                 .version = version(kInstrumentVersion, 1),
                 .base_asset = "BTC",
                 .quote_asset = "USDT",
                 .product_class = ProductClass::Spot,
                 .effective_interval = interval,
             },
             ListingDefinition{
                 .listing_id = id<ListingId>(kListingId),
                 .instrument_id = id<CanonicalInstrumentId>(kInstrumentId),
                 .version = version(kListingVersion, 7),
                 .venue = "bybit",
                 .source_symbol = "BTCUSDT",
                 .status = status,
                 .price_tick = DecimalIncrement::parse("0.10").value(),
                 .quantity_step = DecimalIncrement::parse("0.001").value(),
                 .effective_interval = interval,
             })
      .value();
}
} // namespace

TEST_CASE("one canonical instrument maps to one distinct listing definition") {
  static_assert(!std::is_same_v<CanonicalInstrumentId, ListingId>);
  const auto reference = snapshot();
  CHECK(reference.instrument().instrument_id ==
        reference.listing().instrument_id);
  CHECK(reference.listing().listing_id.to_string() == kListingId);
  CHECK(reference.listing().version.version() == 7);
  CHECK(reference.resolve("bybit", "BTCUSDT", 10) != nullptr);
  CHECK(reference.resolve("bybit", "BTCUSDT", 19) != nullptr);
  CHECK(reference.resolve("bybit", "BTCUSDT", 9) == nullptr);
  CHECK(reference.resolve("bybit", "BTCUSDT", 20) == nullptr);
  CHECK(reference.resolve("other", "BTCUSDT", 10) == nullptr);
  CHECK(snapshot(ListingStatus::Inactive).resolve("bybit", "BTCUSDT", 10) ==
        nullptr);
}

TEST_CASE("tick and step definitions drive exact fixed-point conversion") {
  const auto listing = snapshot().listing();
  const auto price = listing.parse_price("42000.10");
  const auto quantity = listing.parse_quantity("1.234");
  CHECK(price->units() == 420001);
  CHECK(quantity->units() == 1234);
  CHECK(price->definition_ref() == listing.version.version());
  CHECK(quantity->definition_ref() == listing.version.version());
  CHECK(listing.price_tick.scale().exponent() == 1);
  CHECK(listing.quantity_step.scale().exponent() == 3);
  CHECK(!listing.parse_price("42000.15").has_value());
  CHECK(!listing.parse_price("42000.101").has_value());
  CHECK(!listing.parse_quantity("1.2345").has_value());
  CHECK(!listing.parse_quantity("-1.000").has_value());
}

TEST_CASE("reference factories reject ambiguous or invalid definitions") {
  CHECK(!EffectiveInterval::from_capture_sequence(0, std::nullopt).has_value());
  CHECK(!EffectiveInterval::from_capture_sequence(10, 10).has_value());
  CHECK(!DecimalIncrement::parse("0").has_value());
  CHECK(!DecimalIncrement::parse("-0.1").has_value());
  CHECK(!DecimalIncrement::parse("1e-3").has_value());

  const auto valid = snapshot();
  auto wrong_listing = valid.listing();
  wrong_listing.instrument_id =
      id<CanonicalInstrumentId>("018f1f6e-7d3a-7c4b-8a91-0123456789aa");
  CHECK(!ReferenceSnapshot::create(valid.version(), valid.instrument(),
                                   std::move(wrong_listing))
             .has_value());

  auto invalid_status = valid.listing();
  invalid_status.status = static_cast<ListingStatus>(255);
  CHECK(!ReferenceSnapshot::create(valid.version(), valid.instrument(),
                                   std::move(invalid_status))
             .has_value());

  auto narrow_instrument = valid.instrument();
  narrow_instrument.effective_interval =
      EffectiveInterval::from_capture_sequence(11, 19).value();
  CHECK(!ReferenceSnapshot::create(
             valid.version(), std::move(narrow_instrument), valid.listing())
             .has_value());
}
