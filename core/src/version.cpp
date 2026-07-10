#include "chronos/version.hpp"

#ifndef CHRONOS_VERSION
#define CHRONOS_VERSION "0.0.0"
#endif

namespace chronos {

std::string_view version() noexcept { return CHRONOS_VERSION; }

bool engine_alive() noexcept { return true; }

}  // namespace chronos
