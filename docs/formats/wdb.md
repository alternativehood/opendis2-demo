# WDB Format

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Verified against:** `Sounds/Battle.wdb`  
**Source:** Karnah/Disciples.Net `SoundsExtractor.cs`  
**Endian:** Little-endian

---

## Summary

`.wdb` files are standard MQDB containers (see `mqdb.md`), with one difference:
the name table record uses `FilesRecordId = 1` and has a different internal layout.

The MQDB header and MQRC record structure are **identical** to `.ff` files.

---

## Battle.wdb Header (verified)

```
000000: 4D 51 44 42  magic="MQDB"
000004: 00 00 00 00  unknown_0=0
000008: 09 00 00 00  record_count(?)=9
00000C: 00 00 00 00  unknown_1=0
000010: 00 00 00 00  unknown_2=0
000014: 00 00 00 00  unknown_3=0
000018: 64 58 4C 01  checksum=0x014C5864
```

---

## Name Table (FilesRecordId = 1)

The name table is in the MQRC record with `id == 1`.

```
Payload starts with:
  [magic: 4 bytes]  "WDB\0" = 57 44 42 00
  [unknown: 4 bytes]  02 00 00 00 = 2

After skipping these 8 bytes, entries follow:
  filesCount = record.Size / 24  (integer division, implicitly accounts for 8-byte header)

Per entry (24 bytes total):
  id:       int32 LE   (4 bytes)  — MQRC record id of the audio payload
  fileName: 20 bytes   null-terminated ASCII
```

**Battle.wdb example:**
- record.Size = 31424
- filesCount = 31424 / 24 = 1309 (integer division)
- First entry: id=974 (0x3CE), fileName='0001'

---

## Audio Payload Format

Each audio record payload is a WAV file (may be compressed WAV/MP3 inside RIFF container).
The extractor should write payloads as-is with `.wav` extension and parse the RIFF header separately.

Expected RIFF header:
```
"RIFF" (4 bytes)
file_size - 8 (uint32 LE)
"WAVE" (4 bytes)
"fmt " (4 bytes)
...
```

Supported `wFormatTag` values include PCM (0x0001) and MP3 (0x0055 = WAVE_FORMAT_MPEGLAYER3).

For playback, format-tag 1 records are passed through as complete RIFF/WAVE files.
Format-tag 85 records retain the original RIFF/WAVE bytes for extraction, but the
runtime preview path passes only the declared `data` chunk bytes to the MP3 decoder;
those records contain an MPEG frame stream inside the WAVE container.

---

## Section 13 — WDB Notes

Stage 5 (`runtime-sound-access`) consumes already extracted sound metadata and
payload files through canonical package sidecars. It establishes no new WDB or
WDT binary-layout conclusions. WDT trigger mapping, compressed-audio duration,
and playback support remain unresolved.

WDB is fully resolved as standard MQDB + different name table. No unknown items specific to WDB.
