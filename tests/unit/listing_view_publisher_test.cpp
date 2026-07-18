#include "chronos/core/market_state/listing_view_publisher.hpp"

#include "microtest.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {
namespace contracts = chronos::contracts;
namespace dispatch = chronos::core::dispatch;
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

contracts::StreamCursor origin(std::uint8_t stream_seed,
                               std::uint64_t epoch = 1) {
  return contracts::StreamCursor::at_origin(
             id<contracts::StreamId>(stream_seed), epoch)
      .value();
}

contracts::StreamCursor cursor(std::uint8_t stream_seed, std::uint64_t sequence,
                               std::uint64_t epoch = 1) {
  return contracts::StreamCursor::at_sequence(
             id<contracts::StreamId>(stream_seed), epoch, sequence)
      .value();
}

market::L2Book make_book() {
  auto book =
      market::L2Book::create({
                                 .listing_id = id<contracts::ListingId>(1),
                                 .price_definition = version(2),
                                 .quantity_definition = version(3),
                                 .maximum_levels_per_side = 4,
                                 .maximum_changes_per_delta = 4,
                             })
          .value();
  const auto applied = book.apply_snapshot({
      .listing_id = id<contracts::ListingId>(1),
      .input_evidence =
          market::L2InputEvidence{
              .event_id = id<contracts::EventId>(23),
              .semantic_checksum =
                  contracts::sha256(std::vector<std::byte>{std::byte{1}}),
              .kind = market::L2InputKind::Snapshot,
          },
      .bids = {{.price = contracts::Price::from_units(100, version(2)).value(),
                .quantity =
                    contracts::Quantity::from_units(2, version(3)).value()}},
      .asks = {{.price = contracts::Price::from_units(101, version(2)).value(),
                .quantity =
                    contracts::Quantity::from_units(3, version(3)).value()}},
      .bid_completeness = market::L2SideCompleteness::Complete,
      .ask_completeness = market::L2SideCompleteness::Complete,
  });
  if (!applied.ok())
    std::abort();
  return book;
}

market::ListingAuxConfig aux_config() {
  return {
      .listing_id = id<contracts::ListingId>(1),
      .price_definition = version(2),
      .quantity_definition = version(3),
      .trade_stream_id = id<contracts::StreamId>(4),
      .trade_stream_epoch = 1,
      .trade_continuity_stream_id = id<contracts::StreamId>(7),
      .trade_continuity_stream_epoch = 1,
      .book_stream_id = id<contracts::StreamId>(8),
      .book_stream_epoch = 1,
      .initial_run_input_sequence = 0,
      .initial_logical_time_nanoseconds = 100,
      .book_freshness_deadline_nanoseconds = 10,
      .trade_freshness_deadline_nanoseconds = 10,
      .freshness_policy_version = version(20),
      .trade_window_policy_version = version(21),
      .accepted_source_clock_domain = id<contracts::ClockDomainId>(22),
      .accepted_source_clock_class = contracts::ClockClass::source_wall,
      .required_source_time_quality = market::SourceTimeQuality::Exact,
      .correction_policy = market::TradeCorrectionPolicy::Reject,
      .trade_window_policy = market::TradeWindowPolicy::AcceptedCount,
      .recent_trade_capacity = 2,
  };
}

market::ListingAuxState make_auxiliary(const market::L2Book &book) {
  auto auxiliary = market::ListingAuxState::create(aux_config()).value();
  market::ListingQualityInput book_sync{
      .event_id = id<contracts::EventId>(23),
      .input_semantic_checksum =
          contracts::sha256(std::vector<std::byte>{std::byte{1}}),
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::BookSynchronized,
      .run_input_sequence = 1,
      .logical_time_nanoseconds = 100,
      .book_proof =
          market::BookSynchronizationProof{
              .snapshot_event_id = id<contracts::EventId>(23),
              .snapshot_cursor = cursor(8, 0),
              .applied_through_cursor = cursor(8, 0),
              .l2_transition_sequence = book.transition_sequence(),
              .bridge_complete = true,
              .reference_compatible = true,
          },
  };
  if (!auxiliary.apply_quality_input(book_sync, &book).ok())
    std::abort();
  return auxiliary;
}

std::vector<contracts::StreamId> required_streams() {
  return {id<contracts::StreamId>(4),  id<contracts::StreamId>(7),
          id<contracts::StreamId>(8),  id<contracts::StreamId>(10),
          id<contracts::StreamId>(11), id<contracts::StreamId>(12),
          id<contracts::StreamId>(13)};
}

