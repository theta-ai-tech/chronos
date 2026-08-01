# M6 portfolio target boundary. This file is included as the final top-level
# configure command, after the complete repository graph has been constructed.
if(COMMAND _message)
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:COMMAND_OVERRIDE: message() was overridden")
  set_property(TARGET __chronos_m6_command_override_is_forbidden PROPERTY TYPE STATIC_LIBRARY)
endif()

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
  "${CMAKE_CURRENT_LIST_DIR}/include")
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

set(_chronos_portfolio_source
  "${CMAKE_CURRENT_LIST_DIR}/src/portfolio_construction.cpp")
get_source_file_property(
  _chronos_portfolio_source_language
  "${_chronos_portfolio_source}"
  TARGET_DIRECTORY chronos_portfolio
  LANGUAGE)
if(NOT "${_chronos_portfolio_source_language}" STREQUAL "CXX")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCE_LANGUAGE: expected `CXX`, got "
    "`${_chronos_portfolio_source_language}`")
endif()

get_source_file_property(
  _chronos_portfolio_source_generated
  "${_chronos_portfolio_source}"
  TARGET_DIRECTORY chronos_portfolio
  GENERATED)
if(NOT "${_chronos_portfolio_source_generated}" STREQUAL "0")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCE_GENERATED: expected `0`, got "
    "`${_chronos_portfolio_source_generated}`")
endif()

set(_chronos_portfolio_empty_source_properties
  COMPILE_DEFINITIONS
  COMPILE_FLAGS
  COMPILE_OPTIONS
  INCLUDE_DIRECTORIES
  HEADER_FILE_ONLY
  EXTERNAL_OBJECT
  KEEP_EXTENSION
  MACOSX_PACKAGE_LOCATION
  OBJECT_DEPENDS
  OBJECT_OUTPUTS
  SKIP_AUTOGEN
  SKIP_AUTOMOC
  SKIP_AUTORCC
  SKIP_AUTOUIC
  SKIP_LINTING
  SKIP_PRECOMPILE_HEADERS
  SKIP_UNITY_BUILD_INCLUSION
  SYMBOLIC
  UNITY_GROUP
  VS_COPY_TO_OUT_DIR
  VS_DEPLOYMENT_CONTENT
  VS_DEPLOYMENT_LOCATION
  VS_SETTINGS
  VS_SOURCE_SETTINGS_CXX
  VS_TOOL_OVERRIDE
  XCODE_EXPLICIT_FILE_TYPE
  XCODE_FILE_ATTRIBUTES
  XCODE_LAST_KNOWN_FILE_TYPE
  CXX_SCAN_FOR_MODULES)
set(_chronos_portfolio_configurations
  Debug
  Release
  Benchmark
  RelWithDebInfo
  MinSizeRel
  ${CMAKE_BUILD_TYPE}
  ${CMAKE_CONFIGURATION_TYPES})
list(REMOVE_DUPLICATES _chronos_portfolio_configurations)
foreach(_chronos_portfolio_configuration IN LISTS _chronos_portfolio_configurations)
  if(NOT "${_chronos_portfolio_configuration}" STREQUAL "")
    string(TOUPPER "${_chronos_portfolio_configuration}" _chronos_portfolio_configuration)
    list(APPEND _chronos_portfolio_empty_source_properties
      "COMPILE_DEFINITIONS_${_chronos_portfolio_configuration}")
  endif()
endforeach()

foreach(_chronos_portfolio_source_property
    IN LISTS _chronos_portfolio_empty_source_properties)
  get_source_file_property(
    _chronos_portfolio_source_property_value
    "${_chronos_portfolio_source}"
    TARGET_DIRECTORY chronos_portfolio
    "${_chronos_portfolio_source_property}")
  if(NOT "${_chronos_portfolio_source_property_value}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCE_${_chronos_portfolio_source_property}: "
      "expected an unset source property, got "
      "`${_chronos_portfolio_source_property_value}`")
  endif()
endforeach()
