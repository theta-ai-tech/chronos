#include "chronos/version.hpp"
#include "microtest.hpp"

TEST_CASE("version() is non-empty") {
  CHECK(!chronos::version().empty());
}

TEST_CASE("engine reports alive") {
  CHECK(chronos::engine_alive());
}

MICROTEST_MAIN
