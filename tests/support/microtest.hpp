#pragma once
//
// microtest — a ~60-line, zero-dependency unit-test harness.
//
// Chronos avoids fetching a test framework over the network during the build
// (the architecture requires builds without undeclared network access). This
// minimal harness keeps M0.2 self-contained. A richer framework can be adopted
// later behind the same CTest interface via an ADR.
//
//   TEST_CASE("name") { CHECK(cond); }
//   MICROTEST_MAIN
//
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace microtest {

struct Case {
  std::string name;
  std::function<void(int &)> fn;
};

inline std::vector<Case> &registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(std::string name, std::function<void(int &)> fn) {
    registry().push_back({std::move(name), std::move(fn)});
  }
};

inline int run_all() {
  int total_failures = 0;
  for (const auto &test_case : registry()) {
    int case_failures = 0;
    // A test body that throws must be attributed to its case and counted as a
    // failure, not allowed to std::terminate the whole run and abort the cases
    // that follow.
    try {
      test_case.fn(case_failures);
    } catch (const std::exception &e) {
      ++case_failures;
      std::printf("    threw std::exception: %s\n", e.what());
    } catch (...) {
      ++case_failures;
      std::printf("    threw unknown exception\n");
    }
    if (case_failures == 0) {
      std::printf("[ PASS ] %s\n", test_case.name.c_str());
    } else {
      std::printf("[ FAIL ] %s (%d check(s) failed)\n", test_case.name.c_str(),
                  case_failures);
      total_failures += case_failures;
    }
  }
  std::printf("%s: %zu case(s), %d failed check(s)\n",
              total_failures == 0 ? "RESULT OK" : "RESULT FAIL",
              registry().size(), total_failures);
  return total_failures == 0 ? 0 : 1;
}

} // namespace microtest

#define MT_CONCAT_INNER(a, b) a##b
#define MT_CONCAT(a, b) MT_CONCAT_INNER(a, b)

#define TEST_CASE(test_name)                                                   \
  static void MT_CONCAT(mt_case_, __LINE__)(int &mt_fail_count);               \
  static const microtest::Registrar MT_CONCAT(mt_reg_, __LINE__)(              \
      test_name, &MT_CONCAT(mt_case_, __LINE__));                              \
  static void MT_CONCAT(mt_case_, __LINE__)(int &mt_fail_count)

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ++mt_fail_count;                                                         \
      std::printf("    CHECK failed: %s  (%s:%d)\n", #expr, __FILE__,          \
                  __LINE__);                                                   \
    }                                                                          \
  } while (0)

#define MICROTEST_MAIN                                                         \
  int main() { return microtest::run_all(); }
