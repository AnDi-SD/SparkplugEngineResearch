# SmoLVLcreator 0.1.0

[Русский](RELEASE_NOTES_0.1.0.md) · [Changelog](CHANGELOG.en.md)

SmoLVLcreator 0.1.0 is the first test release of the Winx Club PC level editor.
It covers the basic workflow from opening an SMO and inspecting the scene to
editing placements and collisions, adding or replacing a rigid model or
texture, saving the project, building a new SMO and passing that file to the
built-in game validation tool.

## Highlights

- The shared SmoViewer GPU renderer provides perspective and orthographic
  views, Lit, Unlit and Wireframe modes, a grid, selection bounds, picking and
  object focus.
- One selection is shared across the viewport, level tree, inspector and
  bottom catalog. Composite models and individual mesh parts can be selected.
- Move, Rotate and Scale support WORLD and LOCAL axes, numeric Position,
  Rotation and Scale, multi-selection, snapping and undo/redo.
- Existing collisions are visible and editable. The editor can create a
  bounded convex mesh around an object and safely remove a collision branch
  together with its registry binding.
- Existing and external models can be placed with a button or by dragging them
  into the scene. New instances reference shared geometry and texture resources.
- A simple or composite model can be replaced with a new resource graph.
  The final SMO excludes the old branches, and Undo can restore them before
  saving.
- RGB and RGBA texture replacement offers a choice for how alpha is handled.
- Multiple selected objects can be exported to GLB, FBX or OBJ.

## Project and build

The editor imports the opened SMO into a version 1 `.smolvlproj` project and
leaves the source file intact.

- `project.json` holds the normalized catalog, stable IDs and operation journal.
- `data.bin` holds the original data section, preserving all known and unknown
  fields.
- `assets/<guid>.bin` holds new object forests and exact payload replacements.

`Ctrl+S` saves the project. **File → Build SMO…** builds a new FFPS container.
The builder copies unchanged ranges, applies edits by stable ID, recalculates
sizes and the catalog, then opens the result with a strict parser. A full SMO
build runs on demand.

An isolated worker imports external models and returns the new object forests
and blobs to the project process. Import can proceed without creating a full
intermediate SMO on disk. History is limited to 256 transactions and cleans up
unused blobs to bound memory use during long editing sessions.

## Release checks

- `SmoLVLcreator.CoreTests` passed 1,676 assertions.
- The suite includes 1,500 randomized transform operations with periodic
  history compaction, project save and reopen, and strict SMO builds.
- The `Alfea02_old.smo` plus `shrek.glb` project check passed 25 assertions. It
  covered importing two textured mesh parts, placing the model again, full
  replacement, undo/redo, archive reopen and checking for stale references.
- The collision check on `Alfea02_old.smo` passed 12 assertions.
- The final stress run with seed `82744` performed 1,000 mixed operations with
  two archive, build and reopen checkpoints. Peak working set was 363.9 MiB,
  and repeated build and re-import produced identical SHA-256 hashes.
- The original game accepted the stress-test SMO. FFPS magic and version
  `0x26` were verified, a native resource was returned, and the game survived
  the observation window.
- Release builds of the GUI and ProjectTool completed successfully.

The automated contextual launch provided a structural check for the final
stress candidate. It confirmed successful loading and stability during the
observation window. Check materials, physics and gameplay for the specific
edits in the original game.

## Known limits

- External model addition supports rigid geometry. Adding a skinned model to a
  level graph requires verified runtime skeleton and skin semantics.
- Manually linking a visual object to a collision stores editor metadata
  in the project. The link participates in undo/redo and persists on reopen.
- OBJ cannot represent varying vertex alpha losslessly. Use GLB or FBX for
  these models.
- Strict readback verifies the structure of the file. Use a copy of the level
  with **Build SMO…** for in-game checks of materials, physics and behavior.

[Getting started](README.en.md)
