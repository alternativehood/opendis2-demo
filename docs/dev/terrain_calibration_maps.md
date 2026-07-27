# Terrain Calibration Maps

Use custom editor maps to calibrate `GrBorder.ff` mapping. Player maps are noisy: objects, roads,
overlays, and authored coastlines hide the raw terrain signals.

Raw SG terrain is normalized exactly once before AdventureWorldState.
AdventureWorldState terrain is already canonical. Terrain calibration and rendering code
operating on AdventureWorldState must use canonical coordinates directly and must not
transpose them.

Recommended map:

- size `16x16` or `24x24`;
- top quadrant: `HU`;
- right quadrant: `NE`;
- bottom quadrant: `HE`;
- left quadrant: `DW`;
- center: `WA` lake;
- straight water boundaries in all four directions;
- diagonal water boundaries;
- single-tile islands;
- known land-land transitions.

Save original game screenshots for each calibration map. Then run:

```bash
opendis2-dev-grborder-atlas \
  --game-root /path/to/Disciples2 \
  --out-dir /tmp/grborder_atlas \
  --family both

opendis2-dev-terrain-preview \
  --scenario /path/to/calibration.sg \
  --game-root /path/to/Disciples2 \
  --output /tmp/terrain_dumps/preview.png
```

Put calibrated logical-shape mappings in a JSON file passed with `--border-shape-map`. Do not
hardcode map-specific border tables in the terrain composer.
