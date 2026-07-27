# Runtime Data Tables Format Boundary

The `runtime-data-tables` stage establishes no new Section 13 conclusions about
DBF, DAT, or DLG source layouts.

It reuses the existing extraction boundary:

- `DbfReader` produces field descriptors and string-valued records.
- `DatParser` produces key-to-string entries.
- `DlgParser` produces provisional dialog and nested element JSON.

Canonical runtime sidecars are package formats, not evidence about additional
source bytes or tokens. Unknown DBF columns and DLG members are preserved
without interpretation. Any future binary/text layout finding must be verified
against the format source of truth and documented separately.

## Data Table Coverage in `extract-all`

As of the `extract-all-missing-data-tables` change, `extract-all` now covers
DBF, DAT, and DLG files alongside images, animations, and sounds. These are
classified as `ContentKind::DataTables` by `GameScanner` and dispatched to their
respective extractors during the one-pass extraction process.

### DBF Content Summary

The game contains approximately 50+ DBF files split into three categories:

- **`G*` (Global Game Data):** Core game logic — units (`Gunits.dbf`, 359 records),
  attacks (`Gattacks.dbf`, 354 records), spells (`Gspells.dbf`, 120 records),
  buildings (`Gbuild.dbf`, 122 records), AI parameters (`GAI.dbf`, 7 records),
  tile data (`GTileDBI.dbf`), immunities (`Gimmu.dbf`), and more.
- **`L*` (Localization):** Translations for units, races, terrain, buildings,
  attacks, spells, etc. (`Lrace.dbf`, `Lterrain.dbf`, `LunitB.dbf`, ...).
- **`T*` (Text):** All in-game text strings (`Tglobal.dbf`, 2292 records,
  ~1.3 MB). Other DBF files reference these via `NAME_TXT` / `DESC_TXT` keys.

### DAT Content Summary

Plain `key=value` text files. The primary example is `gameinfo.dat` containing
`FullName`, `Version`, `Localization`, `Platform`, and `OS` fields.

### DLG Content Summary

UI dialog definitions. `Interf.dlg` (128 dialogs) and `ScenEdit.dlg` (116 dialogs)
contain a total of ~244 dialog layouts with ~1029 UI elements (470 TEXT,
446 IMAGE, 356 BUTTON, 157 UNKNOWN). Each element defines its `id`, `bounds`,
`type`, and references to image assets (e.g., `DLG_BATTLE_A_DEFEND_N` for
button states).
