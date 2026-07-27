#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace d2engine {

enum class ImageAssetKind : std::uint8_t {
    ComposedSprite,
    RawPng,
};

enum class ImagePostprocess : std::uint8_t {
    None = 0,
    MagentaKey = 1 << 0,
    DetectMagentaBorder = 1 << 1,
};

[[nodiscard]] constexpr ImagePostprocess operator|(ImagePostprocess lhs,
                                                   ImagePostprocess rhs) noexcept {
    return static_cast<ImagePostprocess>(static_cast<std::uint8_t>(lhs) |
                                         static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr bool has_postprocess(ImagePostprocess flags,
                                             ImagePostprocess flag) noexcept {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
}

struct ImageAssetKey {
    std::string      container_path;
    std::string      image_name;
    ImageAssetKind   kind = ImageAssetKind::ComposedSprite;
    ImagePostprocess postprocess = ImagePostprocess::None;

    [[nodiscard]] friend bool operator==(const ImageAssetKey& lhs,
                                         const ImageAssetKey& rhs) noexcept {
        return lhs.kind == rhs.kind && lhs.postprocess == rhs.postprocess &&
               lhs.container_path == rhs.container_path && lhs.image_name == rhs.image_name;
    }
};

/// Physical image identity: canonical container + physical record name.
/// Two logical sprites from different containers never share a physical key,
/// even when their record names coincide.
struct PhysicalImageKey {
    std::string container;
    std::string record_name;

    friend bool operator==(const PhysicalImageKey&, const PhysicalImageKey&) noexcept = default;
};

[[nodiscard]] std::string to_string(const ImageAssetKey& key);

[[nodiscard]] inline ImageAssetKey make_world_composed_sprite_key(std::string_view container_path,
                                                                  std::string_view image_name) {
    return ImageAssetKey{.container_path = std::string(container_path),
                         .image_name = std::string(image_name),
                         .kind = ImageAssetKind::ComposedSprite};
}

} // namespace d2engine

template <> struct std::hash<d2engine::ImageAssetKey> {
    std::size_t operator()(const d2engine::ImageAssetKey& key) const noexcept;
};

template <> struct std::hash<d2engine::PhysicalImageKey> {
    std::size_t operator()(const d2engine::PhysicalImageKey& k) const noexcept {
        std::size_t h1 = std::hash<std::string>{}(k.container);
        std::size_t h2 = std::hash<std::string>{}(k.record_name);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
