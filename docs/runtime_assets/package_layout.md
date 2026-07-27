# Runtime Asset Package Layout

`opendis2-dev-extractor build-runtime-assets <game_dir> --out <asset_root>` produces the
portable package consumed by `d2asset::AssetDatabase`.

## Canonical Tree

```text
<asset_root>/
  game_manifest.json
  asset_links.json
  images/<container-path>/*.json
  images/<container-path>/*.png
  animations/<container-path>/<animation>/anim.json
  animations/<container-path>/<animation>/frame_NNN.png
  atlases/<container-path>/atlas.json
  atlases/<container-path>/atlas_NNN.png
  sounds/<container-path>/*.json
  sounds/<container-path>/*.wav
  sounds/<container-path>/*.bin
  data/<source-file-path>/<table>.json
  reports/build_report.json
  reports/validation_report.json
```

All six top-level directories are present even when a package contains no asset
of that type. Paths recorded in `game_manifest.json` are relative to
`asset_root`, use forward slashes, and must remain beneath the package root
after lexical normalization.

## Ownership

- `images/` owns decoded image metadata and optional standalone PNG payloads.
- `animations/` owns animation sidecars and frame PNG fallbacks.
- `atlases/` owns atlas sidecars and texture sheets.
- `sounds/` owns versioned runtime sound sidecars and pass-through `.wav` or
  unknown-format `.bin` payloads.
- `data/` owns versioned canonical DBF, DAT, and DLG runtime table sidecars.
- `reports/` owns versioned build and validation diagnostics.

Container-relative paths preserve source provenance while canonical asset IDs
remain independent of package placement.

## Portability

The `game_root` field is provenance only. Opening and validating a completed
package never reads the original game installation. A package may be moved as
one directory without rewriting its manifests.

## Publication

The builder refuses an existing destination. It writes to a unique sibling
staging directory, validates the staged package, and publishes it by
same-filesystem rename only after validation succeeds. Failed builds remove
their staging directory and never expose a partial destination.

## Validation

`opendis2-dev-extractor validate-runtime-assets <asset_root> [--report <path>]` checks:

- required package entries;
- manifest JSON and safe relative asset paths;
- referenced files;
- schema-v1 manifest rules;
- atlas sidecars, sheets, mappings, and rectangles;
- animation sidecars and frame resolution;
- sound sidecars, safe payload paths, payload presence, and byte sizes;
- data-table sidecars, identities, kinds, columns, row keys, and recursive values;
- successful `AssetDatabase::open`.

Recoverable extraction, atlas packing, and animation resolution issues are
warnings. Missing required files, unsafe paths, malformed sidecars, duplicate
IDs, missing sound payloads, payload-size mismatches, and runtime database
failures are errors. Unknown sound formats and unrecognized WAVE format tags
are warnings.

The builder uses scanner-owned `other_files` discovery for DBF, DAT, and DLG
inputs. Existing standalone extractors write temporary JSON under `.work`;
package assembly converts it to the canonical schema before validation and
removes `.work` before publication.

After canonical assets and tables are staged, `AssetReferenceResolver` opens
that staged package through `AssetDatabase`, writes `asset_links.json`, and the
builder reopens it before publication. Resolver generation does not scan the
game root or parse raw containers. Validation summaries include successful and
unresolved asset-link counts.
