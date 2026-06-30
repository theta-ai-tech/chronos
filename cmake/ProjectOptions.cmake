# Shared build options, exposed as the INTERFACE target `chronos_options`.
# Carries the language standard and, when CHRONOS_SANITIZE is on, the
# AddressSanitizer + UndefinedBehaviorSanitizer flags (compile and link).
add_library(chronos_options INTERFACE)
target_compile_features(chronos_options INTERFACE cxx_std_20)

if(CHRONOS_SANITIZE)
  set(_chronos_san -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_compile_options(chronos_options INTERFACE ${_chronos_san})
  target_link_options(chronos_options INTERFACE ${_chronos_san})
  message(STATUS "Chronos sanitizers enabled: ASan + UBSan")
endif()
