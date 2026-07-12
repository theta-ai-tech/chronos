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

inline constexpr std::array<std::string_view, 20> kReservedEventNamespaces{
    "source",
    "reference",
    "market.book",
    "market.trade",
    "market.control",
    "external",
    "run.control",
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
  if (!event_namespace(event_type).has_value() || event_type.back() == '.') {
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
  return true;
}

using SubjectRef = std::variant<CanonicalInstrumentId, ListingId>;
using CausationRef = std::variant<CommandId, EventId, StateViewId, DecisionId>;

struct EventEnvelopeDraft final {
  EventId event_id;
  std::string event_type;
  std::uint32_t envelope_version;
  VersionRef schema_version;
  ProducerId producer_id;
  VersionRef producer_version;
  std::optional<RunId> run_id;
  std::optional<StreamCursor> stream_cursor;
  std::optional<std::uint64_t> run_input_sequence;
  std::optional<StateLineage> state_lineage;
  std::optional<SourceEventId> source_event_id;
  std::optional<std::vector<CausationRef>> causation_refs;
  std::optional<std::vector<SubjectRef>> subject_refs;
  std::optional<TimePoint> source_event_time;
  std::optional<TimePoint> chronos_receive_time;
  TimePoint accept_time;
  std::optional<TimePoint> record_time;
  DataQuality quality;
  std::vector<std::uint8_t> payload;

  bool operator==(const EventEnvelopeDraft &) const = default;
};

class EventEnvelope final {
public:
  [[nodiscard]] static std::optional<EventEnvelope>
  from(EventEnvelopeDraft draft) {
    if (!is_valid_event_type(draft.event_type) || draft.envelope_version == 0) {
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
  [[nodiscard]] const ProducerId &producer_id() const noexcept {
    return draft_.producer_id;
  }
  [[nodiscard]] const VersionRef &producer_version() const noexcept {
    return draft_.producer_version;
  }
  [[nodiscard]] const std::optional<RunId> &run_id() const noexcept {
    return draft_.run_id;
  }
  [[nodiscard]] const std::optional<StreamCursor> &
  stream_cursor() const noexcept {
    return draft_.stream_cursor;
  }
  [[nodiscard]] const std::optional<std::uint64_t> &
  run_input_sequence() const noexcept {
    return draft_.run_input_sequence;
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
  [[nodiscard]] const std::optional<TimePoint> &record_time() const noexcept {
    return draft_.record_time;
  }
  [[nodiscard]] const DataQuality &quality() const noexcept {
    return draft_.quality;
  }
  [[nodiscard]] const std::vector<std::uint8_t> &payload() const noexcept {
    return draft_.payload;
  }

  bool operator==(const EventEnvelope &) const = default;

private:
  explicit EventEnvelope(EventEnvelopeDraft draft) : draft_(std::move(draft)) {}

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

  EventEnvelopeDraft draft_;
};

} // namespace chronos::contracts
