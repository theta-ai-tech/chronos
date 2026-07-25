#include "chronos/core/dispatch/run_input_dispatcher.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <optional>
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
      .control_stream_id = id<contracts::StreamId>(5),
      .control_stream_epoch = 1,
      .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(6),
      .merge_policy_version = version(3),
      .registry_snapshot_version = version(4),
      .initial_configuration_epoch = 1,
      .maximum_payload_bytes = 1024,
      .maximum_control_payload_bytes = 1024,
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
  dispatch::RunInputRecoveryLoad load_recovery_state(
      const dispatch::RunInputDispatcherConfig &config) override {
    if (!allow_recovery)
      return {};
    if (!state.has_value()) {
      state = dispatch::RunInputRecoveryState{
          .input_cursor = contracts::StreamCursor::at_origin(
                              config.input_stream_id, config.input_stream_epoch)
                              .value(),
          .control_cursor =
              contracts::StreamCursor::at_origin(config.control_stream_id,
                                                 config.control_stream_epoch)
                  .value(),
          .configuration_epoch = config.initial_configuration_epoch,
      };
      return {.success = true};
    }
    return {.success = true, .state = state};
  }

  bool commit_control_reservation(
      const dispatch::ControlBoundaryReservation &reservation) override {
    if (!allow_control_reservation ||
        std::find(
            control_outcome_history.begin(), control_outcome_history.end(),
            reservation.control_outcome_id) != control_outcome_history.end())
      return false;
    control_outcome_history.push_back(reservation.control_outcome_id);
    reservations.push_back(reservation);
    state->control_cursor =
        contracts::StreamCursor::at_sequence(reservation.control_stream_id,
                                             reservation.control_stream_epoch,
                                             reservation.control_sequence)
            .value();
    state->pending_controls.push_back({.reservation = reservation});
    return true;
  }

  bool commit_control_visibility(
      const dispatch::ControlBoundaryReservation &reservation) override {
    if (!allow_control_visibility)
      return false;
    visible_controls.push_back(reservation);
    const auto found = std::find_if(
        state->pending_controls.begin(), state->pending_controls.end(),
        [&](const dispatch::RecoverableControl &control) {
          return control.reservation == reservation;
        });
    if (found == state->pending_controls.end())
      return false;
    found->visible = true;
    return true;
  }

  bool commit_selection(const dispatch::RunInputSelectionRecord &record,
                        const dispatch::RunInputCandidate &candidate) override {
    if (!allow_selection)
      return false;
    selections.push_back(record);
    state->input_cursor = record.post_selection_cursors.front();
    state->control_cursor = record.control_cursor;
    state->run_input_sequence = record.run_input_sequence;
    state->configuration_epoch = record.active_configuration_epoch;
    state->pending_controls.erase(
        std::remove_if(state->pending_controls.begin(),
                       state->pending_controls.end(),
                       [&](const dispatch::RecoverableControl &control) {
                         return std::find(record.applied_controls.begin(),
                                          record.applied_controls.end(),
                                          control.reservation) !=
                                record.applied_controls.end();
                       }),
        state->pending_controls.end());
    state->pending_publication = dispatch::RecoverablePublication{
        .selection = record, .candidate = candidate};
    return true;
  }

  bool commit_publication_transition(
      const dispatch::PublicationTransition &transition) override {
    if (blocked_transition == transition.to ||
        !state->pending_publication.has_value() ||
        state->pending_publication->selection.selection_id !=
            transition.selection_id ||
        state->pending_publication->selection.consumer_boundary_id !=
            transition.consumer_boundary_id ||
        state->pending_publication->state != transition.from)
      return false;
    transitions.push_back(transition);
    state->pending_publication->state = transition.to;
    state->pending_publication->attempt_number = transition.attempt_number;
    state->pending_publication->attempt_id = transition.attempt_id;
    if (transition.to == dispatch::PublicationState::ConsumerAccepted)
      state->pending_publication.reset();
    return true;
  }

  bool allow_recovery{true};
  bool allow_control_reservation{true};
  bool allow_control_visibility{true};
  bool allow_selection{true};
  std::optional<dispatch::PublicationState> blocked_transition;
  std::optional<dispatch::RunInputRecoveryState> state;
  std::vector<contracts::EventId> control_outcome_history;
  std::vector<dispatch::ControlBoundaryReservation> reservations;
  std::vector<dispatch::ControlBoundaryReservation> visible_controls;
  std::vector<dispatch::RunInputSelectionRecord> selections;
  std::vector<dispatch::PublicationTransition> transitions;
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
  contracts::ConsumerBoundaryId boundary_id() const noexcept override {
    return configured_boundary_id;
  }

  dispatch::ConsumerDisposition
  accept(const dispatch::RunInputSelectionRecord &selection,
         const dispatch::RunInputCandidate &input,
         contracts::PublicationAttemptId attempt_id) override {
    selections.push_back(selection);
    candidates.push_back(input);
    attempts.push_back(attempt_id);
    const auto result = next_disposition;
    if (next_disposition == dispatch::ConsumerDisposition::Accepted &&
        duplicate_after_acceptance) {
      next_disposition = dispatch::ConsumerDisposition::AlreadyAccepted;
    }
    return result;
  }

  dispatch::ConsumerDisposition next_disposition{
      dispatch::ConsumerDisposition::Accepted};
  contracts::ConsumerBoundaryId configured_boundary_id{
      id<contracts::ConsumerBoundaryId>(6)};
  bool duplicate_after_acceptance{};
  std::vector<dispatch::RunInputSelectionRecord> selections;
  std::vector<dispatch::RunInputCandidate> candidates;
  std::vector<contracts::PublicationAttemptId> attempts;
};

