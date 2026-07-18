# Chronos — top-level C++ build (M0.2)
#
# Establishes the C++ toolchain for the hot-path modules: a pinned language
# standard, three build profiles (Debug / Release / Benchmark), shared warning
# and option targets, and one sample library + unit-test target so the toolchain
# is provably working. No domain logic lives here yet.
cmake_minimum_required(VERSION 3.24)

project(Chronos
  VERSION 0.0.1
  DESCRIPTION "Local-first, replay-first crypto decision and research engine"
  LANGUAGES CXX)

include(cmake/ToolchainPolicy.cmake)

# --- Guard rails -------------------------------------------------------------
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not allowed. Configure into a separate directory, e.g.\n"
    "  cmake -S . -B build")
endif()

# --- Pinned language standard ------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Project options ---------------------------------------------------------
option(CHRONOS_WERROR   "Treat compiler warnings as errors" ON)
option(CHRONOS_SANITIZE "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

# --- Build profiles ----------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BuildTypes)

get_property(_chronos_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _chronos_multi_config)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release Benchmark)
endif()

# --- Shared interface targets ------------------------------------------------
include(CompilerWarnings)   # -> chronos_warnings
include(ProjectOptions)     # -> chronos_options (std + optional sanitizers)

# --- Modules -----------------------------------------------------------------
enable_testing()
add_subdirectory(contracts)
add_subdirectory(adapters)
add_subdirectory(core)
add_subdirectory(strategies)
add_subdirectory(normalization)
add_subdirectory(runtime)
add_subdirectory(tests)

# CMAKE_BUILD_TYPE is empty for multi-config generators (the profile is chosen
# at build time, not configure time); report that instead of a blank field.
if(CMAKE_BUILD_TYPE)
  set(_chronos_profile "${CMAKE_BUILD_TYPE}")
else()
  set(_chronos_profile "multi-config")
endif()

message(STATUS
  "Chronos ${PROJECT_VERSION} | profile=${_chronos_profile} "
  "| werror=${CHRONOS_WERROR} | sanitize=${CHRONOS_SANITIZE}")
