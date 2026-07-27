# Open Questions — Runtime Asset Layer

Reference graph ownership, deterministic evidence, and
confirmed-versus-heuristic policy are resolved by schema version 1. Unit-ID to
BatUnits matching currently uses a normalized prefix, preserving `UU` and
accepting legacy `UN` as `UU`. Other naming families still need scoped evidence. Symbolic sound
triggers remain unresolved until a reliable WDT-to-canonical-sound mapping
exists. Schema version 1 targets assets, not data rows or other runtime objects.

Questions that must be resolved before specific later stages can be safely
implemented. Each entry names the blocking stage and states whether the question
is currently answerable from existing extracted data.

## Runtime Data Semantics

**Blocks:** `runtime-asset-reference-resolver` (Stage 7)  
**Currently answerable from data:** Partially

Stage 6 preserves DBF, DAT, and DLG data in canonical runtime tables but does
not infer domain meaning. Remaining questions include:

- which DBF columns are authoritative primary or foreign keys;
- which string fields represent enums, booleans, IDs, or localization keys;
- how dialog image names resolve across image containers;
- which rows correspond to units, spells, buildings, and other game entities;
- whether later typed views should wrap or replace raw table access.

The Stage 7 resolver must keep inferred links explainable and must not treat
zero-padded DBF source indexes as gameplay identities.

## Engine Contract Selectors

**Blocks:** Higher-level engine sample selection after `runtime-data-tables` and
`runtime-asset-reference-resolver`  
**Currently answerable from data:** No

Stage 9 intentionally selects animations and optional sounds by exact canonical
asset ID. This proves the current runtime package boundary without inventing a
unit or game-rule relationship.

Later stages must decide:

- whether `opendis2-dev-asset-inspect` (planned) should accept a data-table row or unit ID;
- whether cross-reference links select a preferred animation role;
- whether logical-name selectors belong in inspection schema v1 or a new
  version;
- whether a higher-level engine sample should replace or extend the low-level
  contract executable.

---

## Canonical Asset IDs

**Blocks:** `runtime-asset-manifest-v1` (Stage 1)  
**Currently answerable from data:** No

Which identifier becomes the stable, engine-facing asset ID?

The extraction pipeline currently uses `logical_name` (a short string like `"HU"`
or `"SELSMALLA"`) as the primary per-image and per-animation identifier. These
names are unique within a container but not globally unique across the full game
(different containers can have images with the same `logical_name`).

Options to evaluate:

1. **Scoped name** — `<container_relative_path>/<logical_name>` (e.g.,
   `Imgs/Battle.ff/SELSMALLA`). Human-readable but couples the engine to the
   installation-relative path of each container.

2. **Container SHA + logical name** — stable across installs but opaque and
   verbose.

3. **Synthetic integer ID** — assigned at manifest-build time, compact and fast,
   but requires a persistent allocation table and complicates debugging.

4. **Record offset** — the physical byte offset or record index within the MQDB
   container. Stable per file version but breaks across patches.

The choice affects every stage from Stage 1 onward. It must be decided before
the Stage 1 manifest schema is frozen.

---

## Animation Timing

**Blocks:** authoritative animation playback timing after
`runtime-animation-model` (Stage 4)  
**Currently answerable from data:** Partially — default timing is in `-ANIMS.OPT`
but per-frame timing is not extracted

`anim.json` currently emits `"frame_delay_ms": 100` for all animations. This is
a hardcoded fallback; the actual value is not decoded from the source data.

The `-ANIMS.OPT` block in each `.ff` container contains a `frame_delay` field
(found in animation descriptor at block offset 0x47e). The current extractor
reads this field but does not yet write it to `anim.json`.

**Stage 4 status:** Runtime clips preserve uniform sidecar timing with
`ProvisionalSidecar` provenance. Missing or unusable values receive a documented
100 ms `FallbackDefault` and a warning. This allows deterministic timelines
without claiming that the value matches original engine timing.

Questions still unresolved:

- Is `frame_delay` a uniform duration for all frames in an animation, or does
  each frame have its own delay?
- Are there animations where `frame_delay == 0` that should loop or pause on the
  last frame?
- Does the engine interpret `frame_delay` as wall-clock milliseconds, engine ticks,
  or something else?

Until real timing is decoded and validated against D2ResExplorer playback,
runtime consumers must treat `frame_delay_ms` as provisional.

Loop mode and facing direction also remain `Unknown`; Stage 4 does not infer
them from suffixes. Animation role classification is best-effort and retains
the matched token and reason.

---

## Sprite Pivot / Anchor

**Blocks:** `runtime-atlas-access` (Stage 3)  
**Currently answerable from data:** No

**Stage 3 status:** Runtime atlas access preserves pivot, anchor, trim size, and
trim offset as unset optional values. This question no longer blocks atlas
rectangle lookup, but it still blocks renderer placement that depends on an
authoritative sprite origin.

Each image sidecar JSON has a `parts` array where `parts[i].dest` gives the pixel
offset at which the tile is composited onto the output canvas. For single-part
images `parts` is empty and the output canvas is exactly `output_size`.

