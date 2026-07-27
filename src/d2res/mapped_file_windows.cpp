#include "mapped_file.hpp"

#include <windows.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace d2res {

struct MappedFile::NativeState {
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    HANDLE mapping_handle = nullptr;
    void*  view = nullptr;

    ~NativeState() noexcept {
        if (view != nullptr) {
            static_cast<void>(::UnmapViewOfFile(view));
        }
        if (mapping_handle != nullptr) {
            static_cast<void>(::CloseHandle(mapping_handle));
        }
        if (file_handle != INVALID_HANDLE_VALUE) {
            static_cast<void>(::CloseHandle(file_handle));
        }
    }
};

namespace {

std::string last_error_message(DWORD error) {
    return std::to_string(error) + ": " + std::system_category().message(static_cast<int>(error));
}

} // namespace

#include "mapped_file_common.inc"

MappedFile MappedFile::open(const std::filesystem::path& path) {
    HANDLE file_handle = ::CreateFileW(path.c_str(), GENERIC_READ,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("cannot open file for mmap: " + path.string() + " (" +
                                 last_error_message(::GetLastError()) + ")");
    }

    LARGE_INTEGER file_size{};
    if (!::GetFileSizeEx(file_handle, &file_size)) {
        const DWORD error = ::GetLastError();
        static_cast<void>(::CloseHandle(file_handle));
        throw std::runtime_error("cannot stat file: " + path.string() + " (" +
                                 last_error_message(error) + ")");
    }

    if (file_size.QuadPart < 0) {
        static_cast<void>(::CloseHandle(file_handle));
        throw std::runtime_error("negative file size for mmap: " + path.string());
    }

    const auto unsigned_size = static_cast<unsigned long long>(file_size.QuadPart);
    if (unsigned_size > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        static_cast<void>(::CloseHandle(file_handle));
        throw std::runtime_error("file too large for mmap: " + path.string());
    }

    if (unsigned_size == 0) {
        static_cast<void>(::CloseHandle(file_handle));
        return {};
    }

    HANDLE mapping_handle =
        ::CreateFileMappingW(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping_handle == nullptr) {
        const DWORD error = ::GetLastError();
        static_cast<void>(::CloseHandle(file_handle));
        throw std::runtime_error("cannot create file mapping: " + path.string() + " (" +
                                 last_error_message(error) + ")");
    }

    void* view = ::MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
        const DWORD error = ::GetLastError();
        static_cast<void>(::CloseHandle(mapping_handle));
        static_cast<void>(::CloseHandle(file_handle));
        throw std::runtime_error("cannot map file view: " + path.string() + " (" +
                                 last_error_message(error) + ")");
    }

    MappedFile mf;
    auto       state = std::make_unique<NativeState>();
    state->file_handle = file_handle;
    state->mapping_handle = mapping_handle;
    state->view = view;
    mf.native_state_ = std::move(state);
    mf.data_ = static_cast<const uint8_t*>(view);
    mf.size_ = static_cast<std::size_t>(unsigned_size);
    return mf;
}

} // namespace d2res
