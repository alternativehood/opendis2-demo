# PNG/APNG Payload Metadata

OpenDis2 treats embedded PNG/APNG chunk metadata as supporting payload data for research.
Pixel decoding still uses lodepng, and composed battle frame dimensions/placement still come from
OPT ImageMap records.

The metadata scanner records:

- PNG signature validity.
- `IHDR` width, height, bit depth, color type, compression, filter, and interlace fields.
- `pHYs` pixels-per-unit fields.
- `acTL` frame/play counts.
- `fcTL` APNG frame control fields.
- text chunk keys from `tEXt`, `zTXt`, and `iTXt`.
- unknown chunk type names and truncation warnings.

No APNG frame placement semantics are inferred from `fcTL` in runtime behavior.

## 2026-06-19 Seed Battle FX Run

Seed reports used local game data to confirm research output shape. No new APNG placement
or role-token semantics were confirmed.
