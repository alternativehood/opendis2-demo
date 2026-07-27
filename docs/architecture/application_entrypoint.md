# Application Entry Point: `opendis2`

## Purpose

`opendis2` is the production executable (future game entry point) for the
OpenDis2 project. It is the thin top-level binary that users run to launch
the application.

## Why `opendis2` Exists

The project previously only produced development/inspection binaries
(`opendis2-dev-extractor`, `opendis2-dev-scenario-gen`). There was no production entry point that could
serve as the future game launcher.

`opendis2` fills this gap: it is the binary that end users will eventually run
to play the reimplemented game. During development it serves as the integration
point for screens and modules.

## Development vs. Production Binaries

| Naming Convention | Purpose |
|---|---|
| `opendis2` | Production entry point. Minimal, stable CLI. |
| `opendis2-dev-*` | Development/debug tools. Inspectors, extractors, generators. |

Development binaries (`opendis2-dev-extractor`,
`opendis2-dev-scenario-gen`, `opendis2-dev-sg-inspect`) are:

- Allowed to expose internal details
- Allowed to have large, debug-oriented CLIs
- Not stable; may change without notice

The production binary (`opendis2`) is:

- A thin launcher layer
- Not an inspector or debug tool
- Stable CLI contract (once stabilized)
- The future entry point for the whole game

## Current Behavior

`opendis2` currently supports:

```bash
opendis2 --help                         # Print usage and exit (0)
opendis2 --version                      # Print version and exit (0)
opendis2 <unknown-option>               # Error, exit non-zero
opendis2 battle-viewer [args]           # Launch the battle viewer (requires engine + game data)
opendis2 adventure --scenario <path>    # Launch adventure mode (graphical, requires engine)
opendis2 adventure --scenario <path> --headless  # Launch adventure mode (no SDL, CI-friendly)
opendis2                                # Print usage and exit (0)
```

When no subcommand is given, help is printed and the process exits successfully.

### Battle Viewer Delegation

The `battle-viewer` subcommand delegates to the existing
`d2engine::Application`. No battle viewer logic is duplicated in the
production entry point.

```text
opendis2 battle-viewer --game-root <path> --battle-script <path> [options]
```

All options accepted by the battle viewer subcommand are accepted here.

### Adventure Mode

The `adventure` subcommand parses an `.sg` scenario file and launches into
a placeholder adventure screen (graphical) or runs headless (CI/debug).

```text
opendis2 adventure --scenario <path.sg> [--headless]
```

- `--scenario` (required, `ExistingFile`) — path to a Disciples II scenario file.
- `--headless` (optional) — run without SDL, print world summary to stdout.

## Launch Configuration

`opendis2::LaunchOptions` lives in `src/opendis2/launch_options.hpp` and
is the caller-owned options object used for CLI binding:

```cpp
struct LaunchOptions {
    enum class Mode { None, BattleViewer, Adventure };
    Mode mode = Mode::None;

    std::filesystem::path scenario_path;
    bool                  headless = false;

    d2engine::AppConfig battle_viewer_config;
};
```

`setup_cli(CLI::App&, LaunchOptions&)` binds CLI11 options directly into
the caller-owned `LaunchOptions`, avoiding dangling-reference bugs.

After `app.parse()`, `main()` dispatches on `opts.mode`.

## Architecture

```
opendis2 (main.cpp)
  └─ CLI parsing (CLI11)
      ├── --help / --version
      ├── battle-viewer subcommand
      │     └─ d2engine::Application
      │          ├── SDL / Renderer / AppRuntimeContext
      │          ├── ScreenManager
      │          │    └── BattleScreen (active screen)
      │          └── (app-level: tuning, debug UI, stdin)
      └── adventure subcommand
            ├── d2scenario::SgParser (SG parsing)
            ├── d2runtime::AdventureWorldBuilder (world construction)
            ├── d2game::GameSession (game loop facade)
            ├── --headless: HeadlessFrontend (no-SDL, stdout)
            └── graphical: d2engine::Application + AdventureScreen (placeholder)
```

