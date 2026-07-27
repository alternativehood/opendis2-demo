#include "mapped_file.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <utility>

namespace d2res {

struct MappedFile::NativeState {
    void*       view = nullptr;
    std::size_t size = 0;
    int         fd = -1;

    ~NativeState() noexcept {
        if (view != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
            ::munmap(view, size);
        }
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

namespace {

std::string errno_str(int e) {
    char buf[256];
#if defined(__APPLE__) || defined(__FreeBSD__)
    if (::strerror_r(e, buf, sizeof(buf)) == 0)
        return {buf};
#else
    return {::strerror_r(e, buf, sizeof(buf))};
#endif
    return "errno=" + std::to_string(e);
}

} // namespace

#include "mapped_file_common.inc"

MappedFile MappedFile::open(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("cannot open file for mmap: " + path.string() + " (" +
                                 errno_str(errno) + ")");
    }

    struct stat st;
    if (::fstat(fd, &st) < 0) {
        int saved = errno;
        ::close(fd);
        throw std::runtime_error("cannot stat file: " + path.string() + " (" + errno_str(saved) +
                                 ")");
    }

    if (st.st_size < 0) {
        ::close(fd);
        throw std::runtime_error("negative file size for mmap: " + path.string());
    }

    std::size_t sz = static_cast<std::size_t>(st.st_size);
    if (sz == 0) {
        ::close(fd);
        return {};
    }

    void* addr = ::mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        int saved = errno;
        ::close(fd);
        throw std::runtime_error("mmap failed for: " + path.string() + " (" + errno_str(saved) +
                                 ")");
    }

    MappedFile mf;
    auto       state = std::make_unique<NativeState>();
    state->view = addr;
    state->size = sz;
    state->fd = fd;
    mf.native_state_ = std::move(state);
    mf.data_ = static_cast<const uint8_t*>(addr);
    mf.size_ = sz;
    return mf;
}

} // namespace d2res
