#include "chronos/contracts/state_lineage.hpp"

#include "microtest.hpp"

#include <array>

using namespace chronos::contracts;

namespace {
RunId run_id(std::string_view value = "018f1f6e-7d3a-7c4b-8a91-0123456789a0") {
  return RunId::parse(value).value();
}

StreamId stream_id(char suffix) {
  auto value = std::string("018f1f6e-7d3a-7c4b-8a91-0123456789a0");
  value.back() = suffix;
  return StreamId::parse(value).value();
}

StreamCursor cursor(char suffix, std::optional<std::uint64_t> sequence,
                    std::uint64_t epoch = 1) {
  const auto id = stream_id(suffix);
  return sequence.has_value()
             ? StreamCursor::at_sequence(id, epoch, *sequence).value()
             : StreamCursor::at_origin(id, epoch).value();
}

StateLineage lineage(std::array<StreamCursor, 2> cursors,
                     std::uint64_t run_input_sequence = 1) {
  const std::array required{stream_id('1'), stream_id('2')};
  return StateLineage::from(run_id(), run_input_sequence, required, cursors)
      .value();
}
} // namespace

TEST_CASE("state lineage requires the complete registered cursor vector") {
  const std::array required{stream_id('1'), stream_id('2')};
  const std::array complete{cursor('2', 0), cursor('1', std::nullopt)};
  const std::array missing{cursor('1', 0)};
  const std::array duplicate_required{stream_id('1'), stream_id('1')};
  CHECK(StateLineage::from(run_id(), 4, required, complete).has_value());
  CHECK(!StateLineage::from(run_id(), 4, required, missing).has_value());
  CHECK(!StateLineage::from(run_id(), 4, duplicate_required, complete)
             .has_value());
}

TEST_CASE("state lineage implements vector-clock ordering") {
  const auto origin =
      lineage({cursor('1', std::nullopt), cursor('2', std::nullopt)});
  const auto first = lineage({cursor('1', 0), cursor('2', std::nullopt)}, 2);
  const auto later = lineage({cursor('1', 2), cursor('2', 3)}, 8);
  CHECK(origin.compare(first) == LineageRelation::precedes);
  CHECK(first.compare(origin) == LineageRelation::succeeds);
  CHECK(first.compare(first) == LineageRelation::equal);
  CHECK(first.compare(later) == LineageRelation::precedes);
}

TEST_CASE("concurrent cuts do not collapse to run-input scalar order") {
  const auto left = lineage({cursor('1', 5), cursor('2', 2)}, 10);
  const auto right = lineage({cursor('1', 3), cursor('2', 7)}, 11);
  CHECK(left.run_input_sequence() < right.run_input_sequence());
  CHECK(left.compare(right) == LineageRelation::concurrent);
  CHECK(right.compare(left) == LineageRelation::concurrent);
}

TEST_CASE("run-input position participates as one vector dimension") {
  const auto earlier = lineage({cursor('1', 2), cursor('2', 2)}, 10);
  const auto later_same_cursors = lineage({cursor('1', 2), cursor('2', 2)}, 11);
  const auto cursor_ahead_input_behind =
      lineage({cursor('1', 3), cursor('2', 3)}, 9);
  CHECK(earlier.compare(later_same_cursors) == LineageRelation::precedes);
  CHECK(later_same_cursors.compare(earlier) == LineageRelation::succeeds);
  CHECK(earlier.compare(cursor_ahead_input_behind) ==
        LineageRelation::concurrent);
}

TEST_CASE("different run, stream set, or epoch is incomparable") {
  const auto base = lineage({cursor('1', 1), cursor('2', 1)});
  const std::array required{stream_id('1'), stream_id('2')};
  const std::array same_cursors{cursor('1', 1), cursor('2', 1)};
  const auto other_run =
      StateLineage::from(run_id("018f1f6e-7d3a-7c4b-8a91-0123456789af"), 1,
                         required, same_cursors)
          .value();
  const auto other_epoch = lineage({cursor('1', 1, 2), cursor('2', 1)});
  CHECK(base.compare(other_run) == LineageRelation::incomparable);
  CHECK(base.compare(other_epoch) == LineageRelation::incomparable);
}
