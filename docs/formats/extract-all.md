# extract-all

## Scope

`opendis2-dev-extractor extract-all <game_dir> --out <dir>` performs one-pass extraction of all supported game assets:

- **Images** — `.ff` containers with `ContentKind::Images` → PNG files
- **Animations** — `.ff` containers with `ContentKind::Animations` → PNG frame sequences + `anim.json`
- **Sounds** — `.wdb` containers with `ContentKind::Sounds` → WAV files + metadata JSON
- **Data Tables** — `.dbf`, `.dat`, and `.dlg` files (both MQDB containers and plain files) → canonical JSON sidecars

## Output Layout

The output directory mirrors the relative path structure of the game root. For each processed container or file:

```
<out_dir>/
  Imgs/
    BatUnits.ff/
      <image_pngs_and_jsons>
    Capital.ff/
      ...
  Sounds/
    Battle.wdb/
      <wav_files_and_jsons>
  Tglobal.dbf/
    Tglobal.schema.json
    Tglobal.records.json
  Global.dlg/
    Global.json
  Config.dat/
    Config.json
  extraction_manifest.json
  game_manifest.json
```

## Manifest Schemas

### `extraction_manifest.json`

Diagnostic manifest tracking extraction results:

```json
{
  "game_root": "...",
  "out_dir": "...",
  "total_containers": 42,
  "extracted": 40,
  "skipped": [
    {"path": "...", "reason": "unknown content type"}
  ],
  "failed": [
    {"path": "...", "error": "..."}
  ],
  "warnings": ["..."]
}
```

### `game_manifest.json`

Runtime asset discovery manifest:

```json
{
  "asset_schema_version": 1,
  "game_root": "...",
  "containers": [
    {
      "container_id": "imgs/batunits.ff",
      "path": "Imgs/BatUnits.ff",
      "content_kinds": ["images", "animations"]
    }
  ],
  "assets": [
    {
      "asset_id": "imgs/batunits.ff/xe",
      "logical_name": "Xe",
      "type": "image",
      "container_id": "imgs/batunits.ff",
      "path": "Imgs/BatUnits.ff/Xe.json"
    },
    {
      "asset_id": "tglobal.dbf/tglobal",
      "logical_name": "Tglobal",
      "type": "data_table",
      "container_id": "tglobal.dbf",
      "path": "Tglobal.dbf/Tglobal.records.json"
    }
  ],
  "warnings": []
}
```

Asset types:
- `image` — extracted image with PNG payload
- `animation` — extracted animation clip
- `sound` — extracted sound with WAV payload
- `data_table` — extracted DBF/DAT/DLG data table

## Data Table Asset Type

Data tables are classified by `GameScanner` as `ContentKind::DataTables` and dispatched based on file extension:

- `.dbf` → `cmd_extract_dbf` → produces `*.schema.json` + `*.records.json`
- `.dat` → `cmd_extract_dat` → produces `*.json`
- `.dlg` → `cmd_extract_dlg` → produces `*.json`

The manifest records each data table as a single `data_table` asset. For DBF, the primary asset references `records.json` (schema.json is extracted but not listed separately in the manifest to avoid duplicate asset IDs).

### Data Table Content Examples (Synthetic)

**DBF schema example:**
```json
{"record_count": 5, "fields": [
  {"name": "ID", "type": "C", "length": 10},
  {"name": "NAME", "type": "C", "length": 10},
  {"name": "VALUE", "type": "N", "length": 4},
  ...
]}

// records
{"ID": "id_test_unit_a", "NAME": "TEST_UNIT_A", "VALUE": 100}
{"ID": "id_test_unit_b", "NAME": "TEST_UNIT_B", "VALUE": 200}
```

**DLG example:**
```json
{
  "id": "DLG_EXAMPLE",
  "bounds": [0, 0, 400, 300],
  "elements": [
    {"id": "BTN_OK", "type": "BUTTON", "bounds": [150, 260, 250, 290],
     "images": ["BTN_OK_N", "BTN_OK_H", "BTN_OK_C", "BTN_OK_N"],
     "text_key": "X100TA0001"}
  ]
}
```

**DAT example:**
```json
{"ExampleKey": "ExampleValue", "Version": "1.00"}
```

## Per-Container Manifests

Each extractor command writes a per-container manifest to track its own extraction results. To avoid filename collisions when a container has both `Images` and `Animations` content, the manifest filenames are type-specific:

| Extractor | Manifest Filename |
|-----------|-------------------|
| `extract-images` | `images_manifest.json` |
| `extract-anim` | `anim_manifest.json` |
| `extract-sounds` | `manifest.json` |
| `extract-dbf` / `extract-dat` / `extract-dlg` | no per-container manifest |

### `images_manifest.json` schema
```json
{
  "source_container": "...",
  "total_images": 65009,
  "written_images": 65009,
  "failed_images": 0,
  "warnings_count": 0
}
```

### `anim_manifest.json` schema
```json
{
  "source_container": "...",
  "total_animations": 4912,
  "written_animations": 4912,
  "failed_animations": 0,
  "warnings_count": 0
}
```

## Unknown Content

Containers with only `ContentKind::Unknown` are skipped and listed in `extraction_manifest.json` under `skipped` with reason `"unknown content type"`. Non-MQDB files that are not data tables (e.g., `.bik`, `.wav`) are also skipped.
