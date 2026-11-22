# SmoLVLcreator

[Русский](README.md)

An editor for Sparkplug PC SMO scenes. It edits object placements, adds and replaces models, manages collision geometry, and exports the scene or the selection. The [release notes](RELEASE_NOTES_0.2.0.en.md) describe the changes in **0.2.0**.

## Getting started

1. Choose **File → Open level or project…** and select an `.smo` or `.smolvlproj` file. The asset catalog, level hierarchy, and inspector will show the scene data.
2. Left-click to select objects. `Ctrl+left click` changes the selection, `Esc` clears it, and `F` frames the active instance. Right-drag rotates the camera. `Shift+right drag` pans. In free-flight perspective, `WASD` and `Q/E` move the camera, and holding the right mouse button while scrolling adjusts flight speed.
3. Choose **+ Model…** in the catalog to add a rigid model from OBJ/MTL, GLB, or FBX. To replace an existing model, select it and use its replacement command. The import dialog offers manual image assignment, matching images by name, and an alternate MTL for an OBJ.
4. Use **Save project** to keep your edits and **Build SMO…** to produce the game file separately. Undo and Redo let you review edits during the current session.

The language selector is in the upper-right corner. **System** selects Russian for a Russian Windows display language and English for any other display language. You can override it manually. The choice is saved and applied to the open window.

## Running from source

```powershell
dotnet run --project tools/SmoLVLcreator/SmoLVLcreator.Gui/SmoLVLcreator.Gui.csproj -- path/to/level.smo
```

You need the full repository and the [shared build dependencies](../../CONTRIBUTING.md). The editor uses components shared with SmoImporter, SmoExporter, and SmoViewer.Editing. See the [package guide](PACKAGE_README.en.md) if you are starting from an archive.

## Scope

The editor handles supported transform and collision layouts within SMO files. Several instances can share a model resource. Whether an individual instance is editable depends on how it refers to that resource in the scene. SPT/SPL gameplay scripts require separate work.

The selection export warning count can include warnings from the entire level. Geometry export skips particle systems. Animated texture support is in development. See [known limitations](docs/guide.en.md#known-limitations).

[Editor and project guide](docs/guide.en.md) · [Architecture and localization](docs/development.en.md) · [ProjectTool](SmoLVLcreator.ProjectTool/README.en.md) · [Changelog](CHANGELOG.en.md)
