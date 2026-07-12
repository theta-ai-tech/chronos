#pragma once

#include "chronos/contracts/state_lineage.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace chronos::contracts {

inline constexpr std::array<std::string_view, 22> kReservedEventNamespaces{
    "source",
    "reference",
    "market.book",
    "market.trade",
    "market.control",
    "external",
    "run.control",
    "run.timer",
    "run.input",
    "feature",
    "strategy",
    "recommendation",
    "opportunity",
    "portfolio",
    "risk",
    "execution.approval",
    "execution.intent",
    "execution.order",
    "execution.fill",
    "ledger",
    "reconciliation",
    "audit",
};

[[nodiscard]] inline std::optional<std::string_view>
event_namespace(std::string_view event_type) noexcept {
  for (const auto namespace_name : kReservedEventNamespaces) {
    if (event_type.size() > namespace_name.size() &&
        event_type.starts_with(namespace_name) &&
        event_type[namespace_name.size()] == '.') {
      return namespace_name;
    }
  }
  return std::nullopt;
}

[[nodiscard]] inline bool
is_valid_event_type(std::string_view event_type) noexcept {
  const auto namespace_name = event_namespace(event_type);
  if (!namespace_name.has_value() || event_type.back() == '.') {
    return false;
  }
  bool previous_dot = false;
  for (const char character : event_type) {
    const bool valid = (character >= 'a' && character <= 'z') ||
                       (character >= '0' && character <= '9') ||
                       character == '_' || character == '.';
    if (!valid || (character == '.' && previous_dot)) {
      return false;
    }
    previous_dot = character == '.';
  }
  const auto suffix = event_type.substr(namespace_name->size() + 1);
  return suffix.find('.') != std::string_view::npos;
}

using SubjectRef = std::variant<CanonicalInstrumentId, ListingId>;
using CausationRef = std::variant<CommandId, EventId, StateViewId, DecisionId>;

enum class AcceptanceClass : std::uint8_t {
  accepted_transition,
  accepted_observation,
  accepted_rejection,
  accepted_correction,
};

enum class RunMode : std::uint8_t {
  capture,
  replay,
  backtest,
  live_read_only,
  live_paper,
};

[[nodiscard]] constexpr bool is_valid(AcceptanceClass value) noexcept {
  return value >= AcceptanceClass::accepted_transition &&
         value <= AcceptanceClass::accepted_correction;
}

[[nodiscard]] constexpr bool is_valid(RunMode value) noexcept {
  return value >= RunMode::capture && value <= RunMode::live_paper;
}

class EventPosition final {
public:
  [[nodiscard]] static constexpr std::optional<EventPosition>
  from(StreamId stream_id, std::uint64_t stream_epoch,
       std::uint64_t stream_sequence) noexcept {
    if (stream_epoch == 0) {
      return std::nullopt;
    }
    return EventPosition(stream_id, stream_epoch, stream_sequence);
  }
  [[nodiscard]] constexpr StreamId stream_id() const noexcept {
    return stream_id_;
  }
  [[nodiscard]] constexpr std::uint64_t stream_epoch() const noexcept {
    return stream_epoch_;
  }
  [[nodiscard]] constexpr std::uint64_t stream_sequence() const noexcept {
    return stream_sequence_;
  }
  bool operator==(const EventPosition &) const = default;

private:
  constexpr EventPosition(StreamId stream_id, std::uint64_t stream_epoch,
                          std::uint64_t stream_sequence) noexcept
      : stream_id_(stream_id), stream_epoch_(stream_epoch),
        stream_sequence_(stream_sequence) {}
  StreamId stream_id_;
  std::uint64_t stream_epoch_;
  std::uint64_t stream_sequence_;
};

struct ProducerRef final {
  ProducerId component_id;
  VersionRef implementation_version;
  RuntimeId runtime_incarnation_id;
  bool operator==(const ProducerRef &) const = default;
};

enum class Applicability : std::uint8_t { forbidden, optional, required };

enum class EffectivePositionPolicy : std::uint8_t {
  forbidden,
  optional,
  required,
  accepted_transition_only,
};

struct EventTypeRegistration final {
  std::string event_type;
  AuthorityId semantic_owner;
  std::uint32_t envelope_version;
  VersionRef schema_version;
  std::vector<ProducerId> authorized_producers;
  bool root_observation;
  Applicability run_scope;
  Applicability event_position;
  bool run_input_eligible;
  Applicability source_event;
  Applicability subjects;
  Applicability mode;
  EffectivePositionPolicy effective_position;
  Applicability integrity;
  Applicability receive_time;
};

