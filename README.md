# Deki LovyanGFX Integration

Documentation: https://dekiengine.github.io/deki-lovyangfx-integration/ (components and properties, generated from the code)

LovyanGFX display and touch panel driver integration for the Deki Engine.

Part of the [Deki Engine](https://github.com/dekiengine/deki-engine) package ecosystem.

## Installation

Install via the Package Manager inside the Deki Editor.

## Dependencies

| Dependency | Type |
|---|---|
| `LovyanGFX` (1.2.19) | External (FreeBSD License) |

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

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.

Third-party licenses are listed in [NOTICE](NOTICE).
