add_library(chronos_market_data_normalization STATIC
  src/book_normalizer.cpp
  src/trade_normalizer.cpp
  ${PROJECT_SOURCE_DIR}/adapters/market_data/src/bybit_book_decoder.cpp
  ${PROJECT_SOURCE_DIR}/adapters/market_data/src/bybit_trade_decoder.cpp)

target_include_directories(chronos_market_data_normalization
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_market_data_normalization
  PUBLIC chronos_core chronos_bybit_adapter chronos_options
  PRIVATE chronos_warnings)
