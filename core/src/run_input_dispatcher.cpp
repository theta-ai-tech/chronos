#include "chronos/core/dispatch/run_input_dispatcher.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace chronos::core::dispatch {
namespace {

void append_u64(std::vector<std::byte> &output, std::uint64_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index)
    output.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
}

void append_string(std::vector<std::byte> &output, std::string_view value) {
  append_u64(output, static_cast<std::uint64_t>(value.size()));
  if (value.empty())
    return;
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  output.insert(output.end(), begin, begin + value.size());
}

template <typename Id>
void append_id(std::vector<std::byte> &output, const Id &value) {
  for (const auto byte : value.bytes())
    output.push_back(static_cast<std::byte>(byte));
}

void append_version(std::vector<std::byte> &output,
                    const contracts::VersionRef &value) {
  append_id(output, value.definition_id());
  append_u64(output, value.version());
}

void append_digest(std::vector<std::byte> &output,
                   const contracts::Sha256Digest &value) {
  for (const auto byte : value.bytes)
    output.push_back(static_cast<std::byte>(byte));
}

void append_cursor(std::vector<std::byte> &output,
                   const contracts::StreamCursor &value) {
  append_id(output, value.stream_id());
  append_u64(output, value.stream_epoch());
  append_u64(output, value.last_consumed_sequence().has_value() ? 1U : 0U);
  if (value.last_consumed_sequence().has_value())
    append_u64(output, *value.last_consumed_sequence());
}

contracts::Sha256Digest
selection_checksum(const RunInputDispatcherConfig &config,
                   std::uint64_t run_input_sequence,
                   const RunInputCandidate &candidate,
                   const contracts::StreamCursor &pre_cursor,
                   const contracts::StreamCursor &post_cursor,
                   const contracts::StreamCursor &control_cursor,
                   std::uint64_t configuration_epoch,
                   const std::vector<ControlBoundaryReservation> &controls) {
  std::vector<std::byte> canonical;
  append_string(canonical, "chronos-run-input-selection-v1");
  append_id(canonical, config.run_id);
  append_u64(canonical, run_input_sequence);
  append_id(canonical, candidate.event_id);
  append_string(canonical, candidate.event_type);
  append_id(canonical, candidate.event_position.stream_id());
  append_u64(canonical, candidate.event_position.stream_epoch());
  append_u64(canonical, candidate.event_position.stream_sequence());
  append_cursor(canonical, pre_cursor);
  append_cursor(canonical, post_cursor);
  append_cursor(canonical, control_cursor);
  append_u64(canonical, configuration_epoch);
  append_u64(canonical, static_cast<std::uint64_t>(controls.size()));
  for (const auto &control : controls) {
    append_id(canonical, control.run_id);
    append_id(canonical, control.control_stream_id);
    append_u64(canonical, control.control_stream_epoch);
    append_id(canonical, control.control_outcome_id);
    append_u64(canonical, control.control_sequence);
    append_u64(canonical, control.effective_position);
    append_u64(canonical, control.prior_configuration_epoch);
    append_u64(canonical, control.new_configuration_epoch);
    append_digest(canonical, control.behavior_checksum);
  }
  append_version(canonical, config.merge_policy_version);
  append_version(canonical, config.registry_snapshot_version);
  append_digest(canonical, candidate.semantic_checksum);
  return contracts::sha256(canonical);
}

contracts::PublicationAttemptId
publication_attempt_id(const contracts::Sha256Digest &selection_checksum,
                       std::uint64_t attempt_number) {
  std::vector<std::byte> canonical;
  append_string(canonical, "chronos-run-input-publication-attempt-v1");
  append_digest(canonical, selection_checksum);
  append_u64(canonical, attempt_number);
  const auto checksum = contracts::sha256(canonical);
  contracts::PublicationAttemptId::bytes_type bytes{};
  std::copy_n(checksum.bytes.begin(), bytes.size(), bytes.begin());
  if (std::all_of(bytes.begin(), bytes.end(),
                  [](std::uint8_t byte) { return byte == 0; }))
    bytes.back() = 1;
  return *contracts::PublicationAttemptId::from_bytes(bytes);
}

