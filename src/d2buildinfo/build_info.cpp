#include "build_info.hpp"

namespace d2buildinfo {

const char* build_timestamp() {
    return __DATE__ " " __TIME__;
}

std::string format_build_version() {
    return std::string(kProjectVersion) + " (" + build_timestamp() + ")";
}

} // namespace d2buildinfo
