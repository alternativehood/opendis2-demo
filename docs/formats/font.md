# Font Rendering

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Status:** No bitmap font resources exist in the game data

---

## Summary

Disciples II does **not** ship with pre-rendered bitmap fonts or TTF files.
All text rendering is handled at runtime via Windows GDI:
`CreateCompatibleDC` + `CreateDIBSection` + `SelectObject` are imported
in `Discipl2.exe` to manage device-dependent bitmaps. Text is rasterized
from system fonts (face name `"Normal"`, which resolves to MS Sans Serif
or a similar GDI default) into DIBs, then uploaded as Direct3D textures.

The game overlay DLL `C4dll-R.dll` likely wraps this GDI rendering
into the engine's `IFormattedText`/`CFormattedTextImpl` class hierarchy
(found in the exe `.rdata` RTTI structures, area starting at `0x2E07D4`
with `MFF\x00` / `MFT\x00` signatures — those are **not** font resource
markers but C++ RTTI descriptors).

## Dialog Font Keys

Interface dialogs (`Interf/Interf.dlg`) reference fonts using the naming
convention `X{height}TA{id}`, e.g. `X100TA0020`, `X120TA0003`. The
`{height}` field appears to be the font size in some internal unit
(tenths of a point? pixel height?). The `{id}` is a sequential identifier
for the font definition within that size class.

## Engine Implementation

Since the original font data is not extractable, opendis2 uses a
different approach for the SDL3 engine target (`D2_ENABLE_ENGINE`):

1. **stb_truetype.h** — single-header public-domain TTF rasterizer
   (fetched via CMake FetchContent from `nothings/stb`)
2. **CharisSIL-Regular.ttf** — open-source font (SIL Open Font License 1.1),
   bundled in `src/d2engine/render/font/`
3. **FontAtlas** (`font_atlas_builder.hpp/cpp`) — renders all ASCII
   printable glyphs (U+0020–U+007E) into a packed RGBA texture atlas
   at the requested pixel height
4. **GameFontManager** (`game_font_manager.hpp/cpp`) — lazily creates
   and caches `FontAtlas` instances for common sizes: 8, 10, 12, 14,
   16, 20, 24, 28, 36, 48 px

### Module Layout

```
src/d2engine/render/font/
├── font_types.hpp              # GlyphInfo, FontMetrics structs
├── font_atlas_builder.hpp/cpp  # stb_truetype atlas builder
├── font_resource.hpp           # Embedded TTF bytes (generated via xxd)
├── game_font_manager.hpp/cpp   # Multi-size font manager
```

### Integration

- `Renderer2D` accepts a `GameFontManager*` via `set_font_manager()`
- `draw_game_text()` forwards to `GameFontManager::render_text()` when
  a manager is set, falling back to SDL debug text otherwise
- `Application` owns a `GameFontManager` instance, initializes it in
  the constructor, and sets it on each frame's `Renderer2D` wrapper

### Color Tinting

Glyphs are stored as RGBA (white RGB + alpha from coverage). The atlas
texture's color mod is set per `render_text()` call, so text can be
rendered in any color without duplicating glyph data.

## Size Mapping

The font size parameter in `draw_game_text()` is in pixels (pixel height).
The game's `X{height}` values are passed directly as this parameter.
When a size has no dedicated atlas, the nearest supported size is used
(difference < 1px is considered a cache hit).
