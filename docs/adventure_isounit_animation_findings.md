# Adventure `Isounit.ff` animation findings

This report describes the Rise of the Elves data set inspected on 2026-07-22. It is a research
result, not a runtime resolver contract. The document records summarized findings, aggregated
statistics, naming rules, architectural conclusions, and representative examples. The full
per-animation/per-unit catalog is intentionally not tracked in this repository.

## 1. Data sources inspected

- `Imgs/Isounit.ff`, exclusively through `FfAssetStore::animations_in`, `sprites_in`, and
  `animation_metadata` for the catalog. Selected decoded frames were inspected only after the
  logical catalog had established their identity.
- All 57 DBFs in `Globals/` were enumerated. The linkage tables actually used were `Gunits.dbf`,
  `Grace.dbf`, `GSubRace.dbf`, `GMabi.dbf`, `Lground.dbf`, `Lrace.dbf`, `LSubRace.dbf`,
  `LunitC.dbf`, `LleadC.dbf`, `LleadA.dbf`, and `Tglobal.dbf` (via `GameDataRegistry`). There is no
  `Gunit.dbf`; `Gunits.dbf` is the unit table.
- `GameDataRegistry` is authoritative for resolved unit names, race IDs, categories,
  `WATER_ONLY`, and GMabi rows.

Exact inventory: **5,039 logical animations** and **41,855 logical sprites/frame records**.

## 2. DBF schemas used

`Gunits`: `UNIT_ID C(10)`, `RACE_ID C(10)`, `SUBRACE N(2)`, `UNIT_CAT N(2)`, `BRANCH N(2)`,
`WATER_ONLY L(1)`, plus name text, base/previous unit and leader/stat fields. `Grace`: `RACE_ID
C(10)`, `RACE_TYPE N(2)`, playable flag and guardian/noble/leader/soldier IDs. `GSubRace`:
`RACE_ID C(10)`, `RACE_TYPE N(2)`, `NAME_TXT C(10)`. `GMabi`: `UNIT_ID C(10)`, `M_ABILITY
N(2)`. `Lground`: `(0 L_PLAIN, 1 L_FOREST, 3 L_WATER, 4 L_MOUNTAIN)`. Localization tables map
numeric race/subrace/unit/leader categories. The inspected installation contains 359 raw Gunits
rows; `GameDataRegistry` exposes 356 non-empty IDs, 6 races, and 655 GMabi rows.

## 3. UNIT_ID ownership model

Ownership is determined by longest case-insensitive prefix among actual DBF unit and race IDs,
then the remainder is parsed as family plus final direction digit. No fixed prefix length is used.
Although both ID fields happen to be `C(10)` here, that fact is not embedded in the algorithm.
All 5,039 animations match a known DBF unit or race prefix; there are no catalog orphans under
this rule. One exceptional name, `G000UU8047HHITA1A00`, demonstrates why suffix classification
must remain catalog-driven rather than assuming only four-letter families.

## 4. Race/subrace ownership model

The six `Grace.RACE_ID` values are `g000rr0000..g000rr0005`. Numeric Gunits subraces map as:
1→race 0, 2→race 3, 3→race 2, 4→race 1, 5/6/8/9/10/11/12/13→race 4, 14→race 5; subrace 7
occurs with both race 4 and race 5. Therefore numeric subrace alone cannot select a boat.
`AdventureSubraceRef::race_id` is copied from the owning scenario player's `race_id`, exactly the
ID namespace that prefixes BOAT/BTMV. That is the correct future boat visual owner; a leader
UnitDef's race is not the authoritative stack-faction identity.

## 5. Full animation-family inventory

| Family | Owner | Sequences | Directions | Frame counts | Classification |
|---|---:|---:|---|---|---|
| STOP | UNIT_ID | 1,144 | 0..7 | 1/7/8/16 | unit idle, confirmed visually |
| MOVE | UNIT_ID | 1,144 | 0..7 | 7/8/12/16 | unit movement, confirmed visually |
| SSTO | UNIT_ID | 1,134 | 0..7 | 1/7/8/16 | STOP shadow, confirmed visually |
| SMOV | UNIT_ID | 1,136 | 0..7 | 7/8/12/16 | MOVE shadow, confirmed visually |
| STO2 | UNIT_ID | 112 | 0..7 | 7/8/16 | alternate idle, semantic trigger unresolved |
| MOV2 | UNIT_ID | 112 | 0..7 | 7/8/16 | alternate move, semantic trigger unresolved |
| BOAT | race_id | 48 | 0..7 | 16 | boat idle/main |
| BTMV | race_id | 48 | 0..7 | 16 | boat movement/main |
| SBOA | race_id | 40 | 0..7 | 16 | BOAT shadow, races 0..4 |
| SBTM | race_id | 40 | 0..7 | 16 | BTMV shadow, races 0..4 |
| BBTMV | race_id | 8 | 0..7 | 16 | elf alternate movement main, unresolved use |
| BOA | race_id | 8 | 0..7 | 16 | elf large wake/shadow-like layer |
| BTM | race_id | 8 | 0..7 | 16 | elf movement shadow layer |
| STOP/MOVE/SSTO/SMOV | race_id | 16 each | 0..7 | 1/16 | generic race 0 and 2 actor set |
| HHITA1A0 | UNIT_ID | 1 | final `0` | 14 | isolated Cyclops record; not an Adventure family |