contracts::StateLineage
lineage(std::uint64_t run_sequence, contracts::StreamCursor book_cursor,
        contracts::StreamCursor trade_cursor = origin(4),
        contracts::StreamCursor continuity_cursor = origin(7),
        contracts::StreamCursor run_control_cursor = origin(12)) {
  const auto required = required_streams();
  const std::array cursors = {
      trade_cursor, continuity_cursor,  book_cursor, origin(10),
      origin(11),   run_control_cursor, origin(13),
  };
  return contracts::StateLineage::from(id<contracts::RunId>(30), run_sequence,
                                       required, cursors)
      .value();
}

contracts::Sha256Digest digest(std::uint8_t seed) {
  contracts::Sha256Digest result;
  result.bytes.front() = seed;
  return result;
}

market::ListingViewPublisherConfig
publisher_config(std::size_t maximum_transitions = 8,
                 std::size_t maximum_views = 8) {
  return {
      .run_id = id<contracts::RunId>(30),
      .listing_id = id<contracts::ListingId>(1),
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .reference_snapshot_version = version(43),
      .listing_definition_version = version(44),
      .reference_configuration_lineage_version = version(45),
      .required_streams = required_streams(),
      .initial_lineage = lineage(0, origin(8)),
      .book_stream_id = id<contracts::StreamId>(8),
      .trade_stream_id = id<contracts::StreamId>(4),
      .trade_continuity_stream_id = id<contracts::StreamId>(7),
      .reference_stream_id = id<contracts::StreamId>(10),
      .market_control_stream_id = id<contracts::StreamId>(11),
      .run_control_stream_id = id<contracts::StreamId>(12),
      .run_timer_stream_id = id<contracts::StreamId>(13),
      .feature_boundary_id = id<contracts::ConsumerBoundaryId>(31),
      .dispatcher_config =
          {
              .run_id = id<contracts::RunId>(30),
              .input_stream_id = id<contracts::StreamId>(14),
              .input_stream_epoch = 1,
              .control_stream_id = id<contracts::StreamId>(15),
              .control_stream_epoch = 1,
              .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(38),
              .merge_policy_version = version(37),
              .registry_snapshot_version = version(39),
              .initial_configuration_epoch = 1,
          },
      .merge_policy_version = version(37),
      .initial_configuration_epoch = 1,
      .view_schema_version = version(32),
      .capability_version = version(33),
      .transition_policy_version = version(34),
      .arithmetic_version = version(35),
      .canonicalization_version = version(36),
      .bundle_schema_version = version(40),
      .identity_policy_version = version(41),
      .maximum_publication_transitions = maximum_transitions,
      .maximum_retained_views = maximum_views,
  };
}

market::ListingViewCutInput cut_input(std::uint64_t run_sequence = 1) {
  const auto event_sequence = run_sequence - 1;
  const auto event_id = run_sequence == 1
                            ? id<contracts::EventId>(23)
                            : id<contracts::EventId>(
                                  static_cast<std::uint8_t>(40 + run_sequence));
  const auto event_type = run_sequence == 1 ? "market.book.observation.snapshot"
                                            : "market.book.observation.delta";
  std::vector<std::byte> payload = {
      static_cast<std::byte>(run_sequence & 0xffU)};
  dispatch::RunInputCandidate candidate{
      .event_id = event_id,
      .event_type = event_type,
      .event_position = contracts::EventPosition::from(
                            id<contracts::StreamId>(14), 1, run_sequence)
                            .value(),
      .semantic_payload = payload,
      .semantic_checksum = contracts::sha256(payload),
  };
  dispatch::RunInputSelectionRecord selection{
      .selection_id = id<contracts::RunInputSelectionId>(1),
      .run_id = id<contracts::RunId>(30),
      .run_input_sequence = run_sequence,
      .selected_event_id = event_id,
      .selected_event_type = event_type,
      .selected_event_position = candidate.event_position,
      .pre_selection_cursors = {run_sequence == 1
                                    ? origin(14)
                                    : cursor(14, run_sequence - 1)},
      .post_selection_cursors = {cursor(14, run_sequence)},
      .control_cursor = origin(15),
      .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(38),
      .active_configuration_epoch = 1,
      .merge_policy_version = version(37),
      .registry_snapshot_version = version(39),
      .input_semantic_checksum = candidate.semantic_checksum,
  };
  const auto config = publisher_config().dispatcher_config;
  selection.selection_semantic_checksum =
      dispatch::derive_run_input_selection_checksum(config, selection,
                                                    candidate);
  selection.selection_id = dispatch::derive_run_input_selection_id(
      selection.selection_semantic_checksum);
  return {
      .selection_id = selection.selection_id,
      .dispatch_selection = selection,
      .dispatch_candidate = candidate,
      .selected_event_id = event_id,
      .selected_event_type = event_type,
      .selected_event_position =
          contracts::EventPosition::from(id<contracts::StreamId>(8), 1,
                                         event_sequence)
              .value(),
      .input_semantic_checksum = candidate.semantic_checksum,
      .selection_semantic_checksum = selection.selection_semantic_checksum,
      .merge_policy_version = version(37),
      .configuration_epoch = 1,
      .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(42),
      .reference_snapshot_version = version(43),
      .listing_definition_version = version(44),
      .reference_configuration_lineage_version = version(45),
      .lineage = lineage(run_sequence, cursor(8, event_sequence)),
  };
}

