# Release layout

All user-facing applications are packaged through `Build-Releases.ps1` and
`release-manifest.json`. Ad-hoc copies of `bin/` or `publish/` directories are
not release artifacts.

The package contract is:

```text
Product-version-win-x64/
|-- Product.exe            # runtime bootstrapper and stable entry point
|-- release.json           # bootstrapper configuration
|-- app/
|   `-- Product.exe        # framework-dependent single-file application
|-- docs/
`-- tools/                 # only for suites
    |-- SmoExporter/
    |   |-- SmoExporter.Gui.exe
    |   `-- docs/
    `-- SmoImporter/
        |-- SmoImporter.Gui.exe
        `-- docs/
```

Only the product executable and configuration/manifest files may be present in
the package root. Documentation belongs in `docs/`; suite applications belong
in `tools/<name>/`. Runtime, managed dependencies, native libraries, symbols,
and satellite assemblies must not be loose files in a release.

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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./release/Build-Releases.ps1 `
    -Product SmoViewer,WinxHairPatcher -NoArchive
```

The script always cleans each project before publishing. This is required:
switching between self-contained and framework-dependent publishing can leave
stale intermediate runtime assets in `obj/`. Packaging fails unless publish
produces exactly one executable. It also validates the root allowlist and
rejects exact duplicate files inside a package.

Only products listed in `release-manifest.json` receive user-facing packages.
`SmoNativeValidator.Core`, its CLI harness, and its tests remain available in
source and may be embedded by Viewer or Importer, but they are intentionally not
packaged or published as a standalone product.
