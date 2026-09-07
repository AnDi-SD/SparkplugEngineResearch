# PC: queued Skin draw and mesh bounds

CP85, 2026-09-07. Pinned pristine WinxClub.exe SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Ten exact native/source captures,257 explicitly reported native assertions:
seven bounds cases35, three full queued draws222 (65 linked plus9 draw
assertions per case). Instruction/child/arena/allocation limits remain
100000/2s,30s,64KiB,32KiB. No GPU/OS forwarding or internal engine stubs.

## Bounds producer

Original RenderMesh initialization424360 calls backend4AA000, then424230,
then marks bounds valid at mesh28.424230 calls468370/468000 for the sphere
and separately scans the index stream for AABB. The helper alone preserves
the prior valid flag. Sphere extrema use every vertex from the CPU vertex
buffer; midpoint/extent reconstruction has observable intermediate float
stores. Radius is the square root of the largest float-stored squared
distance, accumulated in x87 order Z²+Y²+X².

The separate AABB scans **primitiveCount**, not indexCount, and reads uint16
words even when the original buffer format is uint32. For the three CP73
vertices (0,1,2),(7,8,9),(14,15,16), the sphere is
(7,8,9,12.12435531616211) but both AABB extrema are (0,1,2).
This missing source producer caused native queue distance194 versus source0;
restoring production bounds corrected the complete queued draw capture.

`probe_pc_mesh_bounds.py` executes actual allocation/CPU constructors and
readers45FB80/460300, actual DXMesh factory and full424230. Triangle,
five/seven vertices,32-bit indices, rounding-sensitive finite values,
zero-primitive strip (two readable indices) and point all match ten raw
float words.1933..2373 instructions,330..424 total engine-requested bytes;
actual destructors release all owners, including late ResourceManager.
The discarded zero-index fixture correctly got false from the native
file-stream zero-byte read; its failed read was not treated as a completed
load. A new zero-primitive strip input isolates the bounds loop condition.

Source DXMesh initialization computes these PC bounds before publishing
materialized buffers. Existing decoded-position analytical Mesh helper and
PS2 implementation remain independent. Out-of-range uint16 references get
a source range guard. Exact float evidence covers the supplied finite
inputs, not all x87 extended-precision/nonfinite corner cases.

## Whole queued draw

The same whole decoded owning Skin/Material/bone, real bbush.san animation,
SceneManager world update and serialized DXMesh from CP76 reach the draw.
Only the encoded material pass blend changes0→1. Actual425520 creates a
RenderNode support; camera view is an explicit prepared identity input.
Completed declaration lookup-map teardown frees scratch and retains the
mesh-owned COM declaration. Neither support nor queue takes Skin ownership.

Whole46A240 first enqueues via423FD0/454C30 without callbacks or GPU calls.
Whole454850 then calls actual4248D0, actual queued46A240, cached shader
selection, constants and4BE210 draw. External qsort receives a single
record and the original454800 comparator. A read-only instruction observer
at45489E records the genuine inner return without modifying registers.
Normal/negative device HRESULT/post-false all match queue record, ordering,
support publication, raw SAN state, world/palette/constants, material,
serialized buffers, device events and teardown. Queue returns1 and clears
count/flushing even when the genuine Skin post callback returns0; in that
case the Skin active-bone count remains1, as in the direct caller.

Read70998, render15517..15520 instructions; fixed arena peak64992 and99
engine owner generations released. Source callbacks dispatch the actual
RenderNode and Skin implementations. No first shader generation, queued
texture/fog, multi-entry full draw, live GPU, full Scene render or arbitrary
SMO graph completion is claimed by this checkpoint.

Reports:

- `local-data/results/bounded-native-runs/20260907T124923401668Z-pc-mesh-bounds.json`
- `local-data/results/bounded-native-runs/20260907T124758245923Z-pc-skin-queued-mesh-render.json`

Source: `spDXMesh.cpp`, `spMesh.h/.cpp`, `spSkinRenderTests.cpp`;
probes/comparators: `probe_pc_mesh_bounds.py`, `compare_pc_mesh_bounds.py`,
`probe_pc_skin_queued_mesh_render.py`, `compare_pc_skin_queued_mesh_render.py`.
