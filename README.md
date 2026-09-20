# Deki LovyanGFX Integration

Docs: https://dekiengine.github.io/deki-lovyangfx-integration/ (components and properties, generated from the code)

LovyanGFX display and touch panel driver integration for the Deki Engine.

Part of [Deki Engine](https://github.com/dekiengine/deki-engine).

## Namespace

Types live in `DekiLovyanGfx`. Scene files store the qualified name, and so does code:

```cpp
using namespace DekiLovyanGfx;
obj->AddComponent<SomeComponent>();
```

Scenes saved before 0.16.0 used bare names and still load; saving writes the current one.

## Install

Package Manager in the Deki Editor, or `DekiEditor --packages-add deki-lovyangfx-integration <project>`.

## Dependencies

| Dependency | Type |
|---|---|
| `deki-i2c` | Deki package |
| `LovyanGFX` (1.2.29) | External (FreeBSD License) |

## Partial present (untested on hardware)

`LovyanGFXDisplay::PresentRegions` collapses the changed rectangles to row
bands and pushes each through two small DMA-capable staging bands (converted
to RGB565 and byte-swapped for big-endian panels there). The same staging path
now serves full presents of a direct or passthrough buffer with `swapBytes`
on, so the engine's framebuffer is no longer byte-swapped in place. Written
in September 2026 against the API the full path already used and compile-checked
only: run a project with the Rendering setting `dirtyTileTracking` on and
report what you see.

## License

Apache 2.0. See [LICENSE](LICENSE).

Third-party licenses are listed in [NOTICE](NOTICE).
