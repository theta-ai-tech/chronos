add_library(chronos_replay_providers STATIC
  src/replay.cpp)

target_include_directories(chronos_replay_providers
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_replay_providers
  PUBLIC chronos_bybit_adapter chronos_contracts chronos_options
  PRIVATE chronos_warnings)
