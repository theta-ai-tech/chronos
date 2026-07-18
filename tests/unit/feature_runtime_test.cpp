#include "chronos/core/features/feature_runtime.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {
namespace contracts = chronos::contracts;
namespace features = chronos::core::features;
namespace market = chronos::core::market_state;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number = 1) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
}

contracts::Sha256Digest digest(std::uint8_t seed) {
  contracts::Sha256Digest result;
  result.bytes.front() = seed;
  return result;
}

contracts::StreamCursor origin(std::uint8_t seed) {
  return contracts::StreamCursor::at_origin(id<contracts::StreamId>(seed), 1)
      .value();
}

contracts::StreamCursor cursor(std::uint8_t seed, std::uint64_t sequence) {
  return contracts::StreamCursor::at_sequence(id<contracts::StreamId>(seed), 1,
                                              sequence)
      .value();
}

std::vector<contracts::StreamId> streams() {
  return {id<contracts::StreamId>(10), id<contracts::StreamId>(11),
          id<contracts::StreamId>(12), id<contracts::StreamId>(13),
          id<contracts::StreamId>(14), id<contracts::StreamId>(15),
          id<contracts::StreamId>(16)};
}

contracts::StateLineage lineage(std::uint64_t run_sequence = 5) {
  const auto required = streams();
  const std::array cursors = {origin(10), origin(11), cursor(12, 4), origin(13),
                              origin(14), origin(15), origin(16)};
  return contracts::StateLineage::from(id<contracts::RunId>(1), run_sequence,
                                       required, cursors)
      .value();
}

market::ListingQualityState quality() {
  return {
      .book_synchronization = market::BookSynchronization::Synchronized,
      .trade_continuity = market::TradeContinuity::Unavailable,
      .book_freshness = market::FreshnessStatus::Fresh,
      .trade_freshness = market::FreshnessStatus::Unknown,
      .trade_window_status = market::TradeWindowStatus::Unavailable,
      .book_age_nanoseconds = 1,
      .trade_age_nanoseconds = std::nullopt,
      .logical_time_nanoseconds = 1005,
      .run_input_sequence = 5,
      .freshness_policy_version = version(20),
      .trade_window_policy_version = version(21),
      .trade_window_policy = market::TradeWindowPolicy::AcceptedCount,
      .book_cursor = cursor(12, 4),
      .trade_cursor = origin(10),
      .trade_continuity_cursor = origin(11),
      .last_book_proof = std::nullopt,
      .last_trade_boundary = std::nullopt,
      .last_applied_event_id = id<contracts::EventId>(22),
      .last_applied_input_semantic_checksum = digest(23),
      .last_quality_input_kind =
          market::ListingQualityInputKind::BookEvidenceObserved,
      .last_applied_input_was_trade = false,
  };
}

market::ListingStateView listing_view() {
  const auto bid = market::L2Level{
      .price = contracts::Price::from_units(100, version(2)).value(),
      .quantity = contracts::Quantity::from_units(30, version(3)).value(),
  };
  const auto ask = market::L2Level{
      .price = contracts::Price::from_units(102, version(2)).value(),
      .quantity = contracts::Quantity::from_units(10, version(3)).value(),
  };
  return {
      .view_id = id<contracts::StateViewId>(4),
      .run_id = id<contracts::RunId>(1),
      .listing_id = id<contracts::ListingId>(5),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(6),
      .reference_snapshot_version = version(7),
      .listing_definition_version = version(8),
      .reference_configuration_lineage_version = version(9),
      .causing_selection_id = id<contracts::RunInputSelectionId>(24),
      .causing_event_id = id<contracts::EventId>(22),
      .causing_event_type = "market.book.observation.delta",
      .causing_event_position =
          contracts::EventPosition::from(id<contracts::StreamId>(12), 1, 4)
              .value(),
      .input_semantic_checksum = digest(23),
      .selection_semantic_checksum = digest(25),
      .merge_policy_version = version(26),
      .configuration_epoch = 2,
      .effective_control_position = 3,
      .lineage = lineage(),
      .l2_transition_sequence = 2,
      .bids = {bid},
      .asks = {ask},
      .top =
          {
              .best_bid = bid,
              .best_ask = ask,
              .spread = contracts::Price::from_units(2, version(2)).value(),
              .bid_completeness = market::L2SideCompleteness::Complete,
              .ask_completeness = market::L2SideCompleteness::Complete,
              .shape = market::L2BookShape::Normal,
          },
      .recent_trades = {},
      .quality = quality(),
      .last_book_input = std::nullopt,
      .prior_view_id = id<contracts::StateViewId>(27),
      .view_schema_version = version(28),
      .capability_version = version(29),
      .transition_policy_version = version(30),
      .arithmetic_version = version(31),
      .canonicalization_version = version(32),
      .semantic_checksum = digest(33),
  };
}