market::ListingViewCutInput cut_input_with_control() {
  auto input = cut_input();
  std::vector<std::byte> behavior_payload = {std::byte{9}};
  input.dispatch_selection.applied_controls = {{
      .run_id = id<contracts::RunId>(30),
      .control_stream_id = id<contracts::StreamId>(15),
      .control_stream_epoch = 1,
      .control_outcome_id = id<contracts::EventId>(46),
      .control_sequence = 1,
      .effective_position = 1,
      .prior_configuration_epoch = 1,
      .new_configuration_epoch = 2,
      .behavior_payload = behavior_payload,
      .behavior_checksum = contracts::sha256(behavior_payload),
  }};
  input.dispatch_selection.control_cursor = cursor(15, 1);
  input.dispatch_selection.active_configuration_epoch = 2;
  input.dispatch_selection.selection_semantic_checksum =
      dispatch::derive_run_input_selection_checksum(
          publisher_config().dispatcher_config, input.dispatch_selection,
          input.dispatch_candidate);
  input.dispatch_selection.selection_id =
      dispatch::derive_run_input_selection_id(
          input.dispatch_selection.selection_semantic_checksum);
  input.selection_id = input.dispatch_selection.selection_id;
  input.selection_semantic_checksum =
      input.dispatch_selection.selection_semantic_checksum;
  input.configuration_epoch = 2;
  input.effective_control_position = 1;
  input.lineage = lineage(1, cursor(8, 0), origin(4), origin(7), cursor(12, 1));
  return input;
}

market::ViewPublicationTransition publication(contracts::StateViewId view_id,
                                              contracts::StateViewId bundle_id,
                                              std::uint8_t attempt_seed,
                                              std::uint64_t attempt_number,
                                              market::ViewPublicationState from,
                                              market::ViewPublicationState to) {
  return {
      .view_id = view_id,
      .bundle_id = bundle_id,
      .attempt_id = id<contracts::PublicationAttemptId>(attempt_seed),
      .boundary_id = id<contracts::ConsumerBoundaryId>(31),
      .attempt_number = attempt_number,
      .from = from,
      .to = to,
  };
}

void apply_book_delta(market::L2Book &book, market::ListingAuxState &auxiliary,
                      std::uint64_t run_sequence) {
  if (!book
           .apply_delta({
               .listing_id = id<contracts::ListingId>(1),
               .input_evidence =
                   market::L2InputEvidence{
                       .event_id = id<contracts::EventId>(
                           static_cast<std::uint8_t>(40 + run_sequence)),
                       .semantic_checksum =
                           contracts::sha256(std::vector<std::byte>{
                               static_cast<std::byte>(run_sequence & 0xffU)}),
                       .kind = market::L2InputKind::Delta,
                   },
               .bid_changes = {{
                   .price =
                       contracts::Price::from_units(100, version(2)).value(),
                   .quantity = contracts::Quantity::from_units(
                                   static_cast<contracts::AmountUnits>(
                                       run_sequence + 2),
                                   version(3))
                                   .value(),
               }},
           })
           .ok()) {
    std::abort();
  }
  auto observed = market::ListingQualityInput{
      .event_id =
          id<contracts::EventId>(static_cast<std::uint8_t>(40 + run_sequence)),
      .input_semantic_checksum = contracts::sha256(
          std::vector<std::byte>{static_cast<std::byte>(run_sequence & 0xffU)}),
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::BookEvidenceObserved,
      .run_input_sequence = run_sequence,
      .logical_time_nanoseconds = static_cast<std::int64_t>(99 + run_sequence),
      .book_proof = *auxiliary.quality().last_book_proof,
  };
  observed.book_proof->applied_through_cursor = cursor(8, run_sequence - 1);
  observed.book_proof->l2_transition_sequence = book.transition_sequence();
  if (!auxiliary.apply_quality_input(observed, &book).ok())
    std::abort();
}

