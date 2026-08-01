add_library(chronos_portfolio STATIC
  src/portfolio_construction.cpp)

target_include_directories(chronos_portfolio
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_portfolio
  PUBLIC chronos_contracts chronos_recommendation
  PRIVATE chronos_options chronos_warnings)
