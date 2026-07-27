# Runtime Asset Engine Contract

Stage 9 provides a headless consumer that demonstrates the supported boundary
between a future engine and a canonical runtime asset package.

## Invocation

```sh
opendis2-dev-asset-inspect <asset_root> \        # planned — not yet implemented
  --animation <canonical-animation-asset-id> \
  [--sound <canonical-sound-asset-id>] \
  [--data-table <canonical-data-table-asset-id> [--row <row-key>]] \
  [--reference-table <table-id> --reference-row <row-key> | \
   --reference-target <asset-id>]
```

The command writes one versioned JSON document to standard output and returns:

| Exit | Meaning |
|---:|---|
| `0` | Inspection succeeded |
| `2` | Arguments are missing or invalid |
| `3` | The package could not be opened |
| `4` | The animation ID is absent or has another asset type |
| `5` | The optional sound ID is absent or has another asset type |
| `6` | The optional data-table ID is absent or has another asset type |
| `7` | The selected row key is absent |
| `8` | The reference source table or row is absent |
| `9` | The reference target asset is absent |

## Supported Flow

```text
game_manifest.json
  -> AssetDatabase
  -> exact animation asset ID
  -> AnimationClip
  -> ordered AnimationFrameRef values
  -> TextureRegion or fallback PNG path
  -> optional SoundAsset
  -> optional DataTable and selected DataRow
```

Reference selectors add a `references` section with ordered outgoing, incoming,
and unresolved records. Supplying neither selector preserves the existing
inspection document. A source requires both reference flags and cannot be
combined with a target selector.

## Dependency Boundary

`opendis2-dev-asset-inspect` (planned) links `libd2asset`. It does not link `libd2res`, CLI extraction
commands, MQDB/OPT parsers, a renderer, or an audio playback library. It reads
only package-local files and remains usable after the original game installation
is removed or the package is relocated.

The optional sound section reports package-local sidecar and payload paths,
payload size, detected format, available technical metadata, and warnings. It
does not decode or play audio and adds no game-rule relationships.

The optional data-table section reports canonical identity, kind, ordered
columns, row count, warnings, and at most one explicitly selected row. Nested
values retain their type and object-member order. Inspection does not emit all
rows by default.

## Verification Policy

Normal tests use synthetic packages and a scoped fixture from the game for
animation frame validation. Full-game extraction remains deferred until
the complete Runtime Asset Layer plan is implemented.

Stage 5 also uses a scoped sound fixture that extracts and packages
one record. Runtime loading occurs after its extraction work directory is
removed.

Stage 6 adds scoped `Globals/Gunits.dbf`, `gameinfo.dat`, and
`Interf/Interf.dlg` coverage. Each input is parsed and packaged independently;
full-game extraction remains deferred.
