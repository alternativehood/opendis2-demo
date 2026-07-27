# Adventure Map Rendering Architecture

Three independent ordering concepts that MUST NOT be conflated into a single
`AdventureDrawLayer`.

- **AdventureRenderPhase** — global compositing domain
- **IsoDepth** — spatial isometric depth within the World phase
- **WorldRenderLevel** — local render level within equivalent spatial depth

## Location

- Core types: `src/d2adventure_render/adventure_render_types.hpp`
- Canonical cell coordinate: `src/d2runtime/MapCellCoord.hpp`
- Geometry: `src/d2adventure_render/map_geometry.hpp` / `.cpp`
- Depth resolver: `src/d2adventure_render/iso_depth_resolver.hpp` / `.cpp`
- Render graph: `src/d2adventure_render/render_graph.hpp` / `.cpp`
- Preparer: `src/d2adventure_render/map_preparer.hpp` / `.cpp`
- Tests: `tests/unit/test_adventure_map_rendering.cpp`

## Coordinate System

All terrain cells and world object positions use the canonical map-cell coordinate
domain defined by `d2runtime::MapCellCoord` (a simple `{int x, int y}` pair). There
is no separate "source" vs "display" coordinate system for map cells — canonical
cells are the single coordinate domain throughout the entire rendering pipeline.

## Data Flow

```
SgParser (raw SG bytes)
       │
       ▼
ScenarioTemplate (serialized tiles[y][x])
       │
       ▼
normalize_raw_sg_terrain(raw)  →  AdventureWorldState.terrain (canonical cells)
       │
       ▼
AdventureWorldState (all positions/objects/terrain as MapCellCoord)
       │
       ▼
AdventureMapPreparer
  ├─ make_road_contributor()       (GroundOverlay phase)
  ├─ make_forest_contributor()     (World / GroundObject)
  ├─ make_mountain_contributor()   (World / Structure)
  └─ make_unit_contributor()       (World / Actor)
       │  project_cell(MapCellCoord) → ScreenPoint (isometric world-space pixels)
       ▼
AdventureRenderGraphBuilder
       │  add_primitive()
       │
       ▼
   finalize(IsoDepthResolver)
   ├─ group by AdventureRenderPhase
   ├─ IsoDepthResolver::resolve() on World primitives
   └─ produce PreparedAdventureRenderGraph
       │
       ▼
PreparedAdventureRenderGraph {
    ground_overlay: vector<Primitive>,
    world:          vector<Primitive>,   -- depth-sorted
    world_overlay:  vector<Primitive>,
    fog:            vector<Primitive>,
    ui_overlay:     vector<Primitive>,
}
```

## Key Types

### `d2runtime::MapCellCoord`

Canonical map-cell coordinate. After `AdventureWorldState` construction, every terrain
cell and world object position uses this same type. `(x, y)` means one physical scenario
cell. Defined in `src/d2runtime/MapCellCoord.hpp`.

### `AdventureMapGeometry`

Single source of truth for isometric projection and canvas bounds. Owns tile
dimensions, computed canvas extent, and the `project_cell()` function. Created once
from the canonical map dimensions via `from_source(map_w, map_h)`.

`project_cell(MapCellCoord)` converts a canonical cell to an isometric world-space
pixel position (`ScreenPoint`):

```
world_x = (cell.x - cell.y) * half_tile_width
world_y = (cell.x + cell.y) * half_tile_height
```

Contributors then apply `-min_world_x` / `-min_world_y` offsets to get canvas-relative
pixel positions plus any needed sprite-anchor adjustments.

`iso_depth(MapCellCoord)` computes `cell.x + cell.y` for draw ordering — increasing
values are spatially closer/front-most. `IsoDepthResolver` sorts primitives in ascending
depth order; lower IsoDepth values are therefore drawn first, and higher IsoDepth values
are drawn later and appear on top.

`derive_depth_anchor(GridFootprint)` returns the front-most cell (maximum IsoDepth)
from a multi-cell footprint, used as the spatial anchor for multi-tile objects.

### `GridFootprint` and `IsoDepthAnchor`

Both are `MapCellCoord` aliases:

- `GridFootprint = std::vector<MapCellCoord>` — the set of canonical cells occupied by an object.
- `IsoDepthAnchor = MapCellCoord` — the single canonical cell used for depth ordering.

### `kTreeSpriteAnchor`

`{125, 145}` — offset from tile center to the tree sprite draw origin. Changes draw
origin only; does not affect cell coordinates or projection math. The render primitive's
`draw_origin` is set to `{tile_center_x - kTreeSpriteAnchor.x, tile_center_y - kTreeSpriteAnchor.y}`.

## The Three Ordering Concepts

### 1. AdventureRenderPhase (global compositing domain)

Phases are ordered (explicit comparator `phase_before()`, not enum values):

```
Terrain < GroundOverlay < World < WorldOverlay < Fog < UIOverlay
```

Objects in different phases never interleave regardless of spatial depth.
Roads (GroundOverlay) always draw before units (World), even if the road is
spatially in front. Mountains taller than their tile may extend into higher
phases in the future, but their *ordering integration point* remains the
World phase.

### 2. IsoDepth (spatial isometric depth)

