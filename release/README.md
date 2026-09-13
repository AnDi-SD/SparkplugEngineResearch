# Release layout

All user-facing applications are packaged through `Build-Releases.ps1` and
`release-manifest.json`. Ad-hoc copies of `bin/` or `publish/` directories are
not release artifacts.

All tool sources and future releases are hosted in `AnDi-SD/SparkplugEngineResearch`.
SmoViewer and SMOTextureTool are ordinary directories under `tools/`.
The existing Viewer 0.5.0 and TextureTool 2.1.0 releases retain their original
download files under `smoviewer-v0.5.0` and `smotexturetool-v2.1.0`.
Those tags contain the release-era workspace with both tools' sources included;
the current development versions remain on `main`.

The package contract is:

```text
Product-version-win-x64/
|-- Product.exe            # runtime bootstrapper and stable entry point
|-- release.json           # bootstrapper configuration
|-- app/
|   `-- Product.exe        # framework-dependent single-file application
|-- docs/
|-- native/                # shared native FBX bridge, SDK DLL and Autodesk license
`-- tools/                 # only for suites
    |-- SmoExporter/
    |   |-- SmoExporter.Gui.exe
    |   `-- docs/
    |-- SmoImporter/
    |   |-- SmoImporter.Gui.exe
    |   `-- docs/
    `-- WinxHairPatcher/
        |-- WinxHairPatcher.Gui.exe
        `-- docs/
```

Only the product executable and configuration/manifest files may be present in
the package root. Documentation belongs in `docs/`; suite applications belong
in `tools/<name>/`. The Autodesk FBX SDK bridge is stored once in `native/`, so a
Viewer suite does not duplicate the same executable, DLL and license under both
Importer and Exporter. Explicit `companionFiles` may accompany the application
inside `app/`; Viewer 0.7 uses `SparkplugViewerNative.dll` there. Undeclared
dependencies, symbols and satellite assemblies must not be loose files.

The application payload is a clean, framework-dependent, single-file Windows
x64 publish. The root executable is a small .NET Framework-based bootstrapper,
which can run on supported Windows 10/11 systems before modern .NET is present.
It checks for Microsoft .NET 8 Desktop Runtime (x64) and, after one explicit
confirmation, downloads the current official Microsoft installer, verifies its
Authenticode signer, runs it with `/install /quiet /norestart`, and starts the
application from `app/`. A normal Windows UAC confirmation can still be shown.
WPF trimming remains disabled.

The runtime installer itself is deliberately not stored in the archive: the
stable Microsoft URL always supplies a serviced .NET 8 patch, while signature
verification prevents an untrusted executable from being launched.

Build every package:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./release/Build-Releases.ps1
```

Build selected packages without ZIP compression:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
    "& ./release/Build-Releases.ps1 -Product @('SmoViewer','WinxHairPatcher') -NoArchive"
```

The script cleans each distinct application project before publishing. This is required:
switching between self-contained and framework-dependent publishing can leave
stale intermediate runtime assets in `obj/`. Packaging fails unless publish
produces exactly the executable and its declared companion DLLs. It also validates the root allowlist and
rejects exact duplicate files inside a package. When the same project occurs in
a suite and as a standalone product, its verified payload and companion DLLs are reused
within that invocation. The temporary cache is hash-checked and removed after
the build; later invocations publish from clean intermediates again.

Use `-PackageSource <local-directory>` for an offline build from an available
NuGet package cache, for example the `.nuget/packages` directory in the user's
profile. Shared compilation is disabled and MSBuild concurrency is
limited to two workers. Package generation never publishes to GitHub.

Manifest schema 2 accepts both source-path strings and `{ "source": "...",
"name": "README.md" }` document entries. The mapped form installs a concise
package guide while the full research README stays in the source repository.
Release notes and guide links remain local to the package's `docs/` directory.
Prepared candidates are written to a separate output directory; their notes
identify them as unpublished.

`-NativeFbxManifest <json>` reuses an already validated native bridge only when
the manifest has kind `native-fbx-reuse-manifest`, schema 1, and exact SHA-256
entries for `Build-Native.ps1`, `CMakeLists.txt`, every file under `src/`, and all
three runtime files. Each `files` entry has repository-relative `path` and
`sha256`. Changed or missing inputs stop packaging. Without this option the
bridge is built normally.

Only selected products that declare `nativeFbx` require the bridge. An unknown
product is rejected before any native build starts.

Check an unpacked candidate against the current source manifest:

```powershell
python release/audit_packages.py artifacts/release/candidates-20260908-1900 `
    --report local-data/results/package-audit.json
```

The audit checks exact file inventories, document copies and local links,
Windows file versions, native runtime hashes and suite/standalone payload
identity. Add `--archives` to stream-compare every ZIP member as well. It does
not launch applications or verify remote links.

`research/smoke_release_packages.py <packages> <new-local-output>` checks
startup on a private Windows desktop using copies of the packages. Launches
are sequential, limited to 512 MiB of aggregate process commit and four owned
processes each; the startup polling deadline is 25 seconds, with bounded
window-message calls and three seconds for graceful close. The installed
.NET 8 Desktop Runtime is required. The probe never confirms an installer
dialog and does not exercise editing, export or rendering workflows.

Only products listed in `release-manifest.json` receive user-facing packages.
`SmoNativeValidator.Core`, its CLI harness, and its tests remain available in
source and may be embedded by Viewer or Importer, but they are intentionally not
packaged or published as a standalone product.
