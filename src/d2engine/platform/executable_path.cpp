#include "executable_path.hpp"

#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace d2engine::platform {

namespace {

#if defined(_WIN32)
std::string last_error_message(unsigned long error) {
    return std::to_string(error) + ": " + std::system_category().message(static_cast<int>(error));
}
#endif

[[noreturn]] void throw_resolution_error(const std::string& message) {
    throw std::runtime_error(message);
}

std::filesystem::path canonical_parent(const std::filesystem::path& executable_path) {
    std::error_code ec;
    const auto      canonical_path = std::filesystem::canonical(executable_path, ec);
    if (ec) {
        throw_resolution_error("cannot resolve executable directory: " + executable_path.string() +
                               " (" + ec.message() + ")");
    }
    return canonical_path.parent_path();
}

} // namespace

std::filesystem::path executable_directory() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(256);
    for (;;) {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw_resolution_error("cannot resolve executable path: " +
                                   last_error_message(::GetLastError()));
        }
        if (length < buffer.size() - 1) {
            return canonical_parent(std::filesystem::path(buffer.data()));
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::vector<char> buffer(256);
    for (;;) {
        uint32_t size = static_cast<uint32_t>(buffer.size());
        if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
            return canonical_parent(std::filesystem::path(buffer.data()));
        }
        buffer.resize(size);
    }
#else
    std::vector<char> buffer(256);
    for (;;) {
        const auto length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (length < 0) {
            throw_resolution_error("cannot resolve executable path: /proc/self/exe");
        }
        if (static_cast<std::size_t>(length) < buffer.size() - 1) {
            buffer[static_cast<std::size_t>(length)] = '\0';
            return canonical_parent(std::filesystem::path(buffer.data()));
        }
        buffer.resize(buffer.size() * 2);
    }
#endif
}

} // namespace d2engine::platform
