add_library(chronos_adapter_sdk STATIC
  sdk/src/source_event.cpp
  sdk/src/sha256.cpp)

target_include_directories(chronos_adapter_sdk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)

target_link_libraries(chronos_adapter_sdk
  PUBLIC chronos_contracts chronos_options
  PRIVATE chronos_warnings)

add_library(chronos_bybit_adapter STATIC
  market_data/src/bybit_adapter.cpp
  market_data/src/source_capture.cpp
  market_data/src/bybit_websocket.cpp
  market_data/src/websocket_framer.cpp
  market_data/src/curl_websocket_transport.cpp)

find_package(CURL 7.86 REQUIRED)

target_include_directories(chronos_bybit_adapter
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/market_data/include)

target_link_libraries(chronos_bybit_adapter
  PUBLIC chronos_adapter_sdk chronos_options
  PRIVATE CURL::libcurl
  PRIVATE chronos_warnings)

add_executable(chronos_bybit_probe
  market_data/src/bybit_probe.cpp)

target_link_libraries(chronos_bybit_probe
  PRIVATE chronos_bybit_adapter chronos_options chronos_warnings)
