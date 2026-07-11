#include "chronos/boundary.hpp"
#include "microtest.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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
  CHECK(output_len ==
        std::string_view("chronos.boundary.v0|hello_ack|hello event").size());
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
  CHECK(output_len == 0);
}

TEST_CASE("C ABI requires an output-length pointer") {
  CHECK(chronos_boundary_hello_event_round_trip("", 0, nullptr, 0, nullptr) ==
        CHRONOS_BOUNDARY_INVALID_ARGUMENT);
}

TEST_CASE("C ABI supports a null-output size query") {
  std::size_t output_len = 0;
  const int status =
      chronos_boundary_hello_event_round_trip("", 0, nullptr, 0, &output_len);

  CHECK(status == CHRONOS_BOUNDARY_OUTPUT_TOO_SMALL);
  CHECK(output_len ==
        std::string_view("chronos.boundary.v0|hello_ack|").size());
}

TEST_CASE("C ABI accepts an exact-capacity output buffer") {
  constexpr std::string_view expected =
      "chronos.boundary.v0|hello_ack|hello event";
  std::vector<char> output(expected.size());
  std::size_t output_len = 0;

  const int status = chronos_boundary_hello_event_round_trip(
      "hello event", 11, output.data(), output.size(), &output_len);

  CHECK(status == CHRONOS_BOUNDARY_OK);
  CHECK(std::string_view(output.data(), output_len) == expected);
}

TEST_CASE("C ABI preserves embedded null bytes") {
  constexpr std::array<char, 3> input{'a', '\0', 'b'};
  std::array<char, 64> output{};
  std::size_t output_len = 0;

  const int status = chronos_boundary_hello_event_round_trip(
      input.data(), input.size(), output.data(), output.size(), &output_len);
  const std::string expected{"chronos.boundary.v0|hello_ack|a\0b", 33};

  CHECK(status == CHRONOS_BOUNDARY_OK);
  CHECK(std::string_view(output.data(), output_len) == expected);
}

TEST_CASE("C ABI enforces the bounded hello-event contract") {
  std::string at_limit(chronos::kMaxHelloPayloadBytes, 'x');
  std::vector<char> output(at_limit.size() + 64);
  std::size_t output_len = 0;

  CHECK(chronos_boundary_max_hello_payload_bytes() ==
        chronos::kMaxHelloPayloadBytes);
  CHECK(chronos_boundary_hello_event_round_trip(
            at_limit.data(), at_limit.size(), output.data(), output.size(),
            &output_len) == CHRONOS_BOUNDARY_OK);

  output_len = 99;
  CHECK(chronos_boundary_hello_event_round_trip(
            at_limit.data(), at_limit.size() + 1, output.data(), output.size(),
            &output_len) == CHRONOS_BOUNDARY_INPUT_TOO_LARGE);
  CHECK(output_len == 0);
}
