# OpenDis2

> **Status: frozen proof-of-concept**
>
> OpenDis2 is a technical demonstration of a portable Disciples II engine prototype. This repository is published as a source-code, architecture, tooling, and reverse-engineering reference. It is not under active development and has no release roadmap or maintenance commitment.

OpenDis2 is an independent open-source engine prototype. It is not affiliated with, endorsed by, or supported by the original game's developers or publishers. The repository does not include the original game's assets. A legally obtained local installation of Disciples II: Rise of the Elves is required. The current snapshot demonstrates implemented engine capabilities, not a complete replacement game. Users should expect incomplete flows, prototype interfaces, and unsupported edge cases.

## Demonstrated capabilities

- Direct reading of original Disciples II resource archives and related metadata.
- SG scenario parsing and runtime world construction.
- Adventure terrain rendering, including water, forests, mountains, roads, cities, capitals, ruins, sites, landmarks, treasures, crystals, mines, and other implemented map objects.
- Adventure stacks, unit presentation, banners, selection, cursors, overlays, and `StackInfo`.
- Adventure animation and movement prototypes.
- Screen stack and runtime screen configuration architecture.
- Battle rules/core prototype and battle development tools.
- Sound and music playback prototype.
- Extraction, inspection, validation, and reverse-engineering tools.
- Automated tests and architecture guardrails.
- Native development build support.
- Windows x64 Docker cross-build support is experimental and unverified in this snapshot.

## Adventure

OpenDis2 currently demonstrates the adventure layer as a playable prototype surface rather than a complete game loop.

- SG scenario parsing and runtime world construction.
- Adventure terrain rendering, including water, forests, mountains, roads, cities, capitals, ruins, sites, landmarks, treasures, crystals, mines, and other implemented map objects.
- Adventure stacks, unit presentation, banners, selection, cursors, overlays, and `StackInfo`.
- Adventure animation and movement prototypes.
- The authored `testdata/test_map.sg` demo scenario can drive the adventure renderer.

## Known limitations

- This is not a complete game.
- Campaign progression and complete gameplay loops are not provided.
- Some screens and interactions are development tools or prototypes.
- Some data formats and edge cases remain unsupported.
- Original game data is required.
- No end-user support or maintenance is promised.
- Virtualized or cross-compiled Windows output does not by itself prove compatibility with every Windows machine.

## Targets

The project produces these executables:

### Production binary

| Binary | Description |
|---|---|
| `opendis2` | Production entry point (`--help`, `--version`, `battle-viewer`) |
| `opendis2 battle-viewer` | SDL3 battle viewer via production binary (requires engine) |

### Development binaries (`opendis2-dev-*`)

| Binary | Description |
|---|---|
| `opendis2-dev-extractor` | Main extractor CLI |
| `opendis2-dev-scenario-gen` | Battle scenario generator |
| `opendis2-dev-tests` | Unit + integration tests (fast) |
| `opendis2-dev-integration-tests` | Full-game extraction tests (opt-in via `make test-integration`) |

Development binaries use the `opendis2-dev-*` naming convention to distinguish
them from the production `opendis2` binary. They are not stable public
binaries; they expose internal tooling and may change without notice.

See [Application Entry Point](docs/architecture/application_entrypoint.md) for
the architecture of the production binary.

## Demo scenario

`testdata/test_map.sg` is an original scenario authored for this repository. It is used by integration tests and can demonstrate the adventure renderer. It does not contain the original game's assets. It requires the user's local installation of Disciples II: Rise of the Elves.

See [`LEGAL.md`](LEGAL.md) for the distribution terms that apply to this fixture.

## Requirements

- C++20 compiler (Clang 16+ recommended)
- CMake 3.25+
- Ninja build system
- vcpkg (for all C++ dependencies)
- A legally owned local copy of Disciples II — Rise of the Elves (for
  integration tests and actual extraction use)

## Quick start

