#include "chronos/core/dispatch/run_input_dispatcher.hpp"

#include "microtest.hpp"

#include <string_view>
#include <vector>

namespace {
namespace dispatch = chronos::core::dispatch;
namespace contracts = chronos::contracts;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number = 1) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

dispatch::RunInputDispatcherConfig config() {
  return {
      .run_id = id<contracts::RunId>(1),
      .input_stream_id = id<contracts::StreamId>(2),
      .input_stream_epoch = 1,
      .merge_policy_version = version(3),
      .registry_snapshot_version = version(4),
      .initial_configuration_epoch = 1,
      .maximum_payload_bytes = 1024,
      .maximum_pending_controls = 8,
  };
}

dispatch::RunInputCandidate candidate(std::uint8_t event_seed,
                                      std::uint64_t stream_sequence,
                                      std::string_view payload = "fact") {
  auto semantic_payload = bytes(payload);
  return {
      .event_id = id<contracts::EventId>(event_seed),
      .event_type = "market.book.observation.snapshot",
      .event_position = contracts::EventPosition::from(
                            id<contracts::StreamId>(2), 1, stream_sequence)
                            .value(),
      .semantic_payload = semantic_payload,
      .semantic_checksum = contracts::sha256(semantic_payload),
  };
}

class RecordingPersistence final
    : public dispatch::RunInputSelectionPersistence {
public:
  bool commit_control_reservation(
      const dispatch::ControlBoundaryReservation &reservation) override {
    if (!allow_control_reservation)
      return false;
    reservations.push_back(reservation);
    return true;
  }

  bool commit_control_visibility(
      const dispatch::ControlBoundaryReservation &reservation) override {
    if (!allow_control_visibility)
      return false;
    visible_controls.push_back(reservation);
    return true;
  }

  bool
  commit_selection(const dispatch::RunInputSelectionRecord &record) override {
    if (!allow_selection)
      return false;
    selections.push_back(record);
    return true;
  }

  bool commit_consumer_acceptance(contracts::RunInputSelectionId selection_id,
                                  std::uint64_t run_input_sequence) override {
    if (!allow_acceptance)
      return false;
    acceptances.emplace_back(selection_id, run_input_sequence);
    return true;
  }

  bool allow_control_reservation{true};
  bool allow_control_visibility{true};
  bool allow_selection{true};
  bool allow_acceptance{true};
  std::vector<dispatch::ControlBoundaryReservation> reservations;
  std::vector<dispatch::ControlBoundaryReservation> visible_controls;
  std::vector<dispatch::RunInputSelectionRecord> selections;
  std::vector<std::pair<contracts::RunInputSelectionId, std::uint64_t>>
      acceptances;
};

class TestRegistry final : public dispatch::RunInputEligibilityRegistry {
public:
  bool is_run_input_eligible(
      std::string_view event_type,
      contracts::VersionRef registry_snapshot_version) const override {
    return event_type == "market.book.observation.snapshot" &&
           registry_snapshot_version == version(4);
  }
};

class RecordingConsumer final : public dispatch::RunInputConsumer {
public:
  dispatch::ConsumerDisposition
  accept(const dispatch::RunInputSelectionRecord &selection,
         const dispatch::RunInputCandidate &input) override {
    selections.push_back(selection);
    candidates.push_back(input);
    const auto result = next_disposition;
    if (next_disposition == dispatch::ConsumerDisposition::Accepted &&
        duplicate_after_acceptance) {
      next_disposition = dispatch::ConsumerDisposition::AlreadyAccepted;
    }
    return result;
  }

  dispatch::ConsumerDisposition next_disposition{
      dispatch::ConsumerDisposition::Accepted};
  bool duplicate_after_acceptance{};
  std::vector<dispatch::RunInputSelectionRecord> selections;
  std::vector<dispatch::RunInputCandidate> candidates;
};

dispatch::ControlBoundaryReservation control(std::uint8_t seed,
                                             std::uint64_t control_sequence,
                                             std::uint64_t effective_position,
                                             std::uint64_t epoch) {
  return {
      .control_outcome_id = id<contracts::EventId>(seed),
      .control_sequence = control_sequence,
      .effective_position = effective_position,
      .new_configuration_epoch = epoch,
  };
}

} // namespace

TEST_CASE("single-stream dispatch allocates one total run-input order") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();

  const auto first = dispatcher.dispatch(candidate(10, 1, "first"), consumer);
  const auto second = dispatcher.dispatch(candidate(11, 2, "second"), consumer);
  CHECK(first.ok());
  CHECK(second.ok());
  if (!first.selection.has_value() || !second.selection.has_value())
    return;
  CHECK(first.selection->run_input_sequence == 1);
  CHECK(second.selection->run_input_sequence == 2);
  CHECK(first.selection->selection_id != second.selection->selection_id);
  CHECK(first.selection->pre_selection_cursors.size() == 1);
  CHECK(first.selection->pre_selection_cursors[0].is_origin());
  CHECK(first.selection->post_selection_cursors[0].last_consumed_sequence() ==
        1);
  CHECK(second.selection->pre_selection_cursors[0] ==
        first.selection->post_selection_cursors[0]);
  CHECK(second.selection->post_selection_cursors[0].last_consumed_sequence() ==
        2);
  CHECK(persistence.selections == consumer.selections);
  CHECK(persistence.selections.size() == 2);
  CHECK(persistence.acceptances.size() == 2);
  CHECK(dispatcher.current_run_input_sequence() == 2);
  CHECK(dispatcher.current_input_cursor().last_consumed_sequence() == 2);
}

