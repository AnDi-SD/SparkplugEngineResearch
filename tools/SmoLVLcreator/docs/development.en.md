# SmoLVLcreator architecture

[Русский](development.md)

The user interface works with the editor document. The document uses the shared SMO reader, scene builder, importer, exporter, and editing components.

| Directory | Responsibility |
| --- | --- |
| [SmoLVLcreator.Core](../SmoLVLcreator.Core/) | Objects, placements, commands, undo and redo |
| [SmoLVLcreator.Gui](../SmoLVLcreator.Gui/) | Application window and interaction |
| [SmoLVLcreator.Viewport.Wpf](../SmoLVLcreator.Viewport.Wpf/) | 3D viewport overlays and gizmos |
| [SmoLVLcreator.ProjectTool](../SmoLVLcreator.ProjectTool/) | Commands for the project format |

The [shared core architecture](../../shared-core.md) explains the common sources, native ABI, and managed layer boundaries. The shared components own format reading and game behavior.

## Projects and background operations

`SmoProjectArchive` reads and writes `.smolvlproj`. `SmoProjectSerializer` builds the game SMO using the object catalog, original data, and operation journal. `SmoProjectSession` owns undo and redo transactions. See the [ProjectTool documentation](../SmoLVLcreator.ProjectTool/README.en.md) for the archive layout and commands.

When an external model is added, a worker reads a snapshot of the prepared model with the selected materials and textures. It returns a plan for adding object branches and the associated data to the project. Isolated SMO saves and project builds write and verify their output. Game validation launches the original game in a separate process and tracks the loading stages.

## Language resources

| UTF-8 catalogs | Purpose |
| --- | --- |
| [`Strings.ru.json`](../SmoLVLcreator.Core/Localization/Strings.ru.json) and [`Strings.en.json`](../SmoLVLcreator.Core/Localization/Strings.en.json) | Editor interface, ProjectTool, and worker messages |
| [`Strings.ru.json`](../../../release/bootstrapper/Strings.ru.json) and [`Strings.en.json`](../../../release/bootstrapper/Strings.en.json) in `release/bootstrapper` | Launcher messages, including the .NET Desktop Runtime check |

[`LevelText`](../SmoLVLcreator.Core/Localization/LevelText.cs) reads resources from `SmoLVLcreator.Core`. The English catalog defines the WPF key set and supplies fallback text for missing translations. Use `DynamicResource Loc.<key>` in XAML and `LevelText.Get(key, arguments)` in C#. Store a history label as a key and arguments with `LevelText.Label`, then render it with `LevelText.Display`. This lets earlier commands display their labels in the selected language.

The editor stores `Auto`, `Russian`, or `English` in the `Language` field of `%LOCALAPPDATA%\SparkplugEngineResearch\SmoLVLcreator\settings.json`. In `Auto` mode, a Russian Windows display language selects the Russian catalog. Other display languages select English. The SmoLVLcreator launcher reads the same preference. At startup, ProjectTool uses the Windows display language for help and localized messages. Isolated editor workers receive the selected language through `SMOLVLCREATOR_WORKER_LANGUAGE`. ProjectTool parses numeric arguments using invariant culture. Localized message arguments are formatted using the current number locale.

## Updating translations

1. Update both catalogs together. Keep keys and argument indexes stable, for example `{0}` and `{1:N0}`. Arguments can appear in a different order to fit the sentence. Keep CLI commands, paths, identifiers, and extensions in their original spelling.
2. To add a language, create a complete UTF-8 `Strings.<code>.json` without a BOM and register it as an embedded resource in the [Core project](../SmoLVLcreator.Core/SmoLVLcreator.Core.csproj) under `SmoLVLcreator.Strings.<code>.json`. In [`LevelText`](../SmoLVLcreator.Core/Localization/LevelText.cs), register a stable preference identifier, catalog selection, and a rule for the system language. Add an item with that identifier to the editor window's `LanguageSelector`.
3. For a new launcher language, provide a complete catalog in `release/bootstrapper`, register it in the [package build](../../../release/Build-Releases.ps1), and add SmoLVLcreator language selection in [`RuntimeBootstrap.cs`](../../../release/bootstrapper/RuntimeBootstrap.cs).
4. Verify key and parameter parity, startup with the system language, explicit switching, labels of earlier undo and redo commands, worker messages, and the width of long labels. Update the Russian and English documentation together.

[Build and contribution guide](../../../CONTRIBUTING.md) · [Home](../README.en.md) · [User guide](guide.en.md)
