# SmoLVLcreator user guide

[Русский](guide.md) · [README](../README.en.md) · [Package instructions](../PACKAGE_README.en.md)

## Open a level or project

Run `SmoLVLcreator.exe` from the extracted package. To run the editor from
source, use this command at the repository root.

```powershell
dotnet run --project tools/SmoLVLcreator/SmoLVLcreator.Gui/SmoLVLcreator.Gui.csproj
```

Choose **File → Open level or project…** and select an SMO level or a
`.smolvlproj` archive. Editing commands become available after the file opens.
The editor fills the catalog, outliner, and inspector from the
workspace and displays the scene with the shared SmoViewer OpenGL renderer.
You can also pass the path to an SMO file as the first command-line argument.

The **Interface language** selector in the upper-right corner offers **System**,
**Русский**, and **English**. **System** uses Russian when the Windows display
language is Russian and English for other display languages. While the editor
is idle, choose a language to apply it immediately. The editor retains the open
project, selection, and undo history. The preference and quick export settings
are saved in
`%LOCALAPPDATA%\SparkplugEngineResearch\SmoLVLcreator\settings.json`.
Windows dialogs and technical messages from shared libraries can use their own language.

## Navigate the scene and select objects

| Input | Action |
| --- | --- |
| Left click | Select an object or a complete composite model. The catalog, outliner, inspector, and highlighted mesh parts follow the selection. |
| `Alt` + left click | Select one mesh part of a composite model. |
| `Ctrl` + left click | Add or remove an object from the selection. |
| `Esc` | Clear the selection or cancel a live transform drag. |
| `F` | Frame the active placement. |
| Right mouse button | Turn the camera. With a focused placement, orbit its center. In free view, turn in place. |
| `Shift` + right mouse button | Pan the view. |
| Wheel | Zoom in orbit and orthographic views. In free perspective view, move forward or back. |
| Right mouse button + wheel | Adjust flight speed in free perspective view for this editor session. The status bar shows the speed relative to the default. |
| `WASD` and `Q`/`E` | Fly. A movement key leaves focused orbit. Hold `Shift` for faster movement or `Ctrl` for slower movement. |

The viewport toolbar switches **Perspective** and **Orthographic** projection
and cycles **Lit**, **Unlit**, and **Wireframe**. **Grid** and **Bounds** toggle
the floor grid and selection bounds. These display controls preserve the loaded
scene and camera position. **View** controls the catalog, outliner, and inspector.
**Help → Controls and shortcuts…** lists the main controls in the app.

### Asset catalog

| Tab | Contents |
| --- | --- |
| **MODELS** | Decoded mesh resources, including models brought into the project. |
| **PLACEMENTS** | Scene instances with their current coordinates. |
| **TEXTURES** | Decodable `spTextureData` resources and the models with confirmed bindings to them. |
| **UNKNOWN** | Unregistered object classes and parser, mesh, and texture diagnostics. |

The search box filters the active tab. Use `Ctrl` or `Shift` to extend the
selection on model and placement cards. The catalog and viewport share the
selection used by the gizmos and export. Double-click a card to show the active
placement. The context menu can show the resource in the scene or outliner and
export the selection. Drag a model card into the viewport or click **Place**
to add an instance at the camera target.

The shared `SmoViewer.Rendering.Wpf` component renders and caches thumbnails
for visible model and placement cards. It uses the prepared `SmoSceneMesh` and
preserves authored vertex colors on untextured meshes. Texture cards display
reduced previews of decoded BGRA data. Scrolling gives newly visible cards
priority, and cached previews appear immediately.

## Edit placements and collisions

Select **Move**, **Rotate**, or **Scale** and drag the gizmo. **WORLD** aligns
the handles to world axes, and **LOCAL** aligns them to the selected object.
Rotation uses the common selection pivot. The center scale handle scales
uniformly. Hold `Ctrl` while dragging to snap movement to 1 unit, rotation to
15°, or scale to steps of 0.1. Use the inspector to enter position, Euler
rotation in degrees, and separate X, Y, and Z scale values. A numeric position
edit sets the active object to the specified position and preserves the relative
offsets of the other selected objects.

