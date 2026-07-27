#include <gtest/gtest.h>

#include "d2engine/battle_view/visual_entity.hpp"

namespace d2engine {

// Compile-time: VisualEntity must be move-constructible
static_assert(std::is_move_constructible_v<VisualEntity>);

TEST(VisualEntity, AddTrack) {
    VisualEntity entity;
    entity.add_track(VisualTrack{.kind = TrackKind::Base});
    entity.add_track(VisualTrack{.kind = TrackKind::DeathFx});
    EXPECT_EQ(entity.tracks.size(), 2u);
}

TEST(VisualEntity, FindTrack) {
    VisualEntity entity;
    entity.add_track(VisualTrack{.kind = TrackKind::Base});
    entity.add_track(VisualTrack{.kind = TrackKind::DeathFx});

    const auto base_idx = entity.find_track(TrackKind::Base);
    EXPECT_TRUE(base_idx.has_value());
    EXPECT_EQ(*base_idx, 0u);

    const auto missing_idx = entity.find_track(TrackKind::ReviveFx);
    EXPECT_FALSE(missing_idx.has_value());
}

TEST(VisualEntity, RemoveTrackByKind) {
    VisualEntity entity;
    entity.add_track(VisualTrack{.kind = TrackKind::Base});
    entity.add_track(VisualTrack{.kind = TrackKind::DeathFx});

    for (auto& track : entity.tracks) {
        if (track.kind == TrackKind::DeathFx) {
            track.lifecycle = TrackLifecycle::PendingRemoval;
        }
    }
    // Not removed yet — update() performs the actual removal
    EXPECT_EQ(entity.tracks.size(), 2u);

    entity.update(0.0f);
    EXPECT_EQ(entity.tracks.size(), 1u);
    EXPECT_FALSE(entity.find_track(TrackKind::DeathFx).has_value());
    EXPECT_TRUE(entity.find_track(TrackKind::Base).has_value());
}

TEST(VisualEntity, UpdateAdvancesActiveTracks) {
    VisualEntity      entity;
    AnimationSequence seq;
    seq.is_looping = true;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    seq.frames.push_back(frame);

    VisualTrack track;
    track.kind = TrackKind::Base;
    track.player.load(seq);
    track.player.play();
    entity.add_track(track);

    EXPECT_EQ(track.player.current_frame_index(), 0u);
    entity.update(150.0f);
    EXPECT_EQ(entity.tracks[0].player.current_frame_index(), 1u);
}

TEST(VisualEntity, UpdateSkipsDisabledTracks) {
    VisualEntity      entity;
    AnimationSequence seq;
    seq.is_looping = true;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    seq.frames.push_back(frame);

    VisualTrack track;
    track.kind = TrackKind::Base;
    track.visibility = TrackVisibility::Disabled;
    track.player.load(seq);
    track.player.play();
    entity.add_track(track);

    entity.update(150.0f);
    EXPECT_EQ(entity.tracks[0].player.current_frame_index(), 0u);
}

TEST(VisualEntity, UpdateSkipsPausedHiddenTracks) {
    VisualEntity      entity;
    AnimationSequence seq;
    seq.is_looping = true;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    seq.frames.push_back(frame);

    VisualTrack track;
    track.kind = TrackKind::Base;
    track.visibility = TrackVisibility::PausedHidden;
    track.player.load(seq);
    track.player.play();
    entity.add_track(track);

    entity.update(150.0f);
    EXPECT_EQ(entity.tracks[0].player.current_frame_index(), 0u);
}

TEST(VisualEntity, UpdateAdvancesHiddenButPlayingTracks) {
    VisualEntity      entity;
    AnimationSequence seq;
    seq.is_looping = true;
    AnimationFrame frame;
    frame.duration_ms = 100;
    seq.frames.push_back(frame);
    seq.frames.push_back(frame);

    VisualTrack track;
    track.kind = TrackKind::Base;
    track.visibility = TrackVisibility::HiddenButPlaying;
    track.player.load(seq);
    track.player.play();
    entity.add_track(track);

    entity.update(150.0f);
    EXPECT_EQ(entity.tracks[0].player.current_frame_index(), 1u);
}

} // namespace d2engine
