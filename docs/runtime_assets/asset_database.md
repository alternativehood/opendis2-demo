# AssetDatabase

`libd2asset` is the read-only runtime boundary for an extracted Disciples II
asset package. It opens the package's `game_manifest.json`, validates the
manifest and referenced files, and provides typed lookup without scanning the
output directory tree.

## Opening a package

```cpp
#include "d2asset/asset_database.hpp"

const d2asset::AssetDatabase db =
    d2asset::AssetDatabase::open("/path/to/extracted-assets");
```

Opening validates:

- `asset_schema_version` is `1`;
- required top-level, container, and asset fields have the expected types;
- container and asset IDs are canonical and unique;
- every asset references a known container;
- every asset path is relative, remains inside the package, and identifies an
  existing regular file.
- every manifest-declared atlas sidecar, sheet reference, entry rectangle, and
  same-container image mapping is valid.
- every manifest-declared animation sidecar has a matching identity, valid
  frame count, contiguous frame indexes, and positive frame dimensions.

Package-level failures throw `d2asset::AssetError`. Its `code()` distinguishes
missing manifests, invalid JSON, unsupported schemas, malformed entries,
duplicate IDs, unsafe paths, missing files, malformed atlases, duplicate atlas
entries, invalid atlas rectangles, missing atlas sheets, and malformed
animations. `context()` and `path()` provide the relevant identifier, field, or
filesystem path when available.

## Lookup

Exact canonical ID lookup:

```cpp
const auto image = db.find_image_by_id("imgs/batitems.ff/a");
```

Case-insensitive logical-name lookup:

```cpp
const auto animation = db.find_animation("walk");
```

Each lookup returns `AssetLookupResult<T>` with one of:

- `Found`: `value` contains a typed value reference.
- `NotFound`: no asset of the requested type matches.
- `Ambiguous`: multiple assets of that type share the normalized logical name;
  `matching_asset_ids` lists every candidate.

Typed value references copy `asset_id`, original-case `logical_name`,
`container_id`, and package-relative `path`. They do not borrow storage from the
database.

Atlas-region lookup uses an exact canonical image ID:

```cpp
const auto region =
    db.find_atlas_region_by_image_id("imgs/batitems.ff/some_image");
```

`Found` returns the atlas asset ID, package-relative `atlas_NNN.png` path,
zero-based sheet index, pixel rectangle, and optional source dimensions.
`Ambiguous` lists every atlas asset that packs the image. No match returns
`NotFound`.

Complete animation clips use an exact canonical animation ID:

```cpp
const auto clip =
    db.get_animation_clip("imgs/batunits.ff/g000uu0001idlea1a00");
```

The clip preserves source frame order and exposes optional canonical image IDs,
atlas regions, and fallback `frame_NNN.png` paths. Recoverable frame resolution
problems remain in the clip as warnings and are also available through
`animation_diagnostics()`.

## Ownership and boundaries

`AssetDatabase` owns its parsed manifest and indexes and exposes only const
access. It does not mutate package files.

`libd2asset` links only to `nlohmann_json` and the C++ standard library. It does
not depend on the CLI, `libd2res`, MQDB/OPT parsers, image or audio codecs, or a
renderer. Loading sidecar content and resolving animation, atlas, and sound
metadata belong to the corresponding runtime-asset stages. Atlas and animation
loading read JSON metadata and validate referenced files but do not decode PNG
bytes.

## Asset references

When root `asset_links.json` is present, opening validates every source,
target, link kind, resolution, confidence, reason, evidence item, and unresolved
candidate. An absent file produces an empty backward-compatible graph.

Read-only queries are available through `links_from_asset`,
`links_from_data_row`, `links_to_asset`, `unresolved_from_asset`, and
`unresolved_from_data_row`. Valid sources with no links return an empty vector;
missing sources or targets return `NotFound`. Successful-link queries may
filter by `AssetLinkKind` and `AssetLinkResolution` without changing canonical
order.
