#include "chronos/contracts/serialization.hpp"

#include "microtest.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace chronos::contracts;

namespace {
std::vector<std::uint8_t> fixture_bytes() {
  const auto path = std::string(CHRONOS_SOURCE_DIR) +
                    "/contracts/conformance/fixtures/m1-full-frame.hex";
  std::ifstream input(path);
  const std::string hex{std::istreambuf_iterator<char>(input),
                        std::istreambuf_iterator<char>()};
  std::vector<std::uint8_t> result;
  int high = -1;
  for (const char character : hex) {
    if (std::isspace(static_cast<unsigned char>(character)) != 0) {
      continue;
    }
    const int value =
        character >= '0' && character <= '9'   ? character - '0'
        : character >= 'a' && character <= 'f' ? character - 'a' + 10
        : character >= 'A' && character <= 'F' ? character - 'A' + 10
                                               : -1;
    if (value < 0) {
      return {};
    }
    if (high < 0) {
      high = value;
    } else {
      result.push_back(static_cast<std::uint8_t>((high << 4) | value));
      high = -1;
    }
  }
  return high < 0 ? result : std::vector<std::uint8_t>{};
}
} // namespace

TEST_CASE("C++ codec preserves the shared conformance fixture") {
  const auto bytes = fixture_bytes();
  CHECK(!bytes.empty());
  const auto frame = decode_conformance_frame(bytes);
  CHECK(frame.has_value());
  if (frame.has_value()) {
    CHECK(encode_conformance_frame(*frame) == bytes);
    CHECK(frame->decimal_scale.exponent() == 8);
    CHECK(frame->price.units() == 12'345);
    CHECK(frame->quantity.units() == 67);
    CHECK(frame->money.units() == -890);
    CHECK(frame->envelope.event_type() == "market.book.depth.snapshot_applied");
    CHECK(frame->envelope.run_input_sequence() == 9);
    CHECK(frame->envelope.state_lineage()->cursors().size() == 2);
  }
}

TEST_CASE("C++ codec rejects truncation and trailing bytes") {
  auto bytes = fixture_bytes();
  bytes.pop_back();
  CHECK(!decode_conformance_frame(bytes).has_value());
  bytes = fixture_bytes();
  bytes.push_back(0);
  CHECK(!decode_conformance_frame(bytes).has_value());
}
