# WDT Format

**Game version:** Disciples II — Rise of the Elves (Steam)  
**Status: NOT PRESENT in this game version**

---

## Availability

No `.wdt` files were found in the Rise of the Elves Steam installation:

```
/path/to/disciples2/Sounds/
  Audiorgn.wdb
  Battle.wdb
  Capital.wdb
  Midgard.wdb
```

The `.wdt` sound-mapping format may only exist in older Disciples II versions:
- Disciples II: Dark Prophecy
- Disciples II: Gallean's Return
- Disciples II: Gold

The research document referenced `Sounds/Battle.wdt` but this file is absent in Rise of the Elves.

---

## Expected Format (from research document and D2Ext descriptions)

Based on the research document (Section 5.3) and D2Ext documentation, `.wdt` was expected to be:
- An MQDB container
- Each entry maps unit attack/hit/damage/movement sounds to frame intervals
- Used to synchronize audio with animation frames in battle

Expected JSON output structure (if file were found):
```json
{
  "unit_id": "g000uu0001",
  "attack": {
    "sounds": ["0001_ATTACK_A"],
    "start_frame": 3,
    "end_frame": 8
  },
  "hit": { "sounds": ["0001_HIT_A"], "damage_result_frame": 7 },
  "receive_damage": { "sounds": ["0001_DAMAGE_A"] },
  "movement": { "sounds": ["0001_MOVE_A"] }
}
```

---

## Section 13 Item 8

**Status: still-unknown** — no WDT files in the target game installation (Rise of the Elves Steam).

Minimum empirical approach for future work:
1. Obtain a WDT file from an older Disciples II version (Dark Prophecy / Gold)
2. Verify it opens as MQDB
3. Hex-dump 3+ records to determine per-unit entry layout
4. Cross-reference with DSLUnpacker source if/when obtained from D2Ext
