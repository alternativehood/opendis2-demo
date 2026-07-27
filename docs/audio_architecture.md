# Audio architecture

```text
WDB/WDT readers
    ↓
audio asset resolver
    ↓
real AudioService backend
    ↓
semantic audio events from screens/presentation
```

`Application` owns one audio service. Screens will emit semantic cues and will never know raw
WDB names. Music, ambience, SFX, UI, and voice use separate buses.

Audio updates in real time: global animation pause does not pause music or ambience. Animation-bound
SFX will later be triggered by timeline markers. Playback backend and format decoding are deferred.

Future research inputs (not supported yet):

- `AudioRgn.wdb`
- `Battle.wdb`
- `Battle.wdt`
- `Capital.wdb`
- `Midgard.wdb`

## Debug source inspection

`DebugSoundCatalog` is a tooling-only exception: it lazily inspects original WDB archives for the
Audio Preview panel. Its raw archive names do not define gameplay cue identifiers and do not enter
`AudioCue` or screen code.