bool valid_control_reservation(const RunInputDispatcherConfig &config,
                               const ControlBoundaryReservation &reservation) {
  return reservation.run_id == config.run_id &&
         reservation.control_stream_id == config.control_stream_id &&
         reservation.control_stream_epoch == config.control_stream_epoch &&
         reservation.control_sequence != 0 &&
         reservation.effective_position != 0 &&
         reservation.prior_configuration_epoch != 0 &&
         reservation.prior_configuration_epoch !=
             std::numeric_limits<std::uint64_t>::max() &&
         reservation.new_configuration_epoch ==
             reservation.prior_configuration_epoch + 1 &&
         !reservation.behavior_payload.empty() &&
         reservation.behavior_payload.size() <=
             config.maximum_control_payload_bytes &&
         reservation.behavior_checksum ==
             contracts::sha256(reservation.behavior_payload);
}

contracts::RunInputSelectionId
selection_id(const contracts::Sha256Digest &checksum) {
  contracts::RunInputSelectionId::bytes_type bytes{};
  std::copy_n(checksum.bytes.begin(), bytes.size(), bytes.begin());
  if (std::all_of(bytes.begin(), bytes.end(),
                  [](std::uint8_t byte) { return byte == 0; }))
    bytes.back() = 1;
  return *contracts::RunInputSelectionId::from_bytes(bytes);
}

} // namespace

struct RunInputDispatcher::State final {
  struct ControlState final {
    ControlBoundaryReservation reservation;
    bool visible{};
  };

  struct PendingPublication final {
    RunInputSelectionRecord selection;
    RunInputCandidate candidate;
    PublicationState publication_state{PublicationState::NotPublished};
    std::uint64_t attempt_number{};
    std::optional<contracts::PublicationAttemptId> attempt_id;
  };

  State(RunInputDispatcherConfig initial_config,
        RunInputSelectionPersistence &initial_persistence,
        const RunInputEligibilityRegistry &initial_registry,
        contracts::StreamCursor initial_cursor,
        contracts::StreamCursor initial_control_cursor)
      : config(std::move(initial_config)), persistence(&initial_persistence),
        registry(&initial_registry), cursor(initial_cursor),
        control_cursor(initial_control_cursor),
        configuration_epoch(config.initial_configuration_epoch) {}

  bool
  commit_publication_state(PublicationState next,
                           std::optional<ConsumerDisposition> disposition) {
    if (!pending.has_value() || !pending->attempt_id.has_value())
      return false;
    const PublicationTransition transition{
        .selection_id = pending->selection.selection_id,
        .run_input_sequence = pending->selection.run_input_sequence,
        .attempt_id = *pending->attempt_id,
        .attempt_number = pending->attempt_number,
        .from = pending->publication_state,
        .to = next,
        .consumer_disposition = disposition,
    };
    if (!persistence->commit_publication_transition(transition))
      return false;
    pending->publication_state = next;
    return true;
  }

