#pragma once
#include <string_view>

namespace d2res::platform {

// Case-insensitive shell-style glob match.
// Supports '*' (any sequence) and '?' (any single character).
// Returns true when pattern matches name entirely.
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view name);

} // namespace d2res::platform
