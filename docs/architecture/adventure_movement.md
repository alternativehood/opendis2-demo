# Adventure movement rules

`libd2adventure_rules` is the authoritative headless adventure gameplay-rules layer. It owns movement profiles and cell traversal policy, with no SDL, rendering, screen, input, asset, or engine dependencies. `d2runtime` owns runtime world/value types and canonical terrain classification. `d2game` will later own commands and authoritative world mutation; `d2engine` will later own pointer interaction and visual interpolation only. `d2adventure_render` must not calculate passability or movement cost.

Movement profiles resolve as follows: `water_only` leaders are Swimming; leaders with native Plain, Forest, and Water capabilities are Flying; all others are Walking. Native capability order is irrelevant, and an empty capability list is Walking.

Traversal costs are authoritative in `evaluate_adventure_movement_cell`: Flying costs 2 on Plain, Forest, and Water; Swimming costs 2 on Water and is restricted elsewhere; Walking costs 1 on any road, otherwise Plain costs 3, Forest 4, and Water 6. Blocking objects and occupied stacks block every profile. Unknown, Exceptional, Mountain, and Hill grounds fail closed as unsupported.

Phase 2 adds the immutable `AdventureNavigationMap`, built from canonical terrain, typed roads, blocking `AdventureMapObject` footprints, and visible stack positions. Generic Stack map objects and contained stacks are deliberately ignored. Navigation cells retain blocking-object and stack identities; a future pathfinder can ignore only the moving stack's own current occupancy through `ignored_stack_id`.

Invalid structural world data fails closed and produces no map. Out-of-bounds roads are warnings. Missing or out-of-bounds blocking footprints are errors. Overlapping blocking objects are warnings because movement remains unambiguously blocked. Stack overlap and stack-on-blocking-object are errors.

The map contains facts only; `AdventureMovementPolicy` remains the sole movement-cost and passability authority. No destination interaction, world mutation, or rendering exists in the navigation-map phase.

Phase 3 implements `AdventurePathfinder` as deterministic weighted A*. It searches eight neighbors in fixed order: `{0,-1}`, `{1,-1}`, `{1,0}`, `{1,1}`, `{0,1}`, `{-1,1}`, `{-1,0}`, `{-1,-1}`. Orthogonal and diagonal edges use the same destination-cell movement cost, and diagonal movement performs no corner-side checks. Passability and costs come only from `AdventureMovementPolicy`; the immutable navigation map is never mutated, and the moving stack ID is ignored only through its existing occupancy API. Occupied destinations remain blocked.

Routes exclude the starting cell. Each route step stores the exact map delta, entry cost, and cumulative cost; no D0..D7 animation-direction mapping exists in the gameplay-rules layer. A* uses Chebyshev distance multiplied by the profile's derived minimum passable step cost. Open-set ordering is lower `f`, lower `h`, then earlier insertion sequence, and equal-cost rediscovery keeps the first predecessor. The pathfinder does not consider movement points.

No destination interactions, UI preview, flags, GameSession mutation, or actor animation exist in this phase. The next phase will add authoritative movement-planning state and commands for selected stack, planned route, confirmation, movement-point validation, and execution lifecycle.

Phase 4 places authoritative adventure interaction state in `GameSession`. Unit movement profiles come from an immutable value catalog derived from `UnitDef`; commands never provide profiles. Planning stores the complete minimum-cost route, including routes beyond current movement points. The affordable prefix and first unaffordable step are retained for preview consumers.

Confirmation enters `Moving` without changing position. Each explicit step command revalidates the next cell and atomically updates stack position, facing, and movement points. Already committed steps are never rolled back after interruption. The selected stack remains selected after successful completion, and all other adventure interaction commands are locked while moving. Quit, Inspect, NoOp, and AdvanceFrame remain available; AdvanceFrame does not drive movement.

No UI, markers, actor interpolation, or D0..D7 mapping exists yet. Ownership and active-player rules are not implemented because no canonical active-player state exists. The next phase will integrate screen-cell projection, selection and destination clicks, route-preview marker classification, `IsoCmon.ff` markers, and repeat-click confirmation.

