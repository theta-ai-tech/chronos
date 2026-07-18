#include "chronos/core/market_state/listing_view_publisher.hpp"

#include "microtest.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
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
        contracts::StreamCursor continuity_cursor = origin(7)) {
  const auto required = required_streams();
  const std::array cursors = {
      trade_cursor, continuity_cursor, book_cursor, origin(10),
      origin(11),   origin(12),        origin(13),
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
publisher_config(std::size_t maximum_transitions = 8) {
  return {
      .run_id = id<contracts::RunId>(30),
      .listing_id = id<contracts::ListingId>(1),
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
      .merge_policy_version = version(37),
      .initial_configuration_epoch = 1,
      .view_schema_version = version(32),
      .capability_version = version(33),
      .transition_policy_version = version(34),
      .arithmetic_version = version(35),
      .canonicalization_version = version(36),
      .maximum_publication_transitions = maximum_transitions,
  };
}

market::ListingViewCutInput cut_input(std::uint64_t run_sequence = 1) {
  const auto event_sequence = run_sequence - 1;
  return {
      .selection_id = id<contracts::RunInputSelectionId>(
          static_cast<std::uint8_t>(39 + run_sequence)),
      .selected_event_id =
          id<contracts::EventId>(static_cast<std::uint8_t>(40 + run_sequence)),
      .selected_event_type = run_sequence == 1
                                 ? "market.book.observation.snapshot"
                                 : "market.book.observation.delta",
      .selected_event_position =
          contracts::EventPosition::from(id<contracts::StreamId>(8), 1,
                                         event_sequence)
              .value(),
      .input_semantic_checksum =
          digest(static_cast<std::uint8_t>(50 + run_sequence)),
      .selection_semantic_checksum =
          digest(static_cast<std::uint8_t>(60 + run_sequence)),
      .merge_policy_version = version(37),
      .configuration_epoch = 1,
      .lineage = lineage(run_sequence, cursor(8, event_sequence)),
  };
}

market::ViewPublicationTransition publication(contracts::StateViewId view_id,
                                              std::uint8_t attempt_seed,
                                              std::uint64_t attempt_number,
                                              market::ViewPublicationState from,
                                              market::ViewPublicationState to) {
  return {
      .view_id = view_id,
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
                 contracts::StateViewId view_id) {
  if (publisher.transition_publication(publication(
          view_id, 60, 1, market::ViewPublicationState::NotPublished,
          market::ViewPublicationState::PublicationInProgress)) !=
          market::ListingViewFailure::None ||
      publisher.transition_publication(publication(
          view_id, 60, 1, market::ViewPublicationState::PublicationInProgress,
          market::ViewPublicationState::PublishedToFeatureBoundary)) !=
          market::ListingViewFailure::None ||
      publisher.transition_publication(
          publication(view_id, 60, 1,
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
  if (!result.view)
    return;
  CHECK(result.view == publisher.accepted_view());
  CHECK(!publisher.published_view());
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
  if (!first_result.view || !second_result.view)
    return;
  const auto &first_view = first_result.view;
  const auto &second_view = second_result.view;
  CHECK(first_view->view_id == second_view->view_id);
  CHECK(first_view->semantic_checksum == second_view->semantic_checksum);
  CHECK(*first_view == *second_view);
}

TEST_CASE("selection redelivery is exact and contradictory reuse fails") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto input = cut_input();
  const auto first = publisher.accept_cut(input, book, auxiliary);
  CHECK(first.ok());
  if (!first.view)
    return;
  const auto duplicate = publisher.accept_cut(input, book, auxiliary);
  CHECK(duplicate.ok());
  CHECK(duplicate.view == first.view);
  auto contradictory = input;
  contradictory.input_semantic_checksum = digest(99);
  CHECK(publisher.accept_cut(contradictory, book, auxiliary).failure ==
        market::ListingViewFailure::ContradictorySelection);
  acknowledge(publisher, first.view->view_id);
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

TEST_CASE("incomplete mismatched and stale cuts fail before acceptance") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();

  auto wrong_sequence = cut_input(2);
  CHECK(publisher.accept_cut(wrong_sequence, book, auxiliary).failure ==
        market::ListingViewFailure::RunInputMismatch);
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
        market::ListingViewFailure::BookStateMismatch);
}

TEST_CASE("feature consumer sees only exact published view") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();
  const auto result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(result.ok());
  if (!result.view)
    return;
  const auto &view = result.view;
  const auto capacity = publisher.publication_storage_capacity();

  const auto begin = publication(
      view->view_id, 60, 1, market::ViewPublicationState::NotPublished,
      market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(begin) ==
        market::ListingViewFailure::None);
  CHECK(!publisher.published_view());
  const auto published = publication(
      view->view_id, 60, 1, market::ViewPublicationState::PublicationInProgress,
      market::ViewPublicationState::PublishedToFeatureBoundary);
  CHECK(publisher.transition_publication(published) ==
        market::ListingViewFailure::None);
  CHECK(publisher.published_view() == view);
  const auto accepted =
      publication(view->view_id, 60, 1,
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
  if (!result.view)
    return;
  const auto &view = result.view;

  const auto begin = publication(
      view->view_id, 60, 1, market::ViewPublicationState::NotPublished,
      market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(begin) ==
        market::ListingViewFailure::None);
  const auto failed = publication(
      view->view_id, 60, 1, market::ViewPublicationState::PublicationInProgress,
      market::ViewPublicationState::PublicationFailedRetryable);
  CHECK(publisher.transition_publication(failed) ==
        market::ListingViewFailure::None);
  const auto retry =
      publication(view->view_id, 61, 2,
                  market::ViewPublicationState::PublicationFailedRetryable,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(retry) ==
        market::ListingViewFailure::None);
  auto wrong_view =
      publication(id<contracts::StateViewId>(99), 61, 2,
                  market::ViewPublicationState::PublicationInProgress,
                  market::ViewPublicationState::PublishedToFeatureBoundary);
  CHECK(publisher.transition_publication(wrong_view) ==
        market::ListingViewFailure::WrongView);
  CHECK(publisher.publication_history().size() == 3);
}

TEST_CASE("publication failures preserve bounded lifecycle state") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config(3)).value();
  const auto result = publisher.accept_cut(cut_input(), book, auxiliary);
  CHECK(result.ok());
  if (!result.view)
    return;
  const auto &view = result.view;

  auto wrong_boundary = publication(
      view->view_id, 60, 1, market::ViewPublicationState::NotPublished,
      market::ViewPublicationState::PublicationInProgress);
  wrong_boundary.boundary_id = id<contracts::ConsumerBoundaryId>(99);
  CHECK(publisher.transition_publication(wrong_boundary) ==
        market::ListingViewFailure::WrongBoundary);
  const auto begin = publication(
      view->view_id, 60, 1, market::ViewPublicationState::NotPublished,
      market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(begin) ==
        market::ListingViewFailure::None);
  const auto failed = publication(
      view->view_id, 60, 1, market::ViewPublicationState::PublicationInProgress,
      market::ViewPublicationState::PublicationFailedRetryable);
  CHECK(publisher.transition_publication(failed) ==
        market::ListingViewFailure::None);
  const auto reused_attempt =
      publication(view->view_id, 60, 2,
                  market::ViewPublicationState::PublicationFailedRetryable,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(reused_attempt) ==
        market::ListingViewFailure::InvalidPublicationTransition);
  const auto retry =
      publication(view->view_id, 61, 2,
                  market::ViewPublicationState::PublicationFailedRetryable,
                  market::ViewPublicationState::PublicationInProgress);
  CHECK(publisher.transition_publication(retry) ==
        market::ListingViewFailure::None);
  const auto published = publication(
      view->view_id, 61, 2, market::ViewPublicationState::PublicationInProgress,
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
  if (!first.view)
    return;
  CHECK(publisher.transition_publication(
            publication(first.view->view_id, 60, 1,
                        market::ViewPublicationState::NotPublished,
                        market::ViewPublicationState::PublicationInProgress)) ==
        market::ListingViewFailure::None);
  CHECK(publisher.transition_publication(publication(
            first.view->view_id, 60, 1,
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
  if (!first_result.view)
    return;
  const auto &first = first_result.view;
  apply_book_delta(book, auxiliary, 2);
  const auto second_input = cut_input(2);
  CHECK(publisher.accept_cut(second_input, book, auxiliary).failure ==
        market::ListingViewFailure::PriorViewPending);

  acknowledge(publisher, first->view_id);

  const auto second_result =
      publisher.accept_cut(second_input, book, auxiliary);
  CHECK(second_result.ok());
  if (!second_result.view)
    return;
  const auto &second = second_result.view;
  CHECK(second->prior_view_id == first->view_id);
  CHECK(second->view_id != first->view_id);
  CHECK(second->bids.front().quantity.units() == 4);
  CHECK(first->bids.front().quantity.units() == 2);
}

TEST_CASE("publisher configuration requires every registered role stream") {
  auto invalid = publisher_config();
  invalid.required_streams.pop_back();
  CHECK(!market::ListingViewPublisher::create(invalid));
  invalid = publisher_config();
  invalid.maximum_publication_transitions = 0;
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
