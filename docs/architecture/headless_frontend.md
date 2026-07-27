# HeadlessFrontend

A no-SDL frontend for debugging and CI validation. Sends commands to
`GameSession` and prints a structured world summary.

## Location

- Header: `src/opendis2/headless_frontend.hpp`
- Implementation: `src/opendis2/headless_frontend.cpp`
- Part of: `opendis2` executable target
- Dependencies: `d2game::GameSession` (no SDL dependency)

## Behavior

1. Sends `InspectWorld` to `GameSession`
2. Prints:
   - Scenario path, name/id
   - Map dimensions
   - Terrain availability
   - Semantic object count
   - Runtime indexed object count
   - Build diagnostics count (warnings + errors)
   - Current game mode ("Adventure")
3. Sends `NoOp` to exercise the command path
4. Sends `Quit` to cleanly exit
5. Returns 0 on success, non-zero on failure

## Usage

```
opendis2 adventure --scenario <path> --headless
```

## Constraints

- No SDL dependency — pure stdout output
- Requires only `d2game` + `d2runtime` + `d2scenario`
- Suitable for CI

## Testing

Tested in `tests/unit/test_headless_frontend.cpp` using a synthetic
`GameSession` (no real `.sg` file needed).

## Build Dependency Note

`HeadlessFrontend` itself has no SDL dependency, but the entire adventure
path (including headless) is currently only built when `D2_ENABLE_ENGINE=ON`.
This is transitional debt — the headless path is engine-gated because it
shares the `opendis2` executable target structure with the graphical path.
A future target split would allow CI testing without the engine flag.
