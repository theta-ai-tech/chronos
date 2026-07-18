#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/event_envelope.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::core::dispatch {

enum class PublicationState : std::uint8_t {
  NotPublished,
  PublicationInProgress,
  PublishedToConsumerBoundary,
  ConsumerAccepted,
  PublicationFailedRetryable,
  PublicationFailedTerminal,
};

enum class DispatchFailure : std::uint8_t {
  None,
  RecoveryStateInvalid,
  InvalidCandidate,
  IneligibleCandidate,
  CursorMismatch,
  ControlBarrierBlocked,
  ControlStateInvalid,
  SelectionPersistenceRejected,
  PublicationTransitionRejected,
  ConsumerRetryableFailure,
  ConsumerTerminalFailure,
  ConsumerBoundaryMismatch,
  PublicationPending,
  NoPendingPublication,
  SequenceExhausted,
};

enum class ConsumerDisposition : std::uint8_t {
  Accepted,
  AlreadyAccepted,
  RetryableFailure,
  TerminalFailure,
};

struct RunInputDispatcherConfig final {
  contracts::RunId run_id;
  contracts::StreamId input_stream_id;
  std::uint64_t input_stream_epoch{};
  std::optional<std::uint64_t> initial_stream_sequence;
  contracts::StreamId control_stream_id;
  std::uint64_t control_stream_epoch{};
  std::optional<std::uint64_t> initial_control_sequence;
  contracts::ConsumerBoundaryId consumer_boundary_id;
  contracts::VersionRef merge_policy_version;
  contracts::VersionRef registry_snapshot_version;
  std::uint64_t initial_configuration_epoch{};
  std::size_t maximum_payload_bytes{1U << 20U};
  std::size_t maximum_control_payload_bytes{1U << 16U};
  std::size_t maximum_pending_controls{1024};
};

struct RunInputCandidate final {
  contracts::EventId event_id;
  std::string event_type;
  contracts::EventPosition event_position;
  std::vector<std::byte> semantic_payload;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const RunInputCandidate &) const = default;
};

struct ControlBoundaryReservation final {
  contracts::RunId run_id;
  contracts::StreamId control_stream_id;
  std::uint64_t control_stream_epoch{};
  contracts::EventId control_outcome_id;
  std::uint64_t control_sequence{};
  std::uint64_t effective_position{};
  std::uint64_t prior_configuration_epoch{};
  std::uint64_t new_configuration_epoch{};
  std::vector<std::byte> behavior_payload;
  contracts::Sha256Digest behavior_checksum;

  bool operator==(const ControlBoundaryReservation &) const = default;
};

struct RunInputSelectionRecord final {
  contracts::RunInputSelectionId selection_id;
  contracts::RunId run_id;
  std::uint64_t run_input_sequence{};
  contracts::EventId selected_event_id;
  std::string selected_event_type;
  contracts::EventPosition selected_event_position;
  std::vector<contracts::StreamCursor> pre_selection_cursors;
  std::vector<contracts::StreamCursor> post_selection_cursors;
  contracts::StreamCursor control_cursor;
  std::vector<ControlBoundaryReservation> applied_controls;
  contracts::ConsumerBoundaryId consumer_boundary_id;
  std::uint64_t active_configuration_epoch{};
  contracts::VersionRef merge_policy_version;
  contracts::VersionRef registry_snapshot_version;
  contracts::Sha256Digest input_semantic_checksum;
  contracts::Sha256Digest selection_semantic_checksum;
  PublicationState initial_publication_state{PublicationState::NotPublished};

  bool operator==(const RunInputSelectionRecord &) const = default;
};

struct PublicationTransition final {
  contracts::RunInputSelectionId selection_id;
  std::uint64_t run_input_sequence{};
  contracts::PublicationAttemptId attempt_id;
  contracts::ConsumerBoundaryId consumer_boundary_id;
  std::uint64_t attempt_number{};
  PublicationState from{PublicationState::NotPublished};
  PublicationState to{PublicationState::PublicationInProgress};
  std::optional<ConsumerDisposition> consumer_disposition;

  bool operator==(const PublicationTransition &) const = default;
};

struct RecoverableControl final {
  ControlBoundaryReservation reservation;
  bool visible{};

  bool operator==(const RecoverableControl &) const = default;
};