market::StateViewBundle bundle() {
  return {
      .bundle_id = id<contracts::StateViewId>(34),
      .run_id = id<contracts::RunId>(1),
      .run_input_sequence = 5,
      .causing_selection_id = id<contracts::RunInputSelectionId>(24),
      .causing_event_id = id<contracts::EventId>(22),
      .listing_views = {{id<contracts::ListingId>(5),
                         id<contracts::StateViewId>(4)}},
      .listing_id = id<contracts::ListingId>(5),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(6),
      .listing_view_id = id<contracts::StateViewId>(4),
      .run_control_cursor = origin(15),
      .run_timer_cursor = origin(16),
      .reference_cursor = origin(13),
      .logical_time_nanoseconds = 1005,
      .selection_semantic_checksum = digest(25),
      .merge_policy_version = version(26),
      .configuration_epoch = 2,
      .effective_control_position = 3,
      .prior_bundle_id = id<contracts::StateViewId>(35),
      .view_schema_version = version(28),
      .bundle_schema_version = version(36),
      .registry_snapshot_version = version(37),
      .reference_snapshot_version = version(7),
      .listing_definition_version = version(8),
      .reference_configuration_lineage_version = version(9),
      .arithmetic_version = version(31),
      .canonicalization_version = version(32),
      .identity_policy_version = version(38),
      .semantic_checksum = digest(39),
  };
}

features::FeatureRuntimeConfig runtime_config() {
  return {
      .run_id = id<contracts::RunId>(1),
      .listing_id = id<contracts::ListingId>(5),
      .imbalance_definition_version = version(40),
      .microprice_definition_version = version(41),
      .spread_definition_version = version(42),
      .implementation_version = version(43),
      .required_view_schema_version = version(28),
      .required_view_capability_version = version(29),
      .required_bundle_schema_version = version(36),
      .required_input_arithmetic_version = version(31),
      .required_input_canonicalization_version = version(32),
      .required_input_identity_policy_version = version(38),
      .feature_arithmetic_version = version(44),
      .canonicalization_version = version(45),
      .identity_policy_version = version(46),
  };
}

const features::FeatureEvaluation &
evaluation(const features::FeatureRuntimeResult &result,
           features::FeatureKind kind) {
  const auto found =
      std::find_if(result.evaluations.begin(), result.evaluations.end(),
                   [&](const auto &value) { return value.kind == kind; });
  if (found == result.evaluations.end())
    std::abort();
  return *found;
}

} // namespace

TEST_CASE("top features are exact deterministic and fully lineaged") {
  const features::FeatureRuntime runtime(runtime_config());
  const auto view = listing_view();
  const auto input_bundle = bundle();
  const auto first = runtime.evaluate(input_bundle, view);
  const auto second = runtime.evaluate(input_bundle, view);
  CHECK(first.ok());
  CHECK(first == second);
  CHECK(first.evaluations.size() == 3);

  const auto &imbalance =
      evaluation(first, features::FeatureKind::OrderBookImbalance);
  CHECK(imbalance.disposition ==
        features::FeatureDisposition::ValidObservation);
  CHECK(imbalance.observation.has_value());
  CHECK(!imbalance.unavailable.has_value());
  const auto ratio =
      std::get<features::ScaledRatio>(imbalance.observation->value);
  CHECK(ratio.units == 500000);
  CHECK(ratio.scale.exponent() == 6);

  const auto &microprice = evaluation(first, features::FeatureKind::Microprice);
  CHECK(std::get<contracts::Price>(microprice.observation->value).units() ==
        102);
  const auto &spread = evaluation(first, features::FeatureKind::Spread);
  CHECK(std::get<contracts::Price>(spread.observation->value).units() == 2);

  for (const auto &item : first.evaluations) {
    CHECK(item.observation.has_value());
    CHECK(item.observation->provenance.bundle_id == input_bundle.bundle_id);
    CHECK(item.observation->provenance.listing_view_id == view.view_id);
    CHECK(item.observation->provenance.lineage == view.lineage);
    CHECK(item.observation->provenance.canonical_instrument_id ==
          view.canonical_instrument_id);
    CHECK(item.observation->provenance.logical_time_nanoseconds ==
          input_bundle.logical_time_nanoseconds);
    CHECK(item.observation->provenance.causing_selection_id ==
          input_bundle.causing_selection_id);
    CHECK(item.observation->provenance.input_view_semantic_checksum ==
          view.semantic_checksum);
    CHECK(item.observation->provenance.input_bundle_semantic_checksum ==
          input_bundle.semantic_checksum);
  }
}

