#pragma once

#include <filesystem>

namespace d2engine::platform {

[[nodiscard]] std::filesystem::path executable_directory();

} // namespace d2engine::platform
