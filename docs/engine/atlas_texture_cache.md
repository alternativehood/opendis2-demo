# Atlas Texture Cache

## Overview

The `TextureCache` bridges runtime asset atlas sheets (PNG-encoded) into the SDL3 rendering pipeline. It is owned by `Application` and manages the lifetime of SDL textures.

## Components

### SdlTexture

RAII wrapper for `SDL_Texture*` using `unique_ptr` with custom deleter.

```cpp
using SdlTexture = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;

// Create from RGBA pixel data
SdlTexture tex = create_sdl_texture(renderer, width, height, pixels, pitch);
```

### Runtime PNG Decode (SDL3_image)

Runtime atlas and sprite PNG decoding uses `SDL3_image::IMG_LoadTyped_IO()` for memory-to-SDL-surface decode. This replaces the handwritten lodepng path that was used in earlier stages.

```cpp
SDL_IOStream* io = SDL_IOFromConstMem(png_data.data(), png_data.size());
SDL_Surface*  surface = IMG_LoadTyped_IO(io, true, "png");
// 'true' = closeio — SDL3_image owns the IOStream; do NOT call SDL_CloseIO.
```

Both the `GameTextureCache::get_raw()` path (for raw MQDB records like Faces.ff) and `GameTextureCache::load()` path (via `RawResourceLoader::decode_sprite()` / `decode_raw_png()`) use SDL3_image for runtime PNG decode. No handwritten PNG decoder exists in the engine runtime path.

### TextureCache

Loads and caches atlas textures.

```cpp
TextureCache cache(renderer, asset_context);
SDL_Texture* tex = cache.first_texture();      // first loaded atlas
SDL_Texture* tex = cache.get("atlas_id");      // by asset ID
```

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Use `SDL3_image` for runtime PNG decode | Leverages OS/GPU-accelerated codec; no handwritten decoder in engine path |
| `TextureCache` owned by `Application` | Lifetime tied to app; simplifies cleanup |
| `SdlTexture` RAII wrapper | Prevents leaks; exception-safe |
| Lazy loading (first atlas only) | Stage 4 scope; on-demand loading for future stages |
| RGBA8888 pixel format | Direct mapping from SDL3_image surface to SDL3 texture |

## Error Handling

- Missing atlas file: `std::runtime_error` with atlas ID and path
- PNG decode failure: `SDL_GetError()` for SDL3_image diagnostics
- SDL texture creation failure: `std::runtime_error` with atlas ID
- Missing atlases in package: `std::runtime_error` (from `RuntimeAssetContext`)

## Testing

Unit tests cover:
- `SdlTexture` construction/destruction with null and valid renderers
- `rgba_buffer_from_sdl_surface` surface-to-RGBA extraction with format conversion and pitch handling
- `PngDecode` (lodepng-based, in `d2res/extractor/` for extraction/comparison only) 1×1 and 2×2 PNG decode, invalid/empty data handling
- `TextureCache` construction, cache hit behavior, unknown atlas lookup

## Future Work

- On-demand loading (load textures as animations request them)
- Async loading to prevent frame drops on large textures
- Texture size limits to prevent VRAM exhaustion
- Multi-atlas rendering support
