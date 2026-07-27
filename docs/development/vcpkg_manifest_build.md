# Vcpkg Manifest Build

This project uses **vcpkg in manifest mode** as the only supported dependency provider.

## Prerequisites (macOS)

```bash
# Build tools
brew install cmake ninja pkg-config

# vcpkg itself
mkdir -p ~/dev
cd ~/dev
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

### Shell setup

Add to `~/.zshrc` (or equivalent):

```bash
export VCPKG_ROOT="$HOME/dev/vcpkg"
export PATH="$VCPKG_ROOT:$PATH"
```

Reload:

```bash
source ~/.zshrc
```

> `pkg-config` is **recommended** — some vcpkg ports (e.g. `cli11`) use it
> during `vcpkg_fixup_pkgconfig` on macOS.

## Quick start

```bash
# 1. Set up .env
cp .env.example .env
# Edit .env — set VCPKG_ROOT to your vcpkg installation

# 2. Build dev binaries
make build

# 3. Run unit tests
make test

# 4. Run lint
make lint
```

## What happens

1. `make build` runs `cmake --preset dev`, which:
   - Picks up `VCPKG_ROOT` from `.env`
   - Configures in vcpkg manifest mode via `CMAKE_TOOLCHAIN_FILE`
   - Triggers `vcpkg install` automatically, downloading/building all deps
2. Then runs `cmake --build --preset dev --target opendis2 opendis2-dev-extractor opendis2-dev-scenario-gen` (Ninja compilation)
3. `make test` builds test binaries first, then runs `ctest --preset dev-unit`
4. `make test-integration` builds test binaries first, then runs `ctest --preset dev-integration`

## CMake Presets

| Preset              | Description                                          |
|---------------------|------------------------------------------------------|
| `dev`               | Default development build (vcpkg + engine)           |
| `dev-unit`          | Unit tests (excludes integration)                    |
| `dev-integration`   | Integration tests (requires game data)               |

List available presets:

```bash
cmake --list-presets
```

`dev-no-engine` exists as an internal preset for CI/headless builds but is not part of the normal development workflow.

## Running tests

```bash
# Unit tests (default)
make test

# Integration tests (requires DISCIPLES2_GAME_ROOT + game data)
make test-integration

# Specific test
ctest --test-dir build/dev --output-on-failure -R "AdventureWorldBuilder"

# All-inclusive run (unit + integration)
ctest --test-dir build/dev --output-on-failure
```

## Dependencies

All dependencies are listed in `vcpkg.json`:

| vcpkg port                    | CMake target(s)                              | Notes                     |
|-------------------------------|----------------------------------------------|---------------------------|
| `cli11`                       | `CLI11::CLI11`                               |                           |
| `nlohmann-json`               | `nlohmann_json::nlohmann_json`               |                           |
| `lodepng`                     | `lodepng::lodepng` / header+source           | Header+src fallback       |
| `gif-h` (vendored)            | header-only (`third_party/gif-h/`)           | No vcpkg port, vendored   |
| `nlohmann-json-schema-validator` | `nlohmann_json_schema_validator`          |                           |
| `gtest`                       | `GTest::gtest`, `GTest::gtest_main`          |                           |
| `spdlog`                      | `spdlog::spdlog`                             |                           |
| `stb`                         | header-only (`find_path`)                    | No CMake CONFIG           |
| `magic-enum`                  | `magic_enum::magic_enum`                     |                           |
| `imgui`                       | `imgui::imgui` (with `sdl3-binding`, `sdl3-renderer-binding` features) | Engine only |
| `sdl3`                        | `SDL3::SDL3`                                 | Engine only               |
| `sdl3-ttf`                    | `SDL3_ttf::SDL3_ttf`                         | Engine only               |
| `sdl3-image`                  | `SDL3_image::SDL3_image`                     | Engine only               |

### Engine dependencies

When `D2_ENABLE_ENGINE=ON` (default), SDL3 + SDL3_ttf + SDL3_image + ImGui are required.

## Cleanup

```bash
make clean       # removes build/dev output (preserves vcpkg dependencies)
make distclean   # removes build/dev and vcpkg_installed (full rebuild needed)
```

## Troubleshooting

| Symptom | Solution |
|---------|----------|
| `.env not found` | Run `cp .env.example .env`, then edit `VCPKG_ROOT` |
| `VCPKG_ROOT not set` | Add `VCPKG_ROOT=/path/to/vcpkg` to `.env` |
| `vcpkg build failed` | Install `pkg-config` (`brew install pkg-config`). Some ports need it on macOS. |
| `Stale CMake cache` errors | Run `make clean`, then `make build` |
| Wrong architecture (arm64 vs x64) | Check `uname -m` and set `VCPKG_DEFAULT_TRIPLET` in `.env` |
| `SDL3 not found` | Ensure `.env` is set up correctly, then `make build` |
| `DISCIPLES2_GAME_ROOT` not set | Add `DISCIPLES2_GAME_ROOT=/path/to/game` to `.env` |
| Dependency not found | Run `make clean && make build` to trigger a fresh vcpkg install |