Phase 4.5 derives an asset-neutral route preview from the authoritative `AdventurePlannedMovement`. The preview is built on demand and is not stored as a second route or state copy. Its only semantic marker kinds are `Normal` and `ActionLimit`; the rules layer contains no `IsoCmon.ff` asset names. A fully affordable route exposes every step as `Normal`. An over-budget route exposes the affordable prefix as `Normal` and every remaining step as `ActionLimit`, while preserving the complete selected destination separately. The starting cell is never a preview marker.

Idle and StackSelected expose no preview. Moving exposes the remaining execution suffix. Malformed route-preview input fails with `std::invalid_argument`. Hostility, diplomacy, battle-boundary, negotiation, and incompatibility markers are not implemented.

Phase 5 wires the graphical movement-profile catalog from `GameDataRegistry` while keeping `GameSession` authoritative for selection and planning. `AdventureScreen` derives selection from session state and converts logical screen coordinates through the camera into canvas coordinates, then uses exact map-cell geometry; empty-cell picking does not use the actor pick index. A stack click dispatches Select, an empty-cell click with a selection dispatches Plan, stack interaction takes precedence, and this phase never dispatches Confirm.

The graphical preview maps `Normal` to `MOVENORMAL`, `ActionLimit` to `MOVEACTION`, and the complete destination to `TILE_HIGHLIGHT`. Markers use authored semantic anchors and remain in canvas coordinates through pan and zoom. Preview animation players are owned separately from static map players and rebuild only when the semantic preview changes.

Phase 6 keeps the complete route visible before confirmation: affordable steps are `Normal` and all later steps are `ActionLimit`. A repeated click on the planned destination with at least one affordable step executes exactly the affordable prefix; zero affordable steps remains rejected. Moving shows only the remaining execution suffix, and flags disappear after authoritative step commits. Confirmed movement is presented as 400 ms interpolated segments using destination-cell visuals; each completed segment commits exactly one authoritative GameSession step, including the canonical D0..D7 facing direction. The final static actor settles at its authoritative position and facing, receives a union interaction mask built from its exact resolved Idle frames, and the pick index is rebuilt. The discarded over-budget suffix is not retained.

The sole authoritative route-delta mapping is:

| Delta | Direction |
|---|---|
| `{1, 0}` | `D6` |
| `{1, -1}` | `D5` |
| `{0, -1}` | `D4` |
| `{-1, -1}` | `D3` |
| `{-1, 0}` | `D2` |
| `{-1, 1}` | `D1` |
| `{0, 1}` | `D0` |
| `{1, 1}` | `D7` |

Moving body and shadow stable IDs persist for the complete route. Future segments are temporal states of one actor, not concurrent render graphs, so repeated movement IDs are valid across segments. Animation-clock validation is scoped to each segment and checks only external static and route-preview domains. Playback identity includes primitive/player IDs, animation names, containers, ordered frame records and durations, loop/timing policy, and shadow synchronization topology. Identical identities preserve player progress; changed identities replace the movement players at frame zero. Equal body/shadow frame counts share the body clock, while unequal counts use independent clocks; missing shadows are valid. Update deltas are split at 400 ms boundaries so excess time advances the next segment's players.

Planning always retains the complete minimum-cost route, including its `MOVEACTION` suffix. On confirmation, an over-budget route with at least one affordable step executes only the canonical affordable prefix; the requested destination remains in the start event, while the execution destination and final position are the last affordable cell. The discarded suffix is not retained as a next-turn order. A route with zero affordable steps remains planned and is rejected with `InsufficientMovementPoints`. The presentation layer receives only the execution route, so its preview and settlement end at the actual prefix endpoint.

The adventure debug Movement panel exposes read-only mode, selection, current points, and the immutable session-start reset value. `Reset move points` restores the non-negative movement value captured when `GameSession` was created; `Free move points` sets exactly `1048576`. Both commands are routed through `GameSession`, are unavailable while Moving, and refresh planned-route affordability without rerunning pathfinding or confirming movement.

The same debug panel provides `Follow unit`; while enabled, the AdventureScreen camera centers on the interpolated actor foot during movement and falls back to the authoritative stack cell at settlement.
