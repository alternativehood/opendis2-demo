# Asset Pipeline Inventory

Stage 0 of the Runtime Asset Layer plan. Documents every file type produced by
`extract-all` (and the individual `extract-*` commands), their JSON schemas,
stability classifications, and intended runtime roles.

Stage 8 adds a separate canonical runtime package. Extraction output remains a
diagnostic and migration format; engine consumers should use the type-oriented
layout documented in [package_layout.md](package_layout.md). The package builder
reuses the existing extractors, assembles their selected outputs under
`images/`, `animations/`, `atlases/`, `sounds/`, and `data/`, then validates the
result through `AssetDatabase`.

Stage 7 adds root `asset_links.json`. Package assembly owns reference discovery
and deterministic serialization after canonical assets and tables exist.
`libd2asset` owns extraction-independent validation, immutable indexes, and
source, target, and unresolved queries. It does not own naming heuristics, raw
DBF/DLG/WDT parsing, rendering, audio, scripting, or database access.

Stage 9 adds the headless
[runtime engine contract](engine_contract.md). `opendis2-dev-asset-inspect` (planned) consumes only a
completed canonical package through `libd2asset` and emits deterministic JSON
for an exact animation ID, its ordered frames and visual sources, plus an
optional existing sound reference. It does not replace the later sound metadata,
data-table, or cross-reference resolver stages.

Stage 5 sound access converts extractor sound JSON into the versioned canonical
sidecar documented in
[sound_asset.md](schemas/sound_asset.md). Runtime packages record the exact
`.wav` or `.bin` payload path and validate its byte size; extractor sidecars
remain provisional extraction output.

**Stability levels:**
- **Stable** — schema and path are locked; safe as engine contract
- **Provisional** — functional but may gain fields; use with forward-compatibility guard
- **Unstable** — known to be temporary or incomplete; do not rely on as engine input yet

**Data source:** Real `extract-all` run on
`Disciples II Rise of the Elves` game root (June 2026).
Sound schema verified via standalone `extract-sounds` on `Battle.wdb`.
DBF/DAT/DLG schemas verified via standalone `extract-*` runs.

---

## Images

Produced by: `extract-images`, `extract-all`

Each logical image in a `.ff` container is written as a pair of files.

### Path pattern

```
<out>/<relative-container-path>/<logical_name>.png
<out>/<relative-container-path>/<logical_name>.json
```

Example: `Imgs/Battle.ff/_0.png` and `Imgs/Battle.ff/_0.json`

### PNG file

32-bit RGBA PNG. Dimensions match `output_size`. Alpha channel reflects the
`transparency_mode` of the source image block. No extra metadata is embedded in
the PNG.

**Stability: Stable.** RGBA PNG format will not change.

### JSON sidecar

```json
{
  "base_image": "string",
  "logical_name": "string",
  "output_size": { "h": 600, "w": 800 },
  "palette_id": 35,
  "parts": [
    {
      "dest": { "x": 352, "y": 310 },
      "source_rect": { "h": 282, "w": 32, "x": 0, "y": 448 }
    }
  ],
  "source_container": "/abs/path/to/Container.ff",
  "transparency_mode": "AdditiveBlend",
  "transparent_colors_bgr": ["#000000"]
}
```

| Field | Type | Description |
|---|---|---|
| `base_image` | string | Logical name of the tile-sheet image used as the pixel source for multi-part reconstruction |
| `logical_name` | string | Unique identifier for this image within its container; used as the stable cross-reference key |
| `output_size` | object | Width and height of the decoded RGBA output in pixels |
| `palette_id` | int | Index into the container's internal palette table |
| `parts` | array | Ordered list of tile-blit operations that reconstruct the image from `base_image`; empty for single-tile images |
| `source_container` | string | Absolute path to the `.ff` container from which this image was decoded |
| `transparency_mode` | string | One of `"None"`, `"ColorKeyMagentaRange"`, `"AdditiveBlend"` |
| `transparent_colors_bgr` | array | List of hex color strings used as transparency keys; `["#ff00ff"]` for magenta, `["#000000"]` for additive |

**Stability: Provisional.** Core fields are stable. `parts.dest` currently carries
render-offset coordinates but lacks a pivot/anchor field; that will be added
when Stage 3 (`runtime-atlas-access`) resolves the pivot question.

### Container-level files

