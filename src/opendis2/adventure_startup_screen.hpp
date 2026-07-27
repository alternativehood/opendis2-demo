#pragma once

#include <d2engine/app/adventure_loading_screen.hpp>
#include <d2engine/app/adventure_visual_resources.hpp>
#include <d2engine/app/app_config.hpp>
#include <d2engine/app/screen.hpp>
#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/render/adventure_render_state.hpp>
#include <d2engine/render/render_asset_runtime.hpp>

#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2game/AdventureUnitMovementProfileCatalog.hpp>
#include <d2log/log.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace d2engine {
class Application;
class ScreenConfigStore;
} // namespace d2engine

namespace opendis2 {

struct AdventureStartupScenarioResult;
struct AdventureStartupMapResult;

namespace detail {

struct AdventureBannerPairingStatistics {
    std::size_t map_stack_bodies = 0;
    std::size_t map_stack_banners = 0;

    std::size_t site_bodies = 0;
    std::size_t site_banners = 0;

    std::size_t ruin_bodies = 0;
    std::size_t ruin_banners = 0;

    std::size_t total_bodies = 0;
    std::size_t total_banners = 0;
    std::size_t total_unique_assets = 0;
    std::size_t total_banner_picks = 0;
};

[[nodiscard]] AdventureBannerPairingStatistics collect_adventure_banner_pairing_statistics(
    const d2engine::adventure_render::PreparedAdventureRenderGraph& graph);

[[nodiscard]] std::optional<std::size_t>
advance_adventure_startup_interaction_masks(d2engine::AdventureLoadingScreen& loading_screen,
                                            d2engine::adventure_render::PreparedAdventureMap& map,
                                            std::optional<d2engine::AssetBatchHandle>& mask_batch,
                                            std::size_t expected_stack_pick_entry_count,
                                            float upload_max_progress, float finalize_progress);

} // namespace detail

class AdventureStartupScreen final : public d2engine::Screen {
public:
    enum class Phase : uint8_t {
        Bootstrap,
        ParseScenario,
        LoadGameDataAndPortraits,
        PrepareAdventureMap,
        BuildVisualResources,
        SplitAssets,
        UploadInitialAssets,
        UploadAnimationAssets,
        BuildInteractionMasks,
        Finalize,
        Running,
        Failed
    };

    AdventureStartupScreen(d2engine::Application& app, d2engine::AppConfig config,
                           d2game::AdventureUnitMovementProfileCatalog movement_profiles,
                           std::function<void()>                       request_quit,
                           std::function<void()>                       request_debug_battle);

    AdventureStartupScreen(d2engine::Application& app, d2engine::AppConfig config,
                           std::function<void()> request_quit,
                           std::function<void()> request_debug_battle);

    ~AdventureStartupScreen() override;

    std::string_view name() const override { return "AdventureStartupScreen"; }

    void on_enter() override;
    void update(const d2::app::ScreenUpdateContext& context) override;
    void render(d2engine::Renderer2D& renderer) override;

private:
    void advance_stage();

    struct AssetSplitData {
        AssetSplitData(d2engine::IncrementalRenderAssetPreload initial_preload_value,
                       d2engine::IncrementalRenderAssetPreload animation_preload_value,
                       d2engine::AssetBatchHandle              mask_batch_value)
            : initial_preload(std::move(initial_preload_value)),
              animation_preload(std::move(animation_preload_value)),
              mask_batch(std::move(mask_batch_value)) {}

        std::optional<d2engine::IncrementalRenderAssetPreload> initial_preload;
        std::optional<d2engine::IncrementalRenderAssetPreload> animation_preload;
        std::optional<d2engine::AssetBatchHandle>              mask_batch;
    };

    std::unique_ptr<AdventureStartupScenarioResult> scenario_;
    std::unique_ptr<AdventureStartupMapResult>      map_result_;
    std::unique_ptr<AssetSplitData>                 asset_split_data_;
    d2engine::LoadedAdventureVisualResources        visuals_;

    d2engine::Application&                      app_;
    d2engine::AppConfig                         config_;
    d2game::AdventureUnitMovementProfileCatalog movement_profiles_;

    std::function<void()> request_quit_;
    std::function<void()> request_debug_battle_;

    // Loading UI
    d2engine::AdventureLoadingScreen loading_screen_;
    Phase                            phase_ = Phase::Bootstrap;
    bool                             rendered_once_ = false;

    // Asset split counts (for logging)
    std::size_t initial_asset_count_ = 0;
    std::size_t animation_asset_count_ = 0;
    std::size_t stack_pick_entry_count_ = 0;
};

} // namespace opendis2