The production binary does not:

- Parse game assets directly
- Implement DBF scanning or world building
- Contain battle rendering logic
- Replace the existing battle viewer

## ScreenManager and BattleScreen (current)

The `d2engine::Application` owns a `ScreenManager` and starts a `BattleScreen`
as its default screen:

- `Application` owns SDL, renderer, texture cache, game data, and
  `ScreenManager`.
- `BattleScreen` owns all battle state (scene, presenter, runtime,
  keyboard/mouse handling, rendering).
- `Application::run()` delegates update, render, and input to
  `ScreenManager`, which forwards to the active `BattleScreen`.
- `BattleScreen` initializes the battle scenario in `on_enter()`.
- ESC / Quit action calls `ScreenManager::request_quit()`, and
  `Application::run()` exits when the quit flag is set.

`opendis2 battle-viewer ...` uses the `d2engine::Application` → `BattleScreen` path.

### Adventure Mode and Application

In adventure mode, `Application` is constructed with `config.scenario_path`
set. After construction, `start_with_screen()` injects `AdventureScreen`
(replacing the default `BattleScreen`).

This is transitional debt (see below).

## Future Direction

Future screens (adventure map, city, capital, etc.) will each implement
the `Screen` interface and be started via `ScreenManager::switch_to()`.
`d2engine::Application` remains the sole composition root and main loop
host.

Target screen layout:

```text
opendis2
  └─ d2engine::Application
       ├── BattleScreen (current — first real screen)
       ├── AdventureScreen (placeholder / debug)
       ├── CityScreen (future)
       ├── CapitalScreen (future)
       ├── MerchantScreen (future)
       ├── TrainerScreen (future)
       └── SpellbookScreen (future)
```

## Remaining Transitional Debt in `Application`

`d2engine::Application` is now the common screen host, but still contains
battle-specific transitional setup that should eventually move into a
launcher/factory layer:

1. **Tuning config loading**: `load_battle_tuning_config()` is called in
   `Application` constructor. This loads debug-tunable visual parameters
   (placement, fonts, layers). For non-battle screens this is irrelevant;
   future work should defer tuning setup per-screen.

2. **Portrait manifest construction**: `load_gunits_rows()` /
   `build_portrait_manifest()` are called in `Application` constructor.
   These are battle viewer concerns (portrait rendering). A future data
   loading layer (`d2runtime` / `d2game`) should own this.

3. **Direct `BattleScreen` construction**: `Application` creates
   `BattleScreen` directly in its constructor. When more screens exist,
   a mode-specific launcher or factory should construct the appropriate
   screen instead.

4. **`AppRuntimeContext` is not final `GameDataContext`**: The shared
   context contains `GameTextureCache` (render-bound). A future
   `d2game` / `d2runtime` context must separate static game data from
   SDL/renderer resources. See `app_runtime_context.hpp` docs.

5. **Adventure mode creates SDL even for placeholder**: Graphical
   adventure mode still initialises SDL and creates the full Application
   (including battle resources), then swaps the screen. A future refactor
   should create only the resources needed for the chosen mode.

6. **Headless adventure is engine-gated**: Both `HeadlessFrontend` and
   adventure mode itself are only built when `D2_ENABLE_ENGINE=ON`, even
   though `HeadlessFrontend` has no SDL dependency. This is transitional
   debt — the headless path should ideally be CI-testable without the
   engine flag.

These items are documented debt — they are not bugs. Moving them now
would require restructuring the initialization flow without adding
functionality. They should be addressed when a second screen is added
or when a dedicated data-loading layer is introduced.

## Current Limitations

- `battle-viewer` is the only fully-functional subcommand
- `adventure` mode is placeholder/debug only
- Game data is required for battle-viewer mode
- `GameSession`, save/load, and campaign flow are not yet designed
- HeadlessFrontend is engine-gated despite no SDL dependency
