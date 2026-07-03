# Shared warning set, exposed as the INTERFACE target `chronos_warnings`.
# Link it PRIVATE into every Chronos C++ target so warnings are consistent and,
# by default, fatal (CHRONOS_WERROR).
add_library(chronos_warnings INTERFACE)

# GCC/Clang (incl. AppleClang) flag spelling only. These are the only compilers
# Chronos targets (dev macOS + Linux bench host, ADR-0002). When/if another
# toolchain (e.g. MSVC) is introduced, gate these with check_cxx_compiler_flag
# rather than assuming support.
set(_chronos_warnings
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wnon-virtual-dtor
  -Wdouble-promotion
  -Wnull-dereference)

if(CHRONOS_WERROR)
  list(APPEND _chronos_warnings -Werror)
endif()

target_compile_options(chronos_warnings INTERFACE ${_chronos_warnings})