void acknowledge(market::ListingViewPublisher &publisher,
                 contracts::StateViewId view_id,
                 contracts::StateViewId bundle_id) {
  if (publisher.transition_publication(publication(
          view_id, bundle_id, 60, 1, market::ViewPublicationState::NotPublished,
          market::ViewPublicationState::PublicationInProgress)) !=
          market::ListingViewFailure::None ||
      publisher.transition_publication(publication(
          view_id, bundle_id, 60, 1,
          market::ViewPublicationState::PublicationInProgress,
          market::ViewPublicationState::PublishedToFeatureBoundary)) !=
          market::ListingViewFailure::None ||
      publisher.transition_publication(
          publication(view_id, bundle_id, 60, 1,
                      market::ViewPublicationState::PublishedToFeatureBoundary,
                      market::ViewPublicationState::FeatureConsumerAccepted)) !=
          market::ListingViewFailure::None) {
    std::abort();
  }
}

} // namespace

TEST_CASE("accepted view is one immutable complete lineage cut") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();

  const auto result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(result.ok());
  if (!result.view || !result.bundle)
    return;
  CHECK(result.view == publisher.accepted_view());
  CHECK(result.bundle == publisher.accepted_bundle());
  CHECK(result.bundle->listing_view_id == result.view->view_id);
  CHECK(result.bundle->causing_selection_id ==
        result.view->causing_selection_id);
  CHECK(result.bundle->causing_event_id == result.view->causing_event_id);
  CHECK(result.bundle->listing_views.size() == 1);
  CHECK(result.view->canonical_instrument_id ==
        id<contracts::CanonicalInstrumentId>(42));
  CHECK(result.bundle->canonical_instrument_id ==
        id<contracts::CanonicalInstrumentId>(42));
  CHECK(result.bundle->reference_snapshot_version == version(43));
  CHECK(result.bundle->listing_definition_version == version(44));
  CHECK(result.bundle->reference_configuration_lineage_version == version(45));
  CHECK(result.bundle->run_control_cursor == origin(12));
  CHECK(result.bundle->run_timer_cursor == origin(13));
  CHECK(result.bundle->reference_cursor == origin(10));
  CHECK(result.bundle->logical_time_nanoseconds == 100);
  CHECK(result.bundle->bundle_schema_version == version(40));
  CHECK(result.bundle->registry_snapshot_version == version(39));
  CHECK(result.bundle->arithmetic_version == version(35));
  CHECK(result.bundle->identity_policy_version == version(41));
  CHECK(!publisher.published_view());
  CHECK(!publisher.published_bundle());
  CHECK(result.view->lineage.cursors().size() == required_streams().size());
  CHECK(result.view->lineage.run_input_sequence() == 1);
  CHECK(result.view->bids ==
        (std::vector<market::L2Level>{book.bids().front()}));
  CHECK(result.view->top.shape == market::L2BookShape::Normal);
  CHECK(result.view->quality == auxiliary.quality());
  CHECK(!result.view->prior_view_id);

  apply_book_delta(book, auxiliary, 2);
  CHECK(result.view->recent_trades.empty());
  CHECK(result.view->lineage.run_input_sequence() == 1);
  CHECK(result.view->bids.front().quantity.units() == 2);
}

TEST_CASE("same semantic cut derives the same checksum and identity") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto first = market::ListingViewPublisher::create(publisher_config()).value();
  auto second =
      market::ListingViewPublisher::create(publisher_config()).value();

  const auto first_result = first.accept_cut(cut_input(), book, auxiliary);
  const auto second_result = second.accept_cut(cut_input(), book, auxiliary);
  CHECK(first_result.ok());
  CHECK(second_result.ok());
  if (!first_result.view || !first_result.bundle || !second_result.view ||
      !second_result.bundle)
    return;
  const auto &first_view = first_result.view;
  const auto &second_view = second_result.view;
  CHECK(first_view->view_id == second_view->view_id);
  CHECK(first_view->semantic_checksum == second_view->semantic_checksum);
  CHECK(*first_view == *second_view);
  CHECK(*first_result.bundle == *second_result.bundle);
}

TEST_CASE("selection redelivery is exact and contradictory reuse fails") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto input = cut_input();
  const auto first = publisher.accept_cut(input, book, auxiliary);
  CHECK(first.ok());
  if (!first.view || !first.bundle)
    return;
  const auto duplicate = publisher.accept_cut(input, book, auxiliary);
  CHECK(duplicate.ok());
  CHECK(duplicate.view == first.view);
  CHECK(duplicate.bundle == first.bundle);
  auto contradictory = input;
  contradictory.input_semantic_checksum = digest(99);
  CHECK(publisher.accept_cut(contradictory, book, auxiliary).failure ==
        market::ListingViewFailure::ContradictorySelection);
  acknowledge(publisher, first.view->view_id, first.bundle->bundle_id);
  CHECK(publisher.accept_cut(input, book, auxiliary).view == first.view);
}

