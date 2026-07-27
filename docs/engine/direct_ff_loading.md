# Direct `.ff` Loading (Engine)

## Overview

The engine now reads game assets directly from `.ff` container files instead of requiring pre-extracted runtime asset packages. This reduces startup time from hours (full extraction + packaging) to approximately **4 seconds** for a complete game scan.

## Architecture

### Components

1. **RawResourceLoader** (`src/d2engine/assets/`)
   - Scans `Imgs/*.ff` on construction
   - Lazily opens containers only when a sprite is requested
   - Stores `MqdbContainer` + `OptMaps` + `ImageResourceDecoder` per container
   - 5 containers are skipped (no `-INDEX.OPT`): Faces, Grborder, Ground, Iconbld, Palmap
   - 18 valid image containers with **121,833 total sprites**

2. **GameTextureCache** (`src/d2engine/render/`)
   - Caches `SDL_Texture` by `(container_path, sprite_name)` key
   - Uses `SdlTexture` RAII wrapper for automatic `SDL_DestroyTexture`
   - Creates texture from `RgbaBuffer` via `create_sdl_texture()`
   - Provides `dimensions()` and `all_keys()` introspection

3. **Application** (`src/d2engine/app/`)
   - Replaced `RuntimeAssetContext` + `TextureCache` with `RawResourceLoader` + `GameTextureCache`
   - Render loop displays first 16 sprites in a grid (4x4) with aspect-ratio scaling
   - Takes `--game-root` CLI argument instead of `--assets`

### Dependency Changes

- `libd2engine` now links **libd2res** instead of libd2asset
- `libd2asset` remains for extraction-only CLI path (`build-runtime-assets`)
- `RuntimeAssetContext` and `TextureCache` still exist for backward compatibility

## Usage

```bash
opendis2 battle-viewer --game-root "${DISCIPLES2_GAME_ROOT:-/path/to/disciples2}"
```

## Performance

- **Startup**: ~4 seconds (scan + lazy open of first container)
- **Memory**: Textures created on-demand, cached until `GameTextureCache` destroyed
- **Trade-off**: No atlas packing (individual sprite textures less efficient than atlases)

## Future Work

- Thread pool for parallel sprite decoding
- Atlas packing for better GPU utilization
- Animation playback (decode OPT-ANIMS frame sequences)
- Sound loading directly from WDB containers
