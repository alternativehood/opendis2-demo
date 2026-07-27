# FF Asset Access Architecture

## Overview

opendis2 reads original Disciples II assets from `.ff` containers (MQDB
format). This document defines the architecture for physical filesystem access,
logical asset identity, and the central access layer (`FfAssetStore`).

## Two separate domains

### Physical filesystem paths

Physical paths are **operating-system paths** that point to actual files and
directories on disk.

Rules:
- Use `std::filesystem::path` for every filesystem-owning variable.
- Use standard filesystem operations: `/`, `filename()`, `extension()`,
  `parent_path()`, `directory_iterator`, `exists()`, `is_directory()`, etc.
- Preserve real filesystem casing. Never normalize casing for physical paths.
- **Forbidden:** manual string-based path construction, `find_last_of('/')`,
  `std::replace('\', '/')`, or any other separator manipulation on physical
  paths.

Conversion physical path → string is allowed only on boundaries:
- Logger messages
- Exception messages
- APIs that currently require strings
- Serialization

After `.string()`, the value must not be used as the primary filesystem
representation.

### Logical asset IDs

Logical IDs are **platform-independent names** that identify assets within the
engine. They are not filesystem paths.

Examples:
```
imgs/grborder.ff
Imgs/GrBorder.ff
G000UU0001FACE.PNG
render/tree/node
```

Rules:
- Logical IDs use `std::string`.
- `canonical_ff_container_key()` is the only place where logical separator
  normalization (`\` → `/`) and ASCII lowercasing are allowed.
- The canonical logical key **must never** be used to reconstruct a physical
  path.
- On Windows the logical ID is still `imgs/grborder.ff`, never
  `imgs\grborder.ff`.

## FfAssetStore responsibilities

`FfAssetStore` is the sole high-level owner for `.ff` asset access.

Responsibilities:
- Discover all `.ff` containers under the game root (`Imgs/`, `Interf/`)
- Lazy-open MQDB on first container access (`std::call_once`)
- Build complete record inventory on first access
- Central source cache (raw bytes + decoded RGBA)
- Access statistics (hits / loads / cache_hits / missing)
- Debug access dump

Thread safety:
- Per-container `std::mutex` protects post-initialization mutable state.
- `std::call_once` guarantees exactly one initializer per container.
- `initialize_container()` **must not** be called while holding `state.mtx`.
- State publication inside `std::call_once` acquires `state.mtx` so that
  `access_report()` (which also acquires `state.mtx`) never observes
  partially initialized state.

## Discovery and canonicalization

Discovery flow:
1. `find_asset_dir(game_root, "imgs")` — case-insensitive directory search.
2. Scan directory for `.ff` files (case-insensitive extension check).
3. `ContainerState::full_path` = actual discovered `std::filesystem::path`.
4. `canonical_ff_container_key(filename)` = logical platform-independent ID.

Example:
```
Physical:  /game/IMGS/GRBORDER.FF
Logical:   imgs/grborder.ff
```

Lookup flow:
1. Caller passes logical container name (`Imgs/GrBorder.ff`).
2. `find_container()` canonicalizes to `imgs/grborder.ff`.
3. `ContainerState::full_path` (preserved physical path) is used for MQDB open.

## Case sensitivity

Different game distributions (Steam, GOG, retail) use different directory and
file casing. Therefore:
- Directory discovery is case-insensitive.
- `.ff` extension discovery is case-insensitive.
- Logical container identity is case-insensitive (canonicalized).
- Physical path preservation is mandatory (do not reconstruct from logical ID).

## Game root layout

```
<GameRoot>/
  Imgs/          — images, animations, atlases
  Interf/        — interface assets
  Globals/       — DBF/DLG/DAT data tables
  ...
```

`FfAssetStore` only touches `Imgs/` and `Interf/`.

## Dependency rules

- `libd2res` provides low-level MQDB/OPT parsing.
- `libd2assets_runtime` provides `FfAssetStore`.
- `libd2engine` and tools depend on `libd2assets_runtime` for asset access.
- No other layer should open MQDB directly in production code.

## Guardrails

`tools/guardrail_filesystem_paths.sh` enforces the physical-path policy in CI.
