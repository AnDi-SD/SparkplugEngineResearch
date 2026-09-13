# Changelog

## Next

- Core получил overload для repack уже декодированных изображений и отдельное
  явное разрешение представимых NPOT-размеров. GUI SMOTextureTool не включает эту
  возможность; API используется SmoImporter, который дополнительно пересчитывает
  внешний FFPS object catalog и проверяет готовый SMO.
- Диагностика повторного parse теперь показывает ожидаемое и найденное число
  текстур вместе с offset/размером найденных блоков.

## 2.1.0 — 2026-08-14

### Fixed

- Corrected `0x32E3`/`0x43E3` parsing: `+0x3C` is the required zero
  `spDataBlockSerializer` marker and the BGRA pixel payload starts at `+0x3D`.
- Corrected the serialized `0x32E3`/`0x43E3` dimension mirror at `+0x38` to
  store `height << 8` instead of repeating the width.

### Added

- Strict serializer-marker validation for `0x32E3`/`0x43E3` blocks.
- First/last-pixel, byte-identical round-trip, malformed-marker and rectangular
  `128x64` resize regressions for the corrected BGRA layout.

### Verified

- Confirmed the corrected fixed-size full-BGRA/Alpha control file through the
  native game loader; resize/repack remains research-only and disabled in the GUI.

## 2.0.1 — 2026-08-13

- Standardized workspace packaging: a clean release root, a framework-dependent
  single-file application, and the shared Microsoft .NET 8 Desktop Runtime bootstrapper.

## 2.0.0

### Added

- Cross-platform Avalonia desktop interface.
- HD texture replacement with a safe `4096×4096` mode and an experimental
  `16384×16384` limit.
- RGBA, RGB, Alpha, checkerboard, and model-coloring preview modes.
- Material graph reconstruction for `spMaterialData`, passes, layers, texture
  states, and linked `spModel`/`spMeshData`.
- Vertex diffuse color preview for grayscale `D3DTOP_MODULATE` textures.
- Channel statistics, content classification, and UV-conflict diagnostics.
- Regression tests for 14 sample SMO files and 170 textures.
- Detailed reverse-engineering notes in `docs/SMO_FORMAT.md`.

### Fixed

- Correct 32-bit nested sizes for `0x32E3`/`0x43E3` textures.
- Correct unaligned 32-bit sizes and dimensions for BGRA `0x29E3` textures.
- Repacking beyond the former 2048×2048 header boundary.
- Preservation of unknown container tails during texture growth.
- Layout clipping and scrolling issues in the texture list.

### Changed

- Core SMO logic is separated from the graphical interface.
- Generated SMO files are parsed and validated before they are saved.
- Grayscale resources are no longer guessed to be shadow maps; actual shadow
  volumes are treated as a separate geometry resource hierarchy.

## 1.0.0

- Original Windows Forms texture extraction and replacement utility.
