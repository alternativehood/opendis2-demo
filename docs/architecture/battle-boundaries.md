# Battle Viewer Boundaries

The battle viewer keeps data flow one-way:

- app shell owns SDL, resource loading, texture cache, smoke/debug tuning, and input plumbing.
- presenter accepts viewer actions and submits semantic battle events; manual viewer controls resolve to `VisualEntityId`.
- animation engine owns timelines and scene mutation; scripts describe commands, not resource loaders.
- scene/visual model exposes ID-based mutation for production behavior.
- renderer consumes `BattleRenderSnapshot` plus render options and does not mutate animation state.

Obsolete raw-index scene mutation APIs are deleted instead of kept as aliases.
