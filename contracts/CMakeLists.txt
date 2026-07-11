add_library(chronos_contracts INTERFACE)

target_include_directories(chronos_contracts
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_contracts
  INTERFACE chronos_options)
