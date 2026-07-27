add_library(chronos_features STATIC
  src/feature_runtime.cpp)

target_include_directories(chronos_features
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(chronos_features
  PUBLIC chronos_contracts chronos_core chronos_options
  PRIVATE chronos_warnings)