TEST_CASE("selection evidence is retained and fails closed") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  auto invalid = cut_input();
  invalid.input_semantic_checksum = {};
  CHECK(publisher.accept_cut(invalid, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);
  invalid = cut_input();
  invalid.merge_policy_version = version(98);
  CHECK(publisher.accept_cut(invalid, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);
  invalid = cut_input();
  invalid.dispatch_candidate.semantic_payload.push_back(std::byte{0x7f});
  CHECK(publisher.accept_cut(invalid, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);
  const auto accepted = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(accepted.ok());
  if (!accepted.view)
    return;
  CHECK(accepted.view->input_semantic_checksum ==
        cut_input().input_semantic_checksum);
  CHECK(accepted.view->selection_semantic_checksum ==
        cut_input().selection_semantic_checksum);
  CHECK(accepted.view->configuration_epoch == 1);
  CHECK(!accepted.view->effective_control_position);
}

TEST_CASE("selected event identity proves the applied state mutation") {
  auto book = make_book();
  auto auxiliary = market::ListingAuxState::create(aux_config()).value();
  const auto input = cut_input();
  const auto applied = auxiliary.apply_quality_input(
      {
          .event_id = input.selected_event_id,
          .input_semantic_checksum = input.input_semantic_checksum,
          .listing_id = id<contracts::ListingId>(1),
          .kind = market::ListingQualityInputKind::ListingClosed,
          .run_input_sequence = 1,
          .logical_time_nanoseconds = 100,
      },
      &book);
  CHECK(applied.ok());
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  CHECK(publisher.accept_cut(input, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);
  CHECK(!publisher.accepted_view());
}

TEST_CASE("applied controls advance the exact run-control lineage") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  auto stale = cut_input_with_control();
  stale.lineage = lineage(1, cursor(8, 0));
  CHECK(publisher.accept_cut(stale, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidLineageTransition);

  const auto accepted =
      publisher.accept_cut(cut_input_with_control(), book, auxiliary);
  CHECK(accepted.ok());
  if (!accepted.bundle)
    return;
  CHECK(accepted.bundle->configuration_epoch == 2);
  CHECK(accepted.bundle->effective_control_position == 1);
  CHECK(accepted.bundle->run_control_cursor == cursor(12, 1));
}

TEST_CASE("book gap quality facts advance immutable cuts") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(first.ok());
  if (!first.view || !first.bundle)
    return;
  acknowledge(publisher, first.view->view_id, first.bundle->bundle_id);

  const auto gap_event_id = id<contracts::EventId>(47);
  const auto gap_checksum =
      contracts::sha256(std::vector<std::byte>{std::byte{2}});
  CHECK(auxiliary
            .apply_quality_input({
                .event_id = gap_event_id,
                .input_semantic_checksum = gap_checksum,
                .listing_id = id<contracts::ListingId>(1),
                .kind = market::ListingQualityInputKind::BookGapDetected,
                .run_input_sequence = 2,
                .logical_time_nanoseconds = 101,
                .event_cursor = cursor(8, 1),
            })
            .ok());
  auto gap = cut_input(2);
  gap.dispatch_candidate.event_id = gap_event_id;
  gap.dispatch_candidate.event_type = "market.book.quality.gap_detected";
  gap.dispatch_selection.selected_event_id = gap_event_id;
  gap.dispatch_selection.selected_event_type =
      gap.dispatch_candidate.event_type;
  gap.dispatch_selection.selection_semantic_checksum =
      dispatch::derive_run_input_selection_checksum(
          publisher_config().dispatcher_config, gap.dispatch_selection,
          gap.dispatch_candidate);
  gap.dispatch_selection.selection_id = dispatch::derive_run_input_selection_id(
      gap.dispatch_selection.selection_semantic_checksum);
  gap.selection_id = gap.dispatch_selection.selection_id;
  gap.selected_event_id = gap_event_id;
  gap.selected_event_type = gap.dispatch_candidate.event_type;
  gap.selection_semantic_checksum =
      gap.dispatch_selection.selection_semantic_checksum;
  const auto accepted = publisher.accept_cut(gap, book, auxiliary);
  CHECK(accepted.ok());
  if (accepted.view)
    CHECK(accepted.view->quality.book_synchronization ==
          market::BookSynchronization::Gapped);
}

TEST_CASE("ordered reference facts advance reference versions with the cut") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(first.ok());
  if (!first.view || !first.bundle)
    return;
  acknowledge(publisher, first.view->view_id, first.bundle->bundle_id);

  const auto reference_event_id = id<contracts::EventId>(48);
  const auto reference_checksum =
      contracts::sha256(std::vector<std::byte>{std::byte{2}});
  CHECK(auxiliary
            .apply_quality_input({
                .event_id = reference_event_id,
                .input_semantic_checksum = reference_checksum,
                .listing_id = id<contracts::ListingId>(1),
                .kind = market::ListingQualityInputKind::BookInvalidated,
                .run_input_sequence = 2,
                .logical_time_nanoseconds = 101,
            })
            .ok());
  auto reference = cut_input(2);
  reference.dispatch_candidate.event_id = reference_event_id;
  reference.dispatch_candidate.event_type =
      "reference.listing.definition_changed";
  reference.dispatch_selection.selected_event_id = reference_event_id;
  reference.dispatch_selection.selected_event_type =
      reference.dispatch_candidate.event_type;
  reference.dispatch_selection.selection_semantic_checksum =
      dispatch::derive_run_input_selection_checksum(
          publisher_config().dispatcher_config, reference.dispatch_selection,
          reference.dispatch_candidate);
  reference.dispatch_selection.selection_id =
      dispatch::derive_run_input_selection_id(
          reference.dispatch_selection.selection_semantic_checksum);
  reference.selection_id = reference.dispatch_selection.selection_id;
  reference.selected_event_id = reference_event_id;
  reference.selected_event_type = reference.dispatch_candidate.event_type;
  reference.selected_event_position =
      contracts::EventPosition::from(id<contracts::StreamId>(10), 1, 0).value();
  reference.selection_semantic_checksum =
      reference.dispatch_selection.selection_semantic_checksum;
  reference.reference_snapshot_version = version(46);
  reference.listing_definition_version = version(47);
  reference.reference_configuration_lineage_version = version(48);
  auto cursors = std::vector<contracts::StreamCursor>(
      reference.lineage.cursors().begin(), reference.lineage.cursors().end());
  for (auto &entry : cursors) {
    if (entry.stream_id() == id<contracts::StreamId>(8))
      entry = cursor(8, 0);
    if (entry.stream_id() == id<contracts::StreamId>(10))
      entry = cursor(10, 0);
  }
  reference.lineage = contracts::StateLineage::from(id<contracts::RunId>(30), 2,
                                                    required_streams(), cursors)
                          .value();
  const auto accepted = publisher.accept_cut(reference, book, auxiliary);
  CHECK(accepted.ok());
  if (!accepted.bundle)
    return;
  CHECK(accepted.bundle->reference_cursor == cursor(10, 0));
  CHECK(accepted.bundle->reference_snapshot_version == version(46));
  CHECK(accepted.bundle->listing_definition_version == version(47));
  CHECK(accepted.bundle->reference_configuration_lineage_version ==
        version(48));
}

TEST_CASE("incomplete mismatched and stale cuts fail before acceptance") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();

  auto wrong_sequence = cut_input(2);
  CHECK(publisher.accept_cut(wrong_sequence, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);
  const std::array partial_required = {id<contracts::StreamId>(4)};
  const std::array partial_cursors = {origin(4)};
  auto incomplete = cut_input();
  incomplete.lineage =
      contracts::StateLineage::from(id<contracts::RunId>(30), 1,
                                    partial_required, partial_cursors)
          .value();
  CHECK(publisher.accept_cut(incomplete, book, auxiliary).failure ==
        market::ListingViewFailure::IncompleteLineage);
  auto mismatched = cut_input();
  mismatched.lineage = lineage(1, cursor(8, 0), cursor(4, 0));
  CHECK(publisher.accept_cut(mismatched, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidLineageTransition);
  auto torn_timer = cut_input();
  auto torn_cursors = std::vector<contracts::StreamCursor>(
      torn_timer.lineage.cursors().begin(), torn_timer.lineage.cursors().end());
  for (auto &entry : torn_cursors) {
    if (entry.stream_id() == id<contracts::StreamId>(13))
      entry = cursor(13, 0);
  }
  torn_timer.lineage =
      contracts::StateLineage::from(id<contracts::RunId>(30), 1,
                                    required_streams(), torn_cursors)
          .value();
  CHECK(publisher.accept_cut(torn_timer, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidLineageTransition);
  CHECK(!publisher.accepted_view());
}

TEST_CASE("book content cannot advance beyond its lineage proof") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  CHECK(book
            .apply_delta({
                .listing_id = id<contracts::ListingId>(1),
                .bid_changes = {{
                    .price =
                        contracts::Price::from_units(100, version(2)).value(),
                    .quantity =
                        contracts::Quantity::from_units(4, version(3)).value(),
                }},
            })
            .ok());
  CHECK(publisher.accept_cut(cut_input(), book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);
}

TEST_CASE("feature consumer sees only exact published view") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(result.ok());
  if (!result.view || !result.bundle)
    return;
  const auto &view = result.view;
  const auto capacity = publisher.publication_storage_capacity();

  const auto begin =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::NotPublished,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(begin) ==
        market::ListingViewFailure::None);
  CHECK(!publisher.published_view());
  CHECK(!publisher.published_bundle());
  const auto published =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublishedToFeatureBoundary);
  CHECK(publisher.transition_publication(published) ==
        market::ListingViewFailure::None);
  CHECK(publisher.published_view() == view);
  CHECK(publisher.published_bundle() == result.bundle);
  const auto accepted =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::PublishedToFeatureBoundary,
                  market::ViewPublicationState::FeatureConsumerAccepted);
  CHECK(publisher.transition_publication(accepted) ==
        market::ListingViewFailure::None);
  CHECK(publisher.transition_publication(accepted) ==
        market::ListingViewFailure::None);
  CHECK(publisher.publication_history().size() == 3);
  CHECK(publisher.publication_storage_capacity() == capacity);
}

