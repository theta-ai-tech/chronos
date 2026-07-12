# tests/ — C++ test targets (M0.2 adds the first unit-test executable).
add_executable(chronos_unit_tests
  unit/adapter_sdk_test.cpp
  unit/bybit_websocket_test.cpp
  unit/source_capture_test.cpp
  unit/reconnect_test.cpp
  unit/boundary_test.cpp
  unit/event_envelope_test.cpp
  unit/fixed_point_test.cpp
  unit/state_lineage_test.cpp
  unit/serialization_test.cpp
  unit/value_objects_test.cpp
  unit/version_test.cpp)

target_include_directories(chronos_unit_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)

target_compile_definitions(chronos_unit_tests
  PRIVATE CHRONOS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_link_libraries(chronos_unit_tests
  PRIVATE chronos_core chronos_boundary chronos_bybit_adapter chronos_options chronos_warnings)

add_test(NAME chronos_unit_tests COMMAND chronos_unit_tests)
