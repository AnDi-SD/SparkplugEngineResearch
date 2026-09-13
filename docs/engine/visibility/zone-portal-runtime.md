# PC `spZonePortal` / `spZonePortalNode`: геометрия и обход зон

## Identity, имена и исходники

| Class | RTTI/base | Primary |
| --- | --- | --- |
| `spZonePortal` | 6523AC37→Named44DE07FD | 6EBB10,8 slots |
| `spZonePortalNode` | ABB5AB2C→Node695C0F65 | 6EBB44,14 slots |

Records7614B8/761518, base records7555F8/75DD88. Factory slots13B245C/13B1918.
Node ctor481550/13B2CD4→4AD550; 4AD584 writes its table and zerosB8/BC/C0
after original Node ctor421BA0. **481550 is ctor, not Open setter**.

Diagnostic strings preserve actual names **GetDestinationZone, IsOpen,
GetPolygonVertex, GetPolygonVertexCount, GetZonePortal**. They are used in
new partial portable classes under `Sparkplug/Code/Sparkplug/`.
Unknown setters/plane methods use explicit `ForAnalysis` names.
Host signatures/containers are not binary ABI. Declaration/implementation paths
are inferred: limited ASCII search in this PC build finds none of the four
`.h/.cpp` names. **Only `spZonePortalNodeSerializer.cpp` TU path survives**;
`spZonePortalSerializer` class name is present but its `.cpp` path is not.

## Layout и ownership

Portal38: Named14; borrowed destinationZone14; vertexCount18; directly owned
XYZ array1C; Open20 defaults1, padding21..23 untouched; plane24..30 untouched
until Init; visibility stamp34 defaults0. Source `planeKnown=false` represents
the uninitialized gap rather than pretending native ctor creates zero plane.

Portal dtor4810A0/13B31A4→13DF7C0 frees **only its polygon** then inherited
Named413090; does not release destinationZone. Deleting481110 uses flag1.
Primary copy slot0C is **Named413120**; clone4813F0 actual factory/register/
inherited copy yields fresh destination/null polygon/Open1/uninitialized plane/
stamp0, not a full portal clone.

PortalNodeC4: NodeB4 plus16-byte vectorB4, allocator untouched; beginB8/endBC/
capacityC0. **Borrowed** entries, duplicates retained. Native append481930/
13B2E98→407D70 calls vector push4818C0→13C5420→4815C0; no retain/no dedup.
Raw append even accepts null; reader44E670 rejects null **before** append
at44E6FE. Known resources use two entries; the container itself is not limited to two.

Dtor4814F0 frees vector allocation then Node422150; deleting4815A0.
Clone481870 copies Node only, preserving local transform but not portal vector.
World slot30 **421420** and Enabled slot34 **421640(enabled,recursive)** are
inherited. Actual world translation(80,90,100) leaves polygon/plane unchanged;
Enabled does not change Open. Game-side Open controls remain to be identified.

## Polygon initialization и shared plane math

Reader44DDE0 field1 reads UInt32count/XYZs, calls **481130(count,points)**
at44DF0A, then frees temporary array. Native Init resolves13B1D00→**4E8490**:
free old1C; count18=count; allocate count×12; copy all input vertices; call
**471420(this+24,p0,p1,p2)**. No local planarity/convexity/short-count validation
was found in the executed/restored control flow. Counts3/4/5 executed.
Host source bounds3..4096/finite input/self-alias safety are explicitly additional.
Native invalid short count/alloc failure/alias-to-old-buffer were not executed.

471420 computes float-stored differences `(p1-p0),(p2-p0)`, calls426A40 for
cross product (components stored float), then41D2D0 normalizer. Norm uses x87
extended intermediates; length **>.001f** normalizes, otherwise zero normal.
Plane is `(normal, dot(normal,p0))`, distance convention dot(n,x)−d.
Degenerate first triangle returns zero plane; no rejection. Later nonplanar or
concave vertices are copied unchanged and do not alter this first-three plane.
Reversed winding reverses plane. No convex hull or triangulation is invented.

Portable double-intermediate finite slice was compared against actual471420:
**1024/1024 fields /256 cases**, all1024 bit-exact on these inputs; acceptance
tolerance2e-6 is documented, not a universal x87 equivalence promise.

## Original whole Scene through a portal

Actual45EC70→46D270→**46C4D0** executes with real native cameras, portal Init,
Node registrations, root/Zone objects and real clip/plane math. Device calls
remain recording boundaries; no image/game correctness claim.

Portal loop46C9F2 reads root68..6C:

1. Open20false skips **before stamp**.
2. stamp34=current skips; otherwise marks portal **before** geometric tests.
3. Signed plane distance uses cameraWorld74 plus incoming offset. Positive
   side beyond epsilon skips; negative side follows clipped-polygon route.
4. Copy polygon into scratch54, iterate enabled current planes and call
   **491AA0(plane,0,alternateScratch,.001f)**, swap scratch54/80 each plane.
5. Empty clipped polygon prevents destination traversal. Nonempty polygon
   creates new plane-stack entry: retains the **first two previous planes**
   (camera-origin near and far), then one471420 side plane per clipped edge.
6. Recurse through every borrowed root of destinationZoneB8..BC; restore
   plane-stack depth afterwards. Portal plane itself is **not** new near plane.

Actual tests:

- camera at origin /quad z10 /remote sphere(0,0,20,.25): visible;
- remote side sphere(8,0,20,.25): outside aperture;
- remote sphere(0,0,5,.25): still inside cone, even before portal plane;
- closed portal: no destination visit, old portal stamp unchanged;
- reversed polygon: no destination visit, portal stamp refreshed;
- quad shiftedx100: fully clipped, no destination visit;
- quad shiftedx6: partially clipped, original intersection code retains the
  visible side sphere and creates four side planes;
- synthetic same-facing A→B→A cycle: visits rootsA,B,A, then current portal
  stamp terminates the cycle; remote support emitted only once. Next frame
  repeats correctly. This synthetic pair is not the stock reversed pair.

For centered quad, side normals contain .9805806875 and .1961161345, near/far
stay unchanged. Root7C itself is **not** an early recursion gate: a root can
be revisited via another portal. Per-portal and per-support stamps differ.

## CRT boundary and still-open near-plane branch

First partial-edge intersection lazily constructs polygon scratch **7629B0**
and calls CRT atexit60DB63→60DB3D→imported **__dllonexit60DFAC**, registering
**6D7F90**. Initial isolated test stopped at unmapped import, not a geometry
failure. Fresh tests use a strictly checked record-only imported boundary:
callback6D7F90, begin84D7B4,end84D7B0; return callback pointer. Actual registered
cleanup is executed before existing native polygon-pool shutdown6D7FA0.
No unknown geometry helper is replaced; no cap is raised or failed call resumed.

## Reproduction and remaining work

Native **63/63** (16 lifetime+25 geometry+9 gates+8 aperture+5 cycle), static
**32/32**, source **19/19**, plane differential **1024/1024**, **CTest19/19**.
ABI38/C4 checked at compile time. Source remains partial: no Scene traversal,
Open game controls, serializer or debug drawing. Existing application code,
assets, original EXE, PS2 and Git publication unchanged.
