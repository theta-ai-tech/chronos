#include "chronos/boundary.hpp"
#include "microtest.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

TEST_CASE("hello event round-trips through C++ boundary surface") {
  CHECK(chronos::round_trip_hello_event("hello event") ==
        "chronos.boundary.v0|hello_ack|hello event");
}

TEST_CASE("C ABI reports required output size when the buffer is too small") {
  std::array<char, 8> output{};
  std::size_t output_len = 0;

  const int status = chronos_boundary_hello_event_round_trip(
      "hello event", 11, output.data(), output.size(), &output_len);

  CHECK(status == CHRONOS_BOUNDARY_OUTPUT_TOO_SMALL);
  CHECK(output_len == std::string_view("chronos.boundary.v0|hello_ack|hello event").size());
}

TEST_CASE("C ABI writes the hello response bytes") {
  std::array<char, 64> output{};
  std::size_t output_len = 0;

  const int status = chronos_boundary_hello_event_round_trip(
      "hello event", 11, output.data(), output.size(), &output_len);
  const std::string_view response{output.data(), output_len};

  CHECK(status == CHRONOS_BOUNDARY_OK);
  CHECK(response == "chronos.boundary.v0|hello_ack|hello event");
}

TEST_CASE("C ABI rejects a non-empty null input") {
  std::array<char, 64> output{};
  std::size_t output_len = 0;

  const int status = chronos_boundary_hello_event_round_trip(
      nullptr, 1, output.data(), output.size(), &output_len);

  CHECK(status == CHRONOS_BOUNDARY_INVALID_ARGUMENT);
}