struct EventEnvelopeDraft final {
  EventId event_id;
  std::string event_type;
  std::uint32_t envelope_version;
  VersionRef schema_version;
  AuthorityId semantic_owner;
  ProducerRef producer;
  AcceptanceClass acceptance_class;
  std::optional<RunId> run_id;
  std::optional<RunMode> mode;
  std::optional<EventPosition> event_position;
  std::optional<std::uint64_t> run_input_sequence;
  std::optional<std::uint64_t> effective_position;
  std::optional<StateLineage> state_lineage;
  std::optional<SourceEventId> source_event_id;
  std::optional<std::vector<CausationRef>> causation_refs;
  std::optional<std::vector<CorrelationId>> correlation_refs;
  std::optional<std::vector<SubjectRef>> subject_refs;
  std::optional<TimePoint> source_event_time;
  std::optional<TimePoint> chronos_receive_time;
  TimePoint accept_time;
  std::optional<TimePoint> recoverability_handoff_time;
  std::optional<TimePoint> record_time;
  DataQuality quality;
  std::vector<std::uint8_t> payload;
  std::optional<IntegrityId> integrity;

  bool operator==(const EventEnvelopeDraft &) const = default;
};

class EventEnvelope final {
public:
  [[nodiscard]] static std::optional<EventEnvelope>
  from(const EventTypeRegistration &registration, EventEnvelopeDraft draft) {
    if (!is_valid_event_type(registration.event_type) ||
        draft.event_type != registration.event_type ||
        draft.semantic_owner != registration.semantic_owner ||
        draft.envelope_version != registration.envelope_version ||
        draft.schema_version != registration.schema_version ||
        std::find(registration.authorized_producers.begin(),
                  registration.authorized_producers.end(),
                  draft.producer.component_id) ==
            registration.authorized_producers.end() ||
        !is_valid(draft.acceptance_class) ||
        (draft.mode.has_value() && !is_valid(*draft.mode))) {
      return std::nullopt;
    }
    if (!valid_registration(registration) ||
        !valid_presence(registration.run_scope, draft.run_id.has_value()) ||
        !valid_presence(registration.event_position,
                        draft.event_position.has_value()) ||
        !valid_presence(registration.source_event,
                        draft.source_event_id.has_value()) ||
        !valid_presence(registration.subjects,
                        draft.subject_refs.has_value()) ||
        !valid_presence(registration.mode, draft.mode.has_value()) ||
        !valid_effective_position(registration.effective_position,
                                  draft.acceptance_class,
                                  draft.effective_position.has_value()) ||
        !valid_presence(registration.integrity, draft.integrity.has_value()) ||
        !valid_presence(registration.receive_time,
                        draft.chronos_receive_time.has_value())) {
      return std::nullopt;
    }
    if (!registration.root_observation && !draft.causation_refs.has_value()) {
      return std::nullopt;
    }
    if (draft.run_input_sequence.has_value() &&
        (!registration.run_input_eligible || !draft.run_id.has_value() ||
         !draft.event_position.has_value())) {
      return std::nullopt;
    }
    if (draft.effective_position.has_value() && !draft.run_id.has_value()) {
      return std::nullopt;
    }
    if (draft.state_lineage.has_value() &&
        (!draft.run_id.has_value() ||
         draft.state_lineage->run_id() != *draft.run_id)) {
      return std::nullopt;
    }
    if (draft.run_input_sequence.has_value() &&
        draft.state_lineage.has_value() &&
        draft.state_lineage->run_input_sequence() !=
            *draft.run_input_sequence) {
      return std::nullopt;
    }
    if (!validate_optional_refs(draft.event_id, draft.causation_refs) ||
        !validate_optional_ids(draft.correlation_refs) ||
        !validate_optional_subjects(draft.subject_refs)) {
      return std::nullopt;
    }
    return EventEnvelope(std::move(draft));
  }

