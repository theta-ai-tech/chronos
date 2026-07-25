add_library(chronos_strategy_runtime STATIC
  src/strategy_runtime.cpp
  src/strategy_evaluation.cpp)

target_include_directories(chronos_strategy_runtime
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_strategy_runtime
  PUBLIC chronos_core chronos_strategy_sdk chronos_options
  PRIVATE chronos_warnings)
