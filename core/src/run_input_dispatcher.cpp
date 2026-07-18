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
                   std::uint64_t configuration_epoch,
                   const std::vector<contracts::EventId> &controls) {
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
  append_u64(canonical, configuration_epoch);
  append_u64(canonical, static_cast<std::uint64_t>(controls.size()));
  for (const auto &control : controls)
    append_id(canonical, control);
  append_version(canonical, config.merge_policy_version);
  append_version(canonical, config.registry_snapshot_version);
  append_digest(canonical, candidate.semantic_checksum);
  return contracts::sha256(canonical);
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
  };

  State(RunInputDispatcherConfig initial_config,
        RunInputSelectionPersistence &initial_persistence,
        const RunInputEligibilityRegistry &initial_registry,
        contracts::StreamCursor initial_cursor)
      : config(std::move(initial_config)), persistence(&initial_persistence),
        registry(&initial_registry), cursor(initial_cursor),
        configuration_epoch(config.initial_configuration_epoch) {}

  DispatchResult publish(RunInputConsumer &consumer) {
    if (!pending.has_value())
      return {.failure = DispatchFailure::NoPendingPublication};
    const auto disposition =
        consumer.accept(pending->selection, pending->candidate);
    if (disposition == ConsumerDisposition::Rejected)
      return {.selection = pending->selection,
              .failure = DispatchFailure::ConsumerRejected};
    if (!persistence->commit_consumer_acceptance(
            pending->selection.selection_id,
            pending->selection.run_input_sequence)) {
      return {.selection = pending->selection,
              .failure = DispatchFailure::PublicationUnconfirmed};
    }
    auto selection = pending->selection;
    pending.reset();
    return {.selection = std::move(selection)};
  }

  RunInputDispatcherConfig config;
  RunInputSelectionPersistence *persistence;
  const RunInputEligibilityRegistry *registry;
  contracts::StreamCursor cursor;
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
  if (config.input_stream_epoch == 0 ||
      config.initial_configuration_epoch == 0 ||
      config.maximum_payload_bytes == 0 || config.maximum_pending_controls == 0)
    return std::nullopt;
  const auto cursor =
      config.initial_stream_sequence.has_value()
          ? contracts::StreamCursor::at_sequence(
                config.input_stream_id, config.input_stream_epoch,
                *config.initial_stream_sequence)
          : contracts::StreamCursor::at_origin(config.input_stream_id,
                                               config.input_stream_epoch);
  if (!cursor.has_value())
    return std::nullopt;
  return RunInputDispatcher(std::make_unique<State>(
      std::move(config), persistence, registry, std::move(*cursor)));
}

bool RunInputDispatcher::reserve_control_boundary(
    const ControlBoundaryReservation &reservation) {
  if (reservation.control_sequence == 0 ||
      reservation.effective_position <= state_->run_input_sequence ||
      reservation.new_configuration_epoch <= state_->configuration_epoch ||
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
  std::vector<contracts::EventId> applied_controls;
  for (const auto *control : applying) {
    if (configuration_epoch == std::numeric_limits<std::uint64_t>::max() ||
        control->reservation.new_configuration_epoch !=
            configuration_epoch + 1) {
      return {.failure = DispatchFailure::ControlStateInvalid};
    }
    configuration_epoch = control->reservation.new_configuration_epoch;
    applied_controls.push_back(control->reservation.control_outcome_id);
  }

  const auto post_cursor = contracts::StreamCursor::at_sequence(
      state_->config.input_stream_id, state_->config.input_stream_epoch,
      expected_stream_sequence);
  if (!post_cursor.has_value())
    return {.failure = DispatchFailure::SequenceExhausted};
  const auto checksum = selection_checksum(
      state_->config, next_run_input_sequence, candidate, state_->cursor,
      *post_cursor, configuration_epoch, applied_controls);
  RunInputSelectionRecord selection{
      .selection_id = selection_id(checksum),
      .run_id = state_->config.run_id,
      .run_input_sequence = next_run_input_sequence,
      .selected_event_id = candidate.event_id,
      .selected_event_type = candidate.event_type,
      .selected_event_position = candidate.event_position,
      .pre_selection_cursors = {state_->cursor},
      .post_selection_cursors = {*post_cursor},
      .applied_control_outcome_ids = std::move(applied_controls),
      .active_configuration_epoch = configuration_epoch,
      .merge_policy_version = state_->config.merge_policy_version,
      .registry_snapshot_version = state_->config.registry_snapshot_version,
      .input_semantic_checksum = candidate.semantic_checksum,
      .selection_semantic_checksum = checksum,
  };
  if (!state_->persistence->commit_selection(selection))
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

std::uint64_t RunInputDispatcher::active_configuration_epoch() const noexcept {
  return state_->configuration_epoch;
}

bool RunInputDispatcher::has_pending_publication() const noexcept {
  return state_->pending.has_value();
}

} // namespace chronos::core::dispatch
