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
  market::ListingQualityInput trade_sync{
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::TradeSynchronized,
      .run_input_sequence = 2,
      .logical_time_nanoseconds = 100,
      .trade_proof =
          market::TradeContinuityProof{
              .boundary_event_id = id<contracts::EventId>(24),
              .boundary_cursor = cursor(7, 0),
              .prior_trade_cursor = origin(4),
              .recovered_trade_cursor = origin(4),
              .fidelity = market::TradeFidelity::Lossless,
          },
  };
  if (!auxiliary.apply_quality_input(trade_sync).ok())
    std::abort();
  return auxiliary;
}

std::vector<contracts::StreamId> required_streams() {
  return {id<contracts::StreamId>(4),  id<contracts::StreamId>(7),
          id<contracts::StreamId>(8),  id<contracts::StreamId>(10),
          id<contracts::StreamId>(11), id<contracts::StreamId>(12),
          id<contracts::StreamId>(13)};
}

market::ListingViewPublisherConfig
publisher_config(std::size_t maximum_transitions = 8) {
  return {
      .run_id = id<contracts::RunId>(30),
      .listing_id = id<contracts::ListingId>(1),
      .required_streams = required_streams(),
      .book_stream_id = id<contracts::StreamId>(8),
      .trade_stream_id = id<contracts::StreamId>(4),
      .trade_continuity_stream_id = id<contracts::StreamId>(7),
      .reference_stream_id = id<contracts::StreamId>(10),
      .market_control_stream_id = id<contracts::StreamId>(11),
      .run_control_stream_id = id<contracts::StreamId>(12),
      .run_timer_stream_id = id<contracts::StreamId>(13),
      .feature_boundary_id = id<contracts::ConsumerBoundaryId>(31),
      .view_schema_version = version(32),
      .capability_version = version(33),
      .transition_policy_version = version(34),
      .arithmetic_version = version(35),
      .canonicalization_version = version(36),
      .maximum_publication_transitions = maximum_transitions,
  };
}

contracts::StateLineage
lineage(std::uint64_t run_sequence,
        contracts::StreamCursor trade_cursor = origin(4)) {
  const auto required = required_streams();
  const std::array cursors = {
      trade_cursor, cursor(7, 0), cursor(8, 0), origin(10),
      origin(11),   origin(12),   origin(13),
  };
  return contracts::StateLineage::from(id<contracts::RunId>(30), run_sequence,
                                       required, cursors)
      .value();
}

