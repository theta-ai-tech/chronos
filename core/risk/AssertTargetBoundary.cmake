# M6 risk target boundary. This file is included immediately before the
# portfolio boundary, after the complete repository graph has been constructed.
if(COMMAND _message)
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:COMMAND_OVERRIDE: message() was overridden")
  set_property(TARGET __chronos_m6_command_override_is_forbidden PROPERTY TYPE STATIC_LIBRARY)
endif()

set(_chronos_risk_expected_link_libraries
  chronos_contracts
  chronos_portfolio
  chronos_options
  chronos_warnings)
get_target_property(
  _chronos_risk_link_libraries chronos_risk LINK_LIBRARIES)
if(NOT "${_chronos_risk_link_libraries}" STREQUAL
    "${_chronos_risk_expected_link_libraries}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:LINK_LIBRARIES: expected "
    "`${_chronos_risk_expected_link_libraries}`, got "
    "`${_chronos_risk_link_libraries}`")
endif()

set(_chronos_risk_expected_interface_link_libraries
  chronos_contracts
  chronos_portfolio
  "$<LINK_ONLY:chronos_options>"
  "$<LINK_ONLY:chronos_warnings>")
get_target_property(
  _chronos_risk_interface_link_libraries
  chronos_risk
  INTERFACE_LINK_LIBRARIES)
if(NOT "${_chronos_risk_interface_link_libraries}" STREQUAL
    "${_chronos_risk_expected_interface_link_libraries}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:INTERFACE_LINK_LIBRARIES: expected "
    "`${_chronos_risk_expected_interface_link_libraries}`, got "
    "`${_chronos_risk_interface_link_libraries}`")
endif()

set(_chronos_risk_expected_sources
  src/risk_arithmetic.hpp
  src/risk_decision.cpp)
get_target_property(
  _chronos_risk_sources chronos_risk SOURCES)
list(SORT _chronos_risk_sources)
if(NOT "${_chronos_risk_sources}" STREQUAL
    "${_chronos_risk_expected_sources}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCES: expected "
    "`${_chronos_risk_expected_sources}`, got "
    "`${_chronos_risk_sources}`")
endif()

set(_chronos_risk_expected_include_directories
  "${CMAKE_CURRENT_LIST_DIR}/include")
get_target_property(
  _chronos_risk_include_directories chronos_risk INCLUDE_DIRECTORIES)
if(NOT "${_chronos_risk_include_directories}" STREQUAL
    "${_chronos_risk_expected_include_directories}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:INCLUDE_DIRECTORIES: expected "
    "`${_chronos_risk_expected_include_directories}`, got "
    "`${_chronos_risk_include_directories}`")
endif()

get_target_property(
  _chronos_risk_interface_include_directories
  chronos_risk
  INTERFACE_INCLUDE_DIRECTORIES)
if(NOT "${_chronos_risk_interface_include_directories}" STREQUAL
    "${_chronos_risk_expected_include_directories}")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:INTERFACE_INCLUDE_DIRECTORIES: expected "
    "`${_chronos_risk_expected_include_directories}`, got "
    "`${_chronos_risk_interface_include_directories}`")
endif()

set(_chronos_risk_empty_properties
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
foreach(_chronos_risk_property IN LISTS _chronos_risk_empty_properties)
  get_property(
    _chronos_risk_property_is_set
    TARGET chronos_risk
    PROPERTY "${_chronos_risk_property}"
    SET)
  if(_chronos_risk_property_is_set)
    get_target_property(
      _chronos_risk_property_value
      chronos_risk
      "${_chronos_risk_property}")
    if(NOT "${_chronos_risk_property_value}" STREQUAL "")
      message(FATAL_ERROR
        "CHRONOS_M6_BOUNDARY_VIOLATION:${_chronos_risk_property}: expected "
        "an empty property, got `${_chronos_risk_property_value}`")
    endif()
  endif()
endforeach()

set(_chronos_risk_source
  "${CMAKE_CURRENT_LIST_DIR}/src/risk_decision.cpp")
get_source_file_property(
  _chronos_risk_source_language
  "${_chronos_risk_source}"
  TARGET_DIRECTORY chronos_risk
  LANGUAGE)
if(NOT "${_chronos_risk_source_language}" STREQUAL "CXX")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCE_LANGUAGE: expected `CXX`, got "
    "`${_chronos_risk_source_language}`")
endif()

get_source_file_property(
  _chronos_risk_source_generated
  "${_chronos_risk_source}"
  TARGET_DIRECTORY chronos_risk
  GENERATED)
if(NOT "${_chronos_risk_source_generated}" STREQUAL "0")
  message(FATAL_ERROR
    "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCE_GENERATED: expected `0`, got "
    "`${_chronos_risk_source_generated}`")
endif()

set(_chronos_risk_empty_source_properties
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
set(_chronos_risk_configurations
  Debug
  Release
  Benchmark
  RelWithDebInfo
  MinSizeRel
  ${CMAKE_BUILD_TYPE}
  ${CMAKE_CONFIGURATION_TYPES})
list(REMOVE_DUPLICATES _chronos_risk_configurations)
foreach(_chronos_risk_configuration IN LISTS _chronos_risk_configurations)
  if(NOT "${_chronos_risk_configuration}" STREQUAL "")
    string(TOUPPER "${_chronos_risk_configuration}" _chronos_risk_configuration)
    list(APPEND _chronos_risk_empty_source_properties
      "COMPILE_DEFINITIONS_${_chronos_risk_configuration}")
  endif()
endforeach()

foreach(_chronos_risk_source_property
    IN LISTS _chronos_risk_empty_source_properties)
  get_source_file_property(
    _chronos_risk_source_property_value
    "${_chronos_risk_source}"
    TARGET_DIRECTORY chronos_risk
    "${_chronos_risk_source_property}")
  if(NOT "${_chronos_risk_source_property_value}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "CHRONOS_M6_BOUNDARY_VIOLATION:SOURCE_${_chronos_risk_source_property}: "
      "expected an unset source property, got "
      "`${_chronos_risk_source_property_value}`")
  endif()
endforeach()

set(_chronos_m6_risk_boundary_assertion_complete TRUE)
