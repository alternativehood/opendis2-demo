# Animation Player

## Overview

The `AnimationPlayer` provides runtime playback of OPT animation sequences decoded directly from `.ff` containers. It supports delta-time driven frame advancement, looping, pause/resume, and frame stepping for debugging.

## Architecture

### Components

- **AnimationFrame** (`src/d2engine/animation/animation_frame.hpp`)
  - `image_name`: Name of the frame image for `GameTextureCache` lookup
  - `duration_ms`: Frame duration in milliseconds (default 100ms)
  - `index`: Frame position in the sequence

- **AnimationSequence** (`src/d2engine/animation/animation_sequence.hpp`)
  - `frames`: Vector of `AnimationFrame`
  - `is_looping`: Loop mode flag
  - `name`: Animation name
  - `container_path`: Source `.ff` container

- **AnimationPlayer** (`src/d2engine/animation/animation_player.hpp` / `.cpp`)
  - States: `Playing`, `Paused`, `Stopped`, `Completed`
  - `update(delta_ms)`: Advances frame based on elapsed time
  - `play()`, `pause()`, `stop()`, `restart()`: Playback control
  - `step_forward()`, `step_backward()`: Frame stepping for debug
  - `current_frame()`: Returns current `AnimationFrame` for rendering

- **RawResourceLoader** (`src/d2engine/assets/raw_resource_loader.hpp` / `.cpp`)
  - `animations_in(container_path)`: List all animation names in a container
  - `decode_animation(container_path, anim_name)`: Decode an `AnimationSequence` from `-ANIMS.OPT`
  - Lazily parses animation maps when first requested

## Usage

```cpp
// Decode an animation
AnimationSequence seq = loader.decode_animation("/path/to/game/Imgs/BatUnits.ff", "idle");
seq.is_looping = true;

// Create player and play
AnimationPlayer player(seq);
player.play();

// In render loop
const auto& frame = player.current_frame();
SDL_Texture* texture = textures.get("/path/to/game/Imgs/BatUnits.ff", frame.image_name);

// In update loop (called every frame)
player.update(delta_ms);
```

## Keyboard Controls (opendis2 battle-viewer)

- `Space`: Play / Restart
- `P`: Pause / Resume
- `Left` / `Right`: Step backward / forward
- `R`: Restart
- `S`: Stop
- `Esc`: Quit

## Timing

- Default frame duration: 100ms (configurable)
- OPT `frame_delay_ms` metadata is used when available
- Delta-time driven: consistent playback regardless of frame rate

## Testing

- 18 unit tests covering frame/sequence data structures and player states
- Integration tested with real game data
- `opendis2 battle-viewer` verifies visual playback with game data

## Future Work

- Sound synchronization with `SoundScheduler`
- Animation role selection (idle/attack/hit/death)
- Battle screen slot positioning
- Event scheduler for scripted sequences