TEST_CASE("cursor and integrity failures allocate no run-input position") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();

  CHECK(dispatcher.dispatch(candidate(10, 2), consumer).failure ==
        dispatch::DispatchFailure::CursorMismatch);
  auto corrupt = candidate(10, 1);
  corrupt.semantic_payload.front() = std::byte{'x'};
  CHECK(dispatcher.dispatch(std::move(corrupt), consumer).failure ==
        dispatch::DispatchFailure::InvalidCandidate);
  auto ineligible = candidate(10, 1);
  ineligible.event_type = "market.trade.observation.executed";
  CHECK(contracts::is_valid_event_type(ineligible.event_type));
  CHECK(dispatcher.dispatch(std::move(ineligible), consumer).failure ==
        dispatch::DispatchFailure::IneligibleCandidate);
  CHECK(dispatcher.current_run_input_sequence() == 0);
  CHECK(dispatcher.current_input_cursor().is_origin());
  CHECK(persistence.selections.empty());
  CHECK(consumer.selections.empty());
}

TEST_CASE("control reservation blocks and applies at its exact boundary") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto reserved = control(20, 1, 2, 2);
  CHECK(dispatcher.reserve_control_boundary(reserved));
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).ok());
  CHECK(dispatcher.active_configuration_epoch() == 1);

  CHECK(dispatcher.dispatch(candidate(11, 2), consumer).failure ==
        dispatch::DispatchFailure::ControlBarrierBlocked);
  CHECK(dispatcher.current_run_input_sequence() == 1);
  CHECK(dispatcher.make_control_visible(reserved));
  const auto selected = dispatcher.dispatch(candidate(11, 2), consumer);
  CHECK(selected.ok());
  if (!selected.selection.has_value())
    return;
  CHECK(selected.selection->active_configuration_epoch == 2);
  CHECK(selected.selection->applied_control_outcome_ids ==
        std::vector<contracts::EventId>{reserved.control_outcome_id});
  CHECK(dispatcher.active_configuration_epoch() == 2);
}

TEST_CASE("multiple controls at one boundary use control history order") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto second = control(22, 2, 1, 3);
  const auto first = control(21, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(second));
  CHECK(dispatcher.reserve_control_boundary(first));
  CHECK(dispatcher.make_control_visible(second));
  CHECK(dispatcher.make_control_visible(first));
  const auto selected = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(selected.ok());
  if (!selected.selection.has_value())
    return;
  CHECK(selected.selection->active_configuration_epoch == 3);
  CHECK(selected.selection->applied_control_outcome_ids ==
        (std::vector<contracts::EventId>{first.control_outcome_id,
                                         second.control_outcome_id}));
}

TEST_CASE("selection persistence failure advances no cursor or epoch") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto reserved = control(20, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(reserved));
  CHECK(dispatcher.make_control_visible(reserved));
  persistence.allow_selection = false;
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).failure ==
        dispatch::DispatchFailure::SelectionPersistenceRejected);
  CHECK(dispatcher.current_run_input_sequence() == 0);
  CHECK(dispatcher.current_input_cursor().is_origin());
  CHECK(dispatcher.active_configuration_epoch() == 1);
  CHECK(consumer.selections.empty());

  persistence.allow_selection = true;
  const auto retried = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(retried.ok());
  if (!retried.selection.has_value())
    return;
  CHECK(retried.selection->active_configuration_epoch == 2);
}

TEST_CASE("uncertain publication retries the exact persisted selection") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  consumer.duplicate_after_acceptance = true;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  persistence.allow_acceptance = false;
  const auto uncertain = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(uncertain.failure == dispatch::DispatchFailure::PublicationUnconfirmed);
  CHECK(uncertain.selection.has_value());
  CHECK(dispatcher.has_pending_publication());
  CHECK(dispatcher.current_run_input_sequence() == 1);
  CHECK(dispatcher.dispatch(candidate(11, 2), consumer).failure ==
        dispatch::DispatchFailure::PublicationPending);

  persistence.allow_acceptance = true;
  const auto retried = dispatcher.retry_pending(consumer);
  CHECK(retried.ok());
  CHECK(retried.selection == uncertain.selection);
  CHECK(consumer.selections.size() == 2);
  CHECK(consumer.selections[0] == consumer.selections[1]);
  CHECK(consumer.candidates[0] == consumer.candidates[1]);
  CHECK(!dispatcher.has_pending_publication());
}
