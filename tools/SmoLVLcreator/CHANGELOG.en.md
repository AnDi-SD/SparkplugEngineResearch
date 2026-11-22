# Changelog

[Русский](CHANGELOG.md) · [Getting started](README.en.md)

## 0.2.0 — 2026-11-22

- Added an interface language switch. The choice takes effect immediately and
  is saved. In System mode, a Russian Windows display language selects
  Russian and other display languages select English.
- Adding or replacing an OBJ/MTL, GLB or FBX model uses a shared preparation
  dialog. Assigning an image manually takes priority over matching by name.
  OBJ files support selecting an MTL library.
- Builds use a snapshot of the prepared model with its selected materials and
  textures. Full replacement transfers collision links to the corresponding
  instances in one undo/redo operation.
- Fixed collision hull generation for small objects far from the scene origin.
- Lower mip levels for compatible transparent PC BGRA textures are generated
  using alpha. The base level preserves the original pixels.
- GLB and FBX represent uniform vertex colors as material colors when the
  game material uses verified texture modulation. Blender combines the color
  and texture in GLB. Its FBX importer connects the texture directly.
- In the free perspective view, holding the right mouse button while scrolling
  adjusts flight speed for the current session.
- Added paired Russian and English documentation.

[0.2.0 release notes](RELEASE_NOTES_0.2.0.en.md)

## 0.1.1 — candidate, 2026-09-08

- Updated the shared decoder, renderer and Importer dependencies, including
  the structured PC texture writer. The version distinguishes the test build
  from the published 0.1.0 release.
- Prepared release notes and the guide included in the archive.

[0.1.1 release notes](RELEASE_NOTES_0.1.1.en.md)

## Unreleased changes — 2026-08-29

- The final Gate 7 passed. Gardenia, Alfea and a large Domino level survived
  mixed project stress tests. Repeated build and re-import produced identical
  SHA-256 hashes, and all seven original-game checks reached an active scene.
  After 1,000 edits, the final Alfea result passed a separate DirectInput
  movement and camera probe.
- Project texture replacement matches `TextureData` to the physical block by
  file offset and offers only verified writable fixed-size slots. Unknown
  texture payloads remain read-only and are preserved byte for byte.
- The stress-test supervisor gained a configurable timeout. Failure tests for
  memory and time limits and the save-cancellation regression confirmed cleanup
  of partial `.smo` and `.tmp` files and preservation of the expected backup
  behavior. The Core suite grew to 1,872 assertions.
- The Gate 1 writer safety check passed. With zero edits, the Alfea project
  reproduced the source SHA-256 after archiving, reopening and rebuilding.
  The 1,864 Core assertions covered layout, inline sizes, reachability, shared
  ownership and determinism. All five native writer checks reached an active
  scene. The shared SmoImporter dependency verifies output before installing it.
- The Gate 6 collision check passed. The real-project check grew to 22
  assertions and the full Core suite to 1,864. The tests covered adding, moving,
  linking, unlinking, regeneration and deletion, the registry, inline removal,
  exact triangle order and nonempty `wxFaceData`.
- Newly generated collision branches explicitly use production-default Group 2
  independently of the nearest spatial template. The generated `spMeshBV`
  leaves unknown per-face surface metadata unspecified.
- All six original-game collision checks passed. A DirectInput gameplay probe
  confirmed movement down a corridor, stopping at a wall and camera collision
  in the resulting SMO.
- The Gate 3/5 rigid import tests passed on the production OBJ, GLB and FBX
  paths. Those formats passed 31, 46 and 57 project assertions respectively.
  Every resulting SMO passed structural checks, and all five original-game
  checks on Alfea02 reached the active scene.
- OBJ geometry without normals receives deterministic smoothed normals.
  The writer checks handedness and winding and preserves distinct UV0 and UV1
  channels.
- Meshes with more than 65,535 vertices are deterministically split within their
  original parts, preserving material indices, normals, UV0, UV1, vertex colors
  and triangle order.
- FBX alpha mode accounts for the actual transparency of the texture. A
  project check compares the written BGRA payload to the source image byte for
  byte.
- All project workflows now use Sparkplug's transposed-basis convention for
  `spStaticRenderObject.InvTransform`, including scaled placements.
- Added safe migration for older version 1 `.smolvlproj` files and verification
  of `Transform/InvTransform` pairs before building.
- Native Gate 2 verified static and node XYZ TRS, LOCAL-to-WORLD composition,
  shared placements with a single mesh resource, addition and removal, and
  reopening the result.

## 0.1.0 — 2026-08-27

[0.1.0 release notes](RELEASE_NOTES_0.1.0.en.md)

