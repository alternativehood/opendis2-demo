# SG Scenario Records

## MidUnit

`MidUnit` has unmarked sequential fields and must not be decoded by independently
searching for each marker. The body begins after the object envelope's
`BEGOBJECT` marker; real files also carry the envelope `OBJ_ID` before
`BEGOBJECT`.

Confirmed body order:

1. optional `00` byte before `UNIT_ID`
2. `UNIT_ID`, followed by a length-prefixed NUL-terminated string
3. `TYPE`, followed by a length-prefixed NUL-terminated string
4. `LEVEL`, followed by `i32le`
5. exactly 10 raw ASCII bytes: `inner_unit_id`
6. unmarked `u32le modifier_count`
7. `modifier_count` repetitions of `MODIF_ID`, followed by a length-prefixed
   NUL-terminated string
8. `CREATION`, followed by `i32le`
9. `NAME_TXT`, followed by a length-prefixed NUL-terminated string
10. `TRANSF`, followed by `u8`
11. optional `DYNLEVEL`, followed by `u8`
12. optional compatibility padding `00 00 00`, only when immediately followed by
    `HP`
13. `HP`, followed by `i32le`
14. `XP`, followed by `i32le`
15. `ENDOBJECT`

The three identity surfaces are preserved separately at parse time:

- envelope `OBJ_ID`
- body `UNIT_ID`
- fixed-width `inner_unit_id`

Normal corpus records have all three equal. Mismatches are diagnostics; they are
not normalization points.

`SgMidUnitWire::name_text_raw` preserves the original `NAME_TXT` length-prefixed
payload bytes without the marker or length prefix. The trailing NUL byte is
included so the wire field can be round-tripped exactly. `SgUnit::name` is the
semantic CP1251-decoded UTF-8 string.

## MidStack Group Semantics

`STACK_ID` and `GROUP_ID` are distinct fields and both are preserved. Group
members are the six `UNIT_0` through `UNIT_5` fields. The empty member sentinel
is `G000000000`.

Formation cells are the six `POS_0` through `POS_5` fields. They are
**cell-indexed**: `POS[formation_cell] = member_index`.

- `-1` means an empty formation cell.
- `0..5` indexes the corresponding `UNIT_n` member.
- A large unit is represented by the same member index appearing in multiple
  formation cells (adjacent positions in the same row).

Example: `UNIT_0 = S143UN0038` with `POS_2 = 0` means member index 0 occupies
formation cell 2. The member-indexed convenience view (`positions[member_index]
= formation_cell`) is derived by inverting this cell→member relation.

## MidCityOrVillage Capital Fields

Capital city records preserve the visiting stack and the garrison separately:

- `STACK` is the stack id stored on the city record.
- `GROUP_ID` is preserved independently from `STACK`.
- `UNIT_0` through `UNIT_5` are the garrison members, using `G000000000` as the empty sentinel.
- `POS_0` through `POS_5` are the cell-indexed formation slots for that garrison.

## Isounit Adventure Standing Animation

Adventure-map unit standing visuals live in `Imgs/Isounit.ff`. For a global unit
type ID, the observed standing animation name is:

```text
uppercase(TYPE_ID) + "STOP0"
```

Examples observed from real `Imgs/Isounit.ff`:

| Unit type | Animation | First frame record |
|-----------|-----------|--------------------|
| `G000UU0020` | `G000UU0020STOP0` | `JED` |
| `G000UU8101` | `G000UU8101STOP0` | `QV2` |
| `G000UU0100` | `G000UU0100STOP0` | `VSJ` |

## MidCityOrVillage Capital Fields

Confirmed capital state rule:

- there is no serialized Alive/Destroyed flag for Capital
- `STACK` is the visiting/inside stack reference
- `STACK` does not determine capital state
- the capital's own garrison lives in `UNIT_0..5`
- guardian type comes from `RaceDef::guardian_unit_id`, loaded from `Grace.dbf::GUARDIAN`
- guardian instance with `current_hp > 0` means Active
- guardian instance with `current_hp <= 0` means Ruined
- a fully valid garrison with no guardian instance means Ruined
- visual selection happens only after gameplay state is determined
