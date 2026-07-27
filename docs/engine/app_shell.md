# Battle Viewer App Shell

## Overview

The `opendis2 battle-viewer` subcommand provides the SDL3-based visual engine for Disciples II battle graphics. It uses the same `d2engine::Application` code path as the standalone prototype but is now accessed through the production `opendis2` binary.

## CLI Arguments

| Argument | Description | Default |
|----------|-------------|---------|
| `--game-root` | Path to game root directory (for direct .ff loading) | Required |
| `--battle-script` | Path to v3 scenario events.json | Required |
| `--scale` | Window scale factor | 1.0 |
| `--fullscreen` | Launch in fullscreen mode | false |
| `--mode` | Viewer mode: normal\|debug | normal |
| `--strict-planned-assets` | Fail on any render-path texture miss after full preload | false |
| `--help` | Show usage information | - |

## Window Controls

- **Close button**: Exits cleanly
- **Escape key**: Exits cleanly
- **Window**: Resizable, opaque native framebuffer, default 1024×768 (scaled by `--scale`)
- **Battle transparency debug mode**: Omits the battle background only within the rendered game
  frame; it does not request a transparent native window.
- **Window opacity debug control**: Uses `SDL_SetWindowOpacity` when supported by the platform;
  unsupported platforms leave the existing opaque window unchanged.

## Rendering Pipeline

The engine renders battle graphics using direct `.ff` container loading via `RawResourceLoader`, with `GameTextureCache` managing SDL texture lifetime.

### Texture Cache

- **Runtime PNG Decode**: Uses `SDL3_image::IMG_LoadTyped_IO()` (memory-to-SDL-surface decode via OS/GPU codec)
- **Caching**: Textures are cached by `(container_path, sprite_name)` key
- **RAII**: `SdlTexture` wrapper manages `SDL_Texture*` lifetime

## Architecture

```
opendis2 battle-viewer
  └── d2engine::Application
      ├── SdlContext (window, renderer, events)
      ├── RawResourceLoader (direct .ff container access)
      ├── GameTextureCache (SDL textures)
      ├── ScreenManager
      │    └── BattleScreen
      └── (app-level: tuning, debug UI, input)
```

## Dependencies

- SDL3 (optional, controlled by `D2_ENABLE_ENGINE` CMake option)
- SDL3_image (runtime PNG decode)
- libd2res (direct .ff parsing, libd2asset not linked)
- CLI11 (CLI parsing)