dispatch::ControlBoundaryReservation control(std::uint8_t seed,
                                             std::uint64_t control_sequence,
                                             std::uint64_t effective_position,
                                             std::uint64_t prior_epoch,
                                             std::uint64_t new_epoch) {
  const auto payload = bytes("configuration-change");
  return {
      .run_id = id<contracts::RunId>(1),
      .control_stream_id = id<contracts::StreamId>(5),
      .control_stream_epoch = 1,
      .control_outcome_id = id<contracts::EventId>(seed),
      .control_sequence = control_sequence,
      .effective_position = effective_position,
      .prior_configuration_epoch = prior_epoch,
      .new_configuration_epoch = new_epoch,
      .behavior_payload = payload,
      .behavior_checksum = contracts::sha256(payload),
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
  CHECK(persistence.transitions.size() == 6);
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
  const auto invalid_epoch = control(19, 1, 1, 5, 6);
  CHECK(!dispatcher.reserve_control_boundary(invalid_epoch));
  CHECK(persistence.reservations.empty());
  const auto reserved = control(20, 1, 2, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(reserved));
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).ok());
  CHECK(dispatcher.active_configuration_epoch() == 1);

  CHECK(dispatcher.dispatch(candidate(11, 2), consumer).failure ==
        dispatch::DispatchFailure::ControlBarrierBlocked);
  CHECK(dispatcher.current_run_input_sequence() == 1);
  CHECK(dispatcher.make_control_visible(reserved));
  consumer.next_disposition = dispatch::ConsumerDisposition::RetryableFailure;
  const auto pending = dispatcher.dispatch(candidate(11, 2), consumer);
  CHECK(!pending.ok());
  CHECK(pending.accepted_control_outcomes.empty());
  consumer.next_disposition = dispatch::ConsumerDisposition::Accepted;
  const auto selected = dispatcher.retry_pending(consumer);
  CHECK(selected.ok());
  if (!selected.selection.has_value())
    return;
  CHECK(selected.selection->active_configuration_epoch == 2);
  CHECK(selected.accepted_control_outcomes.size() == 1);
  if (selected.accepted_control_outcomes.empty())
    return;
  CHECK(selected.accepted_control_outcomes.front().reservation() == reserved);
  CHECK(selected.accepted_control_outcomes.front().selection_id() ==
        selected.selection->selection_id);
  CHECK(selected.accepted_control_outcomes.front()
            .selection_semantic_checksum() ==
        selected.selection->selection_semantic_checksum);
  CHECK(selected.selection->applied_controls ==
        std::vector<dispatch::ControlBoundaryReservation>{reserved});
  CHECK(dispatcher.active_configuration_epoch() == 2);
}

