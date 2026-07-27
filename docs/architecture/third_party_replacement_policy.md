# Third-Party Library Replacement Policy

Use ready-made libraries for standard infrastructure tasks. Keep project-specific
reverse-engineering and pixel-parity logic. Do not write handwritten alternatives
where a proven library exists.

## ✅ Approved replacements

| Library | Purpose | Notes |
|---------|---------|-------|
| SDL3\_ttf | Runtime game/UI text layout and rendering | Already integrated in `TextBoxRenderer` |
| SDL3\_image | Runtime image loading (PNG, etc.) | Replaces hand-written lodepng decode in engine runtime path |
| SDL3\_mixer | Future runtime audio playback and mixing | Do NOT write a custom mixer/audio scheduler |
| Dear ImGui | Dev/tuning UI panels | Preferred over hand-written SDL widget/overlay systems |
| stb\_rect\_pack or rectpack2D | Atlas rectangle packing | Replaces custom shelf packer in `AtlasPacker` |
| JSON Schema validator (pboettch/json-schema-validator) | Structural JSON validation | Replaces hand-written `contains/is_string` checks |
| vcpkg manifest mode (or Conan 2) | Dependency management | Prefer manifest-managed deps over FetchContent for common libs |
| magic\_enum | Internal debug/log/test enum string conversion | NOT for serialized config/manifest values |

## 🚫 Keep project-specific

- Disciples II resource parsers and semantic extraction (MQDB, OPT, WDB, DLG, DAT, DBF)
- `TreeLayout` and pixel-parity placement profiles
- Battle visual events and animation semantics
- Unit/effect role mapping and reverse-engineered timing
- Game scenario/presenter model
- Battle animation engine (deterministic C++/data-driven)

## 🛑 Not now

- **ECS/EnTT rewrite**: Do not rewrite `BattleScene`/`BattlePresenter`/`BattleAnimationEngine`
  into ECS. Consider EnTT only after map layer/full game loop creates broad entity-component
  pressure.
- **Lua/scripting rewrite**: Do not introduce Lua or another scripting runtime for battle
  animation. Current roles/timings are reverse-engineered and must remain deterministic
  C++/data-driven until the visual model stabilizes.
- **Generic UI layout engine**: Do not replace `TreeLayout` with Yoga/Flexbox or similar.
- **Generic game engine framework**: No wholesale engine replacement.

## 📋 Policy enforcement

- Code reviews must catch new hand-written infrastructure before merge.
- New dependencies for standard tasks must match this policy table.
- Any exception must be documented in both the commit message and a nearby source comment.