There are **no zero-frame or invalid sequences**.

## 6. STOP family

Pattern: `upper(UNIT_ID) + "STOP" + direction`, direction 0..7. Exactly 143/356 registry units
(40.17%) have STOP, and every one of those 143 has all eight directions; none is STOP0-only and
none has partial direction coverage. Per-owner frame count is consistent across directions:
99 owners have 1 frame, 13 have 8, 30 have 16, and one has 7. Thus static units are common.
Decoded frames show a standing/idle actor. SSTO is the paired shadow except Arch-Angel
`g000uu0021` (all eight missing), plus isolated missing SSTO4 for Blue Dragon and SSTO7 for Green
Dragon. Current `STOP0 -> first frame` remains untouched.

## 7. Ordinary unit movement family

Pattern: `upper(UNIT_ID) + "MOVE" + direction`. It covers exactly the same 143 units and all
directions. Per-owner counts: 77×8, 63×16, 2×12, 1×7. Ordered decoded frames visibly change
pose/stride/wing phase, confirming movement rather than merely relying on the word MOVE. SMOV
is its shadow counterpart. Unit Move is therefore **confirmed as MOVE0..7**. MOV2 is a complete
paired alternate for only 14 owners; the state that selects it is unresolved and it must not be
silently substituted for MOVE.

## 8. BOAT family

Pattern: `upper(race_id) + "BOAT" + direction`. All six Grace races have 0..7; every sequence has
16 frames. Races 0,1,2,4 use 320×320 canvases; races 3 and 5 use 250×250. Visual inspection shows
race-specific boats. DBF prefix equality, full race coverage, directional variants, animated
frames, and paired shadow/wake layers together confirm race-specific idle boat presentation.

## 9. BTMV family

Pattern: `upper(race_id) + "BTMV" + direction`. All six races have 0..7 and exactly 16 frames.
Visual inspection shows the same race boat with moving water/pose details. It is the canonical
main movement family because it has uniform all-race coverage and pairs with SBTM for races 0..4.

For `G000RR0005`, **both** `BTMV0..7` and `BBTMV0..7` exist; every one has 16 non-empty frames and
is usable. BTMV remains canonical by cross-race consistency. BBTMV is a visually valid alternate
elf boat main layer (not an empty alias); its trigger is unresolved. Its canvas/foot/top values
match BTMV direction-by-direction, but decoded pixels and physical frame names differ.

## 10. SBOA shadow family

Pattern: `upper(race_id) + "SBOA" + direction`, races 0..4 only, all directions, 16 frames.
Each BOAT/SBOA pair has equal frame count. Decoded SBOA contains the boat's ground/water shadow
layer. Confidence: **Confirmed** for races 0..4. Race 5 replaces this naming/layout with BOA/BTM
variants rather than supplying SBOA.

## 11. SBTM shadow family

Pattern: `upper(race_id) + "SBTM" + direction`, races 0..4 only, all directions, 16 frames.
Each BTMV/SBTM pair has equal frame count, and decoded frames contain the moving shadow/water
layer. Confidence: **Confirmed** for races 0..4. Race 5 has no SBTM.

## 12. Ordinary-unit shadow findings

SSTO and SMOV share UNIT_ID/direction/canvas families with STOP and MOVE; decoded samples contain
only detached shadow pixels. STOP/SSTO frame counts always match when both exist. MOVE/SMOV match
except direction 0 for four IDs (`g000uu0023`, `g000uu0074`, `g000uu0100`, `g000uu5130`), where
MOVE has 8 and SMOV 16 frames. Arch-Angel lacks both shadow families; the two dragon SSTO holes
are noted above. Shadows must therefore be optional assets, not a universally required resolver
result. Integration is explicitly deferred.

## 13. Direction model

Every recurring Adventure family has exactly suffixes 0..7. No ordinary owner has only STOP0,
and no direction hole exists in a main STOP/MOVE/BOAT/BTMV set. This confirms an eight-value
stored direction domain `D0..D7`; mapping those numbers to compass/world deltas was not encoded in
the inspected DBFs and remains unresolved. The final zero in the isolated HHITA name is not
evidence of an Adventure direction family.

## 14. Frame-count distributions

Main unit STOP: 792 one-frame, 104 eight-frame, 8 seven-frame, 240 sixteen-frame sequences.
Main unit MOVE: 616 eight, 504 sixteen, 16 twelve, 8 seven. BOAT/BTMV and all named boat shadow
families are uniformly 16. STO2/MOV2 follow their owner's alternate animation lengths. Counts and
frame order are authoritative ANIMS/OPT data.

## 15. Canvas/foot observations

Ordinary composed actor canvases are usually square (most commonly 360×360, with 260/270/300/
326/340/350/410 variants); generic race actors use tightly cropped canvases. Boat canvases are
250×250 or 320×320. Elf BOA uniquely uses 750×750. Canvas width/height and foot/top are derived
from authoritative OPT ImageFrame pieces of the first non-empty sequence frame; the JSON records
every value. They describe source composition space, not future screen placement.

