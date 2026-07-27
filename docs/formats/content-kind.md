# ContentKind

`ContentKind` is the typed classification used by `GameScanner` to categorize game containers and files. It determines which extractor is dispatched during `extract-all` and `build-runtime-assets`.

## Enum Values

| Enum Value | JSON String | Source Extensions | Extractor |
|------------|-------------|-------------------|-----------|
| `Images` | `images` | `.ff` (with `-IMAGES.OPT`) | `cmd_extract_images` |
| `Animations` | `animations` | `.ff` (with `-ANIMS.OPT`) | `cmd_extract_anim` |
| `Sounds` | `sounds` | `.wdb` in `Sounds/` | `cmd_extract_sounds` |
| `SoundMapping` | `sound_mapping` | `.wdt` | — |
| `DataTables` | `data_tables` | `.dbf`, `.dat`, `.dlg` | `cmd_extract_dbf` / `cmd_extract_dat` / `cmd_extract_dlg` |
| `Unknown` | `unknown` | everything else | — (skipped) |

## Classification Rules

### MQDB Containers (`.ff`)

1. If special files include `-IMAGES.OPT` → `Images`
2. If special files include `-ANIMS.OPT` → `Animations`
3. If in `Sounds/` directory and extension is `.wdb` → `Sounds`
4. If extension is `.wdt` → `SoundMapping`
5. If extension is `.dbf`, `.dat`, or `.dlg` → `DataTables`
6. Otherwise → `Unknown`

### Plain Files (non-MQDB)

Plain files are recorded in `ScanResult::other_files`. Their `type` field is set by extension, and `likely_content` is set for data tables:

- `.dbf` → `type: "dbf"`, `likely_content: ["data_tables"]`
- `.dat` → `type: "dat"`, `likely_content: ["data_tables"]`
- `.dlg` → `type: "dlg"`, `likely_content: ["data_tables"]`
- `.bik` → `type: "bik"`, `likely_content: ["unknown"]`
- `.wav` → `type: "wav"`, `likely_content: ["unknown"]`
- `.sg` / `.csg` → `type: "sg"` / `type: "csg"`, `likely_content: ["unknown"]`
- `.mft` → `type: "mft"`, `likely_content: ["unknown"]`
- everything else → `type: "unknown"`, `likely_content: ["unknown"]`

## Notes

- `SoundMapping` (`.wdt`) is classified but not currently extracted by `extract-all`.
- `DataTables` is a single `ContentKind` that covers all three data formats (DBF, DAT, DLG) because they share the same runtime consumer (`DataTable`) and the same manifest type (`data_table`).
- The dispatch loop in `extract-all` inspects the file extension to route `DataTables` to the correct parser.