TEST_CASE("restart reconstructs an unexposed control barrier") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto reserved = control(20, 1, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(reserved));

  auto recovered =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  CHECK(recovered.dispatch(candidate(10, 1), consumer).failure ==
        dispatch::DispatchFailure::ControlBarrierBlocked);
  CHECK(recovered.make_control_visible(reserved));
  const auto selected = recovered.dispatch(candidate(10, 1), consumer);
  CHECK(selected.ok());
  if (selected.selection.has_value())
    CHECK(selected.selection->applied_controls ==
          std::vector<dispatch::ControlBoundaryReservation>{reserved});
}

TEST_CASE("multiple controls at one boundary use control history order") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto second = control(22, 2, 1, 2, 3);
  const auto first = control(21, 1, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(first));
  CHECK(dispatcher.reserve_control_boundary(second));
  CHECK(dispatcher.make_control_visible(second));
  CHECK(dispatcher.make_control_visible(first));
  const auto selected = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(selected.ok());
  if (!selected.selection.has_value())
    return;
  CHECK(selected.selection->active_configuration_epoch == 3);
  CHECK(selected.selection->applied_controls ==
        (std::vector<dispatch::ControlBoundaryReservation>{first, second}));
}

TEST_CASE("an applied control outcome cannot be reserved again") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto original = control(20, 1, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(original));
  CHECK(dispatcher.make_control_visible(original));
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).ok());

  const auto duplicate = control(20, 2, 2, 2, 3);
  CHECK(!dispatcher.reserve_control_boundary(duplicate));
  CHECK(dispatcher.current_control_cursor().last_consumed_sequence() == 1);
}

TEST_CASE("selection persistence failure advances no cursor or epoch") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto reserved = control(20, 1, 1, 1, 2);
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

TEST_CASE("restart recovers and publishes the exact persisted selection") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  persistence.blocked_transition =
      dispatch::PublicationState::PublicationInProgress;
  const auto uncertain = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(uncertain.failure ==
        dispatch::DispatchFailure::PublicationTransitionRejected);
  CHECK(uncertain.selection.has_value());
  CHECK(dispatcher.has_pending_publication());
  CHECK(dispatcher.current_run_input_sequence() == 1);
  CHECK(consumer.selections.empty());
  CHECK(dispatcher.dispatch(candidate(11, 2), consumer).failure ==
        dispatch::DispatchFailure::PublicationPending);

  persistence.blocked_transition.reset();
  auto recovered =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  RecordingConsumer wrong_consumer;
  wrong_consumer.configured_boundary_id = id<contracts::ConsumerBoundaryId>(7);
  CHECK(recovered.retry_pending(wrong_consumer).failure ==
        dispatch::DispatchFailure::ConsumerBoundaryMismatch);
  CHECK(wrong_consumer.selections.empty());
  const auto retried = recovered.retry_pending(consumer);
  CHECK(retried.ok());
  CHECK(retried.selection == uncertain.selection);
  CHECK(consumer.selections.size() == 1);
  CHECK(consumer.selections[0] == *uncertain.selection);
  CHECK(!recovered.has_pending_publication());
}