## 16. DBF unit ↔ FF animation coverage

143/356 registry units have complete STOP+MOVE sets; 213 have neither. Coverage is concentrated
in Adventure-relevant categories: all 119 Leaders, all 5 Nobles, all 16 Summons, and all 3
Illusions have STOP/MOVE; 0/5 Guardians and 0/208 ordinary Soldiers do. One Soldier, Cyclops
`g000uu8047`, owns only the isolated HHITA record. No STOP owner lacks MOVE or vice versa. There
are no animation prefixes unmatched by DBF IDs. Missing ordinary soldiers are expected evidence
that Isounit is an Adventure actor catalog, not a complete battle-unit catalog; it is not proof
that those DBF rows are deprecated.

## 17. DBF race ↔ boat animation coverage

All 6/6 Grace races have complete BOAT and BTMV direction sets (100%). Races 0..4 have complete
SBOA/SBTM (5/6, 83.33%). Race 5 has complete BBTMV/BOA/BTM variants and no SBOA/SBTM. Numeric
subrace does not own boat assets; scenario/player race_id does.

## 18. WATER_ONLY / M_ABILITY correlation

Exactly three units are `WATER_ONLY`: Mermaid `g000uu5126`, Sea Serpent `g000uu5129`, Kraken
`g000uu5127`. All three have only GMabi capability 3 (`L_WATER`) and complete ordinary STOP,
MOVE, SSTO, SMOV sets. In total 29 units have native Water capability; all 29 have ordinary
STOP/MOVE visuals. This includes flying units and aquatic units. Asset availability therefore
supports displaying their unit visuals rather than a race boat. It does not create a flying
heuristic. The future domain rule remains `terrain == Water && !leader_can_natively_traverse(Water)
-> Boat`.

## 19. Naming anomalies and exceptions

- Race 5: additional BBTMV/BOA/BTM, absent SBOA/SBTM.
- Race prefixes 0 and 2 also own generic STOP/MOVE/SSTO/SMOV sets; these are valid logical assets
  but are not UnitDef-owned and their gameplay trigger is unresolved.
- STO2/MOV2 occur for 14 units and are valid complete sets; selection semantics unresolved.
- `G000UU8047HHITA1A00` is the only non-recurring unit suffix.
- Arch-Angel has no SSTO/SMOV; two dragons have one SSTO direction hole; four SMOV direction-0
  counts differ from MOVE.
- Zero/invalid sequences: **none**. Duplicate logical names: none reported by the logical index.

## 20. Authoritative facts

Authoritative: logical names and counts from OPT index; frame count/order and logical frame names
from ANIMS/OPT; ImageFrame canvas/foot/top; DBF identities, categories, flags and ability IDs;
catalog existence/absence and the numerical coverage above. Physical decoded inspection is
supporting evidence for semantics, not identity.

## 21. Strong inferences

STOP=idle and MOVE=movement are supported by paired complete directional catalogs, frame behavior,
and visual inspection. BOAT=boat idle and BTMV=boat movement are supported by DBF race coverage,
visual inspection and synchronized shadow families. BOA is an elf wake/shadow-like auxiliary,
BTM an elf movement-shadow layer, and BBTMV an alternate main movement layer; their exact runtime
composition/selection is only strongly inferred and must not become a resolver rule yet.

## 22. Unresolved questions

- Compass meaning and rotation order of D0..D7.
- Original timing and loop policy for every family.
- Gameplay/state trigger for STO2/MOV2, generic race STOP/MOVE, and elf BBTMV/BOA/BTM.
- Correct synchronization policy for the four MOVE/SMOV length mismatches.
- Whether missing shadow directions are intentional data or original-engine fallback behavior.

`FfAssetStore` currently synthesizes `is_looping=false` and `duration_ms=100` for every frame.
Neither is original timing/loop evidence; both remain unresolved.

## 23. Recommended future `AdventureActorAnimationProfile` contract

```text
AdventureActorPresentationKind { Unit, Boat }
AdventureActorMotionState { Idle, Moving }
AdventureIsoDirection { D0, D1, D2, D3, D4, D5, D6, D7 }

Unit + Idle   + Dn -> upper(unit_visual_id) + "STOP" + n
Unit + Moving + Dn -> upper(unit_visual_id) + "MOVE" + n
Boat + Idle   + Dn -> upper(AdventureSubraceRef::race_id) + "BOAT" + n
Boat + Moving + Dn -> upper(AdventureSubraceRef::race_id) + "BTMV" + n
```

Optional, separately resolved layers: Unit Idle Shadow→SSTO, Unit Move Shadow→SMOV, Boat Idle
Shadow→SBOA, Boat Move Shadow→SBTM. Race-5 alternatives require an explicit later decision, not
an alias hidden in the resolver. Timing/loop must be represented as unknown until original data
or engine behavior establishes it. No runtime, preload, rendering, picking, or resolver change
was made by this research.
