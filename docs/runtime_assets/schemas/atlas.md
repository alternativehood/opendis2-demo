# atlas.json Runtime Contract

`extract-atlas` writes this sidecar beside one or more PNG sheets. Stage 3
runtime readers consume the following fields.

| Field | Type | Constraint |
|---|---|---|
| `source_container` | string | Producer information; not used for runtime identity. |
| `max_sheet_size` | integer | Positive shared width and height of generated sheets. |
| `sheet_count` | integer | Non-negative number of sheets. |
| `total_sprites` | integer | Non-negative and equal to `entries.length`. |
| `skipped_sprites` | integer | Non-negative producer diagnostic. |
| `entries` | array | Sprite rectangle records. |

Each entry requires:

| Field | Type | Constraint |
|---|---|---|
| `name` | string | Non-empty and unique under ASCII case folding. |
| `sheet` | integer | `0 <= sheet < sheet_count`. |
| `x`, `y` | integer | Non-negative pixel coordinates. |
| `w`, `h` | integer | Positive pixel dimensions. |

`x + w` and `y + h` must not exceed `max_sheet_size`.

Sheet `N` must be a regular file named `atlas_NNN.png` in the same directory as
the sidecar. The index is decimal and zero-padded to at least three digits, for
example `atlas_000.png`.

The current contract does not provide per-sheet dimensions, trim offsets,
pivots, or anchors.
