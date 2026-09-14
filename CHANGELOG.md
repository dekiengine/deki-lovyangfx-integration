# Changelog

Notable changes to `deki-lovyangfx-integration`. Engine and editor changes are in the
[engine changelog](https://github.com/dekiengine/deki-engine/blob/master/CHANGELOG.md).

A package's `minEngine` names the engine version it needs. Before 1.0 a
breaking change bumps the minor across the editor, the engine and every
package together, so a package with no changes of its own is still released
alongside one that has them.

## 0.15.0

### Changed
- `Present` and `PresentRegions` take a `Deki::ColorFormat` instead of an `int`
  whose numbering lived in a comment and as literals in every display.
- Allocates through the engine, DMA buffers included, naming both the region
  and the DMA requirement rather than inferring either. The framebuffer's
  placement comes from the board's own config.

### Fixed
- A PSRAM cache flush sized an `RGB565A8` buffer at 2 bytes per pixel instead
  of 3. Sizes now come from `Deki::FrameBufferBytes`.
