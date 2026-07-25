add_library(chronos_recommendation STATIC
  src/recommendation.cpp)

target_include_directories(chronos_recommendation
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_recommendation
  PUBLIC chronos_strategy_runtime chronos_options
  PRIVATE chronos_warnings)
