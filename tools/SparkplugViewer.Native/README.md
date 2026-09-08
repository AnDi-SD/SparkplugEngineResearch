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

ABI version 1 is declared in [ViewerBridge.h](ViewerBridge.h). Node/palette
ordinals are host indices, not original pointers or file IDs. C++ exceptions
are caught at the ABI; buffer lengths are checked. Managed SafeHandle owners
use a serialized native gate. Bound scenes retain their own animation owner;
disposing/replacing the managed clip cannot leave dangling evaluator pointers.
Seeking resets authored PRS before applying the requested time. Failed binding
leaves the old binding intact.

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
