#pragma once

#include <d2asset/asset_database.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace d2engine {

class RuntimeAssetContext {
public:
    explicit RuntimeAssetContext(const std::filesystem::path& asset_root);
    ~RuntimeAssetContext() = default;

    // Disable copy/move
    RuntimeAssetContext(const RuntimeAssetContext&) = delete;
    RuntimeAssetContext& operator=(const RuntimeAssetContext&) = delete;
    RuntimeAssetContext(RuntimeAssetContext&&) = delete;
    RuntimeAssetContext& operator=(RuntimeAssetContext&&) = delete;

    [[nodiscard]] const d2asset::AssetDatabase& database() const noexcept { return *database_; }

private:
    std::unique_ptr<d2asset::AssetDatabase> database_;
};

} // namespace d2engine
