# Runtime Sound Sidecar Schema v1

Each canonical sound manifest entry points to a JSON sidecar with
`sound_schema_version: 1`. The sidecar is package-local and can be loaded
without the source WDB container.

Required fields:

- `sound_schema_version`: integer `1`.
- `asset_id`, `logical_name`, `container_id`: identity matching
  `game_manifest.json`.
- `payload_path`: forward-slash path relative to the package root.
- `payload_size`: positive payload byte count.
- `detected_format`: `"wave"` or `"unknown"`.
- `warnings`: array of strings.

Optional technical fields are `format_tag`, `channels`, `sample_rate`,
`bit_depth`, and `duration_ms`. They are positive integers when known and JSON
`null` when unavailable. Readers must not infer absent values.

`AssetDatabase::open` validates identity, path safety, payload presence, and
file size. Unknown formats and unrecognized WAVE format tags remain loadable
with warnings because `libd2asset` does not claim playback support.

New optional fields may be added within version 1. Changes to required fields or
their meanings require a new `sound_schema_version`.
