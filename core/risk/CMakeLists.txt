add_library(chronos_risk STATIC
  src/risk_decision.cpp)

target_include_directories(chronos_risk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_risk
  PUBLIC chronos_contracts chronos_portfolio
  PRIVATE chronos_options chronos_warnings)
