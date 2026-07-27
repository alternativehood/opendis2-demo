#include "visual_entity.hpp"

namespace d2engine {

std::optional<std::size_t> VisualEntity::find_track(TrackKind kind) const {
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].kind == kind) {
            return i;
        }
    }
    return std::nullopt;
}

void VisualEntity::add_track(VisualTrack track) {
    tracks.push_back(std::move(track));
}

void VisualEntity::update(float delta_ms) {
    // Advance active tracks
    for (auto& track : tracks) {
        if (track.lifecycle == TrackLifecycle::PendingRemoval) {
            continue;
        }
        if (track.visibility == TrackVisibility::Visible ||
            track.visibility == TrackVisibility::HiddenButPlaying) {
            track.player.update(delta_ms);
        }
    }

    // Remove pending tracks
    std::erase_if(
        tracks, [](const VisualTrack& t) { return t.lifecycle == TrackLifecycle::PendingRemoval; });
}

} // namespace d2engine
