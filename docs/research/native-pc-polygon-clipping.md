# PC polygon clipping: unnamed value helper и точные пограничные правила

Checkpoint18, 6 сентября2026. Тот же pristine PC executable и bounded CPU-only
fixture100k/2sec per call,30sec child. No OS/GPU/game/assets/publication.
Это логическое продолжение [portal traversal](native-pc-zone-portal-runtime.md),
не новая произвольно названная engine class. Original value-class name/path
пока не найдены; limited ASCII search for Polygon source/method paths negative.
Denominator784/373/411/direct37 не расширяется из-за unnamed helper.

## Scratch и pool: исправленная семантика491660

Observed record **2C**: field00 untouched, identity4 fromglobal740384,
**logical vertexCount8**, circularHeadC, plane10/4floats, opaque20 untouched,
field24/28 zero. Shared pool740388 supplies **1C** nodes:
free-list link0, serial4, position8/3floats, **previous14 /next18**.

**491660(this,count)** is a logical resize, **not capacity-only reserve**.
Предыдущее analytical слово reserve в C13 уточняется: native ctor действительно
создаёт два32-vertex rings, не два пустых polygon buffers с capacity32.
This is independent of the still-unresolved manager46C0F0 plane-stack operation.

Grow appends nodes before head, reusing shared free-list entries and assigning
new global7629AC serials even to reused addresses. Shrink removes tail nodes,
preserves leading vertices/IDs, closes remaining ring. Shrink-to-zero returns
all nodes to pool but leaves head stale; next resize sees oldcount0 and clears
head before processing. Count must govern access. Native dtor491510 returns
live nodes to pool; global shutdown6D7FA0 frees actual blocks.
Actual8→3→8 reused same addresses with fresh tail serials;0→0 resets stale head.

**491A30(destination,source)** resolves13B3278→13E1E50. Self-copy immediately
returns. Otherwise resize destination to sourcecount, deep-copy XYZs in ring
order, copy **plane10 and opaque20** only. Keeps destination identity4 and
field00/24/28. This is neither whole-record memcpy nor shared vertex ownership.

## 491AA0 classification and clipping

Signature: `this=source`, arguments`(plane,keepCoplanar,destination,epsilon)`;
AL bool return. Distance is `(z*nz+y*ny)+nx*x−d`, stored double for intersections.

| Classification | Original test | Stored side |
|---|---|---:|
| negative |distance < −epsilon|1|
| positive |otherwise distance > epsilon|0|
| on-plane |otherwise, including unordered NaN|2|

No positive vertices: clear destination/false, **except** keepCoplanartrue and
also no negative vertices, which deep-copies/true. Thus all-on-plane and empty
inputs are rejected with flag0 but accepted with flag1. Positive present and
no negative: copy/true. Only mixed positive+negative reaches edge construction.

Mixed output follows input ring order:

- on-plane current vertex: append once, no intersection for this edge;
- positive current: append current;
- if next is on-plane or same classification: no intersection;
- otherwise append `current+(next-current)*distance/(distance-nextDistance)`.

No convex hull, polygon normal recomputation or all-point normalization is
added. Tests include unordered vertices/nonconvex input, zero/negative epsilon,
non-unit planes and NaN plane; they are algorithm-contract tests, not assertions
that those inputs are used in the shipped levels.

### Original repeated-first correction

After constructing output, native491DC0..491E24 repeats outputCount iterations
**without advancing EBP point pointer**. Since construction wrapped around the
ring, the point is the **first output vertex**. If its distance<=−epsilon,
subtract distance×normal, then test that same point again. NaN is not corrected.
Y/Z delta products are float-stored before addition; X stays x87 until final sum.
Do not silently replace this with an all-vertex projection pass.

Native example normal(1,0,0),d0,epsilon.001f:
input`[(-eps,0,0),(-2,1,0),(2,2,0),(-eps,3,0)]` becomes
`[(0,0,0),(0,1.5,0),(2,2,0),(-eps,3,0)]`.
The first epsilon-boundary point is snapped, the last stays unchanged.
This observed quirk is **not blamed for any existing Viewer bug**.

### Stack limit, not a guessed safe polygon size

Local classification UInt32 array atESP+2C has128 entries before distance
arrayESP+22C; double distance array has128 entries before SEH recordESP+62C.
Function duplicates element0 at index**count** for the closing edge. Therefore
input<=**127** fits these two arrays; original function has no local bound guard.
Native127 all-positive points were executed safely;128 was **not executed**.
Portable analytical helper rejects>127 before destination mutation. This is a
host safety guard, not a claim that the original executable rejects oversized data.

## In-place versus out-of-place metadata

Mixed out-of-place clip resizes/writes destination vertices and leaves its
existing metadata unchanged; unlike all-positive/coplanar-copy491A30 it does
**not** copy source plane10/opaque20.

Mixed in-place clip uses lazy global scratch7629B0, copying source opaque20
into it. After output construction it clears the old source ring, copies all
**11 words** from global to source, then restores source identity4. Global
nodeCount/head are zeroed to relinquish transferred ring ownership.
Other metadata comes from global scratch, **not source**. In the independently
initialized fixture this resets plane10/field00/24/28 to zero. No claim is made
that external mutation could never change that global metadata.

Original CRT exit registration for6D7F90 and actual callback/pool teardown use
the already audited explicit C17 boundary; no geometry helper replacement.
All tested native allocations are released exactly once.

## Partial portable implementation and checks

`Sparkplug/Analysis/PC/spPolygonClip.h` is a **geometry-only analytical utility**,
not invented `spPolygon` source. It preserves return flags, point order,
intersections and repeated-first correction; host vector ownership/aliasing
does not model original scratch metadata move or memory-pool ABI.
Exact2C/1C observed ABI records remain separate.

```powershell
python research/probe_pc_polygon_clip.py
python research/inspect_pc_polygon_clip.py
python research/compare_pc_polygon_clip.py
```

Native **28/28** (9 ring counts+11 copy/alias+8 epsilon), static **22/22**,
source **11/11**, differential **3842/3842 fields /256cases**:
512 return/count plus3330 coordinates, all3330 numeric fields bit-exact in
these inputs; tolerance3e-6, **not universal x87 equivalence**. **CTest20/20**.

Remaining: original helper type/name, huge/malformed polygon upstream guarantees,
full scalar metadata meanings, pool allocation failure and other consumers.
Near-portal-plane/normal Octree45E870 copy and full Visibility ctor46C0F0 remain
protected gaps. Next logical spatial leaf: **spBSPNode**, the other concrete
PartitionNode query implementation actually serialized by the SMO corpus.
