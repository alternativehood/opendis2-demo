# GameSession

`GameSession` is the shared logical game container. All game-state mutation flows through this type.

## Location

- Header: `src/d2game/GameSession.hpp`
- Implementation: `src/d2game/GameSession.cpp`
- Module: `libd2game`
- Dependencies: `d2runtime` (`AdventureWorldState`), `libd2adventure_rules`, and immutable `UnitDef` snapshots from `libd2game_data`

## Design

```
UI / HeadlessFrontend
    │  GameCommand
    ▼
GameSession::handle_command()
    │
    ▼
GameCommandResult { events, quit_requested }
```

- UI/headless code sends `GameCommand` values.
- `GameSession` processes the command and produces a `GameCommandResult`.
- Screens do not mutate runtime state directly.
- All logical changes go through `GameSession::handle_command(...)`.

## Supported Commands (this milestone)

| Command         | Behavior |
|-----------------|----------|
| `GameQuitCommand` | Sets `quit_requested = true`, emits `GameQuitRequested` |
| `GameNoOpCommand` | No-op, empty result |
| `GameInspectWorldCommand` | Emits deterministic `GameInspectResult` |
| `GameAdvanceFrameCommand` | No-op; never advances movement |
| Adventure movement commands | Select, plan, confirm, and explicitly commit one route step |

## State

`GameSession` owns an `AdventureWorldState` (from `d2runtime`), an immutable movement-profile catalog, and read-only-exposed `AdventureMovementState`. Typed command variants are dispatched with `std::visit`; logical movement outcomes are returned as typed events. Movement execution is authoritative and cell-by-cell: each explicit step revalidates the immutable navigation facts and mutates position and movement points atomically.

The adventure preview accessor is read-only and derives an asset-neutral `AdventureRoutePreview` from the current planned movement. It does not cache a second preview or mutate session state. Typed command variants never carry movement profiles; the immutable catalog is the only profile source. Public world and movement-state access remains read-only, and execution remains explicit one-cell-at-a-time.

## Constraints

- No I/O
- No SDL dependency
- No direct mutation from UI code
- No mutable world or movement-state accessors
- No SDL, rendering, animation, or input dependencies

## Future

- Full turn processing
- AI command execution
- Game event emission/subscription
- Scenario win/loss detection

See also: `AdventureWorldState`, `HeadlessFrontend`, `AdventureScreen`.
