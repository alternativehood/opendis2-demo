#include "adventure_startup_screen.hpp"
#include "adventure_scenario_loader.hpp"

#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/contained_stack_presentation_contributor.hpp>
#include <d2adventure_render/capital_contributor.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/city_contributor.hpp>
#include <d2adventure_render/site_contributor.hpp>
#include <d2adventure_render/ruin_contributor.hpp>
#include <d2engine/assets/ruin_asset_catalog_builder.hpp>
#include <d2adventure_render/treasure_contributor.hpp>
#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>
#include <d2adventure_render/terrain/capital_asset_catalog.hpp>
#include <d2adventure_render/terrain/ruin_asset_catalog.hpp>
#include <d2engine/assets/ruin_asset_catalog_builder.hpp>
#include <d2adventure_render/terrain/treasure_asset_catalog.hpp>
#include <d2adventure_render/terrain/terrain_asset_catalog.hpp>
#include <d2adventure_render/terrain/tree_asset_catalog.hpp>
#include <d2adventure_render/resource_node_contributor.hpp>
#include <d2engine/app/adventure_interaction_mask.hpp>
#include <d2engine/app/adventure_screen.hpp>
#include <d2engine/app/adventure_visual_resources.hpp>
#include <d2engine/app/application.hpp>
#include <d2engine/app/app_runtime_context.hpp>
#include <d2engine/app/stack_info_screen.hpp>
#include <d2engine/assets/adventure_stack_actor_request_resolver.hpp>
#include <d2engine/assets/adventure_terrain_asset_resolver.hpp>
#include <d2engine/assets/asset_runtime_catalog_adapter.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/assets/iso_actor_visual_resolver.hpp>
#include <d2engine/assets/contained_stack_shield_asset_catalog_builder.hpp>
#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>
#include <d2engine/assets/city_asset_catalog_builder.hpp>
#include <d2engine/assets/adventure_actor_visual_conversion.hpp>
#include <d2engine/assets/capital_visual_resolver.hpp>
#include <d2engine/assets/site_asset_catalog_builder.hpp>
#include <d2engine/assets/landmark_asset_catalog_builder.hpp>
#include <d2engine/assets/capital_asset_catalog_builder.hpp>
#include <d2engine/assets/mountain_asset_catalog_builder.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>
#include <d2engine/assets/resource_node_asset_catalog_builder.hpp>
#include <d2engine/assets/road_asset_catalog_builder.hpp>
#include <d2engine/assets/treasure_asset_catalog_builder.hpp>
#include <d2engine/assets/tree_asset_catalog_builder.hpp>
#include <d2engine/render/adventure_render_state.hpp>
#include <d2engine/render/render_asset_runtime.hpp>
#include <d2engine/render/render_tree.hpp>
#include <d2engine/render/sdl_texture.hpp>

#include <d2runtime/AdventureTerrainDecoder.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <map>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace opendis2 {

namespace ar = d2engine::adventure_render;

// ──
// Phase weight boundaries
// ──
static constexpr float kPwBackground = 0.05f;
static constexpr float kPwScenarioParse = 0.10f;
static constexpr float kPwGameDataPortraits = 0.20f;
static constexpr float kPwMapPreparation = 0.45f;
static constexpr float kPwVisualResources = 0.55f;
static constexpr float kPwUploadMin = 0.55f;
static constexpr float kPwUploadMax = 0.90f;
static constexpr float kPwFinalizeEnd = 1.0f;

// ──
// Internal data structures
// ──
struct AdventureStartupScenarioResult {
    ScenarioLoadResult outcome;
};

struct AdventureStartupMapResult {
    d2engine::adventure_render::PreparedAdventureMap    map;
    d2engine::AdventureTerrainSurface                   terrain_surface;
    d2engine::AdventureRenderState                      render_state;
    d2engine::adventure_render::StackBannerAssetCatalog stack_banner_catalog;
};

[[nodiscard]] bool
is_stack_body_primitive(const d2engine::adventure_render::PreparedAdventureRenderPrimitive& prim) {
    return prim.debug_label.rfind("StackLeader:", 0) == 0;
}