  DispatchResult publish(RunInputConsumer &consumer) {
    if (!pending.has_value())
      return {.failure = DispatchFailure::NoPendingPublication};
    if (pending->publication_state ==
        PublicationState::PublicationFailedTerminal) {
      return {.selection = pending->selection,
              .failure = DispatchFailure::ConsumerTerminalFailure};
    }
    if (pending->publication_state ==
        PublicationState::PublishedToConsumerBoundary) {
      if (!commit_publication_state(PublicationState::ConsumerAccepted,
                                    ConsumerDisposition::Accepted)) {
        return {.selection = pending->selection,
                .failure = DispatchFailure::PublicationTransitionRejected};
      }
      auto selection = pending->selection;
      pending.reset();
      return {.selection = std::move(selection)};
    }
    if (pending->publication_state == PublicationState::NotPublished ||
        pending->publication_state ==
            PublicationState::PublicationFailedRetryable) {
      if (pending->attempt_number == std::numeric_limits<std::uint64_t>::max())
        return {.selection = pending->selection,
                .failure = DispatchFailure::SequenceExhausted};
      ++pending->attempt_number;
      pending->attempt_id =
          publication_attempt_id(pending->selection.selection_semantic_checksum,
                                 pending->attempt_number);
      if (!commit_publication_state(PublicationState::PublicationInProgress,
                                    std::nullopt)) {
        pending->attempt_id.reset();
        --pending->attempt_number;
        return {.selection = pending->selection,
                .failure = DispatchFailure::PublicationTransitionRejected};
      }
    }
    if (pending->publication_state != PublicationState::PublicationInProgress ||
        !pending->attempt_id.has_value()) {
      return {.selection = pending->selection,
              .failure = DispatchFailure::RecoveryStateInvalid};
    }
    const auto disposition = consumer.accept(
        pending->selection, pending->candidate, *pending->attempt_id);
    if (disposition == ConsumerDisposition::RetryableFailure) {
      if (!commit_publication_state(
              PublicationState::PublicationFailedRetryable, disposition)) {
        return {.selection = pending->selection,
                .failure = DispatchFailure::PublicationTransitionRejected};
      }
      return {.selection = pending->selection,
              .failure = DispatchFailure::ConsumerRetryableFailure};
    }
    if (disposition == ConsumerDisposition::TerminalFailure) {
      if (!commit_publication_state(PublicationState::PublicationFailedTerminal,
                                    disposition)) {
        return {.selection = pending->selection,
                .failure = DispatchFailure::PublicationTransitionRejected};
      }
      return {.selection = pending->selection,
              .failure = DispatchFailure::ConsumerTerminalFailure};
    }
    if (!commit_publication_state(PublicationState::PublishedToConsumerBoundary,
                                  disposition) ||
        !commit_publication_state(PublicationState::ConsumerAccepted,
                                  disposition)) {
      return {.selection = pending->selection,
              .failure = DispatchFailure::PublicationTransitionRejected};
    }
    auto selection = pending->selection;
    pending.reset();
    return {.selection = std::move(selection)};
  }

  RunInputDispatcherConfig config;
  RunInputSelectionPersistence *persistence;
  const RunInputEligibilityRegistry *registry;
  contracts::StreamCursor cursor;
  contracts::StreamCursor control_cursor;
  std::uint64_t run_input_sequence{};
  std::uint64_t configuration_epoch{};
  std::vector<ControlState> controls;
  std::optional<PendingPublication> pending;
};

RunInputDispatcher::RunInputDispatcher(std::unique_ptr<State> state)
    : state_(std::move(state)) {}

RunInputDispatcher::RunInputDispatcher(RunInputDispatcher &&) noexcept =
    default;
RunInputDispatcher &
RunInputDispatcher::operator=(RunInputDispatcher &&) noexcept = default;
RunInputDispatcher::~RunInputDispatcher() = default;

