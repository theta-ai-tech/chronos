#pragma once

#include "chronos/contracts/value_objects.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace chronos::contracts {

enum class LineageRelation : std::uint8_t {
  equal,
  precedes,
  succeeds,
  concurrent,
  incomparable,
};

class StateLineage final {
public:
  [[nodiscard]] static std::optional<StateLineage>
  from(RunId run_id, std::uint64_t run_input_sequence,
       std::span<const StreamId> required_streams,
       std::span<const StreamCursor> cursors) {
    if (required_streams.empty() || cursors.size() != required_streams.size()) {
      return std::nullopt;
    }

    std::vector<StreamId> sorted_required(required_streams.begin(),
                                          required_streams.end());
    std::sort(sorted_required.begin(), sorted_required.end());
    if (std::adjacent_find(sorted_required.begin(), sorted_required.end()) !=
        sorted_required.end()) {
      return std::nullopt;
    }

    std::vector<StreamCursor> sorted_cursors(cursors.begin(), cursors.end());
    std::sort(sorted_cursors.begin(), sorted_cursors.end(),
              [](const StreamCursor &left, const StreamCursor &right) {
                return left.stream_id() < right.stream_id();
              });
    for (std::size_t index = 0; index < sorted_cursors.size(); ++index) {
      if (sorted_cursors[index].stream_id() != sorted_required[index]) {
        return std::nullopt;
      }
    }

    return StateLineage(run_id, run_input_sequence, std::move(sorted_cursors));
  }

  [[nodiscard]] RunId run_id() const noexcept { return run_id_; }
  [[nodiscard]] std::uint64_t run_input_sequence() const noexcept {
    return run_input_sequence_;
  }
  [[nodiscard]] std::span<const StreamCursor> cursors() const noexcept {
    return cursors_;
  }

  [[nodiscard]] LineageRelation
  compare(const StateLineage &other) const noexcept {
    if (run_id_ != other.run_id_ || cursors_.size() != other.cursors_.size()) {
      return LineageRelation::incomparable;
    }

    bool less = false;
    bool greater = false;
    for (std::size_t index = 0; index < cursors_.size(); ++index) {
      const auto &left = cursors_[index];
      const auto &right = other.cursors_[index];
      if (left.stream_id() != right.stream_id() ||
          left.stream_epoch() != right.stream_epoch()) {
        return LineageRelation::incomparable;
      }

      const auto left_sequence = left.last_consumed_sequence();
      const auto right_sequence = right.last_consumed_sequence();
      if (left_sequence == right_sequence) {
        continue;
      }
      if (!left_sequence.has_value() ||
          (right_sequence.has_value() && *left_sequence < *right_sequence)) {
        less = true;
      } else {
        greater = true;
      }
    }

    if (less && greater) {
      return LineageRelation::concurrent;
    }
    if (less) {
      return LineageRelation::precedes;
    }
    if (greater) {
      return LineageRelation::succeeds;
    }
    return LineageRelation::equal;
  }

  bool operator==(const StateLineage &) const = default;

private:
  StateLineage(RunId run_id, std::uint64_t run_input_sequence,
               std::vector<StreamCursor> cursors)
      : run_id_(run_id), run_input_sequence_(run_input_sequence),
        cursors_(std::move(cursors)) {}

  RunId run_id_;
  std::uint64_t run_input_sequence_;
  std::vector<StreamCursor> cursors_;
};

} // namespace chronos::contracts
