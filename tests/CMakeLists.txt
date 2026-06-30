# tests/ — C++ test targets (M0.2 adds the first unit-test executable).
add_executable(chronos_unit_tests
  unit/version_test.cpp)

target_include_directories(chronos_unit_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)

target_link_libraries(chronos_unit_tests
  PRIVATE chronos_core chronos_options chronos_warnings)

add_test(NAME chronos_unit_tests COMMAND chronos_unit_tests)
