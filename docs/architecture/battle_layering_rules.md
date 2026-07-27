# Battle Layering Rules

Phase 0 freezes architecture drift before more battle animation features are added.

## Layers

Dependency direction is one-way:

1. Application / executable
2. Viewer and debug shell
3. D2 visual scripts and adapters
4. Battle domain events
5. Visual runtime and command timeline
6. Animation primitives
7. Render core
8. Backend and raw asset adapters

The application composes the layers. Lower runtime layers must not reach back into SDL,
raw `.ff` loading, viewer input, HUD, or debug controls.

## Stop Rules

- No new feature may add SDL includes to animation primitives, battle domain, visual runtime,
  command timeline, animation engine, or D2 visual scripts.
- No new feature may add `RawResourceLoader` includes to animation primitives, battle domain,
  visual runtime, command timeline, animation engine, renderer core, or D2 visual scripts.
- No new feature may add `GameTextureCache` includes to animation primitives, battle domain,
  visual runtime, command timeline, animation engine, renderer core, or D2 visual scripts.
- No keyboard, mouse, HUD, or debug tuning behavior belongs in `BattleScene`,
  `BattleAnimationEngine`, visual entities, visual tracks, or visual commands.
- No hardcoded D2 animation names or raw container paths belong in renderer core or visual
  runtime.
- No semantic battle event may use `VisualEntityId` or `TrackId`; semantic events use
  `UnitInstanceId`.
- No new feature may use `TrackKind` as unique identity; `TrackId` is identity.
- No render core file may depend on `BattlePresenter`, `BattleAnimationEngine`, or mutable
  `BattleScene` APIs.

## Phase 0 Scope

This phase only documents boundaries, classifies current files, and adds a lightweight
forbidden-include check. It does not split modules or change runtime behavior.

Attack timing, combat outcomes, drain/heal, AOE behavior, and other gameplay-event details
must wait until selection/debug resolver boundaries, visual scene isolation, renderer core
isolation, and raw FF adapter isolation are in place.
