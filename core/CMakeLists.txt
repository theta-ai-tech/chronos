# core/ — deterministic domain authorities on the hot path (optimized C++, ADR-0001).
#
# M0.2 adds a single placeholder library, `chronos_core`, purely to prove the
# toolchain builds and links. Real authorities (dispatch, market_state, ...)
# arrive in later milestones.
add_library(chronos_core
  src/version.cpp)

target_include_directories(chronos_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(chronos_core
  PRIVATE CHRONOS_VERSION="${PROJECT_VERSION}")

target_link_libraries(chronos_core
  PUBLIC  chronos_options
  PRIVATE chronos_warnings)