struct RecoverablePublication final {
  RunInputSelectionRecord selection;
  RunInputCandidate candidate;
  PublicationState state{PublicationState::NotPublished};
  std::uint64_t attempt_number{};
  std::optional<contracts::PublicationAttemptId> attempt_id;

  bool operator==(const RecoverablePublication &) const = default;
};

struct RunInputRecoveryState final {
  contracts::StreamCursor input_cursor;
  contracts::StreamCursor control_cursor;
  std::uint64_t run_input_sequence{};
  std::uint64_t configuration_epoch{};
  std::vector<RecoverableControl> pending_controls;
  std::optional<RecoverablePublication> pending_publication;

  bool operator==(const RunInputRecoveryState &) const = default;
};

struct RunInputRecoveryLoad final {
  bool success{};
  std::optional<RunInputRecoveryState> state;
};

[[nodiscard]] contracts::Sha256Digest
derive_run_input_selection_checksum(const RunInputDispatcherConfig &config,
                                    const RunInputSelectionRecord &selection,
                                    const RunInputCandidate &candidate);

[[nodiscard]] contracts::RunInputSelectionId derive_run_input_selection_id(
    const contracts::Sha256Digest &selection_checksum);

[[nodiscard]] bool
validate_run_input_selection(const RunInputDispatcherConfig &config,
                             const RunInputSelectionRecord &selection,
                             const RunInputCandidate &candidate);

class RunInputSelectionPersistence {
public:
  virtual ~RunInputSelectionPersistence() = default;
  [[nodiscard]] virtual RunInputRecoveryLoad
  load_recovery_state(const RunInputDispatcherConfig &config) = 0;
  [[nodiscard]] virtual bool
  commit_control_reservation(const ControlBoundaryReservation &reservation) = 0;
  [[nodiscard]] virtual bool
  commit_control_visibility(const ControlBoundaryReservation &reservation) = 0;
  [[nodiscard]] virtual bool
  commit_selection(const RunInputSelectionRecord &record,
                   const RunInputCandidate &candidate) = 0;
  [[nodiscard]] virtual bool
  commit_publication_transition(const PublicationTransition &transition) = 0;
};

class RunInputEligibilityRegistry {
public:
  virtual ~RunInputEligibilityRegistry() = default;
  [[nodiscard]] virtual bool is_run_input_eligible(
      std::string_view event_type,
      contracts::VersionRef registry_snapshot_version) const = 0;
};

class RunInputConsumer {
public:
  virtual ~RunInputConsumer() = default;
  [[nodiscard]] virtual contracts::ConsumerBoundaryId
  boundary_id() const noexcept = 0;
  [[nodiscard]] virtual ConsumerDisposition
  accept(const RunInputSelectionRecord &selection,
         const RunInputCandidate &candidate,
         contracts::PublicationAttemptId attempt_id) = 0;
};

struct DispatchResult final {
  std::optional<RunInputSelectionRecord> selection;
  DispatchFailure failure{DispatchFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return selection.has_value() && failure == DispatchFailure::None;
  }
};

class RunInputDispatcher final {
public:
  [[nodiscard]] static std::optional<RunInputDispatcher>
  create(RunInputDispatcherConfig config,
         RunInputSelectionPersistence &persistence,
         const RunInputEligibilityRegistry &registry);

  RunInputDispatcher(RunInputDispatcher &&) noexcept;
  RunInputDispatcher &operator=(RunInputDispatcher &&) noexcept;
  ~RunInputDispatcher();

  [[nodiscard]] bool
  reserve_control_boundary(const ControlBoundaryReservation &reservation);
  [[nodiscard]] bool
  make_control_visible(const ControlBoundaryReservation &reservation);

  [[nodiscard]] DispatchResult dispatch(RunInputCandidate candidate,
                                        RunInputConsumer &consumer);
  [[nodiscard]] DispatchResult retry_pending(RunInputConsumer &consumer);

  [[nodiscard]] std::uint64_t current_run_input_sequence() const noexcept;
  [[nodiscard]] contracts::StreamCursor current_input_cursor() const noexcept;
  [[nodiscard]] contracts::StreamCursor current_control_cursor() const noexcept;
  [[nodiscard]] std::uint64_t active_configuration_epoch() const noexcept;
  [[nodiscard]] bool has_pending_publication() const noexcept;

private:
  struct State;
  explicit RunInputDispatcher(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

} // namespace chronos::core::dispatch
