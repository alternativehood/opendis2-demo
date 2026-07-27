#pragma once

// Runtime audio policy:
// WDB/MQDB parsing and sound id extraction are D2-specific and stay here.
// Playback, mixing, channels, music streaming, panning and volume control should
// Runtime playback is implemented by the SDL3_mixer backend in this layer.
// Do not build a handwritten mixer/audio scheduler in this codebase.
