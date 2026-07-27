# Third-Party Licenses

## Bundled in source

### sha256.hpp (platform/sha256_impl.hpp)

A minimal public-domain SHA-256 implementation derived from FIPS 180-4.
The implementation is original code written for this project following the
published standard and carries no license restrictions.

### CharisSIL-Regular.ttf

Bundled in `src/d2engine/render/font/` for the SDL3 engine prototype.
- License: SIL Open Font License 1.1 (`src/d2engine/render/font/OFL.txt`)
- URL: https://software.sil.org/charis/
- Copyright (c) 1997-2022 SIL International

### gif.h (third_party/gif-h/gif.h)

- Version: commit 70b645280d5e687f5217177c9cfa2889b0a2ad5f
- License: Public Domain / Unlicense
- URL: https://github.com/charlietangora/gif-h

### AMD FidelityFX Super Resolution 1 (third_party/fidelityfx-fsr/)

- Version: v1.0.2, source commit `a21ffb8f6c13233ba336352bdff293894c706575`
- License: MIT (`third_party/fidelityfx-fsr/LICENSE.txt`)
- URL: https://github.com/GPUOpen-Effects/FidelityFX-FSR
- Used for the whole-frame EASU and RCAS shader passes. The FSR algorithms are unmodified.
  `ffx_a.h` has one documented SDL_shadercross/DXC portability adjustment: vector conditional
  operators in `AZolZeroPassF2/F3/F4` use the compiler-required `select()` equivalent.

## Via CMake FetchContent or find_package

### CLI11
- Version: v2.4.2 (pinned to commit 6c7b07a878ad834957b98d0f9ce1dbe0cb204fc9)
- License: BSD 3-Clause
- URL: https://github.com/CLIUtils/CLI11

### nlohmann/json
- Version: 3.11.3
- License: MIT
- URL: https://github.com/nlohmann/json

### nlohmann/json-schema-validator
- Version: v2.3.0 (pinned to commit d0a18559ae6573e88d47febde77251f7282e8ce5)
- License: MIT
- URL: https://github.com/pboettch/json-schema-validator

### lodepng
- Version: pinned to commit ed6fe5825c6a4fbb7f58ab35a4231c7543cd452a
- License: zlib
- URL: https://github.com/lvandeve/lodepng

### GoogleTest
- Version: 1.14.0 (pinned to commit f8d7d77c06936315286eb55f8de22cd23c188571)
- License: BSD 3-Clause
- URL: https://github.com/google/googletest

### spdlog
- Version: v1.17.0 (pinned to commit 79524ddd08a4ec981b7fea76afd08ee05f83755d)
- License: MIT
- URL: https://github.com/gabime/spdlog

### magic_enum
- Version: v0.9.8 (pinned to commit 1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd)
- License: MIT
- URL: https://github.com/Neargye/magic_enum

### stb (stb_image.h, stb_truetype.h)
- Version: pinned to commit 31c1ad37456438565541f4919958214b6e762fb4
- License: Public Domain / MIT
- URL: https://github.com/nothings/stb

### Dear ImGui
- Version: pinned to commit dbb5eeaadffb6a3ba6a60de1290312e5802dba5a
- License: MIT
- URL: https://github.com/ocornut/imgui

## Via vcpkg (system dependencies)

### SDL3
- License: zlib
- URL: https://github.com/libsdl-org/SDL

### SDL3_shadercross
- License: zlib
- URL: https://github.com/libsdl-org/SDL_shadercross
- Build-only host tool used to compile OpenDis2's canonical HLSL FSR shaders to embedded SPIR-V,
  DXIL, and MSL binaries.

### SDL3_ttf
- License: zlib
- URL: https://github.com/libsdl-org/SDL_ttf

### SDL3_image
- License: zlib
- URL: https://github.com/libsdl-org/SDL_image

## Third-party code references in documentation

The project documentation references the following open-source reverse-engineering
tools for context and comparison:

- **Karnah/Disciples.Net** — C# extraction reference (no code copied)
- **NevendaarTools/toolsqt** — C++ Qt extraction reference (no code copied)
