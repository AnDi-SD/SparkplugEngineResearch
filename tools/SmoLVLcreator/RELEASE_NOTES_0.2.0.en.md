# SmoLVLcreator 0.2.0

[Русский](RELEASE_NOTES_0.2.0.md) · [Changelog](CHANGELOG.en.md) · [Getting started](README.en.md)

Version 0.2.0 of the Winx Club PC level editor was released on November 22, 2026.

## Language and controls

- The language switch is in the upper-right corner. In System mode, the editor
  uses Russian when Windows has a Russian display language and English for
  other display languages. You can also select either language explicitly.
- The choice is saved in settings and takes effect immediately. The open
  project, selection, unsaved edits and undo/redo history are preserved when
  you switch languages.
- In the free perspective view, hold the right mouse button and scroll to
  adjust flight speed for the current session. Scrolling alone controls zoom.

## Import, build and export

- Adding or replacing a rigid model uses the same preparation dialog for
  OBJ/MTL, GLB and FBX. You can match images by name or assign an image to a
  material manually. Manual assignments take precedence. You can select an
  MTL library for OBJ.
- The background build receives a snapshot of the prepared model, including
  the selected materials and textures. After accepting the snapshot, you can
  move the source OBJ, MTL or image without assigning it again.
- Full model replacement transfers collision links and explicit unlinks to
  the corresponding new instances in one undo/redo operation. The editable
  project uses version 1 of the `.smolvlproj` format.
- Collision hull generation uses coordinates relative to the model center.
  This fixes hull generation for small objects far from the scene origin.
- For compatible transparent PC BGRA textures, the shared writer uses alpha
  when generating lower mip levels. It preserves the original pixels of the
  base level.
- For the verified material configuration that multiplies vertex color by a
  single texture, the shared exporter represents a uniform vertex color as a
  material factor in GLB and FBX. Blender applies that factor together with
  the texture in GLB.
  FBX stores the material color, and Blender connects the texture directly on
  import.

## Known limitations

- The selection export warning count includes diagnostics from the entire
  level. Geometry export skips particle systems. Models with no explicit
  material can depend on the game renderer's state.
- Animated texture support is in development. Preview, editing, and export
  of frames with their timing still need verification.
- Independently moving baked geometry and replacing skinned models while
  retaining their original SAN animations remain planned.

## Verification scope

The OBJ/MTL, GLB and FBX import and replacement checks covered saving and strict
readback. The BSP and transparent-texture test containers and the stock Alfea03
level loaded in the original PC game. Comparing frames from the same camera
in Blender and the game, and checking player contact with added collision
geometry, remain open. The original LVL-05 cases require the cherry blossom
and piano models.
