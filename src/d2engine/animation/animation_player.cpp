#include "animation_player.hpp"

#include <stdexcept>

namespace d2engine {

AnimationPlayer::AnimationPlayer(AnimationSequence sequence) {
    load(std::move(sequence));
}

void AnimationPlayer::load(AnimationSequence seq) {
    sequence_ = std::move(seq);
    current_frame_index_ = first_playback_frame();
    elapsed_ms_ = 0.0f;
    state_ = AnimationPlayerState::Stopped;
}

void AnimationPlayer::play() {
    if (sequence_.frames.empty()) {
        return;
    }
    if (state_ == AnimationPlayerState::Completed) {
        restart();
    } else {
        state_ = AnimationPlayerState::Playing;
    }
}

void AnimationPlayer::pause() {
    if (state_ == AnimationPlayerState::Playing) {
        state_ = AnimationPlayerState::Paused;
    }
}

void AnimationPlayer::stop() {
    state_ = AnimationPlayerState::Stopped;
    current_frame_index_ = first_playback_frame();
    elapsed_ms_ = 0.0f;
}

void AnimationPlayer::restart() {
    current_frame_index_ = first_playback_frame();
    elapsed_ms_ = 0.0f;
    state_ = AnimationPlayerState::Playing;
}

void AnimationPlayer::set_reverse_playback(bool reverse) {
    if (reverse_playback_ == reverse) {
        return;
    }
    reverse_playback_ = reverse;
    if (state_ == AnimationPlayerState::Stopped || state_ == AnimationPlayerState::Completed) {
        current_frame_index_ = first_playback_frame();
        elapsed_ms_ = 0.0f;
    }
}

void AnimationPlayer::step_forward() {
    if (sequence_.frames.empty()) {
        return;
    }
    advance_frame();
    elapsed_ms_ = 0.0f;
}

void AnimationPlayer::step_backward() {
    if (sequence_.frames.empty()) {
        return;
    }
    retreat_frame();
    elapsed_ms_ = 0.0f;
}

void AnimationPlayer::step(int delta_frames) {
    if (sequence_.frames.empty()) {
        return;
    }
    if (delta_frames > 0) {
        for (int i = 0; i < delta_frames; ++i) {
            advance_frame();
        }
    } else {
        for (int i = 0; i > delta_frames; --i) {
            retreat_frame();
        }
    }
    elapsed_ms_ = 0.0f;
}

void AnimationPlayer::update(float delta_ms) {
    if (state_ != AnimationPlayerState::Playing || sequence_.frames.empty()) {
        return;
    }

    elapsed_ms_ += delta_ms;

    std::size_t zero_duration_advances = 0;
    while (true) {
        const auto frame_duration =
            static_cast<float>(sequence_.frames[current_frame_index_].duration_ms);
        if (frame_duration == 0.0f) {
            advance_frame();
            if (state_ != AnimationPlayerState::Playing ||
                ++zero_duration_advances > sequence_.frames.size()) {
                break;
            }
            continue;
        }
        if (elapsed_ms_ < frame_duration) {
            break;
        }
        elapsed_ms_ -= frame_duration;
        advance_frame();
        if (state_ != AnimationPlayerState::Playing) {
            break;
        }
    }
}

const AnimationFrame& AnimationPlayer::current_frame() const {
    if (sequence_.frames.empty()) {
        throw std::runtime_error("AnimationPlayer has no frames loaded");
    }
    return sequence_.frames[current_frame_index_];
}

std::size_t AnimationPlayer::current_frame_index() const noexcept {
    return current_frame_index_;
}

bool AnimationPlayer::is_done() const noexcept {
    return state_ == AnimationPlayerState::Completed;
}

AnimationPlayerState AnimationPlayer::state() const noexcept {
    return state_;
}

const AnimationSequence& AnimationPlayer::sequence() const noexcept {
    return sequence_;
}

std::size_t AnimationPlayer::first_playback_frame() const noexcept {
    return reverse_playback_ && !sequence_.frames.empty() ? sequence_.frames.size() - 1 : 0;
}

void AnimationPlayer::advance_frame() {
    if (reverse_playback_) {
        if (current_frame_index_ > 0) {
            --current_frame_index_;
        } else if (sequence_.is_looping) {
            current_frame_index_ = sequence_.frames.size() - 1;
        } else {
            state_ = AnimationPlayerState::Completed;
        }
        return;
    }
    if (current_frame_index_ + 1 < sequence_.frames.size()) {
        ++current_frame_index_;
    } else {
        // Reached last frame
        if (sequence_.is_looping) {
            current_frame_index_ = 0;
        } else {
            state_ = AnimationPlayerState::Completed;
        }
    }
}

void AnimationPlayer::retreat_frame() {
    if (sequence_.frames.empty()) {
        current_frame_index_ = 0;
        return;
    }
    if (reverse_playback_) {
        if (current_frame_index_ + 1 < sequence_.frames.size()) {
            ++current_frame_index_;
        } else {
            current_frame_index_ = 0;
        }
        return;
    }
    if (current_frame_index_ > 0) {
        --current_frame_index_;
    } else {
        current_frame_index_ = sequence_.frames.size() - 1;
    }
}

} // namespace d2engine
