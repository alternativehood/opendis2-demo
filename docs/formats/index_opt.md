# -INDEX.OPT Format

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Verified against:** Game `.ff` archive  
**Source:** Karnah/Disciples.Net `ImagesExtractor.cs::LoadMqIndexes`  
**Endian:** Little-endian

---

## Purpose

Maps logical resource names (images and animations) to:
- For images: the MQRC record id of the base PNG + position within -IMAGES.OPT
- For animations: position within -ANIMS.OPT

---

## Binary Layout

### File header

| Offset | Size | Type  | Name        | Notes |
|--------|------|-------|-------------|-------|
| 0x00   | 4    | int32 | framesCount | Total number of entries (images + animations) |

### Per entry (framesCount entries follow)

| Offset | Size | Type  | Name          | Notes |
|--------|------|-------|---------------|-------|
| +0     | 4    | int32 | id            | MQRC record id of base PNG, or -1 for animation |
| +4     | var  | cstr  | name          | Null-terminated ASCII string |
| +4+len | 4    | int32 | relatedOffset | Byte offset within -IMAGES.OPT (id != -1) or -ANIMS.OPT (id == -1) |
| +8+len | 4    | int32 | size          | Byte length of the block at relatedOffset |

"cstr" = null-terminated ASCII, variable length. No alignment padding between entries.

---

## Special id Values

| id Value | Meaning |
|----------|---------|
| -1       | Animation entry — relatedOffset/size point into -ANIMS.OPT |
| any other | Image entry — id is the MQRC record id of the base PNG; relatedOffset/size point into -IMAGES.OPT |

---

## Animation Name Matching

Animations are matched to -INDEX.OPT entries **by position**:
- The N-th animation block in -ANIMS.OPT corresponds to the N-th entry with id == -1 in -INDEX.OPT.
- The ordering of entries in -INDEX.OPT must be preserved during parsing.

---

## Duplicate Names

Duplicate entries can appear (observed in production files). Correct behavior: first occurrence wins, emit warning for duplicates.

---

## Verified Sample

framesCount = 69921

First entries:
```
id=35110  name='0'                    relatedOffset=0       size=2005
id=2888   name='00'                   relatedOffset=2005    size=1846
id=-1     name='BLISTUCHA1B00'        relatedOffset=...     (animation)
id=-1     name='G000UU0001HHITA1A00'  relatedOffset=...     (animation)
```

Entry for G000UU0001IDLEA1A00 (animation):
```
id=-1  relatedOffset=520  size=52
```

Entry for frame XE (image):
```
id=35131  relatedOffset=2918011  size=1918
```

---

## Notes

- The animation name in the research document (G000UU0001IDLE1A00) differs from the actual file (G000UU0001IDLEA1A00). Accept both variants in tests.
- Short frame names (e.g., 'XE', 'YE', 'ZE') are internal identifiers; they appear as image entries referenced by -ANIMS.OPT blocks.

---

## Section 13 Item 3

**Status: resolved** — layout documented and verified against game data.