TEST_CASE("recovery rejects a control present as applied and pending") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  persistence.blocked_transition =
      dispatch::PublicationState::PublicationInProgress;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto applied = control(20, 1, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(applied));
  CHECK(dispatcher.make_control_visible(applied));
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).failure ==
        dispatch::DispatchFailure::PublicationTransitionRejected);

  auto duplicated = control(20, 1, 2, 2, 3);
  persistence.state->pending_controls.push_back(
      {.reservation = std::move(duplicated), .visible = true});
  CHECK(!dispatch::RunInputDispatcher::create(config(), persistence, registry)
             .has_value());
}

TEST_CASE("recovery rejects pending control order behind applied order") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto first = control(20, 1, 1, 1, 2);
  CHECK(dispatcher.reserve_control_boundary(first));
  CHECK(dispatcher.make_control_visible(first));
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).ok());

  const auto second = control(21, 2, 2, 2, 3);
  CHECK(dispatcher.reserve_control_boundary(second));
  CHECK(dispatcher.make_control_visible(second));
  persistence.blocked_transition =
      dispatch::PublicationState::PublicationInProgress;
  CHECK(dispatcher.dispatch(candidate(11, 2), consumer).failure ==
        dispatch::DispatchFailure::PublicationTransitionRejected);

  auto stale_pending = control(22, 1, 3, 3, 4);
  persistence.state->pending_controls.push_back(
      {.reservation = std::move(stale_pending), .visible = true});
  CHECK(!dispatch::RunInputDispatcher::create(config(), persistence, registry)
             .has_value());
}

TEST_CASE("recovery rejects a selection whose semantic identity changed") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  persistence.blocked_transition =
      dispatch::PublicationState::PublicationInProgress;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  CHECK(dispatcher.dispatch(candidate(10, 1), consumer).failure ==
        dispatch::DispatchFailure::PublicationTransitionRejected);
  persistence.state->pending_publication->selection.run_input_sequence = 2;
  CHECK(!dispatch::RunInputDispatcher::create(config(), persistence, registry)
             .has_value());
}

TEST_CASE("retryable publication uses a new deterministic attempt") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  consumer.next_disposition = dispatch::ConsumerDisposition::RetryableFailure;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();

  const auto first = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(first.failure == dispatch::DispatchFailure::ConsumerRetryableFailure);
  consumer.next_disposition = dispatch::ConsumerDisposition::Accepted;
  const auto second = dispatcher.retry_pending(consumer);
  CHECK(second.ok());
  CHECK(second.selection == first.selection);
  CHECK(consumer.attempts.size() == 2);
  if (consumer.attempts.size() == 2)
    CHECK(consumer.attempts[0] != consumer.attempts[1]);
}

TEST_CASE("accepted consumer acknowledgement resumes without republishing") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  persistence.blocked_transition = dispatch::PublicationState::ConsumerAccepted;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto uncertain = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(uncertain.failure ==
        dispatch::DispatchFailure::PublicationTransitionRejected);
  CHECK(consumer.selections.size() == 1);

  persistence.blocked_transition.reset();
  auto recovered =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();
  const auto accepted = recovered.retry_pending(consumer);
  CHECK(accepted.ok());
  CHECK(accepted.selection == uncertain.selection);
  CHECK(consumer.selections.size() == 1);
}

TEST_CASE("terminal publication failure cannot be retried") {
  RecordingPersistence persistence;
  TestRegistry registry;
  RecordingConsumer consumer;
  consumer.next_disposition = dispatch::ConsumerDisposition::TerminalFailure;
  auto dispatcher =
      dispatch::RunInputDispatcher::create(config(), persistence, registry)
          .value();

  const auto failed = dispatcher.dispatch(candidate(10, 1), consumer);
  CHECK(failed.failure == dispatch::DispatchFailure::ConsumerTerminalFailure);
  const auto retried = dispatcher.retry_pending(consumer);
  CHECK(retried.failure == dispatch::DispatchFailure::ConsumerTerminalFailure);
  CHECK(retried.selection == failed.selection);
  CHECK(consumer.selections.size() == 1);
  CHECK(consumer.attempts.size() == 1);
}
