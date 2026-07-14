add_library(chronos_contracts STATIC
  src/digest.cpp
  src/serialization.cpp)

target_include_directories(chronos_contracts
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_contracts
  PUBLIC chronos_options
  PRIVATE chronos_warnings)

set_target_properties(chronos_contracts PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
