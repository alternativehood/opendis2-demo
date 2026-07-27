# Runtime Atlas Access

`libd2asset` loads every atlas asset explicitly listed in
`game_manifest.json`. It validates the atlas sidecar and sheet files while
opening the package, then indexes entries by canonical image ID.

## Lookup

```cpp
const d2asset::AssetDatabase db =
    d2asset::AssetDatabase::open("/path/to/package");

const auto result =
    db.find_atlas_region_by_image_id("imgs/batitems.ff/some_image");
```

The result follows the common lookup contract:

- `Found`: one atlas contains the image and `value` contains a `TextureRegion`.
- `NotFound`: no loaded atlas contains the image.
- `Ambiguous`: multiple atlases contain it and `matching_asset_ids` identifies
  the atlas assets.

`TextureRegion` contains the atlas and image IDs, original logical name,
package-relative sheet path, zero-based sheet index, and an integer pixel
rectangle. The library does not normalize UVs, decode PNG data, upload
textures, or select sampling behavior.

Source dimensions are copied from a mapped image sidecar's positive
`output_size` when present. Trim size, trim offset, pivot, and anchor are
optional and remain unset because current extraction output does not define
those semantics.

## Package Assembly

`extract-atlas` writes `atlas.json` and `atlas_NNN.png` files. A package builder
must add an atlas asset to `game_manifest.json` with:

- the same `container_id` as the image assets packed by the atlas;
- a unique canonical `asset_id`;
- `type` set to `atlas`;
- `path` pointing to the package-relative `atlas.json`.

Atlas entry names are matched case-insensitively only against image assets in
that container. Runtime loading never scans directories to discover unlisted
atlases.

Current generated sheets are square and use `max_sheet_size` for both
dimensions. Rectangle validation relies on that producer invariant rather than
reading PNG headers.
