# anim.json Runtime Contract

Stage 4 runtime readers consume an `anim.json` sidecar explicitly listed as an
`animation` asset in `game_manifest.json`.

| Field | Type | Constraint |
|---|---|---|
| `name` | string | Non-empty; ASCII case-insensitively equal to the manifest `logical_name`. |
| `frame_count` | integer | Non-negative and equal to `frames.length`. |
| `frame_delay_ms` | integer | Optional policy input. Positive values are provisional uniform timing; other values use the 100 ms fallback. |
| `source_container` | string | Producer information; not used for runtime identity. |
| `frames` | array | Ordered frame descriptors. |

Each frame requires:

| Field | Type | Constraint |
|---|---|---|
| `index` | integer | Equal to its zero-based array position. |
| `logical_name` | string | Non-empty source image name. |
| `width`, `height` | integer | Positive source dimensions in pixels. |

Frame `N` may have a regular fallback file named `frame_NNN.png` beside the
sidecar. The decimal index is zero-padded to at least three digits, for example
`frame_000.png`.

The sidecar does not currently provide authoritative per-frame timing, loop
mode, facing direction, role, pivot, or anchor metadata. Runtime readers must
not infer those fields except for the documented best-effort role
classification.
