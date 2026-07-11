#include "chronos/contracts/value_objects.hpp"

#include "microtest.hpp"

#include <compare>
#include <type_traits>

using namespace chronos::contracts;

namespace {
constexpr auto kIdentity = "018f1f6e-7d3a-7c4b-8a91-0123456789ab";
constexpr auto kOtherIdentity = "018f1f6e-7d3a-7c4b-8a91-0123456789ac";
} // namespace

TEST_CASE("opaque IDs parse canonical UUIDs without business semantics") {
  const auto event_id = EventId::parse(kIdentity);
  const auto listing_id = ListingId::parse(kIdentity);
  CHECK(event_id.has_value());
  CHECK(listing_id.has_value());
  CHECK(event_id->to_string() == kIdentity);
  CHECK(EventId::parse(kOtherIdentity) != event_id);
  CHECK(!EventId::parse("BTCUSDT").has_value());
  CHECK(!EventId::parse("00000000-0000-0000-0000-000000000000").has_value());
  CHECK((!std::is_same_v<EventId, ListingId>));
  CHECK((!std::is_same_v<CanonicalInstrumentId, ListingId>));
}

TEST_CASE("stream cursors distinguish origin from consumed sequence zero") {
  const auto stream_id = StreamId::parse(kIdentity).value();
  const auto origin = StreamCursor::at_origin(stream_id, 1);
  const auto first = StreamCursor::at_sequence(stream_id, 1, 0);
  CHECK(origin->is_origin());
  CHECK(!first->is_origin());
  CHECK(origin != first);
  CHECK(!StreamCursor::at_origin(stream_id, 0).has_value());
}

TEST_CASE("version references require an immutable positive version") {
  const auto definition_id = DefinitionId::parse(kIdentity).value();
  CHECK(VersionRef::from(definition_id, 3)->version == 3);
  CHECK(!VersionRef::from(definition_id, 0).has_value());
}

TEST_CASE("time points compare only within one named clock domain") {
  const auto earlier =
      TimePoint::from(10, ClockDomain::chronos_wall, 1).value();
  const auto later = TimePoint::from(20, ClockDomain::chronos_wall, 1).value();
  const auto replay =
      TimePoint::from(20, ClockDomain::replay_logical, 1).value();
  CHECK(earlier.checked_compare(later) == std::strong_ordering::less);
  CHECK(!earlier.checked_compare(replay).has_value());
  CHECK(!TimePoint::from(10, ClockDomain::chronos_wall, 0).has_value());
}

TEST_CASE("data quality requires explicit reasons for non-valid states") {
  CHECK(DataQuality::from(QualityStatus::valid, 0).has_value());
  CHECK(DataQuality::from(QualityStatus::stale, 7).has_value());
  CHECK(!DataQuality::from(QualityStatus::valid, 7).has_value());
  CHECK(!DataQuality::from(QualityStatus::gapped, 0).has_value());
}
