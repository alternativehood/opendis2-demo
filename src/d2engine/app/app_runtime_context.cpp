#include "app_runtime_context.hpp"

#include "../assets/asset_runtime.hpp"
#include "../render/render_asset_runtime.hpp"

namespace d2engine {

FfAssetStore& AppRuntimeContext::store() const {
    return assets.store();
}

GameTextureCache& AppRuntimeContext::textures() const {
    return render_assets.textures();
}

} // namespace d2engine
