#pragma once

#include <cstddef>
#include <string>

namespace d2engine {

class FfAssetStore;
class GameDataRegistry;
class GameTextureCache;
class AssetRuntime;
class RenderAssetRuntime;
class PortraitManifestIndex;

/// Screen/app runtime context — shared resources passed to every Screen.
///
/// This context is suitable for SDL-backed screens (BattleScreen, future
/// AdventureScreen, etc.) but is NOT the final logical game-data context:
///
///   - GameTextureCache is a render-bound resource (SDL_Texture cache).
///     A future logical game-data context (d2game / d2runtime) must NOT
///     depend on GameTextureCache — it should separate static data from
///     renderer resources.
///
///   - GameDataRegistry is a convenience wrapper that combines
///     d2gamedata::DbfGameDataIndex with extra asset lookups. The two
///     systems (DbfGameDataIndex + GameDataRegistry) coexist today but
///     may converge in the future.
///
/// Relationship between data systems:
///
///   d2gamedata::DbfGameDataIndex   — header-only index of DBF/DLG/DAT files
///                                     (parsed at scan time, stable by commit).
///   GameDataRegistry               — d2engine/assets/ runtime registry that
///                                     wraps DbfGameDataIndex with unit data,
///                                     attack data, death animations, etc.
///
/// AppRuntimeContext exposes GameDataRegistry (not DbfGameDataIndex) because
/// screens need the higher-level registry. This is acceptable as long as the
/// render-bound GameTextureCache dependency is not inherited by non-SDL layers.
struct AppRuntimeContext {
    AssetRuntime&                assets;
    RenderAssetRuntime&          render_assets;
    GameDataRegistry&            game_data;
    const PortraitManifestIndex& portraits;

    [[nodiscard]] FfAssetStore&     store() const;
    [[nodiscard]] GameTextureCache& textures() const;
};

} // namespace d2engine
