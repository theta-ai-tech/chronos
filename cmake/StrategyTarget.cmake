include(CMakeParseArguments)

function(chronos_add_strategy target)
  set(options)
  set(one_value_args DEFINITION)
  set(multi_value_args)
  cmake_parse_arguments(STRATEGY
    "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT STRATEGY_DEFINITION OR STRATEGY_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "chronos_add_strategy(${target}) requires exactly one DEFINITION manifest")
  endif()

  set(generated_root "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(generated_header
    "${generated_root}/include/chronos/strategies/generated/${target}.hpp")
  set(generated_source "${generated_root}/src/${target}.cpp")
  add_custom_command(
    OUTPUT ${generated_header} ${generated_source}
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/development/generate_strategy_definition.py
            --target ${target}
            --manifest ${STRATEGY_DEFINITION}
            --header ${generated_header}
            --source ${generated_source}
    DEPENDS ${STRATEGY_DEFINITION}
            ${PROJECT_SOURCE_DIR}/tools/development/generate_strategy_definition.py
    COMMENT "Generating trusted ${target} strategy definition"
    VERBATIM)

  add_library(${target} STATIC ${generated_source})
  target_include_directories(${target}
    PUBLIC ${generated_root}/include)
  target_link_libraries(${target}
    PUBLIC chronos_strategy_sdk chronos_options
    PRIVATE chronos_warnings)

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/development/verify_strategy_symbols.py
            $<TARGET_FILE:${target}>
    COMMENT "Checking ${target} linked capability boundary"
    VERBATIM)
endfunction()