A drag or numeric edit is one history entry. `Esc` and a change of tool or
coordinate system cancel an active drag. **Edit** provides **Undo** and **Redo**
(`Ctrl+Z`, `Ctrl+Y` or `Ctrl+Shift+Z`). `Ctrl+C`, `Ctrl+V`, and `Ctrl+D`
copy, paste, and duplicate placements that share their mesh and texture
resources.

Visual objects and `spCollisionInfo` shapes are separate entities. The editor
can link a visual object and a collision shape when their geometry matches with
high confidence in the same `spPartitionNode`. **BINDINGS** in the inspector
shows the linked object, its index, and the match confidence. **MOVE LINKED** is
on by default. It moves the model and collision in one undoable operation. Turn
it off to move them independently. **Link selected** and **Unlink** let you
adjust those associations manually.

**Create collision** previews a convex hull around the selected model. The
dialog shows the source model and proposed hull and allows you to adjust the
triangle budget and padding. Generated collision shapes can be rebuilt, moved
independently, linked, or deleted. The **Delete** button and `Delete` also
remove supported original collision shapes. A subsequent SMO build removes the
field-7 physics registry binding and its inline `spCollisionInfo`/`spMeshBV`
branch. Undo restores the pending edit before the build.

## Add a model, replace a model, or replace a texture

1. To add a rigid model, click **+ Model…** in the bottom catalog and choose an
   OBJ, GLB, or FBX file. To replace a model, choose **Replace…** in its card’s
   context menu. FBX uses the shared `SmoFbxBridge.exe` and its runtime.
2. In **Prepare imported model**, use **Choose image…** on a material row to
   assign an exact image. **Match images by name…** replaces the previous
   automatic selection. Manual assignments take priority. When two matches are
   ambiguous, use the row control to pick the intended image. The dialog shows
   unassigned images and provides reset controls for the row and the automatic
   selection.
3. For OBJ, **Choose MTL…** loads the selected material library. Changing the
   library resets manual texture assignments. **Use original** restores the
   library declared by the OBJ. GLB and FBX use their own material data.
4. Choose **Continue**. Review the import diagnostics before committing. On
   add, the project creates the first placement at the camera target. This
   add path applies a scale factor of 100 to the new GLB placement. On replace,
   **Fit Model** previews the original and imported geometry. Adjust position,
   rotation, and scale, then choose **Confirm Replacement**.

Validation rejects invalid topology, inconsistent optional vertex data,
invalid material or texture references, non-finite positions, and skinned
geometry before modifying the project journal. The editor can display a warning
for a missing image or a non-fatal decoder repair. It shows a compatibility
warning when mapping a donor MASK material to the level's alpha path. Both
preview and build use the material and image assignments accepted in the dialog.

Full project replacement can change the number of mesh parts. The fit sets a
reference transform, and each replacement instance keeps its transform relative
to that reference. The command serializes a complete new mesh, material, and
texture graph with alpha state. It retires the old branches and preserves shared
descendants that remain in use. Manual collision associations follow the
replacement instances. When the branches cannot be separated safely from other
models, the editor reports the constraint.

To replace an existing image, select a texture card in **TEXTURES** and choose
**Replace…**. In the alpha prompt, **Yes** uses RGB and alpha from the new image,
**No** replaces RGB and keeps the original SMO alpha, and **Cancel** leaves the
texture untouched. The editor fits the image to the target texture size for
supported replacements. Compatible new PC BGRA textures used by transparent
materials can include alpha-aware mip levels. The base level keeps its authored
pixels. Supported full mip chains can be regenerated on replacement.

## Save the project and build a game file

