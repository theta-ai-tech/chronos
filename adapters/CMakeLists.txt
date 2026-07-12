add_library(chronos_adapter_sdk INTERFACE)

target_include_directories(chronos_adapter_sdk
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)

target_link_libraries(chronos_adapter_sdk
  INTERFACE chronos_contracts chronos_options)

add_library(chronos_bybit_adapter STATIC
  market_data/src/bybit_adapter.cpp)

target_include_directories(chronos_bybit_adapter
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/market_data/include)

target_link_libraries(chronos_bybit_adapter
  PUBLIC chronos_adapter_sdk chronos_options
  PRIVATE chronos_warnings)
