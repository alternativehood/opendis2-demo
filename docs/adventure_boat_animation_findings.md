# Adventure Boat Animation Findings

Research conducted 2026-07-22 via FfAssetStore public APIs against
Disciples II Rise of the Elves game data.

## Container

All boat-related animations reside in:

    Imgs/Isounit.ff

## Logical Animation Families

Four animation families exist per race, each with 8 directional variants (0–7):

| Family | Pattern | Role | Notes |
|--------|---------|------|-------|
| BOAT | `{race_id}BOAT{d}` | Idle (visual) | Present for all 6 races, 16 frames/direction |
| BTMV | `{race_id}BTMV{d}` | Movement (visual) | Present for all 6 races, 16 frames/direction |
| SBOA | `{race_id}SBOA{d}` | Shadow (idle) | Present for races 0–4; absent for G000RR0005 |
| SBTM | `{race_id}SBTM{d}` | Shadow (movement) | Present for races 0–4; absent for G000RR0005 |

Frame counts: 16 per directional variant (BOAT, BTMV, SBOA, SBTM).
Canvas dimensions: 320×320 (races 0,1,2,4), 250×250 (races 3,5).

Loop/timing: frame duration and looping metadata are currently
synthesised/defaulted by FfAssetStore. Actual timing values are UNRESOLVED.

## Race G000RR0005

G000RR0005 has a catalog anomaly: both `G000RR0005BTMV0-7` and
`G000RR0005BBTMV0-7` exist. Both resolve with 16 frames each.
The standard `BTMV` naming is used; `BBTMV` appears to be a duplicate
entry, not a different animation family. No exception needed in the resolver.

## Shadow Animations

SBOA{d} and SBTM{d} exist for races G000RR0000 through G000RR0004.
These are intended for synchronized shadow rendering (future feature,
not integrated). G000RR0005 does not have shadow families.

## Unit Movement

Unit movement animation naming convention remains UNRESOLVED.
Resolver returns `std::nullopt` for Move role + Unit presentation.
