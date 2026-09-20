add_library(chronos_market_replay STATIC src/market_replay.cpp)

target_include_directories(chronos_market_replay
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/adapters/market_data/src)

target_link_libraries(chronos_market_replay
  PUBLIC chronos_replay_providers
         chronos_market_data_normalization
         chronos_core
         chronos_features
         chronos_reference_strategies
         chronos_strategy_runtime
         chronos_recommendation
  PRIVATE chronos_options chronos_warnings)
