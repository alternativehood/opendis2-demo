// Full-game validation: exhaustive per-frame geometry check for ALL
// animated landmarks.  This is NOT in CTest (may exceed 3s).
// Run via: make validate-full-game

#include <d2adventure_render/terrain/landmark_asset_catalog.hpp>
#include <d2adventure_render/adventure_render_types.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/landmark_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;

} // namespace

struct FrameValidation {
    std::string type_id;
    bool        uniform;
    int         frames;
    std::string detail;
};

TEST(LandmarkFullGameGeometry, ValidateAllAnimatedFrameUniformity) {
    if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(GAME_ROOT);
    const auto             catalog = d2engine::build_landmark_asset_catalog(store);

    std::vector<FrameValidation> results;

    for (const auto& [type_id, visual] : catalog.visuals) {
        const auto* av = std::get_if<d2engine::adventure_render::AnimatedLandmarkVisual>(&visual);
        if (av == nullptr)
            continue;

        const int frame_count = static_cast<int>(av->animation_data.frames.size());
        if (frame_count == 0)
            continue;

        const auto& f0 = av->animation_data.frames[0];
        bool        uniform = true;
        for (std::size_t i = 1; i < av->animation_data.frames.size(); ++i) {
            const auto& fi = av->animation_data.frames[i];
            if (fi.canvas_width != f0.canvas_width || fi.canvas_height != f0.canvas_height) {
                uniform = false;
                break;
            }
        }

        results.push_back(
            {type_id, uniform, frame_count,
             "w=" + std::to_string(f0.canvas_width) + " h=" + std::to_string(f0.canvas_height)});
    }

    int uniform_count = 0;
    int non_uniform_count = 0;
    for (const auto& r : results) {
        if (r.uniform) {
            ++uniform_count;
            SUCCEED() << "uniform   type=" << r.type_id << " frames=" << r.frames << " "
                      << r.detail;
        } else {
            ++non_uniform_count;
            SUCCEED() << "non-uniform type=" << r.type_id << " frames=" << r.frames << " "
                      << r.detail;
        }
    }

    ASSERT_GT(static_cast<int>(results.size()), 0) << "no animated landmarks found";

    SUCCEED() << "animated_landmark_total=" << results.size() << " uniform=" << uniform_count
              << " non-uniform=" << non_uniform_count;
}
