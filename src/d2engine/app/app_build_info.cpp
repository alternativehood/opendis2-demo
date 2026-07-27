#include "app_build_info.hpp"

namespace d2engine {

const char* app_build_timestamp() {
    return __DATE__ " " __TIME__;
}

} // namespace d2engine