For rendering, the engine needs to know the anchor point of the sprite (e.g.,
the ground contact point for a unit sprite, or the center for a UI element).
This is distinct from `dest` offsets, which describe internal tile reconstruction,
not the world-space render origin.

Questions to resolve:

- Is the anchor point derivable from the `-IMAGES.OPT` header fields that are
  not yet fully mapped?
- Is it stored elsewhere (e.g., a separate `.dbf` column like `BASE_X`, `BASE_Y`)?
- For battle-screen unit sprites, is the anchor the bottom-center of the
  bounding box, or a dedicated field?

Until resolved, Stage 3 cannot produce correct atlas entry metadata for renderers
that need pivot-based sprite placement.

The current atlas producer also writes every sheet at `max_sheet_size` square.
Runtime bounds validation uses that invariant without decoding PNG headers.
Supporting variable-sized sheets requires explicit per-sheet dimensions in a
future versioned atlas schema.

---

## Sound Name Mapping

**Blocks:** `runtime-asset-reference-resolver` (Stage 7)  
**Currently answerable from data:** No

`.wdb` container logical names (e.g., `"0001_HIT_B"`) follow an internal
numbering scheme. The in-game sound trigger system references sounds by symbolic
name. The mapping between trigger names and `.wdb` record names is defined in
`.wdt` files.

Current status: `extract-wdt` exists but is best-effort. The `.wdt` parsing is
incomplete and the mapping table it produces is not yet integrated into the
extraction output.

Questions to resolve:

- What is the full schema of a parsed `.wdt` file and how reliable is the current
  parser?
- Should the resolved trigger→record mapping be embedded in the sound sidecar
  JSON, in a separate lookup table, or in the top-level manifest?
- How are sounds referenced from dialog (`.dlg`) and scenario scripts?

**Stage 5 status:** `runtime-sound-access` loads package-local WDB record
metadata, payload paths, formats, channels, and sample rates by canonical sound
ID. It deliberately does not claim symbolic trigger names. Until `extract-wdt`
is stable and integrated, the reference resolver cannot produce reliable
trigger-to-sound links.

---

## Manifest Granularity

**Blocks:** `runtime-asset-manifest-v1` (Stage 1)  
**Currently answerable from data:** Partially — current per-container manifests
and top-level `extraction_manifest.json` provide a model to evaluate

Stage 1 must define what a `game_manifest.json` at the output root should contain
versus what stays in per-container sidecar files.

Tension points:

- **Global asset ID table** — should the top-level manifest contain a flat index
  of all asset IDs with their container paths, or should the engine build this
  at load time by scanning container directories?

- **Extraction stats vs. runtime data** — current `manifest.json` per container
  mixes extraction diagnostics (`warnings_count`, `failed_images`) with data the
  runtime might use (`source_container`, `total_images`). These have different
  consumers and different update frequencies.

- **Overwrite conflict** — `extract-all` runs `extract-images` then `extract-anim`
  on the same output directory; the animation run overwrites the image run's
  `manifest.json`. Stage 1 must decide whether to merge these, rename them, or
  eliminate one.

- **Versioning** — no current manifest includes a schema version field. Stage 1
  should define how manifest consumers detect schema changes.

---

## Capital.wdb UTF-8 Bug

**Blocks:** Stable classification for sound output  
**Currently answerable from data:** Yes — bug is reproducible

`Capital.wdb` contains sound records whose names include non-UTF-8 byte sequences
(likely Windows-1252 encoded). The `extract-sounds` command passes these names
directly into `nlohmann/json`, which throws an exception on invalid UTF-8 input,
causing the entire container extraction to fail.

This means:

- `extract-all` cannot extract `Capital.wdb` sounds.
- The per-container manifest for `Capital.wdb` is never written.
- Sounds from this container cannot be classified as Stable or Provisional until
  the bug is fixed.

Questions to resolve:

- Should invalid name bytes be sanitized (replace with `_` or percent-encode), or
  should `extract-sounds` accept an explicit encoding hint flag?
- Does the same issue affect `Sounds/Midgard.wdb` or `Sounds/Audiorgn.wdb`?
- Are any capital-screen audio assets referenced by gameplay logic that requires
  them to be resolvable at runtime?

Fix is straightforward (sanitize or transcode the name before JSON serialization)
but must be validated against D2ResExplorer to confirm the expected record names.

---

## Runtime Package Compaction

**Blocks:** No current Stage 8 functionality  
**Currently answerable from data:** Partially

Stage 8 packages standalone image PNGs and animation frame PNGs alongside atlas
sheets. This guarantees fallback availability and keeps the schema-v1 package
self-contained, but duplicates pixel data.

Questions for a future versioned package profile:

- May standalone frame PNGs be omitted when every frame has a validated atlas
  region?
- Should standalone image PNGs remain available for tools even when atlas
  coverage is complete?
- How should a compact package declare that fallback payloads were
  intentionally omitted rather than lost?

Until that contract exists, the canonical builder keeps fallback files.
