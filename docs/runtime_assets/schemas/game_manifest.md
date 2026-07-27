# game_manifest.json Schema Reference

Written by `extract-all` to the `--out` root after all containers are processed.
This is the runtime asset discovery document — a single file sufficient to
enumerate all extracted assets without scanning the output directory tree.

Distinct from `extraction_manifest.json`, which records extraction diagnostics
(counts, errors, skipped list) and is not intended as engine input.

---

## Top-level fields

| Field | Type | Description |
|---|---|---|
| `asset_schema_version` | integer | Always `1` for this schema. Increment on breaking changes. |
| `game_root` | string | Absolute path to the game root used for extraction. |
| `containers` | array | One entry per processed MQDB container. See [Container entry](#container-entry). |
| `assets` | array | One entry per successfully extracted asset. See [Asset entry](#asset-entry). |
| `warnings` | array of strings | Extraction warnings: failed containers, skipped containers, and other non-fatal issues. |

### Example

```json
{
  "asset_schema_version": 1,
  "game_root": "/abs/path/to/game",
  "containers": [...],
  "assets": [...],
  "warnings": ["failed to extract Sounds/Capital.wdb: UTF-8 error in sound name"]
}
```

---

## Container entry

Each element of `containers` describes one MQDB container that was encountered
during the scan.

| Field | Type | Description |
|---|---|---|
| `container_id` | string | Stable ID: container path relative to game root, lowercased, forward slashes. E.g. `"imgs/battle.ff"`. |
| `path` | string | Original relative path from the game root. Preserves original casing. E.g. `"Imgs/Battle.ff"`. |
| `content_kinds` | array of strings | Asset type hints from the scanner: `"images"`, `"animations"`, `"sounds"`, etc. |

### Example

```json
{
  "container_id": "imgs/battle.ff",
  "path": "Imgs/Battle.ff",
  "content_kinds": ["images", "animations"]
}
```

---

## Asset entry

Each element of `assets` describes one successfully extracted asset.
The `assets` array is a flat list typed by the `type` field. Unknown types are
forward-compatible — consumers that don't recognize a type should skip the entry.

| Field | Type | Description |
|---|---|---|
| `asset_id` | string | Stable runtime ID. See [Asset ID construction](#asset-id-construction). |
| `logical_name` | string | Original name from the container index, original casing preserved. |
| `type` | string | Asset type: `"image"`, `"animation"`, `"sound"`, `"atlas"`, or `"data_table"`. |
| `container_id` | string | ID of the container this asset came from. Matches a `container_id` in `containers`. |
| `path` | string | Relative path (from the `--out` root) to this asset's sidecar JSON or `anim.json`. Uses forward slashes on all platforms. |

### Examples

```json
{ "asset_id": "imgs/battle.ff/selsmalla",   "logical_name": "SELSMALLA",   "type": "animation", "container_id": "imgs/battle.ff",   "path": "Imgs/Battle.ff/SELSMALLA/anim.json" }
{ "asset_id": "imgs/battle.ff/_0",          "logical_name": "_0",          "type": "image",     "container_id": "imgs/battle.ff",   "path": "Imgs/Battle.ff/_0.json" }
{ "asset_id": "sounds/battle.wdb/0001_hit_b", "logical_name": "0001_HIT_B", "type": "sound",    "container_id": "sounds/battle.wdb", "path": "Sounds/Battle.wdb/0001_HIT_B.json" }
```

---

## Asset ID construction

`asset_id` is constructed deterministically from two components:

```
asset_id = container_id + "/" + to_lower(logical_name)
```

Where `container_id` is the container's game-root-relative path with all characters
lowercased and backslashes replaced by forward slashes.

### Rules

1. All ASCII letters are lowercased.
2. Backslashes (`\`) become forward slashes (`/`).
3. All other characters are preserved as-is.
4. `asset_id` is unique within a single `game_manifest.json`.

### Example derivation

| Input | Value |
|---|---|
| Container relative path | `Imgs/Battle.ff` |
| `container_id` | `imgs/battle.ff` |
| `logical_name` | `SELSMALLA` |
| `asset_id` | `imgs/battle.ff/selsmalla` |

---

## Versioning policy

`asset_schema_version` is incremented when a breaking change is made to the
schema (field removed, field renamed, type changed). Adding new optional fields
does not require a version bump — consumers must tolerate unknown fields.

Current version: **1**

---

## Canonical runtime package paths

`build-runtime-assets` retains schema version 1 while placing assets in
type-owned subtrees:

- images: `images/<container-path>/<logical-name>.json`
- animations: `animations/<container-path>/<animation>/anim.json`
- atlases: `atlases/<container-path>/atlas.json`
- sounds: `sounds/<container-path>/<logical-name>.json`
- data tables: `data/...`

Every generated atlas is registered explicitly as an asset with
`"type": "atlas"` and reserved logical name `__runtime_atlas`. Runtime readers
do not discover atlases by scanning directories.

The top-level `game_root` remains extraction provenance. Runtime loading and
validation resolve only paths relative to the package root, so moving the
package does not require access to the original installation.

---

## Runtime reader validation

`libd2asset` treats the following as required schema-v1 fields:

- top level: integer `asset_schema_version`, arrays `containers`, `assets`, and
  `warnings`;
- container: string `container_id`, string `path`, array `content_kinds`;
- asset: string `asset_id`, `logical_name`, `type`, `container_id`, and `path`.

The reader rejects duplicate canonical IDs, unknown container references,
absolute or package-escaping paths, and paths that do not identify an existing
regular file.

Schema-v1 readers recognize `image`, `animation`, `sound`, `atlas`, and
`data_table`. A well-formed entry with another `type` string is retained as an
unknown generic asset for forward compatibility, but it is not returned by
typed lookup methods. Atlas assets must be listed explicitly; runtime readers do
not scan directories for `atlas.json`.