**File → Save project** (`Ctrl+S`) and **Save project as…** (`Ctrl+Shift+S`)
write a `.smolvlproj` archive. An opened SMO becomes the immutable source of
the project. **File → Build SMO…** produces a separate game file from confirmed
transform, collision, resource, and payload operations in the project journal.
The serializer materializes omitted identity rotation or unit-scale fields in
the confirmed order when needed, preserves unknown fields byte for byte, and
rejects transforms whose field layout remains unconfirmed.

The project build writes a temporary file beside the destination, reads it
back before installation, and verifies the installed result by SHA-256. An
atomic replace backs up a prior destination as a timestamped `.bak`.
**Tools → Test level in game…** checks the built project in an isolated run of
original Winx Club with the shared contextual validator. Use a separate game-file
path when building a level for testing.

Each final SMO build writes `<output>.SmoLVLcreator.log` next to the output.
It records the snapshot, object and byte counts, worker limit, output hash,
backup path, outcome, and detailed exception information. The error dialog
shows the expected log path.

The status bar reports snapshot, load, journal verification, streaming build,
and completion with the current container size. **Cancel** stops the owned
worker before an incomplete temporary file can be installed. A Windows Job
Object caps the build worker and its process tree at 1536 MiB. This is a maximum
usage limit. The memory estimate may warn before a build and lets you choose
whether to continue. The worker receives an exact archive snapshot and reports
progress and the result through atomic JSON files.

### Project storage and shared resources

- `project.json` records normalized FFPS header, catalog, direct-field
  metadata, and stable-ID operations. `data.bin` holds the immutable source
  payload, including unknown data. Newly serialized branches are immutable
  `assets/<guid>.bin` entries. The archive stores the source as structured
  components without a copied `base.smo`.
- The copy-on-write layout updates field spans, materializes confirmed optional
  fields, and recalculates nested sizes and catalog offsets. A project with
  zero edits reproduces the source SHA-256.
- Project import and generated collision creation reserve stable IDs and
  attach complete serialized branches at confirmed owner boundaries. The
  model worker returns additive forest plans, blobs, and catalog metadata.
  The adapter rejects unintended changes to imported objects. The separate
  non-project SMO save path can pack large models in one-part workers and
  passes texture IDs between parts so shared textures are embedded once.
- Level imports use a 128 MiB decoded-BGRA budget. The preparation step reduces
  oversized texture sets while retaining material assignments and alpha. The
  same prepared images are shared by the viewport and fitting preview. If a
  worker reaches a memory or time limit, the previous destination remains intact.
- Additional placements use static shells that reference the existing mesh
  and its material and textures. The owner’s transform is editable through
  project overlays on the immutable asset. A resource with a physical
  `spStaticRenderObject` supplies its own placement shell template. A shared
  resource reuses a shell for that exact resource. Definitions precede references
  within the confirmed owner and partition.
- If the physical placement is deleted and generated placements still use its
  mesh, the first survivor becomes the owner. References are tracked by field
  type and occurrence. Baked geometry in `spPartitionRenderable` requires a
  separate vertex-copy and rebase operation to produce an independent placement.
- Manual collision associations live in the archive manifest. They survive
  reopen, follow promoted object IDs, participate in Undo/Redo, and are removed
  when either object is deleted. The built SMO stores the collision geometry
  and transforms as separate engine objects.
- A stable-ID resource redirect can update fields of imported objects,
  added branches, and generated placement shells. At build time, the serializer
  omits inline resources no longer referenced. Replacement and redirect
  participate in Undo/Redo while the imported payload remains immutable.

See the [SMO field mutation contract](../../../docs/formats/smo-field-mutation.md)
and [ProjectTool commands](../SmoLVLcreator.ProjectTool/README.en.md).

## Inspect SMO data and run a reproducible test

The inspector’s **RAW DATA** section is read-only. Shared
`SmoObjectFieldReader` and `SmoSchemaRegistry` code provide the table, logical
and physical offsets, direct SBOO fields, a bounded hex preview, and confirmed
property values for placements, resources, and collision objects. For recovered
`spFog`, `spLightData`, navigation, BSP, and particle classes, the most-derived
serializer section shows established engine names and payload layouts. Earlier
inherited sections can reuse the same field numbers and remain raw. Unknown
fields stay visible and are preserved byte for byte.

