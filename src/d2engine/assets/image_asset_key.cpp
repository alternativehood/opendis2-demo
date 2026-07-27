#include "image_asset_key.hpp"

#include <functional>

namespace d2engine {

namespace {

[[nodiscard]] std::string kind_name(ImageAssetKind kind) {
    switch (kind) {
    case ImageAssetKind::ComposedSprite:
        return "ComposedSprite";
    case ImageAssetKind::RawPng:
        return "RawPng";
    }
    return "Unknown";
}

} // namespace

std::string to_string(const ImageAssetKey& key) {
    return kind_name(key.kind) + "|" + std::to_string(static_cast<unsigned>(key.postprocess)) +
           "|" + key.container_path + "/" + key.image_name;
}

} // namespace d2engine

std::size_t
std::hash<d2engine::ImageAssetKey>::operator()(const d2engine::ImageAssetKey& key) const noexcept {
    std::size_t seed = std::hash<std::string>{}(key.container_path);
    const auto  mix = [&](std::size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    };
    mix(std::hash<std::string>{}(key.image_name));
    mix(std::hash<unsigned>{}(static_cast<unsigned>(key.kind)));
    mix(std::hash<unsigned>{}(static_cast<unsigned>(key.postprocess)));
    return seed;
}
