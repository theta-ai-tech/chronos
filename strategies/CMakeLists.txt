find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(StrategyTarget)

add_custom_target(chronos_strategy_capability_check ALL
  COMMAND ${Python3_EXECUTABLE}
          ${PROJECT_SOURCE_DIR}/tools/development/verify_strategy_capabilities.py
          --root ${PROJECT_SOURCE_DIR}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Checking native strategies for forbidden ambient capabilities"
  VERBATIM)

add_library(chronos_strategy_sdk STATIC
  sdk/src/strategy.cpp
  sdk/src/strategy_host.cpp)

target_include_directories(chronos_strategy_sdk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)

target_link_libraries(chronos_strategy_sdk
  PUBLIC chronos_contracts chronos_options
  PRIVATE chronos_core chronos_warnings)

add_dependencies(chronos_strategy_sdk chronos_strategy_capability_check)

add_custom_command(TARGET chronos_strategy_sdk POST_BUILD
  COMMAND ${Python3_EXECUTABLE}
          ${PROJECT_SOURCE_DIR}/tools/development/verify_strategy_symbols.py
          $<TARGET_FILE:chronos_strategy_sdk>
  COMMENT "Checking strategy host linked capability boundary"
  VERBATIM)

add_subdirectory(conformance)
add_subdirectory(reference)
