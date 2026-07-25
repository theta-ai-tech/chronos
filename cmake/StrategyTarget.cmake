include(CMakeParseArguments)

function(chronos_add_strategy target)
  set(options)
  set(one_value_args)
  set(multi_value_args SOURCES PUBLIC_INCLUDE_DIRECTORIES)
  cmake_parse_arguments(STRATEGY
    "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT STRATEGY_SOURCES)
    message(FATAL_ERROR "chronos_add_strategy(${target}) requires SOURCES")
  endif()

  add_library(${target} STATIC ${STRATEGY_SOURCES})
  target_include_directories(${target}
    PUBLIC ${STRATEGY_PUBLIC_INCLUDE_DIRECTORIES})
  target_link_libraries(${target}
    PUBLIC chronos_strategy_sdk chronos_options
    PRIVATE chronos_warnings)

  add_custom_command(TARGET ${target} PRE_LINK
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/development/verify_strategy_capabilities.py
            ${STRATEGY_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking ${target} source capability boundary"
    VERBATIM)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/development/verify_strategy_symbols.py
            $<TARGET_FILE:${target}>
    COMMENT "Checking ${target} linked capability boundary"
    VERBATIM)
endfunction()