Within the World phase only. Computed as `depth_anchor.x + depth_anchor.y` via
`AdventureMapGeometry::iso_depth()`. Increasing values are farther from the
camera and draw first (painter's algorithm: far → near).

**IsoDepth has priority over WorldRenderLevel.** A spatially front object
always draws on top of a spatially behind object, regardless of their
WorldRenderLevel values. For example, a mountain (Structure) at depth 10
draws *before* a unit (Actor) at depth 5 because the unit is spatially
in front.

### 3. WorldRenderLevel (local level within equivalent depth)

Only matters when two primitives share the same `IsoDepth` (occupy the same
spatial plane). The levels are:

```
GroundObject < Structure < Actor < Foreground
```

Confirmed same-cell ordering: Forest (GroundObject) draws before
Unit/Stack (Actor) within the same tile.

This is NOT a global object-type draw layer. A Structure at depth 5 does
**not** globally draw below an Actor at depth 10 — the Actor at depth 10
is farther away and draws first.

## Invariants

1. **Phase separation**: No cross-phase interleaving.
2. **Depth priority**: `IsoDepth` overrides `WorldRenderLevel`.
3. **No global semantic layering**: Object type does not imply a global draw
   layer. "Mountain" is a World primitive with `WorldRenderLevel::Structure`;
   it participates in the same IsoDepth pass as units and forests.
4. **Deterministic tie-breaking**: `StableRenderId` (uint64_t) breaks ties
   when both depth and level are equal.
5. **Shared geometry**: All contributors use the same `AdventureMapGeometry`.
   Canonical cells are projected once via `project_cell(MapCellCoord)`.
6. **Contributors are stateless**: Each contributor receives the world state
   and appends primitives. No contributor knows about other contributors.

## Render Primitive

`PreparedAdventureRenderPrimitive` is type-agnostic. It carries:
- `phase` / `level` — ordering metadata
- `container_path` / `record_name` — asset reference
- `draw_origin` — screen-space top-left
- `visual_bounds` — screen-space AABB for clipping
- `footprint` / `depth_anchor` — canonical `MapCellCoord` spatial data
- `stable_id` + `debug_label` — identity and debugging

Semantic preparers populate these fields; the graph builder never inspects
semantic type.

## Geometry

`AdventureMapGeometry` provides the canonical isometric transformation.
Default tile is 64×32 (standard Disciples II diamond). Contributors call
`project_cell(MapCellCoord)` to convert canonical cells to world-space
pixel positions, then apply canvas offsets and sprite anchors as needed.

## Scaffold Contributors

Scaffold contributors produce diagnostic primitives only. No asset filenames
are guessed:

| Contributor | Phase | WorldRenderLevel | Status |
|---|---|---|---|
| Road | GroundOverlay | n/a | Scaffold (no asset mapping) |
| Forest | World | GroundObject | Active (tree sprite mapping) |
| Mountain | World | Structure | Scaffold (asset mapping TBD) |
| Unit/Stack | World | Actor | Active (unit visual resolver) |

Terrain is handled externally via `AdventureTerrainSurfaceComposer` for
performance reasons. A future terrain contributor may integrate it into the
generic pipeline.

## Tests

20 architecture tests cover:
- Phase ordering is explicit (not enum-value-dependent)
- WorldRenderLevel ordering is explicit
- IsoDepth has priority over WorldRenderLevel
- Same-cell Forest before Unit
- Front mountain renders over back unit (and vice versa)
- No global object-type ordering (two objects of different types at
  different depths sort by depth, not by type)
- Deterministic StableRenderId tie-breaking
- VisualBounds independent from depth
- Multi-tile footprint accepted

Animated movement primitives use persistent `MovingStack:Body:<stack-id>` and
`MovingStack:Shadow:<stack-id>` IDs. Segment states are sequential and are never
validated as one simultaneous graph. Stable render IDs, animation clock IDs, and
current animation sequences are separate identities: a sequence may change while
the actor and player slot remain stable. Collision checks compare the active
movement graph with simultaneously rendered static and route-preview primitives.

Ruin runtime data, placement, footprint, and encounter information remain
available. Production visual resolution is supported through `Imgs/IsoCmon.ff`.
Current authored contract:
- typed ruin IMAGE range remains `0..10`
- resolved authored visual range is `0..8`
- selection uses IMAGE plus terrain placement
- land family is `G000RU0000000..0008`
- water family is `G000RU0000100..0108`
- IMAGE `9` and `10` remain explicit unresolved values
- ruin banner index is fixed to `4`
- a ruin banner exists only when its body resolves
- Semantic entity emits zero to N primitives (no assumed 1:1)
- Shared projection across contributors
- Builder groups by phase
- Preparer integration (add_contributor + prepare)
- Road contributor emits GroundOverlay phase
- Empty world produces empty graph
- Depth resolver handles empty, single, and multiple primitives
- StableRenderId defaults to zero

119 terrain tests continue to pass unchanged.

## Phase 5 route preview

Route preview is a dynamic presentation stream derived from `GameSession`'s
authoritative planned movement. The complete destination is emitted as one
dynamic `GroundOverlay` highlight; route markers are dynamic `World`
`ActorUnderlay` primitives at local suborder 50. Static terrain and ground
objects render first, then the destination highlight, then the composed
static and dynamic world stream (including actors and selection), followed by
the existing world overlay.

Preview animation players are owned separately from static map players and
are rebuilt only when the semantic preview changes. Camera pan and zoom only
transform canonical canvas coordinates; they do not rebuild marker geometry or
reset playback.

## Phase 6 movement presentation

During execution the selected stack's static body and shadow are temporarily
excluded from the composed world stream. A dynamic actor body and optional
shadow interpolate between canonical cell foot anchors over 400 ms, using the
destination presentation and midpoint depth-anchor rule. Movement animation
players are owned separately from static and route-preview players. Movement
point labels are screen-space text derived from canvas anchors and disappear
with their authoritative route flags. After each committed step the static
actor is settled at the authoritative cell and the pick index is rebuilt,
without re-preparing unrelated world content or resetting unrelated animation
playback.
