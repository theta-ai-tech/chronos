# core/ — deterministic domain authorities on the hot path (optimized C++, ADR-0001).
#
# M0.2 adds `chronos_core`; M0.4 adds a tiny shared boundary library so Python
# can prove it can invoke C++ without introducing domain logic yet.
add_library(chronos_core STATIC
  src/boundary.cpp
  src/feature_runtime.cpp
  src/l2_book.cpp
  src/listing_aux_state.cpp
  src/listing_view_publisher.cpp
  src/run_input_dispatcher.cpp
  src/reference_data.cpp
  src/version.cpp)

target_include_directories(chronos_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(chronos_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES)

target_compile_definitions(chronos_core
  PRIVATE CHRONOS_VERSION="${PROJECT_VERSION}")

target_link_libraries(chronos_core
  PUBLIC  chronos_contracts chronos_options
  PRIVATE chronos_warnings)

add_library(chronos_boundary SHARED
  src/boundary_c_api.cpp)

set_target_properties(chronos_boundary PROPERTIES
  OUTPUT_NAME chronos_boundary
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES)

target_compile_definitions(chronos_boundary
  PRIVATE CHRONOS_BOUNDARY_BUILD)

target_link_libraries(chronos_boundary
  PUBLIC  chronos_options
  PRIVATE chronos_core chronos_warnings)
