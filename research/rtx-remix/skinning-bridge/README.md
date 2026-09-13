# Mesh skinning bridge correction — 2026-09-13

This package corrects our editable x86→x64 Remix bridge. It does not change game
code, installed DLLs, the stock x64 renderer, or its skinning formulas. The final
manifest records the completed CPU tests and matching pair build. It does not
claim that native game characters or stock renderer deformation have passed.

The API's `blendWeights_count` and `blendIndices_count` are total array element
counts, normally `vertexCount*bonesPerVertex`. The previous bridge multiplied
these counts by both factors again. A three-vertex mesh with two influences and
six floats therefore read 144 bytes from a valid 24-byte weight array. The same
problem affected indices. The corrected wire copies exactly `count*4` bytes.
Rigid mesh wire is unchanged. Vertex padding is omitted: one vertex is 36 wire
bytes, not the 64-byte public structure. Bone transforms remain 48-byte affine
matrices and pass through the existing Instance extension chain.

Both sides must be updated together. The bridge version is
`remix-main-skinwire-v1`; the existing server startup comparison rejects a mixed
old/new pair. The full patch includes the previously qualified camera command
and server instance audit. The incremental patch contains this task alone and
requires the exact `before-v1` source snapshot. The immutable reference remains
at `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4` and is only read or checked.

Mesh preflight checks required pointers, count equality and 32-bit wire size
arithmetic before opening a command. Unsupported Mesh `pNext` is rejected;
previously that branch could loop indefinitely. Palette count is limited to 256,
with a required pointer when nonempty. DrawInstance preflight bounds its chain
to 64 extensions and rejects duplicate bone palettes. Input pointers must remain
readable and stable throughout the call. Weight values, normalization and index
values are renderer/input semantics and are not silently rewritten here.

The server validates the entire mesh or palette payload before invoking the
existing owning decoder. Invalid mesh payloads consume their following handle
UID and skip creation. Invalid or duplicate palette payloads are drained with
the Instance extension chain and skip its renderer call. Other commands retain
their existing behavior. This is not a general hostile-IPC or OOM recovery layer.

Owning decode uses typed arrays with matching `delete[]`, releases the surfaces
array itself, and initializes all partial ownership before further allocation.
The client serializer temporary also uses its matching array delete. These
changes preserve the original caller ownership of serialized inputs.

## Reproduce

From the repository root, with the existing editable source and local
MSVC/Meson/Ninja prerequisites:

```powershell
./research/rtx-remix/skinning-bridge/Test-Skinning.ps1 -Name cpu-v1
./research/rtx-remix/direct-camera-bridge/Test-Camera.ps1 -Name skinwire-camera-v1
./research/rtx-remix/skinning-bridge/Build-Pair.ps1 -Name pair-v2 -TestName cpu-v1
python -B research/rtx-remix/skinning-bridge/Capture-Manifest.py --tests cpu-v1 --build pair-v2
```

Names must be fresh; failures and previous binaries remain in evidence folders.
Each CPU helper has a 30-second limit and creates no graphics device. Test and
build scripts preserve source hashes; the package is not an installer.

## Source contract and next live test

Primary source paths under the immutable reference:

- `public/include/remix/remix_c.h:328`: mesh weights and indices, total counts;
  `:408`: 256-bone palette, three rows of four floats per transform.
- `src/dxvk/rtx_render/rtx_remix_api.cpp:124`: 3x4 row storage converts to
  column-vector affine math; translation is `matrix[0..2][3]`.
- `src/dxvk/shaders/rtx/pass/skinning.h:67`: first `B-1` weights are explicit,
  last is `1-sum`; bone matrices transform positions and normals, then normalize
  the summed normal. Ordinary rigid matrices are the first fixture's scope.
- `src/dxvk/rtx_render/rtx_scene_manager.cpp:1102`: palette plus skinned geometry
  enables deformation; `:1969` uses a separate Instance object-to-world matrix.
- `src/dxvk/rtx_render/rtx_remix_api.cpp:1022`: local renderer source fixes weight
  stride at 4 despite public tuples of `B` floats; index stride is also 4. This is
  a source concern for `B>1`/`B>4`, not a proven defect of stock binary F7C31082…
  whose exact revision 68edea01 is not available in the local reference.

After CPU qualification, the minimal isolated live fixture is a small triangle
with `B=1`, two rigid bones, two poses and an independent Instance translation,
beside a CPU-baked geometric reference. Then use `B=2` with different weights
on every vertex. Category flags 0 use Main; no special skin-enable flag was found
on this API path. Up to 256 vertices may use the renderer's CPU skinning branch,
which calls the same shared deformation routine; that does not prove the GPU
compute branch. Palette indices must be below the palette size and 256.

The existing bridge forwards BoneTransforms. GPU instancing and renderer output
readback APIs are not forwarded by this client. CreateMesh/DrawInstance success
still means client enqueue; server audit records actual renderer API returns,
which still do not establish GPU completion or correct deformation. The next
live fixture must compare the visible deformation, not only successful calls.

CPU evidence `cpu-v1`: each writer passed 5813 checks and each opposite-architecture
reader passed 5988, with 11 byte-identical records totaling 15332 bytes. Tests cover
rigid and B1..5 meshes, multiple surfaces, guarded exact arrays, malformed packets,
palettes 0/1/2/256 and an allocation-failure sweep with zero remaining ownership.
Camera regression `skinwire-camera-v1` also passed 214/219 checks per writer/reader.
The first pair-build attempt stopped while regenerating `version.h` because its
wrapper lacked the workspace Meson `PYTHONPATH`; the corrected wrapper retains
that failed attempt as `pair-v1` and builds into the fresh `pair-v2` evidence folder.

The final pair is `pair-v2/d3d9.dll` (SHA256 `4BB7BB4F…28BE783E`) and
`pair-v2/NvRemixBridge.exe` (`A99FEF75…5618E4F`). The manifest contains full hashes.
A bounded independent read-only review found no blockers in the new preflight,
malformed-command drain and partial decode ownership. No live test is attributed
to this package; subsequent renderer evidence belongs to its own isolated run.
