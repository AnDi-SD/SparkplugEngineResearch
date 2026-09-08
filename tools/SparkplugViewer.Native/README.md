# Sparkplug Viewer native bridge

Application C ABI over reconstructed C++ classes in `Sparkplug/`. This directory
contains interop, ownership and input guards. It does not define new classes
claimed to be original engine classes and does not load the game executable.

The call path is `spSerializerManager` → `spAnimationSerializer` →
`spAnimation`/`spAnimTrack` → `spTransformTrackEval` → `spNodeController` →
`spNode::UpdateWorldForAnalysis`. Skin matrices call
`spSkin::ComposePaletteMatrixForAnalysis`. No second SAN sampler is implemented
inside the bridge. Existing C# SMO decoders provide node PRS, child references
and palette inputs; the complete portable SMO loader has unsupported collision
dependencies. OpenGL/WPF remain the display adapters.

Build on Windows with the Visual Studio C++ and CMake workloads:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/SparkplugViewer.Native/Build-Native.ps1 -Configuration Release -RunChecks
```

The x64 DLL with a static CRT is built into `artifacts/native/viewer/Release/`.
The script uses two compiler processes and incremental Ninja builds. It repairs
an observed localized MSVC include-prefix encoding mismatch so changed headers
are tracked. `-Fresh` regenerates CMake configuration. `-RunChecks` runs five
selected existing engine suites; no game corpus is scanned.

ABI version 2 is declared in [ViewerBridge.h](ViewerBridge.h). Node/palette
ordinals are host indices, not original pointers or file IDs. C++ exceptions
are caught at the ABI; buffer lengths are checked. Managed SafeHandle owners
use a serialized native gate. Bound scenes retain their own animation owner;
disposing/replacing the managed clip cannot leave dangling evaluator pointers.
Seeking resets authored PRS before applying the requested time. Failed binding
leaves the old binding intact.

ABI 2 also exposes channel metadata, key times, tool-created linear keys,
`spDataBlockSerializer` headers, local `spNode` matrices and static `spSkin`
palette composition. `BorrowedInput` is an application memory adapter over a
pinned span; it avoids copying or allocating a buffer for each header. Encoded
terminator IDs remain available for the raw field inspector; engine semantics
still treat a terminator as an invalid field ID. Core references the interop
project; the interop project has no dependency on Core. The SMO scene adapter
lives in Core, retaining its existing `SmoViewer.Sparkplug` namespace.

The duplicate C# SAN decoder/sampler has been removed. All Core consumers now
need this DLL, including the editor, importer, exporter and TextureTool.
Their development builds require the complete workspace and Visual Studio
C++/CMake, or an explicitly supplied prebuilt bridge. The current
[Viewer audit](../../docs/research/viewer-core-audit-2026-09-08.md) lists the
remaining C# SMO readers and unresolved Viewer heuristics. This does not mean
the whole Viewer has been migrated, and no new release package was made.

Limits: 16,384 nodes, depth 256, one-object PC SAN up to 64 MiB and existing
serializer/key bounds. Events, actor scheduling/blending, camera-dependent
billboards and native GPU startup are outside this bridge. Exact-name binding
and choosing the first conflicting channel are explicit Viewer policies.

The host opts into `AllowMissingWithOwnedKeys` for SAN files such as Bloom's
`blwalk.san` that omit pool-size declarations. Owned vectors remain bounded;
present declarations (including zero) are still enforced. Other serializer
callers keep the strict default. This is a host allocation policy, not a claim
to reproduce the original allocator's missing-declaration behavior.

The shared C++ quaternion interpolation now follows the observed PC x87 spill
schedule. All 64 Icy tracks sampled at 1 second match the original PC probe
exactly; see the [integration dossier](../../docs/research/tool-viewer-sparkplug-core-2026-09-08.md)
for the selected validation and remaining adapters.
