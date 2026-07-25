add_library(chronos_strategy_sdk STATIC
  sdk/src/strategy.cpp)

target_include_directories(chronos_strategy_sdk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)

target_link_libraries(chronos_strategy_sdk
  PUBLIC chronos_core chronos_options
  PRIVATE chronos_warnings)

add_library(chronos_reference_strategies STATIC
  reference/src/imbalance_threshold_strategy.cpp)

target_include_directories(chronos_reference_strategies
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/reference/include)

target_link_libraries(chronos_reference_strategies
  PUBLIC chronos_strategy_sdk chronos_options
  PRIVATE chronos_warnings)
