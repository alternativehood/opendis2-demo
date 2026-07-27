# MQDB Format

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Verified against:** Game `.ff` archive  
**Sources:** Karnah/Disciples.Net (C#) · NevendaarTools/toolsqt (C++ Qt)  
**Endian:** Little-endian throughout

---

## File Extensions

| Extension | Content | FilesRecordId |
|-----------|---------|--------------|
| `.ff`     | Images, animations, interface | 2 |
| `.wdb`    | Sound bank | 1 |
| `.wdt`    | Sound mapping/timing | unknown |
| `.csg`    | Campaign/saga archive | unknown |

---

## Container Layout

```
[MQDB Header]   28 bytes
[MQRC Record 1] 28 bytes header + payload
[MQRC Record 2] 28 bytes header + payload
...
[MQRC Record N] 28 bytes header + payload
```

Records are contiguous — no padding between end of one payload and start of next MQRC magic.

---

## MQDB Header (28 bytes)

| Offset | Size | Type   | Name         | Notes |
|--------|------|--------|--------------|-------|
| 0x00   | 4    | char[4]| magic        | `"MQDB"` = `4D 51 44 42` |
| 0x04   | 4    | uint32 | unknown_0    | Always 0 in observed files |
| 0x08   | 4    | uint32 | record_count | Likely total record count; sample value 9 |
| 0x0C   | 4    | uint32 | unknown_1    | Always 0 |
| 0x10   | 4    | uint32 | unknown_2    | Always 0 |
| 0x14   | 4    | uint32 | unknown_3    | Always 0 |
| 0x18   | 4    | uint32 | checksum     | Non-zero in some files; purpose unclear |

**Parsing:** Verify magic == `"MQDB"`, then seek to offset 28. All remaining bytes are MQRC records.

---

## MQRC Record (28-byte header + payload)

| Offset | Size | Type   | Name         | Notes |
|--------|------|--------|--------------|-------|
| +0x00  | 4    | char[4]| magic        | `"MQRC"` = `4D 51 52 43` |
| +0x04  | 4    | uint32 | unknown      | Always 0 in observed records |
| +0x08  | 4    | int32  | id           | Record identifier, unique within container |
| +0x0C  | 4    | int32  | size         | Same as realFileSize in all observed records |
| +0x10  | 4    | int32  | realFileSize | Payload byte count |
| +0x14  | 4    | int32  | isNotDeleted | 1 = active record; possibly 0 = deleted/replaced |
| +0x18  | 4    | int32  | recordMagic  | Always 0 in observed records |
| +0x1C  | N    | bytes  | payload      | `N = realFileSize` bytes |

**Total record size:** `28 + realFileSize` bytes.

**End condition:** When `stream.Position >= stream.Length - 1` or next 4 bytes != `"MQRC"`.

---

## Name Table Record (FilesRecordId=2 for .ff)

The name table is stored as the payload of the record with `id == FilesRecordId`.

**For .ff files (FilesRecordId = 2):**

```
[filesCount: int32 LE]
For each file:
  [fileName: 256 bytes, null-terminated ASCII]
  [id:       int32 LE]
```

Each entry: 260 bytes. Total payload size = 4 + filesCount x 260.

**For .wdb files (FilesRecordId = 1):**

```
[unknown: 8 bytes, skip]
Entries derived: filesCount = record.Size / 24
For each file:
  [id:       int32 LE   — 4 bytes]
  [fileName: 20 bytes, null-terminated ASCII]
```

Each entry: 24 bytes.

---

## Record ID Assignment

Special record IDs observed in .ff files:
- `id = 1` — Internal small record ("MFF\0" magic, 8-byte payload in observed files)
- `id = 2` — Name table (FilesRecordId for .ff)
- Other IDs — PNG payloads (base images)

Special files identified by name (via name table lookup):
- `-INDEX.OPT` — logical name to base image mapping
- `-IMAGES.OPT` — image composition metadata, palettes, transparency
- `-ANIMS.OPT` — animation frame sequences

---

## Duplicate Names

The name table can contain duplicate names. Observed in production files (noted as a known issue in Karnah's code). Correct behavior: first occurrence wins; emit warning for subsequent duplicates.

---

## Algorithm: Open Container

```
1. Read 4 bytes, verify magic == "MQDB"
2. Read 24 bytes, parse header fields
3. Loop from position 28:
   a. Read 4 bytes, verify == "MQRC"; if not or EOF, stop
   b. Read 4 bytes, skip (unknown)
   c. Read int32 -> id
   d. Read int32 -> size
   e. Read int32 -> realFileSize
   f. Read int32 -> isNotDeleted
   g. Read int32 -> recordMagic
   h. offset = current position
   i. Advance realFileSize bytes
   j. Store Record{id, size, realFileSize, isNotDeleted, offset}
4. Find record with id == FilesRecordId, parse name table
5. Build name->id map
```

---

## Sample Hex (first 56 bytes)

```
000000: 4D 51 44 42  magic="MQDB"
000004: 00 00 00 00  unknown_0=0
000008: 09 00 00 00  record_count=9
00000C: 00 00 00 00  unknown_1=0
000010: 00 00 00 00  unknown_2=0
000014: 00 00 00 00  unknown_3=0
000018: E4 36 A7 14  checksum=0x14A736E4
--- record 1 starts at 0x1C ---
00001C: 4D 51 52 43  magic="MQRC"
000020: 00 00 00 00  unknown=0
000024: FF 65 00 00  id=26111
000028: B9 06 00 00  size=1721
00002C: B9 06 00 00  realFileSize=1721
000030: 01 00 00 00  isNotDeleted=1
000034: 00 00 00 00  recordMagic=0
000038: 89 50 4E 47  payload: PNG magic
```

Second MQRC at 0x6F1 (= 0x1C + 28 + 1721):
```
0006F1: 4D 51 52 43  magic="MQRC"
0006F5: 00 00 00 00  unknown=0
0006F9: 01 00 00 00  id=1
0006FD: 08 00 00 00  size=8
000701: 08 00 00 00  realFileSize=8
000705: 01 00 00 00  isNotDeleted=1
000709: 00 00 00 00  recordMagic=0
00070D: 4D 46 46 00  payload: "MFF\0..."
```

---

## Unknowns Resolved

| Section 13 Item | Status | Notes |
|----------------|--------|-------|
| 1. Exact MQRC RECORD binary layout | **resolved** | 28-byte header documented above |
| 2. Exact name table binary layout | **resolved** | 260 bytes/entry (.ff), 24 bytes/entry (.wdb) |
| 9. All files little-endian | **resolved** | Confirmed via C# source + hex verification |
| 10. Alignment/padding | **resolved** | No padding; records are contiguous |
| 12. Deleted/replaced records | **partially-resolved** | `isNotDeleted` field exists; value 0 meaning not verified |
| 13. Case sensitivity | **partially-resolved** | NevendaarTools uses case-insensitive `.toLower()` comparison |
