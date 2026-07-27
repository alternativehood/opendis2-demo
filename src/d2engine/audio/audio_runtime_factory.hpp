#pragma once

#include "audio_runtime.hpp"

#include <memory>

namespace d2::audio {
[[nodiscard]] std::unique_ptr<AudioRuntime> create_audio_runtime();
}
