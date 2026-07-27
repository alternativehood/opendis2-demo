# Application Screens Architecture

## Overview

OpenDis2 uses a **Screen** / **ScreenManager** pattern with config-driven UI layout.

Every Screen owns exactly one `TreeLayout` through the `Screen` base class.
All production UI destination geometry is defined in configuration files under `configs/`.

## UI Layout Rule

```
configs/screens/<screen_name>.json → render_tree → TreeLayout → layout_rect() → Renderer2D
```

**Single invariant**: Every production Screen has exactly one canonical configuration
file under `configs/screens/<screen_name>.json`. Every screen config contains a
mandatory `"render_tree"` section with the Screen's TreeLayout nodes. Screen-specific
presentation settings may live beside `render_tree` in the same file.

**Canonical Screen filename convention**: `class FooScreen : public Screen` →
`foo_screen.hpp` / `foo_screen.cpp` under `src/d2engine/app/`, with config at
`configs/screens/foo_screen.json`. Enforced by the guardrail.

**Non-optional config_source**: Screen constructor requires an explicit non-empty
string identifying the canonical config. Test screens use synthetic provenance.

**Required-node contracts**: Every production Screen defines `required_layout_nodes()`.
Configs validated via `ScreenConfigStore::load_validated()` before activation.

**Runtime config root**: Resolved from the executable's directory by
`resolve_runtime_config_root()`. Every screen config loaded from
`<config_root>/screens/<screen_name>.json`. No OPENDIS2_SOURCE_DIR or CWD dependency.

## Classes

### `Screen` (`src/d2engine/app/screen.hpp`)

```cpp
class Screen {
public:
    explicit Screen(TreeLayout tree_layout, std::string config_source = {});
    virtual ~Screen() = default;

    [[nodiscard]] const TreeLayout& tree_layout() const;
    [[nodiscard]] Rect               layout_rect(std::string_view path) const;
    [[nodiscard]] const std::string& config_source() const;

    virtual std::string_view name() const = 0;
    virtual void on_enter() {}
    virtual void on_exit() {}
    virtual bool handle_input(const InputEvent& event);
    virtual void update(float delta_ms) = 0;
    virtual void render(Renderer2D& renderer) = 0;
};
```

- **TreeLayout owned by base**: Every Screen instance owns one non-optional TreeLayout.
  Constructors of derived screens must pass TreeLayout to the Screen base.
  No default constructor exists.
- **config_source provenance**: Records the canonical config file path (e.g.
  `configs/screens/stack_info.json`). Useful for diagnostics and debugging.
- **layout_rect(path)**: Shorthand for `tree_layout_.compose(path)`. Resolves
  parent-relative offsets into a final destination Rect.
- **handle_input()**: Returns true if the event was consumed. Modal overlays
  consume input exclusively; base screens don't see events while overlay is active.

### `ScreenManager` (`src/d2engine/app/screen_manager.hpp`)

Manages base screen + stack of modal overlays.

```cpp
class ScreenManager {
public:
    void switch_to(std::unique_ptr<Screen> screen);
    void request_switch_to(std::unique_ptr<Screen> screen);

    void push_overlay(std::unique_ptr<Screen> overlay);
    void pop_overlay();
    void request_push_overlay(std::unique_ptr<Screen> overlay);
    void request_pop_overlay();

    void apply_pending_transition();

    [[nodiscard]] bool quit_requested() const;
    void request_quit();

    bool handle_input(const InputEvent& event);
    void update(float delta_ms);
    void render(Renderer2D& renderer);
};
```

- **Deferred transitions**: `request_*()` variants queue operations; they are
  applied on the next `apply_pending_transition()` call.
- **Modal input**: Top overlay receives input exclusively. If an overlay exists,
  `handle_input` returns true even if the overlay doesn't consume the event.
- **Modal update**: When an overlay is active, only the top overlay's `update()`
  is called. The base screen's gameplay update is suspended.
- **Overlay lifecycle**: `switch_to()` calls `on_exit()` on each live overlay
  before clearing them.

### `AppRuntimeContext` (`src/d2engine/app/app_runtime_context.hpp`)

Shared runtime context passed to every screen.

```cpp
struct AppRuntimeContext {
    AssetRuntime&                assets;
    RenderAssetRuntime&          render_assets;
    GameDataRegistry&            game_data;
    const PortraitManifestIndex& portraits;

    [[nodiscard]] FfAssetStore&     store() const;
    [[nodiscard]] GameTextureCache& textures() const;
};
```

### `ScreenConfigStore` (`src/d2engine/app/screen_config_store.hpp`)

Centralized config loader for screen TreeLayouts with validated loading and live editing support.

