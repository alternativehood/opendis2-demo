#pragma once

#include <cstdint>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace test_support {

[[nodiscard]] inline std::uint64_t process_id() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(::_getpid());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

} // namespace test_support