TEST_CASE("publication retry is append-only and exact-view guarded") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(result.ok());
  if (!result.view || !result.bundle)
    return;
  const auto &view = result.view;

  const auto begin =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::NotPublished,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(begin) ==
        market::ListingViewFailure::None);
  const auto failed =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublicationFailedRetryable);
  CHECK(publisher.transition_publication(failed) ==
        market::ListingViewFailure::None);
  const auto retry =
      publication(view->view_id, result.bundle->bundle_id, 61, 2,
                  market::ViewPublicationState::PublicationFailedRetryable,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(retry) ==
        market::ListingViewFailure::None);
  auto wrong_view =
      publication(id<contracts::StateViewId>(99), result.bundle->bundle_id, 61,
                  2, market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublishedToFeatureBoundary);
  CHECK(publisher.transition_publication(wrong_view) ==
        market::ListingViewFailure::WrongView);
  CHECK(publisher.publication_history().size() == 3);
  auto wrong_bundle =
      publication(view->view_id, id<contracts::StateViewId>(98), 61, 2,
                  market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublishedToFeatureBoundary);
  CHECK(publisher.transition_publication(wrong_bundle) ==
        market::ListingViewFailure::WrongView);
}

TEST_CASE("publication failures preserve bounded lifecycle state") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config(3)).value();
  const auto result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(result.ok());
  if (!result.view || !result.bundle)
    return;
  const auto &view = result.view;

  auto wrong_boundary =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::NotPublished,
                  market::ViewPublicationState::PublicationInProgress);
  wrong_boundary.boundary_id = id<contracts::ConsumerBoundaryId>(99);
  CHECK(publisher.transition_publication(wrong_boundary) ==
        market::ListingViewFailure::WrongBoundary);
  const auto begin =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::NotPublished,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(begin) ==
        market::ListingViewFailure::None);
  const auto failed =
      publication(view->view_id, result.bundle->bundle_id, 60, 1,
                  market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublicationFailedRetryable);
  CHECK(publisher.transition_publication(failed) ==
        market::ListingViewFailure::None);
  const auto reused_attempt =
      publication(view->view_id, result.bundle->bundle_id, 60, 2,
                  market::ViewPublicationState::PublicationFailedRetryable,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(reused_attempt) ==
        market::ListingViewFailure::InvalidPublicationTransition);
  const auto retry =
      publication(view->view_id, result.bundle->bundle_id, 61, 2,
                  market::ViewPublicationState::PublicationFailedRetryable,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(retry) ==
        market::ListingViewFailure::None);
  const auto published =
      publication(view->view_id, result.bundle->bundle_id, 61, 2,
                  market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublishedToFeatureBoundary);
  CHECK(publisher.transition_publication(published) ==
        market::ListingViewFailure::PublicationHistoryExhausted);
  CHECK(publisher.publication_state() ==
        market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.publication_history().size() == 3);
}

