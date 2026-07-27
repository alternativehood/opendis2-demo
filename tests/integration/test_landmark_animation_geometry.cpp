#include <d2adventure_render/terrain/landmark_asset_catalog.hpp>
#include <d2adventure_render/adventure_render_types.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/landmark_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path get_game_root() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env == nullptr || env[0] == '\0')
        return {};
    return env;
}

} // namespace

// Scans only the IsoCmon container for a fast per-frame geometry check.
// Full cross-container validation runs via make validate-full-game.
TEST(LandmarkAnimationGeometryIntegration, IsoCmonFramesHavePopulatedGeometry) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_landmark_asset_catalog(store);

    int checked_frames = 0;
    for (const auto& [type_id, visual] : catalog.visuals) {
        const auto* av = std::get_if<d2engine::adventure_render::AnimatedLandmarkVisual>(&visual);
        if (av == nullptr)
            continue;

        for (const auto& af : av->animation_data.frames) {
            EXPECT_GT(af.canvas_width, 0) << type_id << " frame " << af.record_name;
            EXPECT_GT(af.canvas_height, 0) << type_id << " frame " << af.record_name;
            ++checked_frames;
        }
    }

    EXPECT_GT(checked_frames, 0) << "no animated landmark frames found";
}

// Diagnostic: detect and report animated landmarks whose per-frame
// visible-piece-derived feet differ.  This is expected data and must
// NOT be treated as an error, but proves that removing per-frame foot
// correction in the render path was correct — animations whose
// visible-piece bounds legitimately change (e.g. fire, lava) no
// longer cause the entire Landmark to oscillate between frames.
TEST(LandmarkAnimationGeometryIntegration, ReportVaryingFrameFeet) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_landmark_asset_catalog(store);

    int total_animations = 0;
    int varying_foot_animations = 0;

    for (const auto& [type_id, visual] : catalog.visuals) {
        const auto* av = std::get_if<d2engine::adventure_render::AnimatedLandmarkVisual>(&visual);
        if (av == nullptr)
            continue;

        const auto& frames = av->animation_data.frames;
        if (frames.empty())
            continue;

        ++total_animations;

        // Compute per-frame derived feet from raw OPT sprite metadata.
        // These feet come from the visible piece bounding boxes and
        // legitimately vary per frame for effects like fire/lava.
        struct FrameFoot {
            int         index;
            std::string name;
            int         fx;
            int         fy;
            int         cw;
            int         ch;
        };
        std::vector<FrameFoot> derived;

        bool all_same_foot = true;
        for (std::size_t fi = 0; fi < frames.size(); ++fi) {
            const auto& af = frames[fi];
            const auto  meta = store.sprite_metadata(av->container_path, af.record_name);
            derived.push_back({static_cast<int>(fi), af.record_name, meta.canvas_foot_x,
                               meta.canvas_foot_y, meta.canvas_width, meta.canvas_height});
            if (fi > 0) {
                if (derived[fi].fx != derived[0].fx || derived[fi].fy != derived[0].fy)
                    all_same_foot = false;
            }
        }

        if (!all_same_foot) {
            ++varying_foot_animations;

            std::ostringstream oss;
            oss << "varying_foot_animation type=" << type_id << " container=" << av->container_path
                << " frame_count=" << derived.size() << " stable_anchor=(" << av->canvas_foot_x
                << "," << av->canvas_foot_y << ")";

            for (const auto& d : derived) {
                oss << " f" << d.index << "=" << d.name << " foot=(" << d.fx << "," << d.fy
                    << ") dims=(" << d.cw << "x" << d.ch << ")";
            }
            SUCCEED() << oss.str();
        }
    }

    EXPECT_GT(total_animations, 0) << "no animated landmarks found";

    std::ostringstream sum;
    sum << "animated_landmark_diagnostic total=" << total_animations
        << " varying_foot=" << varying_foot_animations;
    SUCCEED() << sum.str();
}
