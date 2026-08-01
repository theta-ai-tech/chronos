add_library(chronos_portfolio STATIC
  src/portfolio_construction.cpp)

target_include_directories(chronos_portfolio
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chronos_portfolio
  PUBLIC chronos_contracts chronos_recommendation
  PRIVATE chronos_options chronos_warnings)

# --- M6 portfolio target boundary -------------------------------------------
set(_chronos_portfolio_expected_link_libraries
  chronos_contracts
  chronos_recommendation
  chronos_options
  chronos_warnings)
get_target_property(
  _chronos_portfolio_link_libraries chronos_portfolio LINK_LIBRARIES)
if(NOT "${_chronos_portfolio_link_libraries}" STREQUAL
    "${_chronos_portfolio_expected_link_libraries}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:LINK_LIBRARIES: expected "
    "`${_chronos_portfolio_expected_link_libraries}`, got "
    "`${_chronos_portfolio_link_libraries}`")
endif()

set(_chronos_portfolio_expected_interface_link_libraries
  chronos_contracts
  chronos_recommendation
  "$<LINK_ONLY:chronos_options>"
  "$<LINK_ONLY:chronos_warnings>")
get_target_property(
  _chronos_portfolio_interface_link_libraries
  chronos_portfolio
  INTERFACE_LINK_LIBRARIES)
if(NOT "${_chronos_portfolio_interface_link_libraries}" STREQUAL
    "${_chronos_portfolio_expected_interface_link_libraries}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:INTERFACE_LINK_LIBRARIES: expected "
    "`${_chronos_portfolio_expected_interface_link_libraries}`, got "
    "`${_chronos_portfolio_interface_link_libraries}`")
endif()

set(_chronos_portfolio_expected_sources
  src/portfolio_construction.cpp)
get_target_property(
  _chronos_portfolio_sources chronos_portfolio SOURCES)
if(NOT "${_chronos_portfolio_sources}" STREQUAL
    "${_chronos_portfolio_expected_sources}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCES: expected "
    "`${_chronos_portfolio_expected_sources}`, got "
    "`${_chronos_portfolio_sources}`")
endif()

set(_chronos_portfolio_expected_include_directories
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
get_target_property(
  _chronos_portfolio_include_directories chronos_portfolio INCLUDE_DIRECTORIES)
if(NOT "${_chronos_portfolio_include_directories}" STREQUAL
    "${_chronos_portfolio_expected_include_directories}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:INCLUDE_DIRECTORIES: expected "
    "`${_chronos_portfolio_expected_include_directories}`, got "
    "`${_chronos_portfolio_include_directories}`")
endif()

get_target_property(
  _chronos_portfolio_interface_include_directories
  chronos_portfolio
  INTERFACE_INCLUDE_DIRECTORIES)
if(NOT "${_chronos_portfolio_interface_include_directories}" STREQUAL
    "${_chronos_portfolio_expected_include_directories}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:INTERFACE_INCLUDE_DIRECTORIES: expected "
    "`${_chronos_portfolio_expected_include_directories}`, got "
    "`${_chronos_portfolio_interface_include_directories}`")
endif()

set(_chronos_portfolio_empty_properties
  LINK_OPTIONS
  INTERFACE_LINK_OPTIONS
  LINK_DIRECTORIES
  INTERFACE_LINK_DIRECTORIES
  INTERFACE_LINK_LIBRARIES_DIRECT
  INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE
  COMPILE_DEFINITIONS
  INTERFACE_COMPILE_DEFINITIONS
  COMPILE_FEATURES
  INTERFACE_COMPILE_FEATURES
  COMPILE_OPTIONS
  INTERFACE_COMPILE_OPTIONS
  PRECOMPILE_HEADERS
  INTERFACE_PRECOMPILE_HEADERS
  SYSTEM_INCLUDE_DIRECTORIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
  INTERFACE_SOURCES)
foreach(_chronos_portfolio_property IN LISTS _chronos_portfolio_empty_properties)
  get_property(
    _chronos_portfolio_property_is_set
    TARGET chronos_portfolio
    PROPERTY "${_chronos_portfolio_property}"
    SET)
  if(_chronos_portfolio_property_is_set)
    get_target_property(
      _chronos_portfolio_property_value
      chronos_portfolio
      "${_chronos_portfolio_property}")
    if(NOT "${_chronos_portfolio_property_value}" STREQUAL "")
      message(FATAL_ERROR
        "CHRONOS_M6_BOUNDARY_VIOLATION:${_chronos_portfolio_property}: expected "
        "an empty property, got `${_chronos_portfolio_property_value}`")
    endif()
  endif()
endforeach()
