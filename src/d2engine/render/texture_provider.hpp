#pragma once

#include <string>
#include <utility>

namespace d2engine {

struct BackendTextureRef {
    void* native = nullptr;

    [[nodiscard]] bool present() const noexcept { return native != nullptr; }
};

class ITextureProvider {
public:
    ITextureProvider() = default;
    virtual ~ITextureProvider() = default;
    ITextureProvider(const ITextureProvider&) = default;
    ITextureProvider& operator=(const ITextureProvider&) = default;
    ITextureProvider(ITextureProvider&&) = default;
    ITextureProvider& operator=(ITextureProvider&&) = default;

    [[nodiscard]] virtual BackendTextureRef       get_texture(const std::string& container_path,
                                                              const std::string& image_name) = 0;
    [[nodiscard]] virtual std::pair<float, float> texture_size(BackendTextureRef texture) const = 0;
};

} // namespace d2engine
