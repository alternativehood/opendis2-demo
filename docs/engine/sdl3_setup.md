# SDL3 Setup for Engine Build

SDL3 is available through vcpkg manifest mode — no manual installation or SDL3 SDK setup needed.

## Build

```bash
cp .env.example .env
# Edit .env — set VCPKG_ROOT to your vcpkg installation

make build
```

## Troubleshooting

### SDL3 not found

```
CMake Error: SDL3 not found.
```

**Solution**: Your vcpkg environment is misconfigured. Ensure `.env` has `VCPKG_ROOT` pointing to a valid vcpkg installation, then run `make build`. Do not switch to a no-engine build — fix the vcpkg setup instead.

### Link errors with SDL3

If you see undefined symbols for SDL3 functions, verify that:
1. `SDL3::SDL3` target was found during configure
2. `otool -L opendis2 | grep SDL3` shows the library path

## References

- `docs/engine/stage0_readiness.md`: Stage 0 readiness review
- `research/engine_sdl3_battle_visual_prototype_plan.md`: Full implementation plan
