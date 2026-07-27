# Game Data Registry Architecture

## 1. `d2gamedata::DbfGameDataIndex` (canonical DBF index)

- **Path**: `src/d2gamedata/DbfGameDataIndex.{hpp,cpp}`
- **Role**: Canonical DBF/global-ID registry for runtime scenario/game-data mapping.
- **Key features**:
  - Scans `.dbf` files from a directory tree, indexes every G*-like value in every field with full provenance (`DbfValueRef`).
  - Provides `find_global_id(id)` returning all `DbfValueRef` entries for a given G* ID.
  - Maintains a text map from Tglobal.dbf for name/description resolution.
  - Produces `DbfScanReport` with accurate counters.
  - Persistent duplicate DBF file load protection (canonical-path `loaded_dbf_files_` set).
  - No dependency on `libd2scenario`.
- **Alias removed**: The `d2gamedata::GameDataRegistry` alias has been removed. Use `DbfGameDataIndex` directly. The engine facade `d2engine::GameDataRegistry` is a separate class.

## 2. `d2gamedata::GlobalIdResolver` (canonical runtime resolver)

- **Path**: `src/d2gamedata/GlobalIdResolver.{hpp,cpp}`
- **Role**: Canonical runtime resolver for G* values in `.sg` scenario files.
- **Key features**:
  - Wraps `DbfGameDataIndex` internally.
  - `resolve(GlobalId)` returns `GlobalIdResolution` with status, source kind, primary match, text resolution.
  - Uses strong ID types at boundaries (`GlobalId`, `TextId`).
  - `TextResolution` honestly reports `resolved` flag — unresolved text IDs are NOT presented as display strings.
  - Null reference (`G000000000`) returns `NullRef` status with `SourceKind::Null`.
  - Asset fallback is **not configured** (`asset_fallback_configured = false`).
  - Works without any `SgScenario` — pure game-data path.

## 3. `d2analysis::ScenarioGlobalIdReport` (report layer)

- **Path**: `src/d2analysis/ScenarioGlobalIdReport.{hpp,cpp}`
- **Role**: Combines `SgParseResult::global_id_usages` with `GlobalIdResolver` for scenario-aware reporting.
- **Key features**:
  - `summarize()` produces `GlobalIdSummary` with `by_source_kind` (counted once per usage), `by_domain_category`, `by_prefix`, `by_class`, `by_field`.
  - `build_unresolved_report()` enumerates unresolved IDs with usage counts and examples.
  - `build_resolution_map()` produces per-ID resolution entries with honest text representation.
  - `find_usages()` returns `SearchEvidence` for a given ID.

## 4. `d2scenario::SgParseResult` and `ScenarioTemplate`

- **`ScenarioTemplate`**: Runtime semantic data only (info, players, units, stacks, map, etc.). No diagnostic fields.
- **`SgParseResult`**: Contains a `ScenarioTemplate scenario` member + diagnostics (`object_index`, `raw_objects`, classification maps, `global_id_usages`, `file_path`, `file_size`, `parse_warnings`, empty hull vectors).
- **Containment, not inheritance**: Code accesses semantic data via `result.scenario.players`, etc.
- **Verified-empty classes** (`MidSpellCast`, `MidSpellEffects`, `MidStackDestroyed`, `MidQuestLog`):
  - Classified directly during parse as `VerifiedEmptyInitialState`.
  - Appear only in `SgParseResult::verified_empty_objects` and `SgParseResult` hull vectors.
  - NOT present in `ScenarioTemplate` semantic vectors.
- **Alias removed**: `using SgScenario = SgParseResult` has been removed. Use `SgParseResult` for parse output and `ScenarioTemplate` for semantic data.

## 5. CLI (`sg-inspect`)

- Thin frontend: parses `.sg`, creates `GlobalIdResolver`, delegates to `d2analysis` for reporting.
- No DBF scanning/indexing/resolver ownership.
- JSON output uses honest text resolution (`name_text_id`, `name_resolved`, `name_value`, `name_unresolved_reason` instead of misleading `display_name`).

## 6. `d2engine::GameDataRegistry` (typed engine/asset facade)

- **Path**: `src/d2engine/assets/game_data_registry.{hpp,cpp}`
- Independent of `d2gamedata`. Bridge/migration is future work.

## Current Architecture

```
libd2gamedata → libd2res        (NO libd2scenario dependency)
libd2scenario → libd2log
libd2analysis → libd2gamedata + libd2scenario  (report layer)
CLI → libd2analysis + libd2gamedata + libd2scenario
```

## Remaining Debts

- Asset/resource fallback — pending future work.
- Campaign/mod-local DBF/data lookup — not implemented.
- Engine typed facade bridge (`d2engine::GameDataRegistry` → `d2gamedata::DbfGameDataIndex`) — pending.
- `SgParser` decomposition (reference provenance at read time) — pending. Current `collect_global_id_usages()` is a temporary manual field list.
- CLI output dumpers (`SgJsonDumper`, `SgCsvWriter`, `GlobalIdReportJsonWriter`) — extraction pending from monolithic `commands_sg_inspect.cpp`.
