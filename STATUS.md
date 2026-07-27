# OpenDis2 frozen snapshot status

## Repository status

OpenDis2 is a frozen technical proof-of-concept snapshot. It is not under active development, has no release roadmap, and carries no support or maintenance commitment.

## Demonstrated systems

- Direct reading of original Disciples II resource archives and related metadata.
- SG scenario parsing and runtime world construction.
- Adventure terrain rendering and implemented map objects.
- Adventure stacks, unit presentation, banners, selection, cursors, overlays, and `StackInfo`.
- Adventure animation and movement prototypes.
- Screen stack and runtime screen configuration architecture.
- Battle rules/core prototype and battle development tools.
- Sound and music playback prototype.
- Extraction, inspection, validation, and reverse-engineering tools.
- Automated tests and architecture guardrails.
- Native development build support.
- Windows x64 Docker cross-build tooling.

## Partial or prototype systems

- Adventure interaction flows that depend on incomplete stack, movement, or animation state.
- Battle presentation paths that still reflect prototype engine behavior.
- Audio and music paths that are implemented as a prototype rather than a full runtime.
- Configuration and screen flows that rely on authored runtime layout data.
- Tooling paths that inspect, extract, or validate data without providing a complete replacement game.

## Not provided

- A complete replacement for Disciples II.
- Original game assets.
- Campaign-complete gameplay.
- A compatibility guarantee.
- An installer.
- A supported end-user release.
- Maintenance or support commitments.

## Reproducing the demonstration

Use the build and test commands documented in `README.md`. The authored demo scenario is `testdata/test_map.sg`, and it requires a legally obtained local installation of Disciples II: Rise of the Elves.