std::optional<RunInputDispatcher>
RunInputDispatcher::create(RunInputDispatcherConfig config,
                           RunInputSelectionPersistence &persistence,
                           const RunInputEligibilityRegistry &registry) {
  if (config.input_stream_id == config.control_stream_id ||
      config.input_stream_epoch == 0 || config.control_stream_epoch == 0 ||
      config.initial_configuration_epoch == 0 ||
      config.maximum_payload_bytes == 0 ||
      config.maximum_control_payload_bytes == 0 ||
      config.maximum_pending_controls == 0)
    return std::nullopt;
  const auto initial_cursor =
      config.initial_stream_sequence.has_value()
          ? contracts::StreamCursor::at_sequence(
                config.input_stream_id, config.input_stream_epoch,
                *config.initial_stream_sequence)
          : contracts::StreamCursor::at_origin(config.input_stream_id,
                                               config.input_stream_epoch);
  const auto initial_control_cursor =
      config.initial_control_sequence.has_value()
          ? contracts::StreamCursor::at_sequence(
                config.control_stream_id, config.control_stream_epoch,
                *config.initial_control_sequence)
          : contracts::StreamCursor::at_origin(config.control_stream_id,
                                               config.control_stream_epoch);
  if (!initial_cursor.has_value() || !initial_control_cursor.has_value())
    return std::nullopt;
  const auto recovered = persistence.load_recovery_state(config);
  if (!recovered.success)
    return std::nullopt;
  auto state = std::make_unique<State>(
      config, persistence, registry, *initial_cursor, *initial_control_cursor);
  if (!recovered.state.has_value())
    return RunInputDispatcher(std::move(state));

  const auto &saved = *recovered.state;
  const auto cursor_matches = [](const contracts::StreamCursor &cursor,
                                 contracts::StreamId stream_id,
                                 std::uint64_t stream_epoch) {
    return cursor.stream_id() == stream_id &&
           cursor.stream_epoch() == stream_epoch;
  };
  const auto at_or_after = [](const contracts::StreamCursor &cursor,
                              const contracts::StreamCursor &origin) {
    if (!origin.last_consumed_sequence().has_value())
      return true;
    return cursor.last_consumed_sequence().has_value() &&
           *cursor.last_consumed_sequence() >= *origin.last_consumed_sequence();
  };
  const auto initial_input_sequence =
      initial_cursor->last_consumed_sequence().value_or(0);
  const auto saved_input_sequence =
      saved.input_cursor.last_consumed_sequence().value_or(0);
  if (!cursor_matches(saved.input_cursor, config.input_stream_id,
                      config.input_stream_epoch) ||
      !cursor_matches(saved.control_cursor, config.control_stream_id,
                      config.control_stream_epoch) ||
      !at_or_after(saved.input_cursor, *initial_cursor) ||
      !at_or_after(saved.control_cursor, *initial_control_cursor) ||
      saved_input_sequence - initial_input_sequence !=
          saved.run_input_sequence ||
      saved.configuration_epoch < config.initial_configuration_epoch ||
      saved.pending_controls.size() > config.maximum_pending_controls) {
    return std::nullopt;
  }
  std::vector<contracts::EventId> control_ids;
  std::vector<std::uint64_t> control_sequences;
  for (const auto &control : saved.pending_controls) {
    const auto &reservation = control.reservation;
    if (!valid_control_reservation(config, reservation) ||
        reservation.effective_position <= saved.run_input_sequence ||
        !saved.control_cursor.last_consumed_sequence().has_value() ||
        reservation.control_sequence >
            *saved.control_cursor.last_consumed_sequence() ||
        std::find(control_ids.begin(), control_ids.end(),
                  reservation.control_outcome_id) != control_ids.end() ||
        std::find(control_sequences.begin(), control_sequences.end(),
                  reservation.control_sequence) != control_sequences.end()) {
      return std::nullopt;
    }
    control_ids.push_back(reservation.control_outcome_id);
    control_sequences.push_back(reservation.control_sequence);
  }
  if (saved.pending_publication.has_value()) {
    const auto &publication = *saved.pending_publication;
    const auto &selection = publication.selection;
    const auto &candidate = publication.candidate;
    const bool attempt_expected =
        publication.state != PublicationState::NotPublished;
    const auto recomputed_checksum =
        selection.pre_selection_cursors.size() == 1 &&
                selection.post_selection_cursors.size() == 1
            ? selection_checksum(config, selection.run_input_sequence,
                                 candidate,
                                 selection.pre_selection_cursors.front(),
                                 selection.post_selection_cursors.front(),
                                 selection.control_cursor,
                                 selection.active_configuration_epoch,
                                 selection.applied_controls)
            : contracts::Sha256Digest{};
    bool applied_controls_valid = true;
    std::uint64_t expected_prior_epoch =
        selection.applied_controls.empty()
            ? selection.active_configuration_epoch
            : selection.applied_controls.front().prior_configuration_epoch;
    std::uint64_t prior_control_sequence{};
    for (const auto &control : selection.applied_controls) {
      if (!valid_control_reservation(config, control) ||
          control.effective_position != selection.run_input_sequence ||
          control.prior_configuration_epoch != expected_prior_epoch ||
          control.control_sequence <= prior_control_sequence) {
        applied_controls_valid = false;
        break;
      }
      expected_prior_epoch = control.new_configuration_epoch;
      prior_control_sequence = control.control_sequence;
    }
    applied_controls_valid =
        applied_controls_valid &&
        expected_prior_epoch == selection.active_configuration_epoch;
    const bool publication_state_valid =
        publication.state == PublicationState::NotPublished ||
        publication.state == PublicationState::PublicationInProgress ||
        publication.state == PublicationState::PublishedToConsumerBoundary ||
        publication.state == PublicationState::PublicationFailedRetryable ||
        publication.state == PublicationState::PublicationFailedTerminal;
    const bool selection_cursor_valid =
        selection.pre_selection_cursors.size() == 1 &&
        selection.post_selection_cursors.size() == 1 &&
        cursor_matches(selection.pre_selection_cursors.front(),
                       config.input_stream_id, config.input_stream_epoch) &&
        cursor_matches(selection.post_selection_cursors.front(),
                       config.input_stream_id, config.input_stream_epoch) &&
        cursor_matches(selection.control_cursor, config.control_stream_id,
                       config.control_stream_epoch) &&
        candidate.event_position.stream_id() == config.input_stream_id &&
        candidate.event_position.stream_epoch() == config.input_stream_epoch &&
        candidate.event_position.stream_sequence() ==
            selection.pre_selection_cursors.front()
                    .last_consumed_sequence()
                    .value_or(0) +
                1 &&
        selection.post_selection_cursors.front().last_consumed_sequence() ==
            candidate.event_position.stream_sequence();
    if (selection.run_id != config.run_id ||
        selection.run_input_sequence == 0 ||
        selection.run_input_sequence != saved.run_input_sequence ||
        !selection_cursor_valid || !applied_controls_valid ||
        selection.post_selection_cursors.front() != saved.input_cursor ||
        selection.control_cursor != saved.control_cursor ||
        selection.selected_event_id != candidate.event_id ||
        selection.selected_event_type != candidate.event_type ||
        selection.selected_event_position != candidate.event_position ||
        selection.input_semantic_checksum != candidate.semantic_checksum ||
        selection.active_configuration_epoch != saved.configuration_epoch ||
        selection.merge_policy_version != config.merge_policy_version ||
        selection.registry_snapshot_version !=
            config.registry_snapshot_version ||
        selection.initial_publication_state != PublicationState::NotPublished ||
        selection.selection_semantic_checksum != recomputed_checksum ||
        selection.selection_id != selection_id(recomputed_checksum) ||
        !contracts::is_valid_event_type(candidate.event_type) ||
        !registry.is_run_input_eligible(candidate.event_type,
                                        config.registry_snapshot_version) ||
        candidate.semantic_payload.empty() ||
        candidate.semantic_payload.size() > config.maximum_payload_bytes ||
        candidate.semantic_checksum !=
            contracts::sha256(candidate.semantic_payload) ||
        !publication_state_valid ||
        attempt_expected != publication.attempt_id.has_value() ||
        (attempt_expected && publication.attempt_number == 0) ||
        (attempt_expected &&
         publication.attempt_id !=
             publication_attempt_id(selection.selection_semantic_checksum,
                                    publication.attempt_number))) {
      return std::nullopt;
    }
  }
  state->cursor = saved.input_cursor;
  state->control_cursor = saved.control_cursor;
  state->run_input_sequence = saved.run_input_sequence;
  state->configuration_epoch = saved.configuration_epoch;
  for (const auto &control : saved.pending_controls) {
    state->controls.push_back(
        {.reservation = control.reservation, .visible = control.visible});
  }
  if (saved.pending_publication.has_value()) {
    state->pending = State::PendingPublication{
        .selection = saved.pending_publication->selection,
        .candidate = saved.pending_publication->candidate,
        .publication_state = saved.pending_publication->state,
        .attempt_number = saved.pending_publication->attempt_number,
        .attempt_id = saved.pending_publication->attempt_id,
    };
  }
  return RunInputDispatcher(std::move(state));
}

