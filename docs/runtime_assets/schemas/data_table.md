# Runtime Data Table Sidecar Schema v1

Each `data_table` asset in `game_manifest.json` points to one canonical JSON
sidecar under `data/`.

## Root Fields

| Field | Type | Meaning |
|---|---|---|
| `data_table_schema_version` | integer | Required value `1`. |
| `asset_id` | string | Exact canonical manifest asset ID. |
| `logical_name` | string | Original table stem. |
| `container_id` | string | Exact canonical source-file container ID. |
| `kind` | string | `dbf`, `dat`, or `dlg`. |
| `columns` | array | Ordered column descriptors. |
| `rows` | array | Ordered canonical rows. |
| `warnings` | array of strings | Recoverable canonicalization diagnostics. |
| `extensions` | value or null | Lossless non-normative source metadata. |

Column descriptors contain `name` and nullable `source_type`, `width`,
`decimal_count`, and `extensions`. Column names are exact and unique.

Rows contain a non-empty unique `row_key` and an ordered `values` array. Each
cell has `name` and `value`. Values support JSON null, boolean, signed 64-bit
integer, finite floating point, string, array, and ordered object.

## Row Keys

- DBF: zero-padded source record index, for example `00000000`.
- DAT: original key; rows are bytewise sorted by key.
- DLG: unique non-empty dialog `id`; otherwise a zero-padded source index and a
  warning.

Row keys are package identities, not inferred gameplay primary keys.

## Compatibility

Readers reject unsupported schema versions and malformed structure. Unknown
columns, nested DLG members, and extension values are preserved. New binary
format interpretations require extractor changes and a documented schema
evolution; runtime loading never parses DBF, DAT, or DLG source files.
