#pragma once

#include <string>

namespace d2buildinfo {

inline constexpr auto kProjectVersion = "0.1.0-dev";

const char* build_timestamp();

std::string format_build_version();

} // namespace d2buildinfo
