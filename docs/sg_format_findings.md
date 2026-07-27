## Object Classification Model

Each object in the `.sg` file is classified into one of three mutually exclusive categories:

| Classification | Description |
|---------------|-------------|
| `Parsed` | Fully deserialized into typed C++ structs with field-by-field extraction (e.g., `MidPlayer`, `MidUnit`, `MidStack`, `MidEvent`, `MidStackTemplate`, `MidgardPlan`, `MidMountains`) |
| `VerifiedEmptyInitialState` | Known class name, but byte evidence proves the object body contains only standard framing (BEGOBJECT/OBJ_ID/ENDOBJECT) with no semantic payload (e.g., `MidSpellCast`, `MidSpellEffects`, `MidStackDestroyed`, `MidQuestLog`) |
| `Unknown` | Class name not in either known set — preserved as raw bytes only |

### Classification Sets

**Parsed classes** (29): `ScenarioInfo`, `MidPlayer`, `MidSubRace`, `MidUnit`, `MidStack`, `Capital`, `MidVillage`, `MidSiteMerchant`, `MidSiteMercs`, `MidSiteTrainer`, `MidSiteMage`, `MidRuin`, `MidBag`, `MidLocation`, `MidEvent`, `MidItem`, `MidLandmark`, `MidRoad`, `MidCrystal`, `MidgardMapBlock`, `MidgardMap`, `MidStackTemplate`, `MidDiplomacy`, `MidTalismanCharges`, `MidgardMapFog`, `PlayerKnownSpells`, `PlayerBuildings`, `MidgardPlan`, `MidMountains`, `MidScenVariables`, `TurnSummary`

**Verified-empty classes** (4): `MidSpellCast`, `MidSpellEffects`, `MidStackDestroyed`, `MidQuestLog`

**Unknown classes**: None for reference maps.

### Semantic Parsers

All formerly recognized-unparsed classes are now fully semantically parsed:

- **MidStackTemplate**: LEADER, LEADER_LVL, NAME_TXT, ORDER, ORDER_TARG, SUBRACE, unit levels (0-5_LVL), USE_FACING, FACING, AIPRIORITY, MODIF_ID, UNIT_POS. Units are structured `SgStackTemplateUnit` with unit_id, level, position.
- **MidMountains**: Hex-indexed fields (ID_MOUNT0-Z, POS_X0-Z, POS_Y0-Z, SIZE_X/Y0-Z, IMAGE0-Z, RACE0-Z) for up to 36 entries per object.
- **MidgardPlan**: Sequential ELEMENT/POS_X/POS_Y triple entries.
- **MidScenVariables**: NAME/VALUE/VALUE2 pairs (fallback INTVAL if NAME absent).
- **MidDiplomacy**: RACE_1/RACE_2/RELATION triples (fallback PLAYER_ID/ATTITUDE).
- **MidTalismanCharges**: ITEM_ID + CHARGES pairs.
- **MidgardMapFog**: PLAYER_ID, FOGBITS/FOGDATA binary, MAP_W/MAP_H.
- **PlayerKnownSpells**: PLAYER_ID + SPELL_ID vector.
- **PlayerBuildings**: PLAYER_ID + BUILDDATA binary.
- **TurnSummary**: TURN field.

### Data Structures

The parser produces:

- `scenario.parsed_objects` — map from class name to index entries for fully parsed objects
- `scenario.verified_empty_objects` — map for proven-empty initial-state containers
- `scenario.unknown_objects` — map for truly unknown classes
- `scenario.raw_objects` — vector of `SgRawObject` with complete raw byte payload for every parsed record
- `scenario.global_id_usages` — vector of `SgGlobalIdUsage` recording every G000* reference with provenance
- Each `SgObjectIndexEntry` has a `classification` field (`Parsed`/`VerifiedEmptyInitialState`/`Unknown`)

### Site World Rendering Pipeline

`SgSite` records use one typed runtime/render pipeline: `AdventureSite` →
`SiteAssetCatalog` → `SiteContributor` → `AdventureStartupScreen`.
The pipeline explicitly supports Mage, Merchant, Mercenary, and Trainer kinds.
Trainer has an independent `IMG_ISO` index space `0..3`; its four world visuals
are static sprites in `Imgs/IsoCmon.ff`. `SITE_ICON_TRAI` and
`SITE_ICON_TRAINER.PNG` are interface assets and are excluded from world rendering.

### Field Lookup Hardening

The `find_field()` helper now enforces **word boundaries**: when searching for a key like `ID`, it checks that the character immediately after the key is not alphanumeric or underscore. This prevents matching `"ID"` inside `"PLAYER_ID"`, `"RUIN_ID"`, `"MODIF_ID"`, etc.

### Global ID Resolution

The `GlobalIdResolver` class indexes all `G[0-9A-Za-z]{9}` values from loaded DBF tables:

- Indexes every DBF row by its ID field (UNIT_ID, ITEM_ID, RACE_ID, SPELL_ID, TXT_ID, etc.)
- Resolves text entries through Tglobal.dbf (TXT_ID -> TEXT)
- Resolves NAME_TXT/DESC_TXT references through Tglobal
- CLI flags:
  - `--globals <dir>` — load all .dbf files from directory
  - `--report-global-ids` — print unresolved IDs with provenance
  - `--annotate-ids` — print all global IDs with DBF resolution and text

Example: `G000MG0027` (a map graphic ID used in MidLandmark.TYPE) is reported with its scenario object ID, position, and any DBF match or explicit "UNRESOLVED" marker.

### Test Data Path

Test files are located via the `OPENDIS2_SG_TEST_DIR` environment variable. If unset, falls back to `OPENDIS2_SOURCE_DIR/Downloads/`, then to `Downloads/` relative to the source tree root.

## Reference Map Stats

### Кошмар Сэра Доргенвилля.sg
- Size: 490,405 bytes
- Scenario: "Кошмар Сэра Доргенвилля"
- Creator: "Risemyself"
- Briefing: "Выжить."
- Map: 72×72, seed 837258718
- Objects: 1309 total
- Players: 2, Units: 137, Stacks: 83, Cities: 2, Ruins: 2
- Bags: 102, Locations: 62, Events: 72
- Items: 186, Landmarks: 90, Roads: 364
- Stack Templates: 10, Plans: 48, Mountains: 0
- Map blocks: 162, Unique tile values: 112

### The_DEFEATED_III_Alt_Mod_v1.2.1.1.sg
- Size: 2,008,581 bytes
- Scenario: "-The Defeated III-  -Alternative mod-"
- Creator: "VIPER"
- Map: 120×120, seed 8475088
- Objects: 6671 total
- Players: 5, Units: 1186, Stacks: 269, Cities: 21, Ruins: 45
- Bags: 21, Locations: 209, Events: 605
- Items: 1349, Landmarks: 1265, Roads: 975, Crystals: 48
- Stack Templates: 173, Plans: 0, Mountains: 0, Diplomacy: 1
- Map Blocks: 450
