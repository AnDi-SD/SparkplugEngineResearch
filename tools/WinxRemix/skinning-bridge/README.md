# Mesh skinning bridge

Our x86-to-x64 Remix transport. Build inputs and patch hashes: [manifest.json](manifest.json).

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


## Build and tests

Use `Test-Skinning.ps1`, `Build-Pair.ps1` and the camera regression in the adjacent component. `Capture-Manifest.py` stores a fresh detailed report in the ignored private evidence directory; it does not replace the public build manifest. CPU checks do not establish GPU deformation or compatibility with every game resource.