  [[nodiscard]] const EventId &event_id() const noexcept {
    return draft_.event_id;
  }
  [[nodiscard]] std::string_view event_type() const noexcept {
    return draft_.event_type;
  }
  [[nodiscard]] std::uint32_t envelope_version() const noexcept {
    return draft_.envelope_version;
  }
  [[nodiscard]] const VersionRef &schema_version() const noexcept {
    return draft_.schema_version;
  }
  [[nodiscard]] const AuthorityId &semantic_owner() const noexcept {
    return draft_.semantic_owner;
  }
  [[nodiscard]] const ProducerRef &producer() const noexcept {
    return draft_.producer;
  }
  [[nodiscard]] AcceptanceClass acceptance_class() const noexcept {
    return draft_.acceptance_class;
  }
  [[nodiscard]] const std::optional<RunId> &run_id() const noexcept {
    return draft_.run_id;
  }
  [[nodiscard]] const std::optional<RunMode> &mode() const noexcept {
    return draft_.mode;
  }
  [[nodiscard]] const std::optional<EventPosition> &
  event_position() const noexcept {
    return draft_.event_position;
  }
  [[nodiscard]] const std::optional<std::uint64_t> &
  run_input_sequence() const noexcept {
    return draft_.run_input_sequence;
  }
  [[nodiscard]] const std::optional<std::uint64_t> &
  effective_position() const noexcept {
    return draft_.effective_position;
  }
  [[nodiscard]] const std::optional<StateLineage> &
  state_lineage() const noexcept {
    return draft_.state_lineage;
  }
  [[nodiscard]] const std::optional<SourceEventId> &
  source_event_id() const noexcept {
    return draft_.source_event_id;
  }
  [[nodiscard]] const std::optional<std::vector<CausationRef>> &
  causation_refs() const noexcept {
    return draft_.causation_refs;
  }
  [[nodiscard]] const std::optional<std::vector<CorrelationId>> &
  correlation_refs() const noexcept {
    return draft_.correlation_refs;
  }
  [[nodiscard]] const std::optional<std::vector<SubjectRef>> &
  subject_refs() const noexcept {
    return draft_.subject_refs;
  }
  [[nodiscard]] const std::optional<TimePoint> &
  source_event_time() const noexcept {
    return draft_.source_event_time;
  }
  [[nodiscard]] const std::optional<TimePoint> &
  chronos_receive_time() const noexcept {
    return draft_.chronos_receive_time;
  }
  [[nodiscard]] const TimePoint &accept_time() const noexcept {
    return draft_.accept_time;
  }
  [[nodiscard]] const std::optional<TimePoint> &
  recoverability_handoff_time() const noexcept {
    return draft_.recoverability_handoff_time;
  }
  [[nodiscard]] const std::optional<TimePoint> &record_time() const noexcept {
    return draft_.record_time;
  }
  [[nodiscard]] const DataQuality &quality() const noexcept {
    return draft_.quality;
  }
  [[nodiscard]] const std::vector<std::uint8_t> &payload() const noexcept {
    return draft_.payload;
  }
  [[nodiscard]] const std::optional<IntegrityId> &integrity() const noexcept {
    return draft_.integrity;
  }

  bool operator==(const EventEnvelope &) const = default;

private:
  explicit EventEnvelope(EventEnvelopeDraft draft) : draft_(std::move(draft)) {}

  [[nodiscard]] static bool
  valid_registration(const EventTypeRegistration &registration) noexcept {
    if (registration.envelope_version == 0 ||
        registration.authorized_producers.empty()) {
      return false;
    }
    auto producers = registration.authorized_producers;
    std::sort(producers.begin(), producers.end());
    return std::adjacent_find(producers.begin(), producers.end()) ==
           producers.end();
  }

  [[nodiscard]] static constexpr bool
  valid_presence(Applicability applicability, bool present) noexcept {
    switch (applicability) {
    case Applicability::forbidden:
      return !present;
    case Applicability::optional:
      return true;
    case Applicability::required:
      return present;
    }
    return false;
  }

  [[nodiscard]] static constexpr bool
  valid_effective_position(EffectivePositionPolicy policy,
                           AcceptanceClass acceptance_class,
                           bool present) noexcept {
    switch (policy) {
    case EffectivePositionPolicy::forbidden:
      return !present;
    case EffectivePositionPolicy::optional:
      return true;
    case EffectivePositionPolicy::required:
      return present;
    case EffectivePositionPolicy::accepted_transition_only:
      return present ==
             (acceptance_class == AcceptanceClass::accepted_transition);
    }
    return false;
  }

  [[nodiscard]] static bool validate_optional_refs(
      EventId event_id,
      const std::optional<std::vector<CausationRef>> &references) noexcept {
    if (!references.has_value()) {
      return true;
    }
    if (references->empty()) {
      return false;
    }
    std::vector<CausationRef> sorted = *references;
    std::sort(sorted.begin(), sorted.end());
    const CausationRef self = event_id;
    return !std::binary_search(sorted.begin(), sorted.end(), self) &&
           std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();
  }

  [[nodiscard]] static bool validate_optional_subjects(
      const std::optional<std::vector<SubjectRef>> &subjects) noexcept {
    if (!subjects.has_value()) {
      return true;
    }
    if (subjects->empty()) {
      return false;
    }
    for (std::size_t left = 0; left < subjects->size(); ++left) {
      for (std::size_t right = left + 1; right < subjects->size(); ++right) {
        if ((*subjects)[left] == (*subjects)[right]) {
          return false;
        }
      }
    }
    return true;
  }

  template <typename Id>
  [[nodiscard]] static bool validate_optional_ids(
      const std::optional<std::vector<Id>> &identities) noexcept {
    if (!identities.has_value()) {
      return true;
    }
    if (identities->empty()) {
      return false;
    }
    auto sorted = *identities;
    std::sort(sorted.begin(), sorted.end());
    return std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();
  }

  EventEnvelopeDraft draft_;
};

} // namespace chronos::contracts