TEST_CASE("terminal publication failure blocks every later cut") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(first.ok());
  if (!first.view || !first.bundle)
    return;
  CHECK(publisher.transition_publication(
            publication(first.view->view_id, first.bundle->bundle_id, 60, 1,
                        market::ViewPublicationState::NotPublished,
                        market::ViewPublicationState::PublicationInProgress)) ==
        market::ListingViewFailure::None);
  CHECK(publisher.transition_publication(publication(
            first.view->view_id, first.bundle->bundle_id, 60, 1,
            market::ViewPublicationState::PublicationInProgress,
            market::ViewPublicationState::PublicationFailedTerminal)) ==
        market::ListingViewFailure::None);
  apply_book_delta(book, auxiliary, 2);
  CHECK(publisher.accept_cut(cut_input(2), book, auxiliary).failure ==
        market::ListingViewFailure::PriorViewPending);
  CHECK(publisher.accepted_view() == first.view);
  CHECK(publisher.publication_history().size() == 2);
}

TEST_CASE("next cut waits for prior acknowledgement and links ancestry") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto first_result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(first_result.ok());
  if (!first_result.view || !first_result.bundle)
    return;
  const auto &first = first_result.view;
  apply_book_delta(book, auxiliary, 2);
  const auto second_input = cut_input(2);
  CHECK(publisher.accept_cut(second_input, book, auxiliary).failure ==
        market::ListingViewFailure::PriorViewPending);

  acknowledge(publisher, first->view_id, first_result.bundle->bundle_id);

  auto jumped = second_input;
  jumped.dispatch_candidate.event_position =
      contracts::EventPosition::from(id<contracts::StreamId>(14), 1, 100)
          .value();
  jumped.dispatch_selection.selected_event_position =
      jumped.dispatch_candidate.event_position;
  jumped.dispatch_selection.pre_selection_cursors = {cursor(14, 99)};
  jumped.dispatch_selection.post_selection_cursors = {cursor(14, 100)};
  jumped.dispatch_selection.selection_semantic_checksum =
      dispatch::derive_run_input_selection_checksum(
          publisher_config().dispatcher_config, jumped.dispatch_selection,
          jumped.dispatch_candidate);
  jumped.dispatch_selection.selection_id =
      dispatch::derive_run_input_selection_id(
          jumped.dispatch_selection.selection_semantic_checksum);
  jumped.selection_id = jumped.dispatch_selection.selection_id;
  jumped.selection_semantic_checksum =
      jumped.dispatch_selection.selection_semantic_checksum;
  CHECK(publisher.accept_cut(jumped, book, auxiliary).failure ==
        market::ListingViewFailure::InvalidSelectionEvidence);

  const auto second_result =
      publisher.accept_cut(second_input, book, auxiliary);
  CHECK(second_result.ok());
  if (!second_result.view || !second_result.bundle)
    return;
  const auto &second = second_result.view;
  CHECK(second->prior_view_id == first->view_id);
  CHECK(second_result.bundle->prior_bundle_id ==
        first_result.bundle->bundle_id);
  CHECK(publisher.publication_history().size() == 3);
  CHECK(publisher.published_view() == first_result.view);
  CHECK(publisher.published_bundle() == first_result.bundle);
  CHECK(second->view_id != first->view_id);
  CHECK(second->bids.front().quantity.units() == 4);
  CHECK(first->bids.front().quantity.units() == 2);
  acknowledge(publisher, second->view_id, second_result.bundle->bundle_id);
  CHECK(publisher.publication_history().size() == 6);
  CHECK(publisher.published_view() == second_result.view);
  CHECK(publisher.published_bundle() == second_result.bundle);
  const auto historical = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(historical.view == first_result.view);
  CHECK(historical.bundle == first_result.bundle);
}

TEST_CASE("accepted selection history has explicit fixed exhaustion") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config(8, 1)).value();
  const auto first = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(first.ok());
  if (!first.view || !first.bundle)
    return;
  acknowledge(publisher, first.view->view_id, first.bundle->bundle_id);
  apply_book_delta(book, auxiliary, 2);
  CHECK(publisher.accept_cut(cut_input(2), book, auxiliary).failure ==
        market::ListingViewFailure::AcceptedViewHistoryExhausted);
  CHECK(publisher.accepted_view() == first.view);
  CHECK(publisher.accepted_bundle() == first.bundle);
}

TEST_CASE("publisher configuration requires every registered role stream") {
  auto invalid = publisher_config();
  invalid.required_streams.pop_back();
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.maximum_publication_transitions = 0;
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.maximum_publication_transitions = 2;
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.maximum_retained_views = 0;
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.run_timer_stream_id = invalid.run_control_stream_id;
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.initial_configuration_epoch = 0;
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.initial_lineage = lineage(1, cursor(8, 0));
  CHECK(!market::ListingViewPublisher::create(invalid));
}
