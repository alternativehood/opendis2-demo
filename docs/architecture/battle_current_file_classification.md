# Battle Current File Classification

This is the phase 1 migration map for the current `d2engine` battle viewer files. It classifies
current ownership without moving files or changing runtime behavior.

## Animation Primitives

- `src/d2engine/animation/animation_frame.hpp`
- `src/d2engine/animation/animation_sequence.hpp`
- `src/d2engine/animation/animation_player.hpp`
- `src/d2engine/animation/animation_player.cpp`

## Battle Domain

- `src/d2engine/battle_view/battle_ids.hpp`
- `src/d2engine/battle_view/battle_slot.hpp`
- `src/d2engine/battle_view/battle_slot.cpp`
- `src/d2engine/battle_view/battle_visual_event.hpp`

## Visual Runtime / Timeline

- `src/d2engine/battle_view/anchor_policy.hpp`
- `src/d2engine/battle_view/battle_scene.hpp` - mixed migration target: owns visual runtime
  state, presentation toggles, debug mutations, and D2-specific centroid helpers.
- `src/d2engine/battle_view/battle_scene.cpp` - mixed migration target.
- `src/d2engine/battle_view/battle_unit.hpp`
- `src/d2engine/battle_view/command_timeline.hpp`
- `src/d2engine/battle_view/command_timeline.cpp`
- `src/d2engine/battle_view/life_visual_state.hpp`
- `src/d2engine/battle_view/track_render_layer.hpp`
- `src/d2engine/battle_view/battle_render_snapshot.hpp`
- `src/d2engine/battle_view/visual_command.hpp`
- `src/d2engine/battle_view/visual_entity.hpp`
- `src/d2engine/battle_view/visual_entity.cpp`
- `src/d2engine/battle_view/visual_track.hpp`
- `src/d2engine/battle_view/visual_visibility.hpp`

## Animation Orchestration

- `src/d2engine/battle_view/battle_animation_engine.hpp`
- `src/d2engine/battle_view/battle_animation_engine.cpp`

## D2 Visual Scripts

- `src/d2engine/battle_view/battle_animation_scripts.hpp` - mixed migration target: still uses
  concrete scene structure and parallel role/death/effect arrays.
- `src/d2engine/battle_view/battle_animation_scripts.cpp` - mixed migration target.
- `src/d2engine/battle_view/battle_effect_role_set.hpp`
- `src/d2engine/battle_view/death_visual_assets.hpp`
- `src/d2engine/battle_view/unit_animation_role_set.hpp`
- `src/d2engine/battle_view/unit_animations.hpp`

## Asset Catalog

- `src/d2engine/battle_view/animation_catalog.hpp`
- `src/d2engine/battle_view/animation_clip_ref.hpp`
- `src/d2engine/battle_view/animation_role.hpp`
- `src/d2engine/battle_view/animation_role_resolver.hpp`
- `src/d2engine/battle_view/animation_role_resolver.cpp`
- `src/d2engine/battle_view/raw_ff_animation_catalog.hpp` - raw `.ff` adapter, should move out
  of battle_view core later.
- `src/d2engine/battle_view/raw_ff_animation_catalog.cpp` - raw `.ff` adapter.

## Render Core / Backend Adapter

- `src/d2engine/battle_view/battle_anchor_resolver.hpp`
- `src/d2engine/battle_view/battle_anchor_resolver.cpp`
- `src/d2engine/battle_view/battle_renderer.hpp` - mixed migration target: command building and
  backend rendering are still coupled.
- `src/d2engine/battle_view/battle_renderer.cpp` - mixed migration target.
- `src/d2engine/battle_view/battle_texture_provider.hpp`
- `src/d2engine/battle_view/sdl_battle_texture_provider.hpp` - SDL adapter, should move out of
  battle_view core later.
- `src/d2engine/battle_view/sdl_battle_texture_provider.cpp` - SDL adapter.

## Viewer / Debug Shell

- `src/d2engine/battle_view/battle_presenter.hpp` - mixed migration target: owns viewer action
  dispatch, selection state, debug resolver behavior, state sync, and engine bridge.
- `src/d2engine/battle_view/battle_presenter.cpp` - mixed migration target.
- `src/d2engine/battle_view/battle_viewer_action.hpp`
- `src/d2engine/battle_view/bottom_hud.hpp`
- `src/d2engine/battle_view/bottom_hud.cpp`
- `src/d2engine/battle_view/formation_status_panel.hpp`
- `src/d2engine/battle_view/formation_status_panel.cpp`
- `src/d2engine/battle_view/hud_unit_info.hpp`
- `src/d2engine/app/application.hpp` - mixed migration target: composition root plus viewer
  controller, debug UI, and renderer loop.
- `src/d2engine/app/application.cpp` - mixed migration target.
- `src/d2engine/app/battle_tuning_state.hpp`
- `src/d2engine/app/debug_element_tuner.hpp`

## Bootstrap

- `src/d2engine/app/app_config.hpp`
- `src/d2engine/app/battle_bootstrap.hpp`
- `src/d2engine/app/battle_bootstrap.cpp`
- `src/d2engine/app/battle_roster.hpp`
- `src/d2engine/app/battle_roster.cpp`