[[nodiscard]] std::size_t
count_contained_stack_primitives(const d2engine::adventure_render::PreparedAdventureMap& map,
                                 std::string_view                                        prefix) {
    std::size_t count = 0;
    for (const auto& prim : map.world_graph.world) {
        if (prim.debug_label.rfind(prefix, 0) == 0) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] std::size_t
count_unique_contained_stack_assets(const d2engine::adventure_render::PreparedAdventureMap& map,
                                    std::string_view prefix) {
    std::unordered_set<std::string> unique_assets;
    for (const auto& prim : map.world_graph.world) {
        if (prim.debug_label.rfind(prefix, 0) != 0) {
            continue;
        }
        unique_assets.insert(prim.container_path + "/" + prim.record_name);
    }
    return unique_assets.size();
}

[[nodiscard]] std::size_t
count_contained_stack_pick_entries(const d2engine::adventure_render::PreparedAdventureMap& map,
                                   std::string_view                                        prefix) {
    std::unordered_set<ar::StableRenderId> rendered_ids;
    for (const auto& prim : map.world_graph.world) {
        if (prim.debug_label.rfind(prefix, 0) == 0) {
            rendered_ids.insert(prim.stable_id);
        }
    }

    std::size_t count = 0;
    for (const auto& entry : map.pick_entries) {
        if (entry.kind == d2engine::adventure_render::PickEntryKind::Stack &&
            rendered_ids.contains(entry.stable_id)) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] std::size_t
count_unique_stack_pick_entry_ids(const d2engine::adventure_render::PreparedAdventureMap& map) {
    std::unordered_set<d2engine::adventure_render::StableRenderId> stack_ids;
    for (const auto& entry : map.pick_entries) {
        if (entry.kind == d2engine::adventure_render::PickEntryKind::Stack) {
            stack_ids.insert(entry.stable_id);
        }
    }
    return stack_ids.size();
}

namespace detail {

namespace {

using Role = d2engine::adventure_render::AdventurePrimitiveRole;

struct RoleCounts {
    std::unordered_map<std::string, std::size_t> bodies;
    std::unordered_map<std::string, std::size_t> banners;
};

[[nodiscard]] bool banner_role(Role role) {
    return d2engine::adventure_render::is_banner_primitive_role(role);
}

[[nodiscard]] std::string role_name(Role role) {
    return std::string(d2engine::adventure_render::adventure_primitive_role_name(role));
}

template <typename Fn>
void for_each_render_primitive(
    const d2engine::adventure_render::PreparedAdventureRenderGraph& graph, Fn&& fn) {
    for (const auto* collection : {&graph.ground_overlay, &graph.world, &graph.world_overlay,
                                   &graph.fog, &graph.ui_overlay}) {
        for (const auto& primitive : *collection) {
            fn(primitive);
        }
    }
}

void bump(std::unordered_map<std::string, std::size_t>& counts, const std::string& object_id) {
    ++counts[object_id];
}

void throw_banner_error(std::string_view kind, Role role, const std::string& object_id,
                        std::size_t count = 0) {
    std::string message = "adventure_banner_";
    message += kind;
    message += " role=";
    message += role_name(role);
    message += " object=";
    message += object_id;
    if (count > 0) {
        message += " count=";
        message += std::to_string(count);
    }
    throw std::runtime_error(message);
}

[[nodiscard]] std::size_t count_unique_banner_assets_for_role(
    const d2engine::adventure_render::PreparedAdventureRenderGraph& graph, Role role) {
    std::unordered_set<std::string> assets;
    for_each_render_primitive(graph, [&](const auto& primitive) {
        if (primitive.semantic_role == role) {
            assets.insert(primitive.container_path + "/" + primitive.record_name);
        }
    });
    return assets.size();
}

void validate_role_counts(Role body_role, Role banner_role, const RoleCounts& counts) {
    for (const auto& [object_id, count] : counts.banners) {
        if (count > 1) {
            throw_banner_error("duplicate_banner", banner_role, object_id, count);
        }
        const auto body_it = counts.bodies.find(object_id);
        if (body_it == counts.bodies.end()) {
            throw_banner_error("orphan", banner_role, object_id);
        }
        if (body_it->second > 1) {
            throw_banner_error("duplicate_body", body_role, object_id, body_it->second);
        }
    }

    for (const auto& [object_id, count] : counts.bodies) {
        if (count > 1) {
            throw_banner_error("duplicate_body", body_role, object_id, count);
        }
        if (counts.banners.find(object_id) == counts.banners.end()) {
            throw_banner_error("missing_for_body", body_role, object_id);
        }
    }
}

} // namespace

[[nodiscard]] AdventureBannerPairingStatistics collect_adventure_banner_pairing_statistics(
    const d2engine::adventure_render::PreparedAdventureRenderGraph& graph) {
    using d2engine::adventure_render::AdventurePrimitiveRole;

    RoleCounts                                                     map_stack;
    RoleCounts                                                     site;
    RoleCounts                                                     ruin;
    std::unordered_set<std::string>                                banner_assets;
    std::unordered_set<d2engine::adventure_render::StableRenderId> banner_ids;

    for_each_render_primitive(graph, [&](const auto& primitive) {
        switch (primitive.semantic_role) {
        case AdventurePrimitiveRole::Unspecified:
            return;
        case AdventurePrimitiveRole::ContainedStackShield:
            return;
        case AdventurePrimitiveRole::MapStackBody:
            bump(map_stack.bodies, primitive.semantic_object_id);
            return;
        case AdventurePrimitiveRole::MapStackBanner:
            if (!banner_role(primitive.semantic_role)) {
                return;
            }
            if (primitive.visibility_group !=
                d2engine::adventure_render::AdventureRenderVisibilityGroup::Banners) {
                throw std::runtime_error("adventure_banner_invalid_visibility_group role=" +
                                         role_name(primitive.semantic_role) +
                                         " object=" + primitive.semantic_object_id);
            }
            if (primitive.interaction_mask != nullptr) {
                throw std::runtime_error("adventure_banner_has_interaction_mask role=" +
                                         role_name(primitive.semantic_role) +
                                         " object=" + primitive.semantic_object_id);
            }
            banner_assets.insert(primitive.container_path + "/" + primitive.record_name);
            banner_ids.insert(primitive.stable_id);
            bump(map_stack.banners, primitive.semantic_object_id);
            return;
        case AdventurePrimitiveRole::SiteBody:
            bump(site.bodies, primitive.semantic_object_id);
            return;
        case AdventurePrimitiveRole::SiteBanner:
            if (primitive.visibility_group !=
                d2engine::adventure_render::AdventureRenderVisibilityGroup::Banners) {
                throw std::runtime_error("adventure_banner_invalid_visibility_group role=" +
                                         role_name(primitive.semantic_role) +
                                         " object=" + primitive.semantic_object_id);
            }
            if (primitive.interaction_mask != nullptr) {
                throw std::runtime_error("adventure_banner_has_interaction_mask role=" +
                                         role_name(primitive.semantic_role) +
                                         " object=" + primitive.semantic_object_id);
            }
            banner_assets.insert(primitive.container_path + "/" + primitive.record_name);
            banner_ids.insert(primitive.stable_id);
            bump(site.banners, primitive.semantic_object_id);
            return;
        case AdventurePrimitiveRole::RuinBody:
            bump(ruin.bodies, primitive.semantic_object_id);
            return;
        case AdventurePrimitiveRole::RuinBanner:
            if (primitive.visibility_group !=
                d2engine::adventure_render::AdventureRenderVisibilityGroup::Banners) {
                throw std::runtime_error("adventure_banner_invalid_visibility_group role=" +
                                         role_name(primitive.semantic_role) +
                                         " object=" + primitive.semantic_object_id);
            }
            if (primitive.interaction_mask != nullptr) {
                throw std::runtime_error("adventure_banner_has_interaction_mask role=" +
                                         role_name(primitive.semantic_role) +
                                         " object=" + primitive.semantic_object_id);
            }
            banner_assets.insert(primitive.container_path + "/" + primitive.record_name);
            banner_ids.insert(primitive.stable_id);
            bump(ruin.banners, primitive.semantic_object_id);
            return;
        }
    });

    for (const auto& entry : graph.pick_entries) {
        if (banner_ids.contains(entry.stable_id)) {
            throw std::runtime_error("adventure_banner_has_pick_entry stable_id=" +
                                     std::to_string(entry.stable_id));
        }
    }

    validate_role_counts(AdventurePrimitiveRole::MapStackBody,
                         AdventurePrimitiveRole::MapStackBanner, map_stack);
    validate_role_counts(AdventurePrimitiveRole::SiteBody, AdventurePrimitiveRole::SiteBanner,
                         site);
    validate_role_counts(AdventurePrimitiveRole::RuinBody, AdventurePrimitiveRole::RuinBanner,
                         ruin);

    AdventureBannerPairingStatistics stats;
    stats.map_stack_bodies = map_stack.bodies.size();
    stats.map_stack_banners = map_stack.banners.size();
    stats.site_bodies = site.bodies.size();
    stats.site_banners = site.banners.size();
    stats.ruin_bodies = ruin.bodies.size();
    stats.ruin_banners = ruin.banners.size();
    stats.total_bodies = stats.map_stack_bodies + stats.site_bodies + stats.ruin_bodies;
    stats.total_banners = stats.map_stack_banners + stats.site_banners + stats.ruin_banners;
    stats.total_unique_assets = banner_assets.size();
    stats.total_banner_picks = 0;
    return stats;
}

} // namespace detail

[[nodiscard]] std::size_t count_settlement_stack_refs(const d2runtime::AdventureWorldState& world) {
    std::size_t count = 0;
    for (const auto& city : world.cities) {
        if (!city.stack_id.empty() && city.stack_id != "G000000000") {
            ++count;
        }
    }
    for (const auto& capital : world.capitals) {
        if (!capital.visiting_stack_id.empty() && capital.visiting_stack_id != "G000000000") {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] std::string describe_image_key(const d2engine::ImageAssetKey& key) {
    return key.container_path + "/" + key.image_name;
}

namespace detail {

[[nodiscard]] std::optional<std::size_t>
advance_adventure_startup_interaction_masks(d2engine::AdventureLoadingScreen& loading_screen,
                                            d2engine::adventure_render::PreparedAdventureMap& map,
                                            std::optional<d2engine::AssetBatchHandle>& mask_batch,
                                            std::size_t expected_stack_pick_entry_count,
                                            float upload_max_progress, float finalize_progress) {
    if (!mask_batch.has_value()) {
        throw std::logic_error("AdventureStartupScreen::BuildInteractionMasks missing mask batch");
    }

    const auto&                                handles = mask_batch->handles();
    std::vector<d2engine::PreparedImageResult> decoded_images;
    decoded_images.reserve(handles.size());

    std::size_t ready_count = 0;
    for (const auto& handle : handles) {
        if (handle.ready()) {
            ++ready_count;
        }
    }

    if (handles.empty()) {
        loading_screen.set_progress(std::max(loading_screen.progress(), finalize_progress));
        mask_batch.reset();
    } else {
        const float ready_ratio =
            static_cast<float>(ready_count) / static_cast<float>(handles.size());
        const float next_progress =
            upload_max_progress + (finalize_progress - upload_max_progress) * ready_ratio;
        loading_screen.set_progress(std::max(loading_screen.progress(), next_progress));

        if (ready_count != handles.size()) {
            return std::nullopt;
        }
    }

    for (const auto& handle : handles) {
        const auto result = handle.get();
        if (!result.success) {
            throw std::runtime_error("AdventureStartupScreen: interaction mask decode failed key=" +
                                     describe_image_key(result.key) + " error=" + result.error);
        }
        if (result.image == nullptr || result.image->pixels == nullptr) {
            throw std::runtime_error(
                "AdventureStartupScreen: interaction mask missing pixels key=" +
                describe_image_key(result.key) + " error=" + result.error);
        }
        decoded_images.push_back(result);
    }

    const auto built = d2engine::attach_stack_interaction_masks(map, decoded_images);
    if (built != expected_stack_pick_entry_count) {
        throw std::runtime_error(
            "AdventureStartupScreen: interaction mask count mismatch expected=" +
            std::to_string(expected_stack_pick_entry_count) + " actual=" + std::to_string(built));
    }

    mask_batch.reset();
    loading_screen.set_progress(finalize_progress);
    return built;
}

} // namespace detail

// ──
// prepare_full_map (moved from adventure_launcher.cpp anonymous ns)
// ──
static AdventureStartupMapResult prepare_full_map(const d2game::GameSession& session,
                                                  d2engine::Application&     app) {
    using namespace d2engine;
    using namespace d2engine::adventure_render;
    using namespace d2runtime;
    auto logger = d2log::get("d2.adventure");

    const auto& world = session.world();
    const auto  geometry = AdventureMapGeometry::from_source(world.map_width, world.map_height);

    // Terrain catalog + decoder
    TerrainAssetCatalogBuilder catalog_builder;
    const auto                 catalog = catalog_builder.build(app.asset_runtime().store());

    const AdventureTerrainDecoder    tile_decoder;
    const AdventureTerrainMapDecoder map_decoder(tile_decoder);
    const auto                       descriptors = map_decoder.decode_grid(world.terrain);

    const auto resolved = AdventureTerrainAssetResolver(catalog).resolve_all(descriptors);

    // Catalogs
    const auto tree_catalog = build_tree_asset_catalog(app.asset_runtime().store());
    const auto mountain_catalog = build_mountain_asset_catalog(app.asset_runtime().store());
    const auto city_catalog = build_city_asset_catalog(app.asset_runtime().store());
    const auto ruin_catalog = build_ruin_asset_catalog(app.asset_runtime().store());
    const auto treasure_catalog = build_treasure_asset_catalog(app.asset_runtime().store());
    const auto site_catalog = build_site_asset_catalog(app.asset_runtime().store());
    const auto landmark_catalog = build_landmark_asset_catalog(app.asset_runtime().store());
    const auto capital_catalog = build_capital_asset_catalog(app.asset_runtime().store());
    const auto contained_stack_shield_catalog =
        build_contained_stack_shield_asset_catalog(app.asset_runtime().store());
    const auto contained_stack_banner_catalog =
        build_stack_banner_asset_catalog(app.asset_runtime().store());
    logger->info("adventure_ruin_asset_catalog land=9 water=9 unsupported_images=2");
    logger->info("adventure_contained_stack_shield_catalog entries={}",
                 contained_stack_shield_catalog.assets.size());
    logger->info("adventure_stack_banner_catalog entries={}",
                 contained_stack_banner_catalog.frames.size());
    const auto road_catalog = build_road_asset_catalog(app.asset_runtime().store());
    const auto resource_node_catalog =
        build_resource_node_asset_catalog(app.asset_runtime().store());

    // Terrain assets (blocking, but happens during map preparation phase)
    {
        std::vector<ImageAssetKey>      source_assets;
        std::unordered_set<std::string> seen_assets;
        auto add_source_asset = [&](const d2runtime::AdventureTerrainAssetRef& asset) {
            if (asset.record_name.empty())
                return;
            const std::string id = asset.container_path + "/" + asset.record_name;
            if (!seen_assets.insert(id).second)
                return;
            source_assets.push_back(ImageAssetKey{.container_path = asset.container_path,
                                                  .image_name = asset.record_name,
                                                  .kind = ImageAssetKind::RawPng});
        };
        for (const auto& tile : resolved) {
            if (tile.ground_asset_found)
                add_source_asset(tile.descriptor.expected_ground_asset);
            if (tile.border.has_value() && tile.border->asset_found &&
                tile.border->descriptor.expected_asset.has_value())
                add_source_asset(*tile.border->descriptor.expected_asset);
        }
        static_cast<void>(app.asset_runtime().request_batch(source_assets, AssetPriority::Critical,
                                                            "AdventureTerrain"));
    }

    AssetRuntimeCatalogAdapter      iso_catalog(app.runtime_context().assets);
    const auto&                     game_data = app.runtime_context().game_data;
    d2engine::CapitalVisualResolver capital_visual_resolver(capital_catalog, game_data);

    d2engine::AdventureStackActorRequestResolver request_resolver(game_data);
    d2engine::IsoActorVisualResolver             iso_resolver(iso_catalog, game_data);

    std::unordered_map<std::string, d2engine::adventure_render::AdventureActorVisual> visual_cache;
    std::size_t map_visible_stacks = 0;
    std::size_t unit_presentations = 0;
    std::size_t boat_presentations = 0;
    std::size_t unresolved = 0;
    std::size_t logged_stack_provenance = 0;

    // Shadow tracking
    std::size_t                     boats_with_visible_shadow = 0;
    std::size_t                     boats_missing_shadow = 0;
    std::size_t                     boats_authored_empty_shadow = 0;
    std::size_t                     static_boats = 0;
    std::size_t                     animated_boats = 0;
    std::unordered_set<std::string> unique_boat_sequences;
    std::unordered_set<std::string> unique_boat_shadow_sequences;
    std::size_t                     boat_body_frames = 0;
    std::size_t                     boat_shadow_frames = 0;
    std::size_t                     boat_shared_clocks = 0;

    std::size_t                     unit_with_visible_shadow = 0;
    std::size_t                     unit_missing_shadow = 0;
    std::size_t                     unit_authored_empty_shadow = 0;
    std::size_t                     unit_static_shadow_pairs = 0;
    std::size_t                     unit_animated_shadow_pairs = 0;
    std::unordered_set<std::string> unit_unique_shadow_sequences;
    std::size_t                     unit_total_shadow_frames = 0;

    for (const auto& stack : world.stacks) {
        if (!d2runtime::is_stack_on_adventure_map(stack))
            continue;
        ++map_visible_stacks;
        const auto request = request_resolver.resolve(world, stack);
        auto       iso = iso_resolver.resolve(request);
        if (!iso.has_value()) {
            ++unresolved;
            if (logged_stack_provenance < 8) {
                ++logged_stack_provenance;
                d2log::get("d2.adventure")
                    ->debug("stack_provenance stack={} pos=({},{}) leader={} type={} "
                            "presentation={} visual=unresolved",
                            stack.id, stack.position.x, stack.position.y, stack.leader_id,
                            request.leader_unit_type_id,
                            static_cast<int>(request.presentation.kind));
            }
            d2log::get("d2.adventure")
                ->warn("unresolved_stack_visual stack={} leader={} type={} presentation={}",
                       stack.id, stack.leader_id, request.leader_unit_type_id,
                       static_cast<int>(request.presentation.kind));
            continue;
        }

        const auto visual = d2engine::to_adventure_actor_visual(*iso);

        const std::string vis_key = "vis:" + stack.id;
        visual_cache[vis_key] = visual;

        if (request.presentation.kind == d2runtime::AdventureActorPresentationKind::Boat) {
            ++boat_presentations;
            const bool is_static = visual.body.frames.size() <= 1;
            if (is_static)
                ++static_boats;
            else
                ++animated_boats;
            unique_boat_sequences.insert(visual.body.logical_animation_name);
            boat_body_frames += visual.body.frames.size();
            switch (iso->shadow_presence) {
            case d2engine::ExactLayerPresence::Visible:
                ++boats_with_visible_shadow;
                break;
            case d2engine::ExactLayerPresence::AuthoredEmpty:
                ++boats_authored_empty_shadow;
                break;
            case d2engine::ExactLayerPresence::Missing:
                ++boats_missing_shadow;
                break;
            }
            if (visual.shadow.has_value()) {
                unique_boat_shadow_sequences.insert(visual.shadow->logical_animation_name);
                boat_shadow_frames += visual.shadow->frames.size();
                if (!is_static)
                    ++boat_shared_clocks;
            }
        } else {
            ++unit_presentations;
            switch (iso->shadow_presence) {
            case d2engine::ExactLayerPresence::Visible:
                ++unit_with_visible_shadow;
                break;
            case d2engine::ExactLayerPresence::AuthoredEmpty:
                ++unit_authored_empty_shadow;
                break;
            case d2engine::ExactLayerPresence::Missing:
                ++unit_missing_shadow;
                break;
            }
            if (visual.shadow.has_value()) {
                unit_unique_shadow_sequences.insert(visual.shadow->logical_animation_name);
                unit_total_shadow_frames += visual.shadow->frames.size();
                if (visual.shadow->frames.size() <= 1)
                    ++unit_static_shadow_pairs;
                else
                    ++unit_animated_shadow_pairs;
            }
        }

        d2engine::adventure_render::validate_adventure_actor_visual_or_throw(
            stack, *world.find_unit(stack.leader_id), visual);

        if (logged_stack_provenance < 8) {
            ++logged_stack_provenance;
            const bool  is_static = visual.body.frames.size() <= 1;
            const bool  has_shadow = visual.shadow.has_value();
            const char* kind_str =
                (request.presentation.kind == d2runtime::AdventureActorPresentationKind::Boat)
                    ? "Boat"
                    : "Unit";
            d2log::get("d2.adventure")
                ->debug("stack_facing stack={} pos=({},{}) raw_facing={} direction=D{} "
                        "body_anim={} body_frames={} kind={} presentation={} shadow={} "
                        "shadow_frames={} owner={} shadow_presence={}",
                        stack.id, stack.position.x, stack.position.y,
                        d2runtime::direction_index(stack.facing),
                        d2runtime::direction_index(stack.facing),
                        visual.body.logical_animation_name, visual.body.frames.size(),
                        is_static ? "static" : "animated", kind_str,
                        has_shadow ? visual.shadow->logical_animation_name : "<absent>",
                        has_shadow ? visual.shadow->frames.size() : 0ul, visual.resolved_owner_id,
                        static_cast<int>(iso->shadow_presence));
        }
    }

    AdventureTerrainSurfaceInput terrain_input;
    terrain_input.map_width = world.map_width;
    terrain_input.map_height = world.map_height;
    terrain_input.descriptors = descriptors;
    terrain_input.resolved_tiles = resolved;

    const AdventureTerrainSurfaceComposer composer(app.asset_runtime().image_store(), catalog);
    AdventureMapPreparer                  preparer(geometry);
    preparer.set_terrain_composer(composer, catalog);
    preparer.add_contributor(make_road_contributor(road_catalog));
    preparer.add_contributor(make_resource_node_contributor(resource_node_catalog));
    preparer.add_contributor(make_mountain_contributor(mountain_catalog));
    preparer.add_contributor(make_ruin_contributor(ruin_catalog, contained_stack_banner_catalog));
    preparer.add_contributor(make_site_contributor(site_catalog, contained_stack_banner_catalog));
    preparer.add_contributor(make_city_contributor(city_catalog));
    preparer.add_contributor(make_treasure_contributor(treasure_catalog));
    preparer.add_contributor(make_landmark_contributor(landmark_catalog));
    preparer.add_contributor(make_capital_contributor(
        capital_catalog, [&capital_visual_resolver](
                             const d2runtime::AdventureWorldState& world_state,
                             const d2runtime::AdventureCapital& capital, std::string_view race_id) {
            return capital_visual_resolver.resolve(world_state, capital, race_id);
        }));
    preparer.add_contributor(
        make_contained_stack_presentation_contributor(contained_stack_shield_catalog));
    preparer.add_contributor(make_forest_contributor(tree_catalog, descriptors));
    preparer.add_contributor(make_stack_actor_contributor(
        [&](const d2runtime::AdventureStack& stack,
            const d2runtime::AdventureUnitInstance&) -> std::optional<AdventureActorVisual> {
            const std::string vis_key = "vis:" + stack.id;
            auto              it = visual_cache.find(vis_key);
            if (it != visual_cache.end())
                return it->second;
            return std::nullopt;
        },
        &contained_stack_banner_catalog));

    auto prepared_map = preparer.prepare_full(world, terrain_input);

    {
        std::size_t                     typed_ruins = world.ruins.size();
        std::size_t                     land_static = 0;
        std::size_t                     land_animated = 0;
        std::size_t                     water_static = 0;
        std::size_t                     water_animated = 0;
        std::size_t                     unresolved_ruins = 0;
        std::unordered_set<std::string> unique_visuals;
        for (const auto& prim : prepared_map.world_graph.world) {
            if (!prim.debug_label.starts_with("Ruin:"))
                continue;
            const auto id = prim.debug_label.substr(5);
            const auto ruin =
                std::find_if(world.ruins.begin(), world.ruins.end(),
                             [&](const auto& candidate) { return candidate.id == id; });
            if (ruin == world.ruins.end())
                continue;
            if (ruin->placement == d2runtime::AdventureSurfacePlacement::Water) {
                if (prim.animation.has_value())
                    ++water_animated;
                else
                    ++water_static;
            } else if (prim.animation.has_value()) {
                ++land_animated;
            } else {
                ++land_static;
            }
            unique_visuals.insert(
                prim.container_path + "/" +
                (prim.animation.has_value() ? prim.animation->animation_name : prim.record_name));
        }
        for (const auto& diag : prepared_map.diagnostics) {
            if (diag.object_kind == "Ruin" &&
                diag.kind != d2engine::adventure_render::PrepareDiagnosticKind::Resolved)
                ++unresolved_ruins;
        }
        d2log::get("d2.adventure")
            ->info("adventure_ruins typed={} land_static={} land_animated={} water_static={} "
                   "water_animated={} unresolved={} unique_visuals={}",
                   typed_ruins, land_static, land_animated, water_static, water_animated,
                   unresolved_ruins, unique_visuals.size());
    }

    {
        std::size_t                     typed_sites = 0;
        std::size_t                     mage_sites = 0;
        std::size_t                     merchant_sites = 0;
        std::size_t                     mercenary_sites = 0;
        std::size_t                     trainer_sites = 0;
        std::size_t                     static_bodies = 0;
        std::size_t                     animated_bodies = 0;
        std::size_t                     shadows = 0;
        std::size_t                     unresolved_sites = 0;
        std::unordered_set<std::string> unique_visuals;

        for (const auto& site : world.sites) {
            ++typed_sites;
            switch (site.kind) {
            case d2runtime::AdventureSiteKind::Mage:
                ++mage_sites;
                break;
            case d2runtime::AdventureSiteKind::Merchant:
                ++merchant_sites;
                break;
            case d2runtime::AdventureSiteKind::Mercenary:
                ++mercenary_sites;
                break;
            case d2runtime::AdventureSiteKind::Trainer:
                ++trainer_sites;
                break;
            }
        }

        for (const auto& diag : prepared_map.diagnostics) {
            if (diag.object_kind == "Site" &&
                diag.kind ==
                    d2engine::adventure_render::PrepareDiagnosticKind::UnresolvedNoSprite) {
                ++unresolved_sites;
            }
        }

        for (const auto& prim : prepared_map.world_graph.world) {
            if (!prim.debug_label.starts_with("Site:")) {
                continue;
            }
            if (prim.debug_label.ends_with(":body")) {
                if (prim.animation.has_value()) {
                    ++animated_bodies;
                    unique_visuals.insert(prim.container_path + "/" +
                                          prim.animation->animation_name);
                } else {
                    ++static_bodies;
                    unique_visuals.insert(prim.container_path + "/" + prim.record_name);
                }
            } else if (prim.debug_label.ends_with(":shadow")) {
                ++shadows;
                if (prim.animation.has_value()) {
                    unique_visuals.insert(prim.container_path + "/" +
                                          prim.animation->animation_name);
                } else {
                    unique_visuals.insert(prim.container_path + "/" + prim.record_name);
                }
            }
        }

        d2log::get("d2.adventure")
            ->info("adventure_sites typed={} mage={} merchants={} mercenaries={} trainers={} "
                   "static_bodies={} animated_bodies={} shadows={} unresolved={} unique_visuals={}",
                   typed_sites, mage_sites, merchant_sites, mercenary_sites, trainer_sites,
                   static_bodies, animated_bodies, shadows, unresolved_sites,
                   unique_visuals.size());
    }

    // Logging summaries
    d2log::get("d2.adventure")
        ->info("adventure_stack_presentations visible_stacks={} unit_presentations={} "
               "boat_presentations={} unresolved={}",
               map_visible_stacks, unit_presentations, boat_presentations, unresolved);

    {
        std::size_t                     visible_stacks = 0, static_stacks = 0, animated_stacks = 0;
        std::unordered_set<std::string> unique_sequences;
        std::size_t                     total_animation_frames = 0;
        for (const auto& prim : prepared_map.world_graph.world) {
            if (!is_stack_body_primitive(prim))
                continue;
            ++visible_stacks;
            if (prim.animation.has_value()) {
                ++animated_stacks;
                unique_sequences.insert(prim.animation->animation_name);
                total_animation_frames += prim.animation->frames.size();
            } else {
                ++static_stacks;
            }
        }
        d2log::get("d2.adventure")
            ->info("adventure_actor_idle_animations visible_stacks={} static_stacks={} "
                   "animated_stacks={} unique_sequences={} total_animation_frames={} "
                   "looping=true frame_duration_ms={} timing_source=provisional_fallback",
                   visible_stacks, static_stacks, animated_stacks, unique_sequences.size(),
                   total_animation_frames,
                   d2engine::adventure_render::AdventureActorIdlePlaybackPolicy::frame_duration_ms);
        d2log::get("d2.adventure")
            ->info("adventure_actor_idle_shadows visible_stacks={} with_visible_shadow={} "
                   "missing_shadow={} authored_empty_shadow={} static_pairs={} animated_pairs={} "
                   "unique_shadow_sequences={} total_shadow_frames={} "
                   "timing_source=provisional_fallback",
                   unit_presentations, unit_with_visible_shadow, unit_missing_shadow,
                   unit_authored_empty_shadow, unit_static_shadow_pairs, unit_animated_shadow_pairs,
                   unit_unique_shadow_sequences.size(), unit_total_shadow_frames);
    }

    if (boat_presentations > 0) {
        d2log::get("d2.adventure")
            ->info("adventure_boat_idle_animations boats={} shadow_visible={} "
                   "shadow_missing={} shadow_authored_empty={} "
                   "static_boats={} animated_boats={} "
                   "unique_boat_sequences={} unique_boat_shadow_sequences={} "
                   "body_frames={} shadow_frames={} "
                   "shared_animation_clocks={} timing_source=provisional_fallback",
                   boat_presentations, boats_with_visible_shadow, boats_missing_shadow,
                   boats_authored_empty_shadow, static_boats, animated_boats,
                   unique_boat_sequences.size(), unique_boat_shadow_sequences.size(),
                   boat_body_frames, boat_shadow_frames, boat_shared_clocks);
    }

    {
        const auto unresolved_visuals_for = [&](std::string_view kind) {
            std::size_t count = 0;
            for (const auto& diag : prepared_map.diagnostics) {
                if (diag.object_kind == kind &&
                    diag.kind ==
                        d2engine::adventure_render::PrepareDiagnosticKind::UnresolvedNoSprite) {
                    ++count;
                }
            }
            return count;
        };

        const std::size_t settlements_with_stack = count_settlement_stack_refs(world);
        const std::size_t shield_rendered =
            count_contained_stack_primitives(prepared_map, "ContainedStackShield:");
        const std::size_t shield_unique_assets =
            count_unique_contained_stack_assets(prepared_map, "ContainedStackShield:");
        const std::size_t shield_picks =
            count_contained_stack_pick_entries(prepared_map, "ContainedStackShield:");

        if (settlements_with_stack != shield_rendered) {
            throw std::runtime_error(
                "adventure_contained_stack_shield_count_mismatch settlements_with_stack=" +
                std::to_string(settlements_with_stack) +
                " rendered=" + std::to_string(shield_rendered));
        }
        if (settlements_with_stack != shield_picks) {
            throw std::runtime_error(
                "adventure_contained_stack_shield_pick_count_mismatch settlements_with_stack=" +
                std::to_string(settlements_with_stack) + " picks=" + std::to_string(shield_picks));
        }

        const auto banner_stats =
            opendis2::detail::collect_adventure_banner_pairing_statistics(prepared_map.world_graph);

        const auto stack_unique_assets = opendis2::detail::count_unique_banner_assets_for_role(
            prepared_map.world_graph,
            d2engine::adventure_render::AdventurePrimitiveRole::MapStackBanner);
        const auto site_unique_assets = opendis2::detail::count_unique_banner_assets_for_role(
            prepared_map.world_graph,
            d2engine::adventure_render::AdventurePrimitiveRole::SiteBanner);
        const auto ruin_unique_assets = opendis2::detail::count_unique_banner_assets_for_role(
            prepared_map.world_graph,
            d2engine::adventure_render::AdventurePrimitiveRole::RuinBanner);

        d2log::get("d2.adventure")
            ->info("adventure_contained_stack_shields settlements_with_stack={} rendered={} "
                   "unique_assets={} picks={}",
                   settlements_with_stack, shield_rendered, shield_unique_assets, shield_picks);
        d2log::get("d2.adventure")
            ->info(
                "adventure_map_stack_banners typed={} resolved_bodies={} unresolved={} rendered={} "
                "unique_assets={} picks=0",
                map_visible_stacks, banner_stats.map_stack_bodies, unresolved,
                banner_stats.map_stack_banners, stack_unique_assets);
        d2log::get("d2.adventure")
            ->info("adventure_site_banners typed={} resolved_bodies={} unresolved={} rendered={} "
                   "unique_assets={} picks=0",
                   world.sites.size(), banner_stats.site_bodies, unresolved_visuals_for("Site"),
                   banner_stats.site_banners, site_unique_assets);
        d2log::get("d2.adventure")
            ->info("adventure_ruin_banners typed={} resolved_bodies={} unresolved={} rendered={} "
                   "unique_assets={} picks=0",
                   world.ruins.size(), banner_stats.ruin_bodies, unresolved_visuals_for("Ruin"),
                   banner_stats.ruin_banners, ruin_unique_assets);
        d2log::get("d2.adventure")
            ->info("adventure_all_banners resolved_bodies={} rendered={} unique_assets={} picks={}",
                   banner_stats.total_bodies, banner_stats.total_banners,
                   banner_stats.total_unique_assets, banner_stats.total_banner_picks);
    }

    {
        const std::size_t               typed_cities = world.cities.size();
        std::size_t                     rendered_city_bodies = 0;
        std::size_t                     rendered_city_shadows = 0;
        std::unordered_set<std::string> unique_city_animations;
        std::size_t                     unresolved_cities = 0;

        for (const auto& prim : prepared_map.world_graph.world) {
            if (!prim.debug_label.starts_with("City:")) {
                continue;
            }

            if (prim.debug_label.ends_with(":body")) {
                ++rendered_city_bodies;
            } else if (prim.debug_label.ends_with(":shadow")) {
                ++rendered_city_shadows;
            }

            if (prim.animation.has_value()) {
                unique_city_animations.insert(prim.animation->animation_name);
            }
        }

        for (const auto& diag : prepared_map.diagnostics) {
            if (diag.object_kind == "City" &&
                diag.kind != d2engine::adventure_render::PrepareDiagnosticKind::Resolved) {
                ++unresolved_cities;
            }
        }

        d2log::get("d2.adventure")
            ->info("adventure_cities typed={} rendered_bodies={} rendered_shadows={} "
                   "unresolved={} unique_city_animations={}",
                   typed_cities, rendered_city_bodies, rendered_city_shadows, unresolved_cities,
                   unique_city_animations.size());
    }

    {
        const std::size_t               typed_treasures = world.treasures.size();
        std::size_t                     rendered_treasures = 0;
        std::size_t                     unresolved_treasures = 0;
        std::size_t                     land_rendered = 0;
        std::size_t                     water_rendered = 0;
        std::size_t                     land_unresolved = 0;
        std::size_t                     water_unresolved = 0;
        std::unordered_set<std::string> rendered_treasure_ids;

        for (const auto& prim : prepared_map.world_graph.world) {
            if (prim.debug_label.starts_with("Treasure:")) {
                rendered_treasure_ids.insert(prim.debug_label.substr(9));
            }
        }
        rendered_treasures = rendered_treasure_ids.size();

        for (const auto& treasure : world.treasures) {
            const bool rendered = rendered_treasure_ids.contains(treasure.id);
            if (rendered) {
                if (treasure.placement == d2runtime::AdventureTreasurePlacement::Water) {
                    ++water_rendered;
                } else {
                    ++land_rendered;
                }
            }
        }

        for (const auto& diag : prepared_map.diagnostics) {
            if (diag.object_kind == "Treasure" &&
                diag.kind != d2engine::adventure_render::PrepareDiagnosticKind::Resolved) {
                ++unresolved_treasures;
            }
        }

        for (const auto& treasure : world.treasures) {
            if (!rendered_treasure_ids.contains(treasure.id)) {
                if (treasure.placement == d2runtime::AdventureTreasurePlacement::Water) {
                    ++water_unresolved;
                } else {
                    ++land_unresolved;
                }
            }
        }

        d2log::get("d2.adventure")
            ->info("adventure_treasures typed={} rendered={} unresolved={} land_rendered={} "
                   "water_rendered={} land_unresolved={} water_unresolved={}",
                   typed_treasures, rendered_treasures, unresolved_treasures, land_rendered,
                   water_rendered, land_unresolved, water_unresolved);
    }

    // Landmark summary
    {
        std::size_t                                  lm_parsed = world.landmarks.size();
        std::size_t                                  lm_resolved_static = 0;
        std::size_t                                  lm_resolved_animated = 0;
        std::size_t                                  lm_unresolved = 0;
        std::unordered_map<std::string, std::size_t> lm_unresolved_types;
        for (const auto& lm : world.landmarks) {
            const auto* visual = landmark_catalog.find(lm.type_id);
            if (visual == nullptr) {
                ++lm_unresolved;
                ++lm_unresolved_types[lm.type_id];
            } else if (std::holds_alternative<d2engine::adventure_render::AnimatedLandmarkVisual>(
                           *visual)) {
                ++lm_resolved_animated;
            } else {
                ++lm_resolved_static;
            }
        }
        d2log::get("d2.adventure")
            ->info("adventure_landmarks parsed={} runtime={} resolved_static={} "
                   "resolved_animated={} unresolved={}",
                   lm_parsed, world.landmarks.size(), lm_resolved_static, lm_resolved_animated,
                   lm_unresolved);
        for (const auto& [type, count] : lm_unresolved_types)
            d2log::get("d2.adventure")
                ->info("unresolved_landmark_type type={} instances={}", type, count);
    }

    // Road summary
    {
        std::size_t                parsed = world.roads.size();
        std::size_t                runtime = world.roads.size();
        std::size_t                roads_resolved = 0;
        std::size_t                roads_unresolved = 0;
        std::map<int, std::size_t> unresolved_indices;
        for (const auto& road : world.roads) {
            const auto* visual = road_catalog.find(road.index);
            if (visual == nullptr) {
                ++roads_unresolved;
                ++unresolved_indices[road.index];
            } else {
                ++roads_resolved;
            }
        }
        d2log::get("d2.adventure")
            ->info("adventure_roads parsed={} runtime={} resolved={} unresolved={}", parsed,
                   runtime, roads_resolved, roads_unresolved);
        for (const auto& [idx, count] : unresolved_indices)
            d2log::get("d2.adventure")
                ->info("unresolved_road_index index={} instances={}", idx, count);
    }

    // CPU-render terrain surface
    d2engine::AdventureTerrainSurface terrain_surface;
    if (prepared_map.has_terrain()) {
        terrain_surface = composer.render_prepared_full_map(*prepared_map.terrain);
    }

    auto render_state =
        d2engine::AdventureRenderState::from_terrain_surface(app.renderer(), terrain_surface);

    return {std::move(prepared_map), std::move(terrain_surface), std::move(render_state),
            std::move(contained_stack_banner_catalog)};
}

// ──
// Constructor
// ──
AdventureStartupScreen::AdventureStartupScreen(
    d2engine::Application& app, d2engine::AppConfig config,
    d2game::AdventureUnitMovementProfileCatalog movement_profiles,
    std::function<void()> request_quit, std::function<void()> request_debug_battle)
    : Screen(d2engine::TreeLayout{}, "adventure_startup_screen"), app_(app),
      config_(std::move(config)), movement_profiles_(std::move(movement_profiles)),
      request_quit_(std::move(request_quit)),
      request_debug_battle_(std::move(request_debug_battle)), loading_screen_(nullptr) {}

AdventureStartupScreen::AdventureStartupScreen(d2engine::Application& app,
                                               d2engine::AppConfig    config,
                                               std::function<void()>  request_quit,
                                               std::function<void()>  request_debug_battle)
    : AdventureStartupScreen(app, std::move(config), {}, std::move(request_quit),
                             std::move(request_debug_battle)) {}

AdventureStartupScreen::~AdventureStartupScreen() = default;

// ──
// on_enter — load background texture
// ──
void AdventureStartupScreen::on_enter() {
    auto logger = d2log::get("d2.adventure");
    logger->info("adventure_loading_screen entered");

    auto& store = app_.asset_runtime().store();

    // Authored composed loading-screen sprite.
    d2res::RgbaBuffer background;
    try {
        background = store.decode_sprite("Interf/Interf.ff", "LOADING");
    } catch (const std::exception& e) {
        throw std::runtime_error("AdventureStartupScreen: authored composed loading-screen sprite "
                                 "could not be decoded for Interf/Interf.ff/LOADING via "
                                 "physical source LOADING_D2ELF.PNG: " +
                                 std::string(e.what()));
    }
    if (background.width == 0 || background.height == 0 || background.rgba.empty()) {
        throw std::runtime_error(
            "AdventureStartupScreen: authored composed loading-screen sprite could not be "
            "decoded for Interf/Interf.ff/LOADING (physical source LOADING_D2ELF.PNG)");
    }

    auto tex = d2engine::create_sdl_texture(
        app_.renderer(), static_cast<int>(background.width), static_cast<int>(background.height),
        background.rgba.data(), static_cast<int>(background.width) * 4);
    if (!tex) {
        throw std::runtime_error(
            "AdventureStartupScreen: texture creation failed for Interf/Interf.ff/LOADING "
            "(physical source LOADING_D2ELF.PNG)");
    }

    loading_screen_ = d2engine::AdventureLoadingScreen(tex.release());
    logger->info("loading_background loaded source=Interf/Interf.ff logical=LOADING "
                 "physical=LOADING_D2ELF.PNG size={}x{} composed=true",
                 background.width, background.height);
    loading_screen_.set_progress(kPwBackground);
}

// ──
// update
// ──
void AdventureStartupScreen::update(const d2::app::ScreenUpdateContext& /*context*/) {
    if (phase_ == Phase::Running || phase_ == Phase::Failed)
        return;

    // Do not begin heavy work until the first frame has been rendered
    if (!rendered_once_)
        return;

    advance_stage();
}

// ──
// render
// ──
void AdventureStartupScreen::render(d2engine::Renderer2D& renderer) {
    if (phase_ == Phase::Running)
        return;

    loading_screen_.render(renderer, config_.logical_width, config_.logical_height);

    if (!rendered_once_) {
        rendered_once_ = true;
        d2log::get("d2.adventure")->info("adventure_loading_screen first_frame_rendered");
    }
}

// ──
// advance_stage — perform one phase per call
// ──
void AdventureStartupScreen::advance_stage() {
    auto logger = d2log::get("d2.adventure");

    switch (phase_) {
    case Phase::Bootstrap: {
        // Background already loaded in on_enter; move to scenario parse.
        loading_screen_.set_progress(kPwBackground);
        phase_ = Phase::ParseScenario;
        logger->info("adventure_startup stage=ParseScenario");
        return;
    }

    case Phase::ParseScenario: {
        loading_screen_.set_progress(kPwScenarioParse);
        phase_ = Phase::LoadGameDataAndPortraits;
        logger->info("adventure_startup stage=LoadGameDataAndPortraits");
        return;
    }

    case Phase::LoadGameDataAndPortraits: {
        app_.ensure_shared_runtime_initialized();
        movement_profiles_ = d2game::AdventureUnitMovementProfileCatalog::from_unit_defs(
            app_.runtime_context().game_data.all_units());
        auto outcome =
            AdventureScenarioLoader::load_semantic(config_.scenario_path, movement_profiles_);
        if (!outcome || !outcome->success) {
            logger->error("failed_to_load_scenario path={}", config_.scenario_path);
            phase_ = Phase::Failed;
            return;
        }
        scenario_ = std::make_unique<AdventureStartupScenarioResult>(
            AdventureStartupScenarioResult{std::move(*outcome)});
        for (const auto& diag : scenario_->outcome.build_diagnostics) {
            logger->info("build_diagnostic kind={} msg={}", static_cast<int>(diag.kind),
                         diag.message);
        }
        logger->info("adventure_world units={} stacks={}",
                     scenario_->outcome.session->world().units.size(),
                     scenario_->outcome.session->world().stacks.size());
        loading_screen_.set_progress(kPwGameDataPortraits);
        phase_ = Phase::PrepareAdventureMap;
        logger->info("adventure_startup stage=PrepareAdventureMap");
        return;
    }

    case Phase::PrepareAdventureMap: {
        auto result = prepare_full_map(*scenario_->outcome.session, app_);
        map_result_ = std::make_unique<AdventureStartupMapResult>(std::move(result));
        loading_screen_.set_progress(kPwMapPreparation);
        phase_ = Phase::BuildVisualResources;
        logger->info("adventure_startup stage=BuildVisualResources");
        return;
    }

    case Phase::BuildVisualResources: {
        d2engine::AdventureVisualResourcesLoader visual_loader(
            app_.asset_runtime(), app_.runtime_context().render_assets);
        visuals_ = visual_loader.load();
        app_.setup_cursors(visuals_.cursors.default_cursor, visuals_.cursors.select_unit);
        if (!app_.activate_cursor()) {
            logger->error("cursor_activation_failed");
        }
        loading_screen_.set_progress(kPwVisualResources);
        phase_ = Phase::SplitAssets;
        logger->info("adventure_startup stage=SplitAssets");
        return;
    }

    case Phase::SplitAssets: {
        auto initial_keys =
            d2engine::collect_adventure_initial_asset_keys(map_result_->map.world_graph);
        auto animation_keys = d2engine::collect_adventure_remaining_animation_asset_keys(
            map_result_->map.world_graph);
        auto mask_keys = d2engine::collect_stack_mask_asset_keys(map_result_->map);
        stack_pick_entry_count_ = count_unique_stack_pick_entry_ids(map_result_->map);

        initial_asset_count_ = initial_keys.size();
        animation_asset_count_ = animation_keys.size();

        logger->info("adventure_asset_split initial_assets={} animation_assets={}",
                     initial_asset_count_, animation_asset_count_);

        if (!map_result_->render_state.has_terrain_texture()) {
            logger->warn("no_terrain_texture canvas={}x{}", map_result_->map.canvas_width(),
                         map_result_->map.canvas_height());
        }

        auto initial_preload = app_.render_assets().begin_incremental_preload(
            std::move(initial_keys), d2engine::AssetPriority::Critical, "AdventureWorldInitial");
        auto animation_preload = app_.render_assets().begin_incremental_preload(
            std::move(animation_keys), d2engine::AssetPriority::Prefetch,
            "AdventureWorldAnimations");
        auto mask_batch = app_.asset_runtime().request_batch(
            mask_keys, d2engine::AssetPriority::Prefetch, "AdventureWorldMasks");

        asset_split_data_ = std::make_unique<AssetSplitData>(
            std::move(initial_preload), std::move(animation_preload), std::move(mask_batch));

        loading_screen_.set_progress(kPwUploadMin);
        phase_ = Phase::UploadInitialAssets;
        logger->info("adventure_startup stage=UploadInitialAssets initial_count={} mask_count={}",
                     initial_asset_count_, mask_keys.size());
        return;
    }

    case Phase::UploadInitialAssets: {
        if (!asset_split_data_ || !asset_split_data_->initial_preload.has_value()) {
            throw std::logic_error("AdventureStartupScreen::UploadInitialAssets missing preload");
        }

        auto& initial_preload = *asset_split_data_->initial_preload;
        initial_preload.advance({.max_textures = 4, .max_ms = 2.0});
        const auto progress = initial_preload.progress();
        const auto completed = progress.uploaded + progress.failed;
        const auto total = initial_asset_count_ + animation_asset_count_;
        if (total > 0) {
            const float t = static_cast<float>(completed) / static_cast<float>(total);
            const float next = kPwUploadMin + (kPwUploadMax - kPwUploadMin) * t;
            loading_screen_.set_progress(std::max(loading_screen_.progress(), next));
        }

        if (!initial_preload.complete()) {
            return;
        }

        if (initial_preload.failed()) {
            throw std::runtime_error("AdventureStartupScreen: initial preload failed");
        }

        auto reclaim = initial_preload.finish();
        app_.render_assets().arm_reclaim_after_next_present(std::move(reclaim));

        logger->info("adventure_startup uploading initial assets uploaded={} failed={}",
                     progress.uploaded, progress.failed);

        asset_split_data_->initial_preload.reset();
        phase_ = Phase::UploadAnimationAssets;
        logger->info("adventure_startup stage=UploadAnimationAssets animation_count={}",
                     animation_asset_count_);
        return;
    }

    case Phase::UploadAnimationAssets: {
        if (!asset_split_data_->animation_preload.has_value())
            throw std::logic_error("AdventureStartupScreen: missing animation preload");

        auto& animation_preload = *asset_split_data_->animation_preload;
        animation_preload.advance({.max_textures = 8, .max_ms = 4.0});
        const auto progress = animation_preload.progress();
        const auto total = initial_asset_count_ + animation_asset_count_;
        if (total > 0) {
            const auto  completed = initial_asset_count_ + progress.uploaded + progress.failed;
            const float t = static_cast<float>(completed) / static_cast<float>(total);
            loading_screen_.set_progress(kPwUploadMin + (kPwUploadMax - kPwUploadMin) * t);
        }

        if (!animation_preload.complete())
            return;
        if (animation_preload.failed())
            throw std::runtime_error("AdventureStartupScreen: animation preload failed");

        auto reclaim = animation_preload.finish();
        app_.render_assets().arm_reclaim_after_next_present(std::move(reclaim));
        logger->info("adventure_startup uploading animation assets uploaded={} failed={}",
                     progress.uploaded, progress.failed);
        asset_split_data_->animation_preload.reset();
        loading_screen_.set_progress(kPwUploadMax);
        phase_ = Phase::BuildInteractionMasks;
        logger->info("adventure_startup stage=BuildInteractionMasks");
        return;
    }

    case Phase::BuildInteractionMasks: {
        if (!asset_split_data_) {
            throw std::logic_error(
                "AdventureStartupScreen::BuildInteractionMasks missing split data");
        }

        const auto built = opendis2::detail::advance_adventure_startup_interaction_masks(
            loading_screen_, map_result_->map, asset_split_data_->mask_batch,
            stack_pick_entry_count_, kPwUploadMax, kPwFinalizeEnd);
        if (!built.has_value()) {
            return;
        }
        logger->info("adventure_interaction_masks built={} expected={}", *built,
                     stack_pick_entry_count_);
        phase_ = Phase::Finalize;
        logger->info("adventure_startup stage=Finalize");
        return;
    }

    case Phase::Finalize: {
        if (!asset_split_data_ || asset_split_data_->initial_preload.has_value() ||
            asset_split_data_->animation_preload.has_value() ||
            asset_split_data_->mask_batch.has_value()) {
            throw std::logic_error("AdventureStartupScreen::Finalize startup assets not finalized");
        }
        auto& store = app_.screen_config_store();
        auto  adventure_layout = store.load_validated(
            "adventure_screen", d2engine::AdventureScreen::required_layout_nodes());
        if (!scenario_ || !scenario_->outcome.session) {
            throw std::logic_error("AdventureStartupScreen::Finalize missing GameSession");
        }
        auto session = std::move(scenario_->outcome.session);
        if (!session) {
            throw std::logic_error("AdventureStartupScreen::Finalize missing GameSession");
        }
        auto request_stack_info = [&app = app_](d2engine::StackInspectionModel model) {
            auto& stack_store = app.screen_config_store();
            auto  stack_info_layout = stack_store.load_validated(
                "stack_info_screen", d2engine::StackInfoScreen::required_layout_nodes());
            auto info_screen = std::make_unique<d2engine::StackInfoScreen>(
                app.runtime_context(), std::move(stack_info_layout.tree_layout),
                stack_info_layout.config_path.string(), std::move(model),
                [&app]() { app.pop_overlay_screen(); });
            app.push_overlay_screen(std::move(info_screen));
        };

        auto adventure_screen = std::make_unique<d2engine::AdventureScreen>(
            app_.runtime_context(), std::move(session), std::move(map_result_->map),
            std::move(map_result_->render_state), config_.logical_width, config_.logical_height,
            std::move(adventure_layout.tree_layout), adventure_layout.config_path.string(),
            request_quit_, std::move(request_debug_battle_), std::move(request_stack_info),
            std::move(visuals_.world), std::move(map_result_->stack_banner_catalog));

        phase_ = Phase::Running;
        app_.switch_screen(std::move(adventure_screen));
        return;
    }

    case Phase::Running:
    case Phase::Failed:
        break;
    }
}

} // namespace opendis2