market::ListingViewCutInput cut_input(std::uint64_t run_sequence = 2) {
  return {
      .selection_id = id<contracts::RunInputSelectionId>(40),
      .selected_event_id = id<contracts::EventId>(41),
      .lineage = lineage(run_sequence),
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

market::RecentTrade first_trade() {
  return {
      .event_id = id<contracts::EventId>(50),
      .source_event_id = id<contracts::SourceEventId>(51),
      .listing_id = id<contracts::ListingId>(1),
      .cursor = cursor(4, 0),
      .source_event_time =
          contracts::TimePoint::from(101, id<contracts::ClockDomainId>(22),
                                     contracts::ClockClass::source_wall, 1)
              .value(),
      .source_time_quality = market::SourceTimeQuality::Exact,
      .fidelity = market::TradeFidelity::Lossless,
      .price = contracts::Price::from_units(101, version(2)).value(),
      .quantity = contracts::Quantity::from_units(1, version(3)).value(),
      .aggressor_side = market::TradeAggressorSide::Buy,
      .run_input_sequence = 3,
      .logical_time_nanoseconds = 101,
  };
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
  CHECK(result.view->lineage.run_input_sequence() == 2);
  CHECK(result.view->bids ==
        (std::vector<market::L2Level>{book.bids().front()}));
  CHECK(result.view->top.shape == market::L2BookShape::Normal);
  CHECK(result.view->quality == auxiliary.quality());
  CHECK(!result.view->prior_view_id);

  CHECK(auxiliary.apply_trade(first_trade()).ok());
  CHECK(result.view->recent_trades.empty());
  CHECK(result.view->lineage.run_input_sequence() == 2);
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

TEST_CASE("incomplete mismatched and stale cuts fail before acceptance") {
  auto book = make_book();
  auto auxiliary = make_auxiliary(book);
  auto publisher =
      market::ListingViewPublisher::create(publisher_config()).value();

  auto wrong_sequence = cut_input(1);
  CHECK(publisher.accept_cut(wrong_sequence, book, auxiliary).failure ==
        market::ListingViewFailure::RunInputMismatch);
  const std::array partial_required = {id<contracts::StreamId>(4)};
  const std::array partial_cursors = {origin(4)};
  auto incomplete = cut_input();
  incomplete.lineage =
      contracts::StateLineage::from(id<contracts::RunId>(30), 2,
                                    partial_required, partial_cursors)
          .value();
  CHECK(publisher.accept_cut(incomplete, book, auxiliary).failure ==
        market::ListingViewFailure::IncompleteLineage);
  auto mismatched = cut_input();
  mismatched.lineage = lineage(2, cursor(4, 0));
  CHECK(publisher.accept_cut(mismatched, book, auxiliary).failure ==
        market::ListingViewFailure::CursorMismatch);
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

  auto observed = market::ListingQualityInput{
      .listing_id = id<contracts::ListingId>(1),
      .kind = market::ListingQualityInputKind::BookEvidenceObserved,
      .run_input_sequence = 3,
      .logical_time_nanoseconds = 101,
      .book_proof = *auxiliary.quality().last_book_proof,
  };
  observed.book_proof->applied_through_cursor = cursor(8, 1);
  observed.book_proof->l2_transition_sequence = book.transition_sequence();
  CHECK(auxiliary.apply_quality_input(observed, &book).ok());
  auto complete = cut_input(3);
  complete.lineage = lineage(3);
  const auto cursors = complete.lineage.cursors();
  std::vector<contracts::StreamCursor> advanced(cursors.begin(), cursors.end());
  for (auto &entry : advanced) {
    if (entry.stream_id() == id<contracts::StreamId>(8))
      entry = cursor(8, 1);
  }
  complete.lineage = contracts::StateLineage::from(id<contracts::RunId>(30), 3,
                                                   required_streams(), advanced)
                         .value();
  CHECK(publisher.accept_cut(complete, book, auxiliary).ok());
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
  CHECK(auxiliary.apply_trade(first_trade()).ok());
  auto second_input = cut_input(3);
  second_input.selection_id = id<contracts::RunInputSelectionId>(42);
  second_input.selected_event_id = id<contracts::EventId>(50);
  second_input.lineage = lineage(3, cursor(4, 0));
  CHECK(publisher.accept_cut(second_input, book, auxiliary).failure ==
        market::ListingViewFailure::PriorViewPending);

  CHECK(publisher.transition_publication(publication(
            first->view_id, 60, 1, market::ViewPublicationState::NotPublished,
            market::ViewPublicationState::PublicationInProgress)) ==
        market::ListingViewFailure::None);
  CHECK(publisher.transition_publication(publication(
            first->view_id, 60, 1,
            market::ViewPublicationState::PublicationInProgress,
            market::ViewPublicationState::PublishedToFeatureBoundary)) ==
        market::ListingViewFailure::None);
  CHECK(publisher.transition_publication(publication(
            first->view_id, 60, 1,
            market::ViewPublicationState::PublishedToFeatureBoundary,
            market::ViewPublicationState::FeatureConsumerAccepted)) ==
        market::ListingViewFailure::None);

  const auto second_result =
      publisher.accept_cut(second_input, book, auxiliary);
  CHECK(second_result.ok());
  if (!second_result.view)
    return;
  const auto &second = second_result.view;
  CHECK(second->prior_view_id == first->view_id);
  CHECK(second->view_id != first->view_id);
  CHECK(second->recent_trades.size() == 1);
  CHECK(first->recent_trades.empty());
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
}
