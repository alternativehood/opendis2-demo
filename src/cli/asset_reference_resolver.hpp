#pragma once

#include "d2asset/asset_link_manifest.hpp"

#include <filesystem>

class AssetReferenceResolver {
public:
    [[nodiscard]] static d2asset::AssetLinkGraph resolve(const std::filesystem::path& package_root);

    static void write(const std::filesystem::path& package_root);
};
