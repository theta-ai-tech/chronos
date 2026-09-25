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

add_library(chronos_paper_loop STATIC src/paper_replay.cpp)
target_include_directories(chronos_paper_loop
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(chronos_paper_loop
  PUBLIC chronos_market_replay
         chronos_portfolio
         chronos_risk
         chronos_reservation
         chronos_execution
         chronos_paper_broker
         chronos_ledger
         chronos_valuation
  PRIVATE chronos_bybit_adapter chronos_options chronos_warnings)

add_executable(chronos_paper_replay src/paper_replay_main.cpp)
target_link_libraries(chronos_paper_replay
  PRIVATE chronos_paper_loop chronos_options chronos_warnings)
