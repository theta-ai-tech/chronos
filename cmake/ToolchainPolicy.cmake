# M0.2 supported compiler lines. Keep this deliberately narrow so build and
# benchmark behavior cannot drift silently with the host's default compiler.
set(_chronos_supported_compiler FALSE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
  set(_chronos_supported_compiler TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
       AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 18
       AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
  set(_chronos_supported_compiler TRUE)
endif()

if(NOT _chronos_supported_compiler)
  message(FATAL_ERROR
    "Unsupported C++ compiler: ${CMAKE_CXX_COMPILER_ID} "
    "${CMAKE_CXX_COMPILER_VERSION}. "
    "Chronos M0.2 supports the pinned Clang 18.x toolchain or "
    "AppleClang 16.x for macOS development.")
endif()

message(STATUS
  "Chronos compiler policy: ${CMAKE_CXX_COMPILER_ID} "
  "${CMAKE_CXX_COMPILER_VERSION} (supported)")
