# Future Engine Dependencies — Guardrails

## ECS (EnTT)

ECS/EnTT is not an immediate refactor target.
Do not rewrite BattleScene/BattlePresenter/BattleAnimationEngine into ECS now.
Consider EnTT only after map layer/full game loop creates broad entity-component pressure.

Current priority:
- BattleSessionController
- BattleRenderPlan
- Presenter/debug split

## Lua / Scripting

Animation scripting policy:
Current battle animation roles/timings are reverse-engineered and must remain
deterministic C++/data-driven until the visual model stabilizes.
Do not introduce Lua or another scripting runtime for battle animation as a
shortcut. If scriptability is needed later, first design a declarative data
format and tests for existing S1/A1/A2/impact/FX semantics.

## Audio (SDL3\_mixer)

WDB/MQDB parsing and sound id extraction are D2-specific and stay in `d2res/`.
Playback, mixing, channels, music streaming, panning and volume control should
use SDL3\_mixer when runtime audio is implemented.
Do not build a handwritten mixer/audio scheduler in this codebase.

## Debug UI (Dear ImGui)

Dear ImGui is the preferred UI layer for interactive tuning panels.
Keep tuning state and edit/apply/save logic in controllers/model classes.
Do not add new hand-written SDL widget systems or ad-hoc overlay panels;
expose new controls through ImGui on top of existing tuning APIs.

## Dependency management

Prefer manifest-managed packages for common third-party libraries (vcpkg/Conan).
Do not add new network-only FetchContent dependencies for standard SDL/image/text/UI/schema
libraries unless the manifest option is impossible and the reason is documented.
