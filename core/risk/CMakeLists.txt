add_library(chronos_risk STATIC
  src/risk_decision.cpp
  src/risk_arithmetic.hpp)

target_include_directories(chronos_risk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_risk
  PUBLIC chronos_contracts chronos_portfolio
  PRIVATE chronos_options chronos_warnings)

add_library(chronos_reservation STATIC src/reservation.cpp)
target_include_directories(chronos_reservation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(chronos_reservation PUBLIC chronos_risk chronos_contracts PRIVATE chronos_options chronos_warnings)