`manifest.json` and `warnings.txt` are written to the container output directory
by both `extract-images` and `extract-anim`. When `extract-all` runs both on the
same container, the animation run's `manifest.json` overwrites the image run's.
See [Warnings and Reports](#warnings-and-reports) for their schemas.

---

## Animations

Produced by: `extract-anim`, `extract-all`

Each named animation in a `.ff` container is written as a directory of frame
PNGs plus a descriptor.

### Path pattern

```
<out>/<relative-container-path>/<anim_name>/frame_NNN.png   (zero-padded to 3 digits)
<out>/<relative-container-path>/<anim_name>/anim.json
<out>/<relative-container-path>/<anim_name>/preview.gif     (optional; not written by extract-all)
```

Example: `Imgs/Battle.ff/SELSMALLA/frame_000.png` through `frame_015.png`
and `Imgs/Battle.ff/SELSMALLA/anim.json`

### Frame PNGs

32-bit RGBA PNG, one per frame. Dimensions match `frames[i].width` × `frames[i].height`.

**Stability: Stable.**

### anim.json

```json
{
  "frame_count": 16,
  "frame_delay_ms": 100,
  "frames": [
    { "height": 89, "index": 0, "logical_name": "HU", "width": 74 },
    { "height": 89, "index": 1, "logical_name": "IU", "width": 74 }
  ],
  "name": "SELSMALLA",
  "source_container": "/abs/path/to/Container.ff"
}
```

| Field | Type | Description |
|---|---|---|
| `frame_count` | int | Total number of frames in the animation |
| `frame_delay_ms` | int | Inter-frame delay in milliseconds — **currently hardcoded to 100 ms**; real per-frame timing is not yet decoded from the source format |
| `frames` | array | Ordered frame descriptors |
| `frames[].index` | int | Zero-based frame index; matches the `NNN` suffix in the frame PNG filename |
| `frames[].logical_name` | string | The source image's `logical_name`; links back to the individual image sidecar JSON |
| `frames[].width` / `frames[].height` | int | Decoded frame dimensions in pixels |
| `name` | string | Animation name; matches the output subdirectory name |
| `source_container` | string | Absolute path to the `.ff` container |

**Stability: Provisional.** `frame_delay_ms` is a placeholder (hardcoded 100 ms,
not derived from game data). This field will change when real animation timing is
decoded. See open question [Animation Timing](open_questions.md#animation-timing).

### preview.gif

Optional animated GIF written by `extract-anim --gif`. Not produced by
`extract-all`. Useful for human inspection; not intended as engine input.

**Stability: Unstable.** GIF format is lossy (256-color palette); not suitable
for engine use.

---

## Atlas Sheets

Produced by: `extract-atlas` — **not produced by `extract-all`**

Atlas packing is a separate step that takes the decoded images from a `.ff`
container and bins them onto large texture sheets.

### Path pattern

```
<out>/atlas_NNN.png    (zero-padded sheet index)
<out>/atlas.json
```

Example: `atlas_000.png` through `atlas_031.png` and `atlas.json`

### atlas_NNN.png

RGBA PNG of dimension up to `max_sheet_size × max_sheet_size`. Contains all
sprites that fit on this sheet, packed by a bin-packing algorithm.

**Stability: Provisional.** Sheet layout is deterministic for a given set of
images and `max_sheet_size`, but will change if new images are added, the packing
algorithm is updated, or the sheet size changes.

### atlas.json

```json
{
  "entries": [
    { "h": 600, "name": "BOAT_0_BG", "sheet": 0, "w": 950, "x": 0, "y": 0 }
  ],
  "max_sheet_size": 4096,
  "sheet_count": 32,
  "skipped_sprites": 0,
  "source_container": "/abs/path/to/Container.ff",
  "total_sprites": 1229
}
```

| Field | Type | Description |
|---|---|---|
| `entries` | array | One entry per packed sprite |
| `entries[].name` | string | Image `logical_name`; links back to the image sidecar JSON |
| `entries[].sheet` | int | Index of the atlas sheet PNG this sprite is on |
| `entries[].x` / `entries[].y` | int | Top-left pixel position on the atlas sheet |
| `entries[].w` / `entries[].h` | int | Sprite dimensions in pixels |
| `max_sheet_size` | int | Maximum sheet side length used for packing |
| `sheet_count` | int | Total number of sheet PNGs written |
| `skipped_sprites` | int | Sprites that did not fit and were skipped |
| `source_container` | string | Absolute path to the `.ff` container |

**Stability: Provisional.** Schema is complete; layout will shift when packing
changes. `skipped_sprites > 0` is a known edge case for very large sprites.

---

## Sounds

Produced by: `extract-sounds`, `extract-all`

Each audio record in a `.wdb` sound bank is written as a pair of files.

### Path pattern

```
<out>/<relative-container-path>/<logical_name>.wav
<out>/<relative-container-path>/<logical_name>.json
```

Example: `Sounds/Battle.wdb/0001_HIT_B.wav` and `Sounds/Battle.wdb/0001_HIT_B.json`

### WAV file

Raw audio payload wrapped in a RIFF/WAV header. Most records use MP3 encoding
(`format_tag: 85`), not uncompressed PCM, despite the `.wav` extension.
Playback requires an MP3-capable audio decoder.

**Stability: Stable.** Binary format is a pass-through from the source container.

### JSON sidecar

```json
{
  "channels": 1,
  "detected_format": "WAV",
  "format_tag": 85,
  "logical_name": "0001_HIT_B",
  "payload_size": 5894,
  "sample_rate": 44100,
  "source_container": "/abs/path/to/Container.wdb"
}
```

| Field | Type | Description |
|---|---|---|
| `channels` | int | Number of audio channels (1 = mono, 2 = stereo) |
| `detected_format` | string | High-level format label; currently always `"WAV"` |
| `format_tag` | int | RIFF WAVEFORMAT tag: 1 = PCM, 85 = MP3 |
| `logical_name` | string | Record name from the container index |
| `payload_size` | int | Byte size of the raw audio payload (not including WAV header) |
| `sample_rate` | int | Samples per second |
| `source_container` | string | Absolute path to the `.wdb` container |

**Stability: Provisional extraction schema.** `bit_depth` field is absent from current output
(present in spec but not yet emitted). `Capital.wdb` extraction fails with a
UTF-8 error in sound names; see [Capital.wdb UTF-8 bug](open_questions.md#capitalwdb-utf-8-bug).

Canonical runtime packages do not expose this extractor JSON directly. They
write `sound_schema_version: 1`, canonical identity, a package-relative
`payload_path`, actual `payload_size`, lowercase format, nullable optional
technical fields, and warnings.

### Container-level manifest

See [Warnings and Reports](#warnings-and-reports).

---

## Data Tables

Produced by: `extract-dbf`, `extract-dat`, `extract-dlg` — **not produced by `extract-all`**

These commands process the game's non-image structured data files found primarily
under `Globals/` and `Interf/`.

### DBF (dBASE III)

Source files: `Globals/*.dbf`

Output:
```
<out>/<name>.schema.json
<out>/<name>.records.json
```

**schema.json:**
```json
{
  "fields": [
    { "decimal_count": 0, "length": 10, "name": "UNIT_ID", "type": "C" },
    { "decimal_count": 0, "length": 2,  "name": "UNIT_CAT", "type": "N" }
  ]
}
```

**records.json:** JSON array of row objects, one object per record, all values as
strings.

```json
[
  { "UNIT_ID": "G000UN0001", "UNIT_CAT": "1", "LEVEL": "1", ... }
]
```

**Stability: Stable.** Direct transformation of the dBASE III binary format; no
interpretation beyond field type codes.

### DAT (KEY=VALUE)

Source files: `*.dat` (e.g., `gameinfo.dat`)

Output: `<out>/<name>.json` — flat object, `key → string value`:

```json
{
  "FullName": "Disciples II - Rise of the Elves",
  "Localization": "English",
  "ShortName": "Disciples2ROTE"
}
```

**Stability: Stable.** Trivial format; no ambiguity.

### DLG (Dialog definitions)

Source files: `Interf/*.dlg`

Output: `<out>/<name>.json` — JSON array, one object per dialog:

```json
[
  {
    "bounds": [0, 0, 573, 586],
    "elements": [
      {
        "bounds": [421, 452, 470, 546],
        "id": "BTN_YES",
        "images": ["DLG_ACTION_RESULT_SEAL_OK_N", "..."],
        "text_key": "X100TA0020",
        "type": "BUTTON"
      }
    ],
    "id": "DLG_ACTION_RESULT"
  }
]
```

| Field | Type | Description |
|---|---|---|
| `id` | string | Dialog identifier |
| `bounds` | array[4] | `[left, top, right, bottom]` in screen coordinates |
| `elements` | array | UI elements within the dialog |
| `elements[].type` | string | Element type: `"BUTTON"`, `"TEXT"`, `"IMAGE"`, etc. |
| `elements[].id` | string | Element identifier |
| `elements[].images` | array | Image logical names for element states (normal, hover, click, disabled) |
| `elements[].text_key` | string | Localization key for display text |

**Stability: Provisional.** Not all element types are fully decoded; unknown
element types emit a partial object. Schema will grow as more element types are
mapped.

### Canonical Runtime Data Tables

Stage 6 converts standalone DBF schema/records JSON, DAT object JSON, and DLG
array JSON into versioned sidecars under `data/`. These sidecars use one
runtime-facing table model with explicit kind, ordered columns, deterministic
row keys, ordered cells, recursive values, warnings, and extensions.

Extractor JSON remains an intermediate format under package `.work`; runtime
consumers read only canonical sidecars through `libd2asset`.

---

## Warnings and Reports

### warnings.txt

Written by `extract-images` and `extract-anim` to the container output directory.
One UTF-8 text line per warning. Empty file (zero bytes) when there are no
warnings.

Example line:
```
duplicate index name: '2UJ' (keeping first occurrence)
```

**Stability: Stable.** Human-readable diagnostic output; format is intentionally
unstructured. Not intended as engine input.

### manifest.json (per container)

Written by `extract-images`, `extract-anim`, and `extract-sounds` to the container
output directory.

**Note:** When `extract-all` runs `extract-images` then `extract-anim` on the
same container directory, the animation run's `manifest.json` overwrites the
image run's. The container directory therefore ends with the animation manifest
after an `extract-all` run.

**Image manifest:**
```json
{
  "failed_images": 0,
  "source_container": "/abs/path/Container.ff",
  "total_images": 1229,
  "warnings_count": 0,
  "written_images": 1229
}
```

**Animation manifest:**
```json
{
  "failed_animations": 0,
  "source_container": "/abs/path/Container.ff",
  "total_animations": 63,
  "warnings_count": 0,
  "written_animations": 63
}
```

**Sound manifest:**
```json
{
  "failed_sounds": 0,
  "source_container": "/abs/path/Container.wdb",
  "total_sounds": 1309,
  "written_sounds": 1309
}
```

**Stability: Provisional.** Counter fields are stable; no version field yet.
The overwrite behavior for image vs. animation manifests is a known issue to
address in Stage 1.

### extraction_manifest.json (top-level, from extract-all)

Written to the `--out` root directory by `extract-all` after all containers
are processed.

```json
{
  "game_root": "/abs/path/to/game",
  "out_dir": "/abs/path/to/out",
  "total_containers": 34,
  "extracted": 30,
  "skipped": [{ "path": "Globals/Gunits.dbf", "reason": "unknown content type" }],
  "failed": [{ "path": "Sounds/Capital.wdb", "error": "sound extractor returned non-zero" }],
  "warnings": []
}
```

**Stability: Provisional.** Summary format is functional. `skipped` and `failed`
arrays provide diagnostic detail but no action codes.

### game_manifest.json (from scan)

Written by `scan` to the `--out` root directory. Produced independently of
extraction; describes what the scanner found, not what was extracted.

```json
{
  "game_root": "/abs/path/to/game",
  "mqdb_containers": [
    {
      "likely_content": ["images", "animations"],
      "path": "Imgs/Battle.ff",
      "record_count": 9,
      "sha256": "88bc78b2...",
      "size": 230011,
      "special_files": ["-ANIMS.OPT", "-IMAGES.OPT", "-INDEX.OPT"]
    }
  ],
  "other_files": [...],
  "scan_timestamp": "...",
  "total_files": 200,
  "warnings": []
}
```

**Stability: Provisional.** `likely_content` classification is heuristic-based
and may be refined. No version field. Feeds Stage 1 (`runtime-asset-manifest-v1`)
as the discovery source.

---

## Relationships

### Container → Image

`source_container` (image sidecar JSON) links each decoded image back to its
source `.ff` file by absolute path.

**Link type:** exact field match  
**Resolution:** `image_sidecar["source_container"] == abs(container_path)`

### Container → Animation

`source_container` (anim.json) links each animation to its source `.ff` file.

**Link type:** exact field match  
**Resolution:** `anim_json["source_container"] == abs(container_path)`

### Animation Frame → Image

`frames[i].logical_name` (anim.json) matches the `logical_name` of the
corresponding image sidecar JSON in the same container output directory.

**Link type:** naming convention  
**Resolution:** `anim_json["frames"][i]["logical_name"] + ".json"` in the parent
container directory

### Image → Atlas Entry

`entries[i].name` (atlas.json) matches the `logical_name` of the image sidecar
JSON in the same container output directory.

**Link type:** naming convention  
**Resolution:** `atlas_json["entries"][i]["name"] + ".json"` in the same container
output directory

### Image → Dialog Element

`elements[i].images` (DLG JSON) contains image logical names that reference
images across the container set. No container is specified; name must be searched
across all extracted image sidecars.

**Link type:** bare logical name (no container qualifier)  
**Resolution:** global scan of all `logical_name` fields across extracted
containers. Currently unresolved — no index or lookup table exists yet.
