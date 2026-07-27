# Inspection Report Schema v1

`opendis2-dev-asset-inspect` (planned — not yet implemented) emits exactly one JSON document followed by one newline.
`inspection_schema_version` is currently `1`.

## Success Document

Top-level fields:

- `inspection_schema_version`: integer schema version.
- `status`: `"ok"`.
- `request`: exact requested animation ID and nullable sound, data-table, and row IDs.
- `animation`: loaded clip and ordered frame data.
- `sound`: present only when `--sound` was supplied and resolved.
- `data_table`: present only when `--data-table` was supplied and resolved.

`animation` contains:

- `asset_id`, `logical_name`, `container_id`, and package-relative
  `sidecar_path`.
- `frame_count`, `frames`, and sorted `warnings`.
- `classification` with lowercase role, matched token, and reason.
- lowercase `loop_mode` and `facing_direction`.

Each frame contains:

- `index`, `logical_name`, and `source_size`.
- `timing.duration_ms` and lowercase timing `source`.
- nullable `image_asset_id`, `texture_region`, and `fallback_path`.
- `resolved`, which is true when an atlas region or fallback PNG exists.

A texture region contains its atlas/image identity, package-relative sheet path,
sheet index, integer pixel rectangle, and nullable source/trim/pivot/anchor
metadata.

When present, `sound` contains:

- `asset_id`, `logical_name`, and `container_id`;
- package-relative `sidecar_path` and `payload_path`;
- positive `payload_size`;
- lowercase `format` and nullable `format_tag`;
- nullable `channels`, `sample_rate`, `bit_depth`, and `duration_ms`;
- deterministically ordered warning strings.

Paths use forward slashes and are relative to the package root. Arrays are
deterministically ordered; absent optional values are JSON `null`, except the
top-level `sound` field, which is omitted when not requested.

When present, `data_table` contains identity, lowercase kind, package-relative
sidecar path, ordered columns, row count, sorted warning strings, and nullable
`selected_row`. A selected row contains ordered cells. Each value is encoded as
`{"kind": ..., "value": ...}`; object values use an ordered array of named
members rather than a JSON object.

## Error Document

Error documents contain:

- `inspection_schema_version`: `1`.
- `status`: `"error"`.
- `error.code`: stable inspection error category.
- `error.message`: deterministic message.
- nullable `detail_code`, `context`, `path`, and `requested_asset_id`.

Stable inspection categories are `invalid_arguments`, `package_open_failed`,
`animation_not_found`, `sound_not_found`, `data_table_not_found`, and
`data_row_not_found`. Package failures preserve the lowercase `AssetErrorCode`
as `detail_code`.

## Compatibility

Version 1 may gain additive fields. Existing field meanings, enum strings, exit
categories, ordering rules, and relative-path behavior remain stable.
# Reference Inspection

When a reference selector is supplied, the report adds `references` with
ordered `outgoing`, `incoming`, and `unresolved` arrays. Each successful link
contains source, target, kind, resolution, confidence, reason code, and ordered
evidence. Unresolved records contain reason, evidence, and candidate IDs.

Without a reference selector the schema-v1 document remains unchanged.
