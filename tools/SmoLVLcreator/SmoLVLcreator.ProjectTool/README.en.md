# SmoLVLcreator.ProjectTool

[Русский](README.md)

This command-line tool uses the same `SmoProject` implementation as
SmoLVLcreator for reproducible imports, SMO builds, and regression checks.

## `.smolvlproj` format version 1

A version 1 project is a ZIP container.

- `project.json` contains the format version, source SMO SHA-256, FFPS header
  words, complete normalized object catalog, stable IDs, physical ownership
  tree, direct-field metadata, and operation journal.
- `data.bin` contains the exact original SMO data section, including unknown
  fields. These bytes are the immutable project baseline.
- `assets/<guid>.bin` contains immutable serialized object branches for external
  models, collisions, and new placements, along with exact object payload
  replacements.

Building a complete SMO uses the metadata, `data.bin`, and operation data.
The copy-on-write planner applies changes to a copy of the stream, materializes
absent fields with confirmed schemas, recalculates nested sizes, inline
boundaries, and the catalog, then writes a new FFPS container. Building an
unedited project must reproduce the source SMO byte for byte.

Version 1 supports the following operations.

- Editing schema-backed fields of imported and added objects.
- Moving a shared inline resource while retaining its object ID.
- Redirecting references to another resource of the same type.
- Removing a branch while preserving shared descendants.
- Creating a placement that references existing meshes, materials, and textures.
- Adding pre-serialized object branches.
- Replacing an object payload with an exact same-size block, including RGB and
  RGBA textures.

Undo and redo history belongs to the open session and holds up to 256
transactions. Unreferenced history data is pruned automatically and through
`CompactHistory`.

## Commands

Run the examples from the repository root. Command names and argument syntax
are shared across languages. Help and localized messages use the Windows
display language. Numeric arguments use a period as the decimal separator.
Commands with `object-index` take an index in the open file's object catalog.
`remove-placement` accepts a decimal object ID or a hexadecimal ID with a
`0x` prefix.

Run ProjectTool without arguments for the complete command list.

```powershell
dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- model-info donor.glb

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- scene-mesh-info rebuilt.smo Model_

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- import source.smo level.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- build level.smolvlproj rebuilt.smo

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- roundtrip source.smo level.smolvlproj rebuilt.smo

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- placements level.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- translate level.smolvlproj 1369 1.25 -2.5 3.75 moved.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- set-vector level.smolvlproj 7 transform.scale 1.25 0.75 1.5 scaled.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- remove-branch-preserve level.smolvlproj 1369 removed-safe.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- clone-reference level.smolvlproj 1373 125 0 0 Darch_A02_copy copied.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- remove-placement copied.smolvlproj 0x000010AB restored.smolvlproj
```

The following are examples of absolute transforms. The editor inspector and
the commands use the same X, Y, and Z Euler-angle convention and
`Scale * Rotation * Translation` order.

```powershell
dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- set-placement-trs level.smolvlproj 1373 `
  100 20 -50 15 30 45 1.25 0.75 1.5 transformed.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- set-entity-trs source.smo vase09-000 `
  -5400 95 -780 20 30 40 1.2 0.8 1.4 transformed-node.smo
```

The `InvTransform` matrix follows the Sparkplug convention with a transposed
3×3 basis and a `-T*A^T` translation. When opening an older version 1 project,
the core corrects the conventional mathematical inverse of a scaled
placement if it recognizes the pair unambiguously. It rejects arbitrary
mismatched pairs.

`roundtrip` protects the source from overwrites and checks the exact SHA-256
of the output for an unedited project. A mismatch causes an error.

`model-info` reports geometry, bounds, 32-bit index requirements, normals,
UVs, vertex colors, skinning, materials, and textures before writing.
`scene-mesh-info` reports a serialized mesh, its texture binding, and the
resolved material render state.

## Verification history

The following results come from separate historical runs.

- The main Core suite passed 1,864 assertions.
- An external model run with `shrek.glb` and `Alfea02_old.smo` passed 25 assertions.
- A collision run on `Alfea02_old.smo` passed 22 assertions.
- Byte checks confirmed that `data.bin` was preserved through transform,
  collision, model, and texture operations.
- The checks covered saving and reopening the archive, building the SMO, undo
  and redo, RGBA payloads, and stale references.
- The external-model worker returned the plan and data for the new object
  branches. It used a temporary copy of the source container and built the
  intermediate result in memory to extract the branches.
- In verification stages 3 and 5, the full Core suite passed 1,856 assertions.
  The GLB, OBJ, and FBX project runs passed 31, 46, and 57 assertions
  respectively. All five native runs reached scene readiness.
- Stage 6 checked explicit collision Group 2, exact triangle order, populated
  `wxFaceData`, removal from the registry and inline branch, and gameplay
  collision. All six native runs reached scene readiness.

A stress test with seed `82744` completed 1,000 operations with two intermediate
checkpoints for archive save, build, and reopen. Rebuilding and reimporting
reproduced the SHA-256
`2E95388BC62E29A29F7ADE974DF9C09EEE2AE12283E6185648FC59CB943B4884`.
The game loaded that SMO to level 28 scene readiness, accepted DirectInput for
movement and camera control, and completed the observation period normally.

[Editor architecture and language resources](../docs/development.en.md)
