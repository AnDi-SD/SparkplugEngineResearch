# SmoFbxBridge

Native x64 bridge between the .NET SMO tools and Autodesk FBX SDK 2020.3.10.
It reads and writes a private versioned binary scene payload; GLB, Blender and
Python are not involved.

Build on Windows with Visual Studio C++ tools and the Autodesk FBX SDK:

```powershell
.\Build-Native.ps1
```

The build places `SmoFbxBridge.exe` and `libfbxsdk.dll` in
`build\bin\Release`.

The import payload remains protocol 1. The export writer emits protocol 4;
the bridge also accepts export protocol 3. Export v4 separates transport mesh
ordinals from real SMO object indices and carries container/member provenance.
Rigid variants of one physical mesh share an FBX mesh attribute only after an
exact geometry comparison; skinned occurrences have separate deformer context.

`SmoFbxBridge inspect-export scene.fbx report.json` reads a written FBX through
the SDK and reports mesh nodes, source identities, material RGB, world matrices
and deformer counts. This diagnostic command is not a game loader or renderer.