TEST_CASE("stale and gapped books produce typed unavailable outcomes") {
  const features::FeatureRuntime runtime(runtime_config());
  auto stale = listing_view();
  stale.quality.book_freshness = market::FreshnessStatus::Stale;
  const auto stale_result = runtime.evaluate(bundle(), stale);
  CHECK(stale_result.ok());
  CHECK(stale_result.evaluations.size() == 3);
  for (const auto &item : stale_result.evaluations) {
    CHECK(item.disposition == features::FeatureDisposition::Unavailable);
    CHECK(!item.observation.has_value());
    CHECK(item.unavailable.has_value());
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::BookStale);
  }

  auto gapped = listing_view();
  gapped.quality.book_synchronization = market::BookSynchronization::Gapped;
  const auto gapped_result = runtime.evaluate(bundle(), gapped);
  for (const auto &item : gapped_result.evaluations) {
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::BookGapped);
  }
  CHECK(stale_result != gapped_result);
}

TEST_CASE("feature-specific invalid quantity never becomes zero-filled") {
  const features::FeatureRuntime runtime(runtime_config());
  auto view = listing_view();
  const auto zero = contracts::Quantity::from_units(0, version(3)).value();
  view.bids[0].quantity = zero;
  view.top.best_bid->quantity = zero;
  const auto result = runtime.evaluate(bundle(), view);
  CHECK(result.ok());
  const auto &imbalance =
      evaluation(result, features::FeatureKind::OrderBookImbalance);
  const auto &microprice =
      evaluation(result, features::FeatureKind::Microprice);
  const auto &spread = evaluation(result, features::FeatureKind::Spread);
  CHECK(imbalance.unavailable->reason ==
        features::FeatureUnavailableReason::InvalidQuantity);
  CHECK(microprice.unavailable->reason ==
        features::FeatureUnavailableReason::InvalidQuantity);
  CHECK(!imbalance.observation.has_value());
  CHECK(!microprice.observation.has_value());
  CHECK(spread.disposition == features::FeatureDisposition::ValidObservation);
  CHECK(std::get<contracts::Price>(spread.observation->value).units() == 2);
}

TEST_CASE("crossed and unproven books are explicit non-valid inputs") {
  const features::FeatureRuntime runtime(runtime_config());
  auto crossed = listing_view();
  crossed.top.shape = market::L2BookShape::Crossed;
  const auto crossed_result = runtime.evaluate(bundle(), crossed);
  for (const auto &item : crossed_result.evaluations) {
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::UnsupportedBookShape);
  }

  auto unproven = listing_view();
  unproven.top.bid_completeness = market::L2SideCompleteness::BoundaryExhausted;
  const auto unproven_result = runtime.evaluate(bundle(), unproven);
  for (const auto &item : unproven_result.evaluations) {
    CHECK(item.unavailable->reason ==
          features::FeatureUnavailableReason::TopNotProven);
  }
}

TEST_CASE("locked books are valid and imbalance preserves its sign") {
  const features::FeatureRuntime runtime(runtime_config());
  auto locked = listing_view();
  locked.asks[0].price = contracts::Price::from_units(100, version(2)).value();
  locked.top.best_ask = locked.asks[0];
  locked.top.spread = contracts::Price::from_units(0, version(2)).value();
  locked.top.shape = market::L2BookShape::Locked;
  locked.bids[0].quantity =
      contracts::Quantity::from_units(10, version(3)).value();
  locked.top.best_bid = locked.bids[0];
  locked.asks[0].quantity =
      contracts::Quantity::from_units(30, version(3)).value();
  locked.top.best_ask = locked.asks[0];
  const auto result = runtime.evaluate(bundle(), locked);
  CHECK(result.ok());
  CHECK(std::get<features::ScaledRatio>(
            evaluation(result, features::FeatureKind::OrderBookImbalance)
                .observation->value)
            .units == -500000);
  CHECK(std::get<contracts::Price>(
            evaluation(result, features::FeatureKind::Microprice)
                .observation->value)
            .units() == 100);
  CHECK(
      std::get<contracts::Price>(
          evaluation(result, features::FeatureKind::Spread).observation->value)
          .units() == 0);
}

