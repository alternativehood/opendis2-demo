# Stage 0 Readiness — Engine SDL3 Battle Visual Prototype

## Status

**Change**: `engine-sdl3-battle-visual-prototype`  
**Date**: 2026-06-10  
**Phase**: Proposal and Architecture Boundary (Stage 0) — **COMPLETE**

All artifacts have been created and validated:

| Artifact | Status |
|----------|--------|
| Proposal | done |
| Design | done |
| Spec | done |
| Tasks | done |

## Architecture Summary

### Module Split

```text
opendis2 battle-viewer
  -> libd2battle_view
      -> libd2engine
          -> libd2asset
          -> SDL3
```

- `libd2engine`: SDL3-based visual runtime infrastructure (platform, render, animation, audio)
- `libd2battle_view`: Battle presentation model (slot layout, unit visuals, animation roles, scheduler)
- `opendis2 battle-viewer`: Production subcommand with CLI and engine loop

### Key Decisions

1. **SDL3 is optional**: `D2_ENABLE_ENGINE` CMake option (default ON). Extraction-only builds set `OFF` and skip all engine targets.
2. **Battle layout**: 3 lanes × 2 depth = 6 positions per side. Front row nearest to center, back row farthest.
3. **Presentation-first**: Deterministic scripted event scheduler. No real combat simulation in this phase.
4. **Asset Database as sole source**: All asset loading through `AssetDatabase` APIs. No raw `.ff`/`.wdb`/`.OPT` access in viewer.
5. **Runtime PNG decoding via SDL3_image**: `IMG_LoadTyped_IO()` for memory-to-SDL-surface decode. `lodepng` retained for extraction/comparison toolchain (no SDL dependency needed there).
6. **Provisional timings**: 100ms default frame duration if manifest timing is missing. All timings configurable.

## Non-Goals (Explicitly Out of Scope)

- Full battle rules engine (damage formulas, ward/immunity, AI)
- Map/adventure rendering or campaign loading
- Real AI
- Exact vanilla damage formulas
- SDL3 required for extraction-only tools
- Bypassing `AssetDatabase` with manual JSON reading

## Open Questions / Decisions Before Stage 1

The following questions are documented in `design.md` and need to be resolved during Stage 1 implementation:

### SDL3 Integration Strategy

**Question**: Should SDL3 be added via `find_package(SDL3 CONFIG)` only, or is FetchContent acceptable as a fallback?  
**Decision**: Try system package first (`find_package(SDL3 CONFIG)`). Document FetchContent as a fallback for environments without system SDL3.  
**Status**: ✅ RESOLVED in Stage 1 (`engine-sdl3-platform-dependency`). Implemented `find_package(SDL3 CONFIG)` with clear error message. FetchContent documented as fallback in `docs/engine/sdl3_setup.md`.

### Battle Screen Resolution

**Question**: What is the exact original battle screen resolution?  
**Decision**: Start with 800×600 or 1024×768 logical resolution. Do not attempt exact original UI resolution unless already known from assets.  
**Action for Stage 1**: Use 1024×768 as initial logical resolution in `AppConfig`. Make it configurable via CLI `--resolution WxH`.

### Renderer Backend

**Question**: Should the battle viewer use `SDL_Renderer` or raw `SDL_Window` + custom renderer?  
**Decision**: Use `SDL_Renderer` for simplicity in the prototype. Custom renderer is a future optimization if needed.  
**Action for Stage 1**: Initialize `SDL_Renderer` in `SdlContext`. Keep renderer abstraction thin so backend can be swapped later.

### Animation Role Classification

**Question**: How should animation role classification work with current asset metadata?  
**Decision**: Heuristic at first (e.g., filename pattern matching for idle/attack/hit/death), explicit CLI override always available.  
**Action for Stage 1**: Implement a simple `BattleAnimationSet` with pattern-based heuristics. Add CLI flags: `--idle-animation`, `--attack-animation`, `--hit-animation`, `--death-animation`. Document heuristic limitations.

### Size-2 Unit Occupancy

**Question**: How do size-2 units occupy battle slots?  
**Status**: Research unknown. Do not fake it as a rule in this phase.  
**Action for Stage 1**: Reserve extension points in `BattleSlotCoord` (e.g., `size` field) but do not implement occupancy logic. Document as a known limitation.

### Sound Sequencing

**Question**: What is the exact sound sequencing for attack/hit/death?  
**Status**: Unknown from research.  
**Action for Stage 1**: Implement no-op `SoundScheduler` that logs events. Optional SDL audio playback only if straightforward. Do not block visual prototype on sound.

## Next Stage: Stage 1 — SDL3 Dependency and Build Gating

**Proposal ID**: `engine-sdl3-platform-dependency`

### Goal

Add SDL3 to the project without breaking extraction-only builds.

### Scope

- Add `D2_ENABLE_ENGINE` CMake option
- Add SDL3 detection (system package first, FetchContent fallback)
- Ensure all existing CLI and tests build with `D2_ENABLE_ENGINE=OFF`
- Ensure `libd2res`, `libd2asset`, and `opendis2-dev-extractor` do not require SDL3
- Add `docs/engine/sdl3_setup.md`

### Acceptance Criteria

- `cmake -DD2_ENABLE_ENGINE=OFF` works without SDL3 installed
- `cmake -DD2_ENABLE_ENGINE=ON` detects or fetches SDL3
- SDL3 warnings are not treated as project warnings
- Existing extraction tests remain unaffected

### Tests

- Configure/build with engine disabled
- Configure/build with engine enabled on macOS
- Verify extraction-only targets do not link SDL3

### Risks

- FetchContent can slow down configure and cause offline build problems
- SDL3 CMake package names vary depending on installation method

---

## Vertical Slice Milestone

The first successful vertical slice is:

1. Build runtime asset package from game data (`build-runtime-assets`)
2. Launch `opendis2 battle-viewer` with `--game-root`
3. Load `AssetDatabase`
4. Resolve one real `AnimationClip`
5. Resolve its frame atlas regions
6. Load atlas texture into SDL
7. Render one attacker and one defender in battle slots
8. Play idle loop
9. Press Space
10. Play attack sequence
11. Flash or mark defender on impact
12. Optionally play death animation
13. Return to idle or end sequence

This is the main milestone. Everything else should support this milestone, not distract from it.

## References

- `research/engine_sdl3_battle_visual_prototype_plan.md`: Full staged implementation plan
- `deep-research-report.md`: Battle screen topology, lifecycle, and presentation assumptions
