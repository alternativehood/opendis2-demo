#pragma once

#include <d2buildinfo/build_info.hpp>

namespace d2engine {

inline const char* app_build_timestamp() {
    return d2buildinfo::build_timestamp();
}

} // namespace d2engine
