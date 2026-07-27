# Runtime Animation Model

`libd2asset` eagerly loads every manifest-declared animation asset into an
immutable `AnimationClip` while opening the package. This turns provisional
`anim.json` output into an ordered runtime timeline without adding playback or
renderer behavior.

## Lookup

```cpp
const d2asset::AssetDatabase db =
    d2asset::AssetDatabase::open("/path/to/package");

const auto result =
    db.get_animation_clip("imgs/batunits.ff/g000uu0001idlea1a00");
```

`Found` contains the complete clip. Missing IDs and IDs belonging to another
asset type return `NotFound`. The existing `find_animation_by_id` API remains
available when only the manifest reference is needed.

## Clip And Frame Values

An `AnimationClip` preserves:

- canonical animation asset ID, original logical name, and container ID;
- package-relative `anim.json` path;
- source frame order and count;
- loop mode and facing direction, currently `Unknown`;
- best-effort role classification with matched token and reason;
- recoverable frame and timing warnings.

Each `AnimationFrameRef` contains its source index, logical image name,
dimensions, timing, optional canonical image ID, optional `TextureRegion`, and
optional package-relative fallback PNG path.

Frame image names are matched with ASCII case-insensitive comparison only
against images in the animation's container. A uniquely resolved image uses the
standard atlas index. Multiple atlas matches are diagnostic ambiguity; the
runtime never chooses the first atlas silently.

## Visual Fallbacks

For source frame index `N`, the runtime checks for `frame_NNN.png` beside
`anim.json`. It records the path only when it is an existing regular file and
does not decode the PNG.

A frame is render-resolvable when it has either a unique atlas region or a
fallback PNG. Missing image mappings, ambiguous atlas mappings, and frames with
neither source remain in their original timeline position and produce
contextual warnings. `animation_diagnostics()` exposes the aggregate warnings
while each clip retains its own copy.

## Timing And Classification

The current extractor writes `frame_delay_ms` as a uniform hardcoded value.
Positive values are therefore marked `ProvisionalSidecar`, not authoritative
game timing. Missing, malformed, zero, or negative values use a documented
100 ms `FallbackDefault` and produce a warning.

Role classification uses a small documented token set for idle, move, attack,
hit, death, cast, and defend. Every inferred role stores its matched token and
reason. Names without a match remain `Unknown`. Loop and facing semantics are
not inferred.

## Boundary

The runtime model uses JSON metadata, paths, and existing atlas values only.
It does not parse MQDB or OPT files, decode images, upload textures, advance
playback time, interpolate frames, or depend on CLI, `libd2res`, audio, or
renderer libraries.
