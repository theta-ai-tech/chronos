# Custom "Benchmark" build profile.
#
# Optimized and assert-free like Release, kept as a distinct configuration so
# benchmark builds are explicit and reproducible (ADR-0002). It deliberately
# does NOT enable -march=native: benchmark numbers must come from a documented
# reference environment, not from host-specific autovectorization.
set(CMAKE_CXX_FLAGS_BENCHMARK "-O3 -DNDEBUG"
    CACHE STRING "C++ flags used by the Benchmark build profile" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_BENCHMARK ""
    CACHE STRING "Linker flags used by the Benchmark build profile" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_BENCHMARK ""
    CACHE STRING "Shared linker flags used by the Benchmark build profile" FORCE)
mark_as_advanced(
  CMAKE_CXX_FLAGS_BENCHMARK
  CMAKE_EXE_LINKER_FLAGS_BENCHMARK
  CMAKE_SHARED_LINKER_FLAGS_BENCHMARK)

# Expose Benchmark to multi-config generators (e.g. Xcode, Ninja Multi-Config).
get_property(_chronos_is_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_chronos_is_multi AND NOT "Benchmark" IN_LIST CMAKE_CONFIGURATION_TYPES)
  list(APPEND CMAKE_CONFIGURATION_TYPES Benchmark)
endif()
