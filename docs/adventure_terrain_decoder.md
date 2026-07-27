# Adventure Terrain Decoder

`src/d2runtime/AdventureTerrainDecoder.*` is production runtime code for decoding raw scenario
terrain values. It produces reusable terrain tile descriptors and expected asset record names.
`src/d2engine/assets/adventure_terrain_asset_resolver.*` resolves those records against
`TerrainAssetCatalog`. `src/d2engine/assets/adventure_terrain_surface.*` composes decoded terrain
into CPU tile surfaces. The PNG preview tool is a visual frontend over this production path.

## Raw SG Orientation And Canonical Terrain

`SgParser` reconstructs raw SG terrain in serialized/source orientation (row-major:
`tiles[y][x]`). `ScenarioTemplate` carries those raw tiles as-is.

`AdventureWorldBuilder::build()` calls `normalize_raw_sg_terrain()` to produce canonical
terrain. `AdventureWorldState.terrain` is the canonical grid — it is always normalized.

Canonical dimensions:

- `width = raw height`
- `height = raw width`

Canonical cell mapping:

- `canonical(x, y) = raw(y, x)`

After `AdventureWorldState` construction, no Adventure renderer performs terrain orientation
conversion. All consumers — terrain preview, border candidate extraction, terrain debug
rendering, `AdventureMapPreparer` contributors — operate on canonical cells directly.

## Raw Value Formula

For each raw `uint32_t` terrain value:

- `low_byte = raw_value & 0xFF`
- `byte1 = (raw_value >> 8) & 0xFF`
- `byte2 = (raw_value >> 16) & 0xFF`
- `high_byte = (raw_value >> 24) & 0xFF`
- `low_word = raw_value & 0xFFFF`
- `high_word = (raw_value >> 16) & 0xFFFF`
- `family_id = low_byte & 0x07`
- `terrain_flags = low_byte & 0x38`
- `variant_bits = (low_byte >> 3) & 0x07`
- `border_shape = (raw_value >> 26) & 0x3F`
- `has_border_shape = border_shape > 0`
- `has_drawable_border_shape = border_shape > 0 && border_shape != 16`
- `has_unknown_high_bits = (raw_value & 0x03FFFF00) != 0`

## Material Mapping

`family_id` is diagnostic only. Production material decoding uses the full low byte.

| low_byte | Material | Terrain code |
| --- | --- | --- |
| 0 | Black | BL |
| 1, 9 | Human | HU |
| 2, 10 | Dwarf | DW |
| 3, 11 | Heretic | HE |
| 4, 12 | Undead | UN |
| 5, 13, 37 | Neutral | NE |
| 6, 14 | Elf | EL |
| 7, 29 | Water | WA |

Any other low byte maps to material `Unknown`, terrain code `BL`, and `unknown_material=true`.

## Asset Mapping

Ground records use `<CODE>_<VARIANT:02d>.PNG`. Land terrain uses `variant_bits & 0x03`; `WA` and
`BL` always use `WA_00.PNG` and `BL_00.PNG`.

Raw `border_shape` values are logical keys, not permanent FF record IDs. The first production
resolver table maps drawable raw shapes to same-numbered records, but future calibration belongs in
`AdventureTerrainBorderShapeResolver`, not in the composer or preview tool.

Transitions use border family `WA` when material A or material B is water; otherwise they use `NE`.
`border_shape == 16` is intentionally non-drawable. `Imgs/GrBorder.ff` has no `NE_16_00.PNG` or
`WA_16_00.PNG`, so shape 16 must not be reported as missing.

## Surface Composition

`Ground.ff` records are continuous material sources, not finished 64x32 tiles. For display tile
`(x, y)` and local pixel `(px, py)`, the composer uses:

- `screen_x = (x - y) * 64 / 2`
- `screen_y = (x + y) * 32 / 2`
- `sample_x = screen_x + px`
- `sample_y = screen_y + py`
- `u = positive_mod(sample_x, texture_width)`
- `v = positive_mod(sample_y, texture_height)`

`GrBorder.ff` records are transition masks, not final sprite art. Mask luminance blends the same
global samples from materials A and B. Magenta or alpha-zero mask pixels keep material A; black keeps
material A; white chooses material B; gray blends linearly.

Water shores may be synthesized from display-space neighboring materials when raw `border_shape` is
zero. Shape 16 remains non-drawable and does not synthesize a border for that tile.

## Preview And Calibration Tools

`opendis2-dev-terrain-preview` operates on canonical terrain cells directly. It writes a full
timestamped dump directory containing preview PNGs, per-tile CSV, border candidates, and a summary.

`opendis2-dev-grborder-atlas` emits raw and composite `GrBorder.ff` atlases. `--border-shape-map`
can calibrate logical border shapes to concrete records and transforms without hardcoding guessed
tables.

## Known Limits

- Border record mapping is still the initial production table and may need calibration.
- No GrBorder mask recoloring yet.
- No IsoTerrn overlays yet.