The stress runner executes random transforms, shared and external placements,
complete composite replacements, texture edits, removals, and collision
operations in a separate process. Archive checkpoints exercise repeated build,
reopen, and import. The source remains intact. A managed heap limit and
supervisor watchdog bound the run, and the resulting SMO, project, and JSON
metrics are retained under `artifacts/stress`.

```powershell
dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.CoreTests/SmoLVLcreator.CoreTests.csproj `
  -c Release -- `
  --stress-project-save `
  "local-data/pc-pristine/Media/Levels/Alfea/Alfea02.smo" `
  "path/to/rigid-models" `
  250 260830
```

Replace the input paths with a local level and donor-model folder. Optional
arguments select the output folder, child private-memory limit in MiB, and
supervisor timeout in seconds. The same seed reproduces the operation sequence.

## Export the selection

Select complete visual entities in the viewport with `Ctrl` + left click.
Export writes the entire selection to one scene using the current transforms
and all selected mesh parts, including pending edits. A collision-only
selection has no visual geometry to export.

- **File → Export** (`Ctrl+E`) uses the configured directory and format. The
  name derives from the level and selection.
- **File → Export as…** (`Ctrl+Shift+E`) opens the Windows save dialog. The
  file-type list selects GLB, FBX, or OBJ.
- **Tools → Settings…** sets the quick export folder and format.

The shared `SmoExporter.Core` handles format writing, materials, textures,
coordinate systems, and instances. GLB and OBJ use managed writers. FBX uses
`SmoFbxBridge.exe`. For materials whose confirmed render state multiplies
vertex color by a single texture, GLB and FBX retain the uniform vertex tint in
the material. GLB can combine that tint with the texture in Blender. FBX retains
the tint in the file, and Blender's FBX importer can connect the image directly
to Base Color. Use GLB or FBX when the export must retain varying per-vertex
alpha, which OBJ lacks.

## Known limitations

- The selection export warning count can include warnings about other objects
  in the level. The **Unknown** catalog tab lists level diagnostics. Select an
  entry to see its text in the inspector's **Diagnostics** section. GLB also
  stores the messages in `extras.warnings`.
- Geometry export skips particle systems. A source model with no explicit
  material may depend on the game renderer's current or default state.
  Visual parity in an external editor requires separate verification in
  these cases.
- Animated texture support is in development. Textures that change images
  over time need a verified workflow for previewing, editing, and exporting
  frames with their timing.
- Moving geometry baked into a partition independently requires converting
  it to a separate instance. The current replacement workflow handles rigid
  models. Replacing a skinned model while retaining its original SAN animation
  remains planned.
- Matching Alfea03 frames from the same camera in the game and Blender, and
  checking player contact with added collision geometry in the game, remain
  open.

## Interface labels used in the guides

| English | Русский |
| --- | --- |
| Interface language / System | Язык интерфейса / Язык системы |
| Open level or project… | Открыть уровень или проект… |
| Save project / Build SMO… | Сохранить проект / Собрать SMO… |
| Asset catalog / Level outliner / Inspector | Каталог ресурсов / Структура уровня / Инспектор |
| + Model… / Replace… | + Модель… / Заменить… |
| Prepare imported model / Fit Model | Подготовка модели к импорту / Подгонка модели |
| Place / MOVE LINKED | Разместить / Перемещать вместе |
| Export / Export as… | Экспорт / Экспортировать как… |

For translation resources and the maintenance workflow, see
[Development](development.en.md).

## Build the package

From the repository root, run the shared release script.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File ./release/Build-Releases.ps1 -Product SmoLVLcreator
```

The build writes to `artifacts/release/current`. The package contains the
`SmoLVLcreator.exe` entry point, the single-file application, documentation, and
the shared FBX runtime. It supports Windows 10/11 x64. The launcher can offer
to install the official .NET 8 Desktop Runtime x64 when needed.
