#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace d2res {

class MappedFile {
public:
    MappedFile();

    static MappedFile open(const std::filesystem::path& path);

    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    ~MappedFile();

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    [[nodiscard]] const uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t    size() const noexcept { return size_; }
    [[nodiscard]] explicit       operator bool() const noexcept { return data_ != nullptr; }

private:
    struct NativeState;

    std::unique_ptr<NativeState> native_state_;
    const uint8_t*               data_ = nullptr;
    std::size_t                  size_ = 0;
};

} // namespace d2res
