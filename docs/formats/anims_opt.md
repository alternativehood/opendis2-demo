# -ANIMS.OPT Format

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Verified against:** A game `.ff` container  
**Source:** Karnah/Disciples.Net `ImagesExtractor.cs::LoadMqAnimations`  
**Endian:** Little-endian

---

## Purpose

Defines animation sequences as ordered lists of frame image names.
Each frame name corresponds to an image entry in -INDEX.OPT.

---

## File Structure

Flat sequence of animation blocks. No top-level count.
Parsing ends when `position >= fileOffset + fileSize - 1`.

Animation blocks are ordered. Block N corresponds to the N-th entry with `id == -1` in -INDEX.OPT (matched by position, not by name).

---

## Block Layout

| Offset | Size | Type  | Name        | Notes |
|--------|------|-------|-------------|-------|
| +0     | 4    | int32 | framesCount | Number of frames in this animation |

### Per frame (framesCount frames follow)

| Offset | Size | Type | Name      | Notes |
|--------|------|------|-----------|-------|
| +0     | var  | cstr | frameName | Null-terminated ASCII; matches an image entry name in -INDEX.OPT |

"cstr" = null-terminated ASCII. No alignment padding.

---

## Accessing a Block via -INDEX.OPT

Given an animation entry from -INDEX.OPT with `(relatedOffset, size)`:

```
block_start = anims_opt_payload_offset + relatedOffset
block_end   = block_start + size
```

Parse `framesCount` at `block_start`, then read that many null-terminated strings.

**No +28 offset** — tested and confirmed. NevendaarTools `+28` formula is incorrect for this file.

---

## Animation Name Lookup

To get the name of animation block N:
1. Iterate -INDEX.OPT entries
2. Collect entries where `id == -1` in order
3. The N-th such entry gives the animation name

This is why -INDEX.OPT must be fully parsed before -ANIMS.OPT can be named.

---

## Verified Sample (synthetic)

Animation `TEST_ANIMATION_IDLE`:
- -INDEX.OPT entry: `id=-1, relatedOffset=520, size=52`
- Block at `anims_opt_offset + 520`:

```
framesCount = 4
frames: ['TEST_FRAME_0001', 'TEST_FRAME_0002',
         'TEST_FRAME_0003', 'TEST_FRAME_0004']
```

---

## Section 13 Item 5

**Status: resolved** — layout documented and verified against a real game `.ff` container.

## Runtime Animation Model Status

Stage 4 (`runtime-animation-model`) consumes extracted `anim.json` and frame PNG
files only. It establishes no new binary-format conclusions beyond the verified
ordered frame-name layout above.

In particular, authoritative timing units, per-frame timing, loop mode, facing
direction, pivots, and anchors remain unresolved. The runtime layer marks the
current extracted `frame_delay_ms` as provisional and does not reinterpret it
as a newly verified `-ANIMS.OPT` field.

## Runtime Package Layout Status

Stage 8 (`runtime-asset-package-layout`) rearranges existing decoded sidecars,
frame PNGs, and generated atlas files into a portable package. It establishes no
new Section 13 binary-format conclusions and does not change the documented
`-ANIMS.OPT` interpretation.

## Runtime Engine Contract Status

Stage 9 (`runtime-assets-engine-contract`) reads only canonical package metadata
through `libd2asset`. It establishes no new Section 13 binary-format conclusions.
Known animation sequences remain contract fixtures;
timing units, loop/facing semantics, pivots, anchors, and WDT mappings remain
unresolved.
