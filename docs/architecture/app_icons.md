# App Icons

## Source Asset

- **Master file**: `assets/app/icon.png` (1254×1254 RGBA PNG)
- The master PNG is the single source of truth for all icon formats.
- Committed to version control (small, lossless, square).

## macOS `.icns` Bundle Icon

- Generated at build time from `assets/app/icon.png` via `tools/make-icns.sh`.
- The script uses `sips` + `iconutil` (both macOS built-in) to create a standard `.icns` with all required sizes (16×16 through 512×512@2x).
- The `.icns` file is **not committed** — it is regenerated on every build via a CMake `POST_BUILD` custom command.
- Output location: `OpenDis2.app/Contents/Resources/icon.icns`.
- The `opendis2` target has `MACOSX_BUNDLE_ICON_FILE` set to `icon.icns`, which informs the macOS dock/Finder to use this icon.

## Window Icon (Title Bar / Taskbar)

- Set at runtime via `SDL_SetWindowIcon()` in `Application` constructor.
- Loaded from `assets/app/icon.png` using `IMG_Load`.
- The path is baked at compile time via the `D2ENGINE_ICON_FILE` definition on `libd2engine`.
- Falls back gracefully if the asset is missing (warning log, no crash).

## Platform Notes

### macOS
- Both bundle icon (`.icns`) and window icon (SDL) are active.
- `.icns` generation requires `iconutil` (Xcode command-line tools) — available on all standard macOS dev environments.

### Windows (future)
- Use `assets/app/icon.png` → convert to `.ico` (e.g., `ImageMagick` or `png2ico`).
- Set `WIN32_EXECUTABLE` and `VS_ICON` in CMake for the `.exe` icon.
- Window icon via `SDL_SetWindowIcon` will work the same (use `IMG_Load` on the PNG).

### Linux (future)
- Set a desktop entry file referencing an icon path, or embed a PNG/XPM in the binary.
- `SDL_SetWindowIcon` works on most window managers via the same code path.

## Build Integration

The macOS `.icns` generation is triggered as a `POST_BUILD` command on the `opendis2` target:

```cmake
add_custom_command(TARGET opendis2 POST_BUILD
    COMMAND "${CMAKE_SOURCE_DIR}/tools/make-icns.sh"
        "${_d2_icon_src}"
        "$<TARGET_BUNDLE_CONTENT_DIR:opendis2>/Resources/icon.icns"
    ...
)
```

The window icon path is baked into `libd2engine`:

```cmake
target_compile_definitions(libd2engine PRIVATE
    D2ENGINE_ICON_FILE="${CMAKE_SOURCE_DIR}/assets/app/icon.png")
```