bool RunInputDispatcher::reserve_control_boundary(
    const ControlBoundaryReservation &reservation) {
  if (state_->control_cursor.last_consumed_sequence().has_value() &&
      *state_->control_cursor.last_consumed_sequence() ==
          std::numeric_limits<std::uint64_t>::max()) {
    return false;
  }
  const auto expected_control_sequence =
      state_->control_cursor.last_consumed_sequence().value_or(0) + 1;
  if (!valid_control_reservation(state_->config, reservation) ||
      reservation.control_sequence != expected_control_sequence ||
      reservation.effective_position <= state_->run_input_sequence ||
      state_->controls.size() >= state_->config.maximum_pending_controls ||
      std::any_of(state_->controls.begin(), state_->controls.end(),
                  [&](const State::ControlState &existing) {
                    return existing.reservation.control_outcome_id ==
                               reservation.control_outcome_id ||
                           existing.reservation.control_sequence ==
                               reservation.control_sequence;
                  })) {
    return false;
  }
  if (!state_->persistence->commit_control_reservation(reservation))
    return false;
  const auto next_control_cursor = contracts::StreamCursor::at_sequence(
      state_->config.control_stream_id, state_->config.control_stream_epoch,
      expected_control_sequence);
  if (!next_control_cursor.has_value())
    return false;
  state_->control_cursor = *next_control_cursor;
  state_->controls.push_back({.reservation = reservation});
  return true;
}

