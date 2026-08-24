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