- Created a modular SMO level editor using the shared parsing, scene, rendering,
  import and export libraries.
- Added GPU level preview, free and focused camera modes, picking,
  multi-selection, and WORLD and LOCAL gizmos for Move, Rotate and Scale.
- Added the inspector, level tree and catalogs for models, placements, textures
  and unknown objects, with on-demand GPU previews.
- Composite models are reconstructed from the SMO structure and verified
  references.
- Added, cloned, moved and deleted placements while preserving shared mesh,
  material and texture resources.
- Added viewing, moving, creating, regenerating and deleting collisions, with
  the choice to edit independently or together with the linked visual object.
- Added selection export to GLB, FBX and OBJ, rigid model import, external
  MTL and texture lookup, and full replacement of composite models with
  materials, textures and alpha.
- Version 1 of `.smolvlproj` became the main editing format. The original
  `data.bin` is preserved, and edits and new object forests are stored in an
  operation journal and separate immutable assets.
- Project import returns object-forest plans and asset blobs from an isolated
  worker, avoiding a full intermediate SMO on disk.
- RGB and RGBA texture replacement now uses the project journal and supports
  undo/redo, project reopen and final builds.
- Added a shared schema-backed field editor for verified transform, collision
  and resource operations. Unknown fields are preserved byte for byte and
  available through the shared RAW API.
- SMO builds stream data when requested. The build recalculates sizes, inline
  boundaries and the catalog and strictly parses the output before installing
  it.
- History is limited to 256 transactions. Eviction and `CompactHistory` remove
  unused asset blobs to bound long-session memory use.
- Isolated import workers received their own memory ceiling and GC conservation
  mode. The GUI reports stages and considers more than available memory when
  deciding whether to start a save.
- Fixed selection of the user's saved `WinxClub.exe` in the validation dialog
  and passed the configured contextual start level.
- The main architecture and CLI adopted the names `SmoProject` and
  `SmoLVLcreator.ProjectTool`, replacing the prior Experimental names.
- The automated Core suite passed 1,676 checks, including 1,500 randomized
  journal edits, archive/reopen/build, RGBA textures and history limits.
- The real-world check using `Alfea02_old.smo` and the two-part `shrek.glb`
  passed 25 structural assertions for project import, replacement, undo/redo
  and the final build.
- Copy, paste and duplication, transforms and deletion of original or added
  placements, collision editing, creation and deletion, manual visual-to-
  collision links, and transfer of external resource ownership when deleting
  its first placement all moved to the project workflow.
- Final `.smolvlproj` to SMO builds moved back to a separate process with a
  1,536 MiB Windows Job Object cap. Progress, cancellation, the adjacent
  `.SmoLVLcreator.log`, strict validation of the temporary file, post-install
  SHA-256 verification and a timestamped `.bak` on overwrite were restored.
- After structural edits, preview is built directly from project blocks in
  memory, avoiding an intermediate SMO on disk.
- Direct full-repack import remains an internal compatibility reference. The
  GUI and stress tests use the same production batched worker.
- The full randomized project stress test passed 1,000 operations. An 80 MiB
  level passed with a hard cap of 1,024 MiB. Repeated build and re-import
  produced identical SHA-256 hashes. Separate checks covered OBJ with MTL
  and OBJ without MTL using nearby textures.
- Reference placements are serialized after the mesh resource definition and
  within the same verified owner and partition. For inline meshes, the
  lightweight copy uses the model's own branch, replaces inline geometry
  with a reference and recalculates shell sizes from the inside out.
- Baked `spPartitionRenderable` geometry without a static placement is left in
  its own structure. It stays outside placement operations pending copy and
  vertex rebase support.
- The native stress candidate with 250 mixed edits was accepted by the game
  loader and survived a full 70-second contextual interval without EOF or
  `0x004BC4B9`.
- Fixed transfer of physical mesh ownership. When the original placement is
  removed, the first remaining reference placement takes the definition and
  the others keep their references. The exact type and occurrence index
  distinguish repeated reference fields.
- Manual visual-to-collision links moved from window state into
  `.smolvlproj`. They survive reopen, are part of undo/redo, and are stored as
  editor metadata.
- The final stress run with seed `82744` passed 1,000 mixed operations, two
  intermediate archive/build/reopen checkpoints and deterministic rebuild and
  re-import. The final SHA-256 was
  `2E95388BC62E29A29F7ADE974DF9C09EEE2AE12283E6185648FC59CB943B4884`.
  Peak worker working set was 356.9 MiB. The same SMO reached active level state
  28 in the original game and passed a movement and camera probe.