TEST_CASE("definition and arithmetic failures remain feature-specific") {
  const features::FeatureRuntime runtime(runtime_config());
  auto mismatched = listing_view();
  mismatched.asks[0].price =
      contracts::Price::from_units(102, version(70)).value();
  mismatched.top.best_ask = mismatched.asks[0];
  mismatched.top.spread = contracts::Price::from_units(2, version(70)).value();
  const auto mismatch = runtime.evaluate(bundle(), mismatched);
  CHECK(mismatch.failure == features::FeatureRuntimeFailure::CutMismatch);

  auto overflowing = listing_view();
  overflowing.bids[0].quantity =
      contracts::Quantity::from_units(std::numeric_limits<std::int64_t>::max(),
                                      version(3))
          .value();
  overflowing.top.best_bid = overflowing.bids[0];
  const auto overflow = runtime.evaluate(bundle(), overflowing);
  CHECK(overflow.ok());
  CHECK(evaluation(overflow, features::FeatureKind::OrderBookImbalance)
            .unavailable->reason ==
        features::FeatureUnavailableReason::ArithmeticOverflow);
  CHECK(evaluation(overflow, features::FeatureKind::Microprice)
            .unavailable->reason ==
        features::FeatureUnavailableReason::ArithmeticOverflow);
  CHECK(evaluation(overflow, features::FeatureKind::Spread)
            .observation.has_value());
}

TEST_CASE("bundle member schema and exact cut mismatches fail admission") {
  const features::FeatureRuntime runtime(runtime_config());
  auto wrong_member = bundle();
  wrong_member.listing_view_id = id<contracts::StateViewId>(60);
  CHECK(runtime.evaluate(wrong_member, listing_view()).failure ==
        features::FeatureRuntimeFailure::BundleMembershipMismatch);

  auto wrong_schema = listing_view();
  wrong_schema.view_schema_version = version(61);
  CHECK(runtime.evaluate(bundle(), wrong_schema).failure ==
        features::FeatureRuntimeFailure::IncompatibleSchema);

  auto wrong_cut = listing_view();
  wrong_cut.quality.logical_time_nanoseconds += 1;
  CHECK(runtime.evaluate(bundle(), wrong_cut).failure ==
        features::FeatureRuntimeFailure::CutMismatch);

  auto wrong_identity_policy = bundle();
  wrong_identity_policy.identity_policy_version = version(62);
  CHECK(runtime.evaluate(wrong_identity_policy, listing_view()).failure ==
        features::FeatureRuntimeFailure::IncompatibleIdentityPolicy);

  auto wrong_instrument = bundle();
  wrong_instrument.canonical_instrument_id =
      id<contracts::CanonicalInstrumentId>(63);
  CHECK(runtime.evaluate(wrong_instrument, listing_view()).failure ==
        features::FeatureRuntimeFailure::IncompatibleReference);
}

TEST_CASE("future views cannot alter a prior cut and versions alter identity") {
  const auto config = runtime_config();
  const features::FeatureRuntime runtime(config);
  const auto old_view = listing_view();
  const auto old_bundle = bundle();
  const auto before = runtime.evaluate(old_bundle, old_view);

  auto future_view = listing_view();
  future_view.view_id = id<contracts::StateViewId>(64);
  future_view.semantic_checksum = digest(65);
  future_view.bids[0].quantity =
      contracts::Quantity::from_units(10, version(3)).value();
  future_view.top.best_bid = future_view.bids[0];
  auto future_bundle = bundle();
  future_bundle.bundle_id = id<contracts::StateViewId>(66);
  future_bundle.listing_view_id = future_view.view_id;
  future_bundle.listing_views = {{future_view.listing_id, future_view.view_id}};
  future_bundle.semantic_checksum = digest(67);
  CHECK(runtime.evaluate(future_bundle, future_view).ok());
  CHECK(runtime.evaluate(old_bundle, old_view) == before);

  auto changed_config = config;
  changed_config.imbalance_definition_version = version(40, 2);
  const auto changed =
      features::FeatureRuntime(changed_config).evaluate(old_bundle, old_view);
  CHECK(evaluation(changed, features::FeatureKind::OrderBookImbalance)
            .evaluation_id !=
        evaluation(before, features::FeatureKind::OrderBookImbalance)
            .evaluation_id);
  CHECK(evaluation(changed, features::FeatureKind::Microprice) ==
        evaluation(before, features::FeatureKind::Microprice));
  CHECK(evaluation(changed, features::FeatureKind::Spread) ==
        evaluation(before, features::FeatureKind::Spread));
}
