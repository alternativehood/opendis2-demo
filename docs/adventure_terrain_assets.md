# Adventure Terrain Assets

This step adds runtime discovery for adventure terrain assets only.

`Imgs/Ground.ff` provides base ground texture PNG records such as `HU_00.PNG` and `WA_00.PNG`.

`Imgs/GrBorder.ff` provides 64x32 border and transition PNG records such as `NE_01_00.PNG` and
`WA_01_00.PNG`. It is loaded through the raw PNG container path.

`Imgs/IsoTerrn.ff` provides OPT-indexed terrain overlays, including `ROAD`, `FOG`, forest families
such as `HUF`/`DWF`/`ELF`, and mountain families such as `MOM`/`MOMDW`/`MOMNE`.

`Imgs/IsoStill.ff` and `Imgs/IsoCmon.ff` provide OPT-indexed static and common adventure assets.

This does not decode scenario terrain values and does not render tile maps. A later step will connect
`ScenarioTemplate` terrain values to terrain descriptors and render commands.
