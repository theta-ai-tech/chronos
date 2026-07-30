#include "chronos/contracts/value_objects.hpp"

#include "chronos/contracts/digest.hpp"

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
  CHECK(!EventId::parse("-18f1f6e-7d3a-7c4b-8a91-0123456789ab").has_value());
  CHECK(!EventId::parse("018f1f6e-7d3a-7c4b-8a91-0123456789a-").has_value());
  CHECK((!std::is_same_v<EventId, ListingId>));
  CHECK((!std::is_same_v<CanonicalInstrumentId, ListingId>));
}

TEST_CASE("opaque IDs derive totally from a SHA-256 digest prefix") {
  Sha256Digest normal{};
  normal.bytes = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                  0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  const auto normal_id = TradeRecommendationId::from_sha256_digest(normal);
  CHECK(normal_id ==
        TradeRecommendationId::parse("01020304-0506-0708-090a-0b0c0d0e0f10")
            .value());

  Sha256Digest zero_prefix{};
  zero_prefix.bytes.back() = 0x7F;
  const auto zero_prefix_id = TargetPositionId::from_sha256_digest(zero_prefix);
  CHECK(
      zero_prefix_id ==
      TargetPositionId::parse("00000000-0000-0000-0000-000000000001").value());

  const Sha256Digest full_zero{};
  CHECK(PortfolioConstructionOutcomeId::from_sha256_digest(full_zero) ==
        PortfolioConstructionOutcomeId::parse(
            "00000000-0000-0000-0000-000000000001")
            .value());
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
  CHECK(VersionRef::from(definition_id, 3)->version() == 3);
  CHECK(!VersionRef::from(definition_id, 0).has_value());
}

TEST_CASE("time points compare only within one named clock domain") {
  const auto domain = ClockDomainId::parse(kIdentity).value();
  const auto other_domain = ClockDomainId::parse(kOtherIdentity).value();
  const auto earlier =
      TimePoint::from(10, domain, ClockClass::monotonic, 1).value();
  const auto later =
      TimePoint::from(20, domain, ClockClass::monotonic, 1).value();
  const auto restarted =
      TimePoint::from(20, other_domain, ClockClass::monotonic, 1).value();
  const auto contradictory =
      TimePoint::from(20, domain, ClockClass::replay_logical, 1).value();
  CHECK(earlier.checked_compare(later) == std::strong_ordering::less);
  CHECK(!earlier.checked_compare(restarted).has_value());
  CHECK(!earlier.checked_compare(contradictory).has_value());
  CHECK(!TimePoint::from(10, domain, ClockClass::chronos_wall, 0).has_value());
  CHECK(!TimePoint::from(10, domain, static_cast<ClockClass>(255), 1)
             .has_value());
}

TEST_CASE("data quality requires explicit reasons for non-valid states") {
  CHECK(DataQuality::from(QualityStatus::valid, 0).has_value());
  CHECK(DataQuality::from(QualityStatus::stale, 7).has_value());
  CHECK(!DataQuality::from(QualityStatus::valid, 7).has_value());
  CHECK(!DataQuality::from(QualityStatus::gapped, 0).has_value());
  CHECK(!DataQuality::from(static_cast<QualityStatus>(255), 7).has_value());
}