```cpp
struct ScreenConfig {
    std::string           screen_name;
    std::filesystem::path config_path;
    nlohmann::json        document;
    TreeLayout            tree_layout;
};

class ScreenConfigStore {
public:
    explicit ScreenConfigStore(std::filesystem::path configs_dir);

    [[nodiscard]] ScreenConfig load(std::string_view screen_name) const;
    [[nodiscard]] ScreenConfig load_validated(std::string_view screen_name,
                                             const std::vector<std::string>& required_paths) const;

    void validate_required_nodes(std::string_view screen_name, const TreeLayout&,
                                 const std::vector<std::string>& required_paths) const;
    [[nodiscard]] TreeLayout load_render_tree(const std::filesystem::path& config_path) const;
    void save_render_tree(const std::filesystem::path& config_path, const TreeLayout& tree) const;

    [[nodiscard]] nlohmann::json load_document(const std::filesystem::path& path) const;
    void                         save_document_atomic(const std::filesystem::path& path,
                                                     const nlohmann::json&        document) const;
};
```

Loader reads `configs_dir/screens/<screen_name>.json`. Missing required nodes cause
an error with screen name, missing path, and config source. Supports live editing via
`TreeLayoutEditor` and atomic save-back to disk.

### `Application` (`src/d2engine/app/application.hpp`)

Composition root: owns SDL, shared resources, `BattleTuningController`,
`ScreenManager`, `AppRuntimeContext`.

- Constructor: initialises SDL, asset runtime, texture cache, game data,
  portrait manifest.
- `run()`: main loop — events → update → render → present.
- `start_with_screen(screen)`: override the initial screen.
- `push_overlay_screen()` / `pop_overlay_screen()`: modal overlay support.
- `start_battle_screen()`: creates BattleScreen with the battle render_tree
  extracted from `configs/screens/battle_screen.json`.

## Event Processing

1. `Application::process_events()` drains stdin commands.
2. Each SDL event is translated to `InputEvent` and passed to
   `ScreenManager::handle_input()`.
3. `apply_pending_transition()` flushes any deferred screen/overlay transitions.
4. The Application main loop handles quit, tuning save/revert, and debug UI toggle
   after screen handler returns.

## Screen Implementations

### `BattleScreen`

Uses the existing `configs/screens/battle_screen.json` render_tree as its canonical
TreeLayout source. Receives TreeLayout through the constructor. Owns all battle state: `BattleScene`,
`BattlePresenter`, `BattleScenarioRuntime`, animation catalogs, and scenario
executor.

### `AdventureScreen`

Receives a minimal TreeLayout from `configs/screens/adventure.json` (root node
only — no screen-space UI elements). Renders the adventure map using world-space
coordinates (not UI layout). Handles right-click hit testing through
`AdventurePickIndex` and delegates to a `request_stack_info` callback for
StackInfoScreen overlay.

### `StackInfoScreen`

Fully config-driven UI. All popup/leader/formation geometry comes from
`configs/screens/stack_info.json` via TreeLayout paths:
- `/stack_info/background` — popup parchment
- `/stack_info/leader/name`, `/faction`, `/battles_won` — text boxes
- `/stack_info/formation/slot_N/portrait`, `/name` — unit portraits and labels
- `/stack_info/formation/large_row_0/portrait` — top-row large unit
- `/stack_info/formation/large_row_1/portrait` — middle-row large unit
- `/stack_info/formation/large_row_2/portrait` — bottom-row large unit

Each portrait's `layout_path` is assigned during asset planning via
`formation_layout_path()` in `plan_stack_info_assets()`. No manual pixel
coordinates in C++.

Available as a modal overlay via `ScreenManager::push_overlay()`. Assets
requested asynchronously through the shared `RenderAssetRuntime`.

Required layout nodes validated via `StackInfoScreen::required_layout_nodes()`
before screen activation.

## Config Layout

```
configs/
    screens/
        battle_screen.json             (battle render_tree + all tuning sections)
        adventure_screen.json          (minimal root-only layout)
        stack_info_screen.json         (full popup UI hierarchy)
```

Each screen has exactly one canonical TreeLayout source under `configs/screens/`.
The battle screen config is the authoritative document for battle UI geometry,
scene layout, unit visual profiles, and all other tuning sections.

## Future Screens

New screens follow the same contract:
- Screen .cpp under `src/d2engine/app/`
- Corresponding config under `configs/screens/<name>.json`
- Required layout nodes validated before activation
- Auto-discovered by `guardrail_ui_layout.sh` (no manual guardrail updates)

## Architecture Invariants

1. Screen base owns exactly one non-optional TreeLayout.
2. Production UI destination geometry lives in configs/.
3. Screen code selects semantic paths; TreeLayout::compose() produces Rects.
4. Renderer2D renders them.
5. No manual UI coordinate arithmetic in Screen implementations.
6. No duplicate authoritative layout configs.
7. Missing required layout nodes fail at screen construction, not during render.
