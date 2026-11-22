# SmoLVLcreator

[Русский](README.md)

An editor for supported PC SMO level scenes.

## Launch

1. Extract the entire archive into one directory. The SMO bridge `SparkplugViewerNative.dll` is in `app/`, and the FBX files are in `native/`.
2. Run `SmoLVLcreator.exe` from the package root. If Microsoft .NET 8 Desktop Runtime (x64) is missing, the launcher offers to install it.
3. From the **File** menu, choose **Open level or project…** and select an `.smo` or `.smolvlproj` file. The language selector is in the upper-right corner. **System** selects Russian for a Russian Windows display language and English for any other display language. A manual choice is saved.

## Editing a scene

- Left-click to select an object. `Ctrl+left click` changes the selection, `Esc` clears it, and `F` frames the active instance. In free-flight perspective, `WASD` and `Q/E` move the camera, and holding the right mouse button while scrolling adjusts flight speed.
- To add a rigid model, click **+ Model…** in the catalog. To replace an existing model, select its card and use the replacement command. The supported formats are OBJ/MTL, GLB, and FBX.
- In the import dialog, assign an image to an individual material row or match images by name. A manual assignment takes priority. You can choose an MTL for an OBJ. Review any warnings, and check position and scale in the fitting window when replacing a model.
- Use **Save project** to write your edits to an `.smolvlproj` file. Use `Ctrl+Z` and `Ctrl+Y` to undo and redo during the session. **Build SMO…** produces a separate game file. Keep the original SMO and open the built file to check the result.

Editing is available for supported objects, fields, and collision layouts. SPT/SPL level scripts are edited with separate tools.

## Limitations

The selection export warning count can include warnings about other objects
in the level. Animated texture support is in development.
See [known limitations in 0.2.0](RELEASE_NOTES_0.2.0.en.md#known-limitations).

## Documents

[Changelog](CHANGELOG.en.md) · [0.2.0 release notes](RELEASE_NOTES_0.2.0.en.md) · [Historical 0.1.1 notes](RELEASE_NOTES_0.1.1.en.md)

The program is distributed under the Mozilla Public License 2.0. The `docs/` directory contains `LICENSE.md`, `NOTICE.md`, and `ACKNOWLEDGEMENTS.md`. The Autodesk FBX SDK license is next to the FBX bridge under `native/`.
