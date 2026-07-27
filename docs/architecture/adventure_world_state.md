# AdventureWorldState

Minimal runtime world state for the adventure/map screen. This is a thinned,
value-type view of the parsed `ScenarioTemplate`.

## Location

- Header: `src/d2runtime/AdventureWorldState.hpp`
- Builder: `src/d2runtime/AdventureWorldBuilder.hpp` / `.cpp`
- Module: `libd2runtime`
- Dependencies: `d2scenario` (uses `ScenarioTemplate` as builder input only —
  no parser/debug headers)

## Design

```
ScenarioTemplate (parser output)
    │
    ▼
AdventureWorldBuilder::build()
    │
    ▼
AdventureWorldBuildResult {
    world: AdventureWorldState,
    diagnostics: vector<BuildDiagnostic>
}
```

## AdventureWorldState Fields (this milestone)

| Field                  | Source |
|------------------------|--------|
| `scenario_id`          | Copied from `SgScenarioInfo::id` |
| `scenario_name`        | Copied from `SgScenarioInfo::name` |
| `map_width`            | Priority: 1. terrain grid, 2. `SgScenarioInfo::map_size` |
| `map_height`           | Priority: 1. terrain grid, 2. `SgScenarioInfo::map_size` |
| `terrain_tiles`        | `width * height` (logical tile count). 0 if no terrain grid. |
| `semantic_object_count`| Total objects across all `ScenarioTemplate` vectors |
| `runtime_object_count` | Objects copied into `AdventureWorldState::objects` |
| `objects`              | `vector<WorldObjectEntry>` — copied ids + kind strings |

### Map Dimension Priority

1. **Terrain grid** — if `scenario.map.terrain.width > 0` and
   `scenario.map.terrain.height > 0`, use terrain dimensions. No map-size
   diagnostics are emitted.
2. **Fallback to `info.map_size`** — if above fails and
   `scenario.info.map_size > 0`, use `map_size x map_size`.
3. **Diagnostics** — if neither is valid, emit `MissingMapDimensions`.

### Terrain Tile Count

`terrain_tiles` = `width * height` (logical tile count for a rectangular grid).

If the stored tile data size does not match, a `ParserWarning` diagnostic is
emitted. No data is truncated.

## Build Diagnostics

`BuildDiagnostic` has a `BuildDiagnosticKind` enum, a message string, and
optional object id/class.

Supported kinds:
- `MissingMapDimensions` — no valid map dimensions from terrain or info
- `InvalidMapDimensions` — (reserved for negative/irregular dimension data)
- `EmptyObjectId` — an object has an empty id (skipped from runtime objects)
- `IgnoredObjectClass` — (reserved) object class explicitly unsupported
- `UnknownObjectClass` — (reserved) object class not recognized
- `ParserWarning` — non-fatal storage issues (e.g. tile count mismatch)

### Error vs Warning Classification

- **Errors**: `MissingMapDimensions`, `InvalidMapDimensions`, `EmptyObjectId`
- **Warnings**: `IgnoredObjectClass`, `UnknownObjectClass`, `ParserWarning`

## Constraints

- `AdventureWorldState` does NOT store pointers into `ScenarioTemplate`
- All data is copied (ids, names, counts)
- Builder is stateless — one `build()` call produces a complete result
- No SDL dependency
- `d2runtime` must NOT include parser/debug headers (e.g. `SgTypes.hpp`).
  The only permitted dependency is `d2scenario/ScenarioTemplate.hpp`.

## Coordinate Contract

AdventureWorldState is the canonical coordinate boundary. After construction:

- `terrain` cells, `map_width`, `map_height` are canonical (normalized from raw SG).
- `AdventureMapObject.position`, `AdventureRoad.position`, `AdventureMountain.position`,
  `AdventureStack.position` all use `d2runtime::MapCellCoord`.
- `AdventureMapObject.footprint` cells use `FootprintCell` (= `MapCellCoord`).
- `AdventureMountain.footprint` cells use `FootprintCell`.

Raw `POS_X` / `POS_Y` exist only in parsed SG structures (`SgMidgardPlan`, `SgMountain`,
etc.) before world building. `AdventureWorldBuilder::build()` converts them into canonical
`MapCellCoord` values.

No consumer of AdventureWorldState may transpose or swap map-cell coordinates. The grid
is already canonical — positions map 1:1 to the isometric projection via
`AdventureMapGeometry::project_cell(MapCellCoord)`.

## Future

- Full unit/stack/city runtime model
- Position/index structures
- Fog of war state
- Turn state
