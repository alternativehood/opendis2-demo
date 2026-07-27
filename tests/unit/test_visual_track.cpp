#include <gtest/gtest.h>

#include "d2engine/battle_view/visual_track.hpp"

namespace d2engine {

// Compile-time: VisualTrack must be move-constructible
static_assert(std::is_move_constructible_v<VisualTrack>);
static_assert(std::is_move_constructible_v<PlaybackTransform>);

TEST(VisualTrack, StoresPlaybackTransformSeparatelyFromPlacementTransform) {
    VisualTrack track;
    track.transform.scale_y = 0.5f;
    track.playback.flip_y = true;

    EXPECT_FLOAT_EQ(track.transform.scale_y, 0.5f);
    EXPECT_TRUE(track.playback.flip_y);
}

TEST(VisualTrack, TrackRenderLayerOrder) {
    EXPECT_LT(static_cast<int>(TrackRenderLayer::Background),
              static_cast<int>(TrackRenderLayer::Base));
    EXPECT_LT(static_cast<int>(TrackRenderLayer::Base), static_cast<int>(TrackRenderLayer::Effect));
    EXPECT_LT(static_cast<int>(TrackRenderLayer::Effect),
              static_cast<int>(TrackRenderLayer::Overlay));
    EXPECT_LT(static_cast<int>(TrackRenderLayer::Overlay),
              static_cast<int>(TrackRenderLayer::Marker));
    EXPECT_LT(static_cast<int>(TrackRenderLayer::Marker),
              static_cast<int>(TrackRenderLayer::Debug));
}

} // namespace d2engine