bool RunInputDispatcher::make_control_visible(
    const ControlBoundaryReservation &reservation) {
  const auto found =
      std::find_if(state_->controls.begin(), state_->controls.end(),
                   [&](const State::ControlState &existing) {
                     return existing.reservation.control_outcome_id ==
                            reservation.control_outcome_id;
                   });
  if (found == state_->controls.end() || found->visible ||
      found->reservation != reservation ||
      !state_->persistence->commit_control_visibility(reservation)) {
    return false;
  }
  found->visible = true;
  return true;
}

DispatchResult RunInputDispatcher::dispatch(RunInputCandidate candidate,
                                            RunInputConsumer &consumer) {
  if (state_->pending.has_value())
    return {.failure = DispatchFailure::PublicationPending};
  if (state_->run_input_sequence == std::numeric_limits<std::uint64_t>::max() ||
      (state_->cursor.last_consumed_sequence().has_value() &&
       *state_->cursor.last_consumed_sequence() ==
           std::numeric_limits<std::uint64_t>::max())) {
    return {.failure = DispatchFailure::SequenceExhausted};
  }
  if (!contracts::is_valid_event_type(candidate.event_type) ||
      candidate.semantic_payload.empty() ||
      candidate.semantic_payload.size() >
          state_->config.maximum_payload_bytes ||
      candidate.semantic_checksum !=
          contracts::sha256(candidate.semantic_payload)) {
    return {.failure = DispatchFailure::InvalidCandidate};
  }
  if (!state_->registry->is_run_input_eligible(
          candidate.event_type, state_->config.registry_snapshot_version))
    return {.failure = DispatchFailure::IneligibleCandidate};

  const auto expected_stream_sequence =
      state_->cursor.last_consumed_sequence().value_or(0) + 1;
  if (candidate.event_position.stream_id() != state_->config.input_stream_id ||
      candidate.event_position.stream_epoch() !=
          state_->config.input_stream_epoch ||
      candidate.event_position.stream_sequence() != expected_stream_sequence) {
    return {.failure = DispatchFailure::CursorMismatch};
  }

  const auto next_run_input_sequence = state_->run_input_sequence + 1;
  for (const auto &control : state_->controls) {
    if (control.reservation.effective_position < next_run_input_sequence)
      return {.failure = DispatchFailure::ControlStateInvalid};
    if (control.reservation.effective_position == next_run_input_sequence &&
        !control.visible)
      return {.failure = DispatchFailure::ControlBarrierBlocked};
  }

  std::vector<State::ControlState *> applying;
  for (auto &control : state_->controls) {
    if (control.reservation.effective_position == next_run_input_sequence)
      applying.push_back(&control);
  }
  std::sort(
      applying.begin(), applying.end(),
      [](const State::ControlState *left, const State::ControlState *right) {
        return left->reservation.control_sequence <
               right->reservation.control_sequence;
      });

  auto configuration_epoch = state_->configuration_epoch;
  std::vector<ControlBoundaryReservation> applied_controls;
  for (const auto *control : applying) {
    if (configuration_epoch == std::numeric_limits<std::uint64_t>::max() ||
        control->reservation.prior_configuration_epoch != configuration_epoch ||
        control->reservation.new_configuration_epoch !=
            configuration_epoch + 1) {
      return {.failure = DispatchFailure::ControlStateInvalid};
    }
    configuration_epoch = control->reservation.new_configuration_epoch;
    applied_controls.push_back(control->reservation);
  }

  const auto post_cursor = contracts::StreamCursor::at_sequence(
      state_->config.input_stream_id, state_->config.input_stream_epoch,
      expected_stream_sequence);
  if (!post_cursor.has_value())
    return {.failure = DispatchFailure::SequenceExhausted};
  const auto checksum =
      selection_checksum(state_->config, next_run_input_sequence, candidate,
                         state_->cursor, *post_cursor, state_->control_cursor,
                         configuration_epoch, applied_controls);
  RunInputSelectionRecord selection{
      .selection_id = selection_id(checksum),
      .run_id = state_->config.run_id,
      .run_input_sequence = next_run_input_sequence,
      .selected_event_id = candidate.event_id,
      .selected_event_type = candidate.event_type,
      .selected_event_position = candidate.event_position,
      .pre_selection_cursors = {state_->cursor},
      .post_selection_cursors = {*post_cursor},
      .control_cursor = state_->control_cursor,
      .applied_controls = std::move(applied_controls),
      .active_configuration_epoch = configuration_epoch,
      .merge_policy_version = state_->config.merge_policy_version,
      .registry_snapshot_version = state_->config.registry_snapshot_version,
      .input_semantic_checksum = candidate.semantic_checksum,
      .selection_semantic_checksum = checksum,
  };
  if (!state_->persistence->commit_selection(selection, candidate))
    return {.failure = DispatchFailure::SelectionPersistenceRejected};

  state_->cursor = *post_cursor;
  state_->run_input_sequence = next_run_input_sequence;
  state_->configuration_epoch = configuration_epoch;
  state_->controls.erase(
      std::remove_if(state_->controls.begin(), state_->controls.end(),
                     [&](const State::ControlState &control) {
                       return control.reservation.effective_position ==
                              next_run_input_sequence;
                     }),
      state_->controls.end());
  state_->pending = State::PendingPublication{
      .selection = std::move(selection), .candidate = std::move(candidate)};
  return state_->publish(consumer);
}

DispatchResult RunInputDispatcher::retry_pending(RunInputConsumer &consumer) {
  return state_->publish(consumer);
}

std::uint64_t RunInputDispatcher::current_run_input_sequence() const noexcept {
  return state_->run_input_sequence;
}

contracts::StreamCursor
RunInputDispatcher::current_input_cursor() const noexcept {
  return state_->cursor;
}

contracts::StreamCursor
RunInputDispatcher::current_control_cursor() const noexcept {
  return state_->control_cursor;
}

std::uint64_t RunInputDispatcher::active_configuration_epoch() const noexcept {
  return state_->configuration_epoch;
}

bool RunInputDispatcher::has_pending_publication() const noexcept {
  return state_->pending.has_value();
}

} // namespace chronos::core::dispatch
