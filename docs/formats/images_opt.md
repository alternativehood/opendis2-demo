# -IMAGES.OPT Format

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Verified against:** Game `.ff` archive frame block  
**Source:** Karnah/Disciples.Net `ImagesExtractor.cs::LoadMqImages`  
**Endian:** Little-endian

---

## Purpose

Describes how logical images are composed from base PNG images stored in MQDB records.
Contains: palette, transparency metadata, source/destination rectangles per image piece.

---

## File Structure

The file is a flat sequence of blocks. There is no top-level count.
Parsing ends when `position >= fileOffset + fileSize - 1`.

Each block covers one "base image group" — all frames that share a palette and source base image.

---

## Block Layout

### Block header (11 + 1024 bytes = 1035 bytes)

| Offset | Size | Type   | Name                 | Notes |
|--------|------|--------|----------------------|-------|
| +0     | 1    | uint8  | transparentColorIndex | Index into palette of the primary transparent color |
| +1     | 2    | int16  | opacityAlgorithm     | Controls per-pixel alpha computation |
| +3     | 4    | int32  | sizeX                | Base image width in pixels |
| +7     | 4    | int32  | sizeY                | Base image height in pixels |
| +11    | 1024 | bytes  | palette              | 256 palette entries, 4 bytes each, BGRA order |

Palette entry layout:
```
byte[0] = Blue
byte[1] = Green
byte[2] = Red
byte[3] = Alpha (stored but not used by Karnah extractor for color lookup)
```

### Block body — frame list

| Offset   | Size | Type  | Name        | Notes |
|----------|------|-------|-------------|-------|
| +1035    | 4    | int32 | framesCount | Number of frames in this block |

### Per frame (framesCount frames follow)

| Offset | Size | Type  | Name       | Notes |
|--------|------|-------|------------|-------|
| +0     | var  | cstr  | frameName  | Null-terminated ASCII; matches an entry name in -INDEX.OPT |
| +len   | 4    | int32 | piecesCount| Number of rectangular pieces |
| +len+4 | 4    | int32 | width      | Output image width in pixels |
| +len+8 | 4    | int32 | height     | Output image height in pixels |

### Per piece (piecesCount pieces follow each frame)

| Offset | Size | Type  | Name       | Notes |
|--------|------|-------|------------|-------|
| +0     | 4    | int32 | outputX    | X coordinate in OUTPUT image (called sourceX in Karnah code) |
| +4     | 4    | int32 | outputY    | Y coordinate in OUTPUT image (called sourceY in Karnah code) |
| +8     | 4    | int32 | baseX      | X coordinate in BASE image (called destX in Karnah code) |
| +12    | 4    | int32 | baseY      | Y coordinate in BASE image (called destY in Karnah code) |
| +16    | 4    | int32 | pieceWidth | Width of rectangle in pixels |
| +20    | 4    | int32 | pieceHeight| Height of rectangle in pixels |

Each piece = 24 bytes.

**Copy operation per piece:**
```
Copy rectangle [baseX, baseY, pieceWidth, pieceHeight] from base PNG
     to output image at position [outputX, outputY]
```

---

## Transparency

### opacityAlgorithm Values

| Value     | Meaning |
|-----------|---------|
| 0-255     | Fixed alpha: every palette color gets alpha = opacityAlgorithm. Transparent color override below. |
| 300       | Palette-index alpha: pixel alpha = palette index of that pixel. Used for auras. |

### Transparent color override

Regardless of opacityAlgorithm, `palette[transparentColorIndex]` always gets alpha = 0.

### Default magenta range (always applied)

These BGR color ranges are always transparent, regardless of opacityAlgorithm or palette:

```
Blue:  249-255 (0xF9-0xFF)
Green:   0-3   (0x00-0x03)
Red:   252-255 (0xFC-0xFF)
```

This is the "magenta range" around #FF00FF. Both Karnah (C#) and NevendaarTools (C++) confirm this range.

NevendaarTools also defines shader modes:
- Shadows: additionally transparent black (R<5, G<5, B<5) rendered at alpha=127
- Border: additionally transparent border colors (R>250, B>250, G<5 or G>250)

### Transparency algorithm (C++ pseudocode)

```cpp
bool is_default_transparent(uint8_t b, uint8_t g, uint8_t r) {
    return b >= 249 && b <= 255 && g >= 0 && g <= 3 && r >= 252 && r <= 255;
}

uint8_t pixel_alpha(int palette_index, uint8_t b, uint8_t g, uint8_t r,
                    int transparent_color_index, int opacity_algorithm) {
    if (palette_index == transparent_color_index)
        return 0;
    if (is_default_transparent(b, g, r))
        return 0;
    if (opacity_algorithm <= 255)
        return static_cast<uint8_t>(opacity_algorithm);
    if (opacity_algorithm == 300)
        return static_cast<uint8_t>(palette_index);
    return 255;  // unknown algorithm, default opaque
}
```

---

## Accessing a Block via -INDEX.OPT

Given an image entry from -INDEX.OPT with `(relatedOffset, size)`:

```
block_start = images_opt_payload_offset + relatedOffset
block_end   = block_start + size
```

Parse the block from `block_start`. The block contains `framesCount` frames; find the one whose `frameName` matches the desired logical name.

**Important:** NevendaarTools code uses `+28` additional offset (`images_opt->offset + 28 + relatedOffset`). This was tested against game data and is **incorrect**. The correct formula is without the +28 (matches Karnah and verified by hex dump).

---

## Verified Sample

```
Block at offset 310789856 + 2918011 = 313707867
  transparentColorIndex = 0
  opacityAlgorithm      = 255
  sizeX                 = 221
  sizeY                 = 218
  palette[0]            = BGRA (255, 0, 255, 119)   <- magenta, transparent
  framesCount           = 9

  frame[0] name='XE'  piecesCount=4  outputWidth=800  outputHeight=600
    piece[0]: outputX=388 outputY=448 baseX=0 baseY=0 w=80 h=32
    piece[1]: outputX=395 outputY=415 baseX=79 baseY=128 w=31 h=33

  frame[1] name='YE'  piecesCount=4  outputWidth=800  outputHeight=600
    piece[0]: outputX=388 outputY=448 baseX=0 baseY=32 w=80 h=32
```

---

## Known Engine Bug

From Karnah's code:
> Some frames have dimensions exceeding the base image (e.g., G000UU0049HMOVA1A00).
> Piece destX (baseX) > baseImage.OriginalWidth — skip such pieces silently.

---

## Section 13 Items 4, 6, 7

| Item | Status | Notes |
|------|--------|-------|
| 4. Exact parsing of -IMAGES.OPT | **resolved** | Layout documented and verified |
| 6. Exact palette storage format | **resolved** | 256 * 4 bytes, BGRA order, confirmed in game data |
| 7. Exact transparency mode flags | **resolved** | opacityAlgorithm: <=255 = fixed alpha, 300 = palette-index alpha |