```bash
# 1. Install/bootstrap vcpkg (one-time)
#    See https://vcpkg.io for details

# 2. Set up environment
cp .env.example .env
# Edit .env — set VCPKG_ROOT to your vcpkg installation path

# 3. Build everything
make build

# 4. Run unit tests
make test

# 5. Run lint/guardrail checks
make lint
```

## Build system details

- **CMake presets** manage the build configuration under `build/dev`.
- **Ninja** performs compilation — called through CMake, never directly.
- **vcpkg manifest mode** (`vcpkg.json`) provides all dependencies automatically.
- The `Makefile` is a thin wrapper around `cmake --preset dev`.
- Generated files: `build/dev` and `vcpkg_installed` are gitignored.

## Windows x64 cross-build in Docker

```bash
make windows-docker
```

Outputs:

- `dist/windows-mingw-x64/opendis2.exe`
- `dist/windows-mingw-x64/pe-info.txt`

The command has been run successfully in this snapshot, and the exported `opendis2.exe` was verified as a PE32+ x86-64 Windows binary.

### Individual Makefile targets

```bash
make configure   # CMake configure only
make build       # configure + build dev binaries (no tests)
make test        # configure + build test binaries + run tests
make lint        # lint/guardrail checks
make clean       # remove build/dev (preserves vcpkg dependencies)
make distclean   # remove build/dev and vcpkg_installed
```

## Pointing tools to a local game installation

Set `DISCIPLES2_GAME_ROOT` in `.env` (recommended):

```bash
DISCIPLES2_GAME_ROOT="/path/to/Disciples II Rise of the Elves"
```

Or set it as an environment variable:

```bash
export DISCIPLES2_GAME_ROOT="/path/to/Disciples II Rise of the Elves"
```

Some tools also accept a `--game-dir` argument at runtime. See `--help` on
each command for details.

## Project structure

- `src/` — C++20 project: core libraries (`libd2res`, `libd2asset`,
  `libd2engine`), CLI, and tests.
- `docs/formats/` — Documented binary format findings.
- `tests/` — Unit and integration tests (synthetic fixtures for default run;
  game-data tests opt-in via `DISCIPLES2_GAME_ROOT`).
- `tools/` — Development and lint scripts.
- `schemas/` — JSON schema definitions.

## Repository policy

- **No game assets** — do not commit original game archives (`.ff`, `.mqdb`, `.wdb`, `.sg`, `.sav`, `.mpq`), extracted media, or wholesale extracted databases.
- **No extracted data** — do not commit generated dumps, full frame lists, complete unit tables, campaign/map data, dialogue text, or other copyable original game content.
- **No local paths** — do not hardcode local system paths or usernames.
- **Authored configs and mappings are welcome** — the repository may track authored runtime configs, asset id mappings, render offsets, compatibility lookup tables, symbolic asset names, and original numeric ids needed by the runtime to load assets from a user-provided legally owned local installation.
- **Research stays local** — `research/` contains local analysis notes, generated dumps, reference images, and scratch work. It is gitignored. Publishable research findings live in `docs/research/`.

## Legal notice

OpenDis2 is an independent open-source reimplementation project.

This project is not affiliated with, endorsed by, sponsored by, or approved by Kalypso Media, Strategy First, Akella, .dat, or any other rights holder.

Disciples, Disciples II, Rise of the Elves, and related names, trademarks, artwork, assets, music, sounds, videos, maps, campaigns, and game data belong to their respective owners.

The GPL-3.0 license applies to the OpenDis2 source code in this repository. It does not apply to the original game or any proprietary assets required from a user-provided local installation.

## License

**GNU General Public License v3.0 only.** See [`LICENSE`](LICENSE).

The source code in this repository is licensed under GPL-3.0-only. This does
**not** cover proprietary game assets, extracted game data, or third-party
dependencies (see [`LEGAL.md`](LEGAL.md) and
[`docs/third_party_licenses.md`](docs/third_party_licenses.md)).
