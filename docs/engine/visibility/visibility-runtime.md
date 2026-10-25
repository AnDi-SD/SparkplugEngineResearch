# PC: SceneInit, partition transfer и spVisibilityManager

## Граница доказательств

Original `45D850` SceneInit, `45F4D0` partition switch и `46D270` visibility
processing исполнены на настоящих Scene/System/Zone/PartitionNode/Camera/
Static/PCPartitionRenderable/RenderNode/Model объектах. Их factories, graph
ownership и нормальное удаление настоящие. Отдельный настоящий Visibility
factory46D1C0, deleting destructor и shutdown scratch pool завершились без
borrowed manager и освободили все выделения. Контракт46C0F0(this+40,32) —
capacity32 при logical size0 внешнего plane-stack.

В `VisibilityFixture` это явно **borrowed manager record**, не перехваченный
успешный factory. Исполнены его Base ctor и оба настоящих scratch ctor4914E0/
logical-resize491660(32), real pool allocation/shutdown. C18 уточнил прежнее
аналитическое название reserve: count8 действительно становится32, не capacity.
Операция46C0F0 в этих graph fixtures опущена; её capacity-only контракт
проверен отдельно настоящим constructor. SceneSetPartition перед использованием
очищает plane stack. Отдельный constructor не превращает прежние graph
fixtures в полный cold engine startup и не квалифицирует allocator failure.

## Identity, layout и containers

`spVisibilityManager` **3D7F4387→spBaseObject415352A1**, registration7605E0,
CALL6D3BD0; factory46D1C0 allocates**AC**, resolved ctor4A2B10.
Original source/header path не найден; `Code/Sparkplug` путь inferred.
Primary6E8CDC имеет **7** slots:
46C4B0 /5B7A00 /46D220 /40ECE0 /46C330 /408350 /408370.
Отдельный support6E8CD8→46C340; это не восьмой primary slot.

| Offset | Подтверждённый смысл |
| --- | --- |
| 00..0F | Base object |
| 10 | secondary vtable |
| 14 | u32 frame/visibility stamp; ctor0 |
| 18..27 | vector borrowed complete OcclusionVolume pointers |
| 28..37 | vector borrowed **adjusted render-support** pointers |
| 38 | borrowed override camera, ctor null |
| 3C | sphere-culling switch, ctor1; padding untouched |
| 40..4F | vector20-byte plane-set records, allocator word untouched |
| 50 | current plane-stack index |
| 54 /80 | two44-byte clipping scratch records |

Plane-set20 = vector16 + activeCount10. Plane20 = four float equation
`dot(n,p)-d`, enabled byte10, padding3. Their original class names unknown;
ABI names `...ObservedLayout` expressly analytical.
Scratch2C: unknown00, generated id04 (global740384), nodeCount08,
circularHead0C, plane10..1F, unknown20, ctor-zero24/28.
Global pool740388 allocates block header8 plus100 nodes×1C=AF0; nodes link
next14/prev18, free link00. Scratch destructor491510 returns nodes;
actual CRT shutdown6D7FA0→491520 frees block data/header. Pool no OS calls.

## SceneInit и partition replacement

Original Init creates fallback `spPartitionSystem1D8`, `spPartitionNode84`,
`spZoneC8`; attaches System to SceneRoot and Zone to System. Both Scene38/3C
reference System; root belongs directly to System, Zone has two intrusive
references (Node child and root60), Zone B8/BC borrowed root vector contains
the root. Counts: System ref3, root ref0, Zone ref2.

Actual Node attachment + SceneManager45A7D0 registers a RenderNode in root.
Attaching another PartitionSystem invokes typed Scene registration, selects
new partition, detaches fallback from Node tree, and48E940 transfers reciprocal
Node links to the new root. Only then48E790→oldRootv80 clears old vectors.
This confirms why raw non-notifying root reset is not safe general Clear.
Nonempty Collision/Occlusion branches of transfer remain separately open.

## Camera-to-visibility processing

46D270 chooses override38 when nonnull, increments u32 stamp14, logically
clears both output vectors and publishes Scene40. If Scene partition absent,
returns without fabricating one. Otherwise selectedSystem1D4 root v40 obtains
camera leaf and leaf60 Zone. Normal path46C4D0 iterates Zone's local roots.

The initial six planes are:

1. Camera forward1A0, `d = y*ny + z*nz + x*nx` at world position74.
2. Camera **far1D4**.
3. Side planes1E4,204,1F4,214 in that order.

**Camera near1C4 is not copied.** Actual camera at0/near1 supplies near plane
(0,0,1,1), but Visibility first plane is(0,0,1,0). A small sphere atz=.25
passes Visibility even though it is before near1. This is selection, not a
claim that GPU clipping draws that sphere. Override camera atz−10 shifts the
first plane d to−10 and makes a formerly behind-camera object eligible.

Each traversal stamps root7C and visits payload78, static vector30, dynamic
vector20, occluders40, selected children and portals. Accepted payload/static/
dynamic support addresses are complete+10/+14/+B4 respectively; their mark is
support64 = complete74/78/118. Accepted pointers are borrowed; duplicate list
entries emit only once per stamp. Rejected supports keep their previous mark.

| Gate | Partition / Static | RenderNode |
| --- | --- | --- |
| mark already current | skip | skip |
| Enabled bit200 | not tested | skip if disabled |
| manager3C true | sphere/plane test | sphere test unless bypass130 |
| manager3C false | no sphere rejection | no sphere rejection; Enabled still applies |
| DebugManager21 | unclipped46B870 | unclipped; Enabled not tested here |

Later RenderNode drawing can still reject Disabled; do not conflate visibility
and draw predicates. Stamp wrapFFFFFFFF→0 is not guarded: zero-mark supports
are skipped on that frame; following stamp1 works normally. No corrected or
"safer" original behavior is invented in the source slice.

## Sphere helpers and portable slice

4902D0 tests activeCount10==0 first. Otherwise enabled planes reject on
`ny*y + nx*x + z*nz - d < -radius`, strict comparison; tangent survives.
490500 does **not** gate by activeCount; returns1 inside,2 outside,3 intersecting,
including both tangent boundaries. Unordered NaN comparison does not reject
in4902D0 and takes intersecting branch in490500. Numeric source uses double
intermediates to approximate x87 extended work; no universal bit-exact claim.

`Sparkplug/Code/Sparkplug/spVisibilityManager.{h,cpp}` implements identity,
Base-only clone, record predicates and `ProcessSceneForAnalysis`: frame
publication, override camera, leaf/zone selection and ordered root traversal.
Borrowed node views hold decoded payload/static/dynamic memberships; spatial
queries use the concrete `spPartitionNode`/`spOctreeNode` implementations.
Callers supply cached world spheres and camera planes. This interface is not
the original Scene ABI or automatic graph registration.

Normal traversal copies the six planes above, applies Octree selection and
child plane reduction, and switches to the unclipped branch when no active
planes remain. This branch also omits the dynamic Enabled test. The original
Debug path repeats the selected system root once per Zone root entry rather
than visiting each different root. Traversal stamps a node on every visit;
support stamps provide deduplication. Final cleanup clears the first plane
record's active count. Manager word10 is the secondary vtable and is preserved;
the native ESI register changes receivers before this cleanup.

Base graph frame/zone/support ordering and marks were compared with the
original. Whole-frame flat and nested Octree composition also preserves child
order, reduced raw planes, incoming flags and node/support/frame marks.
When the active plane count becomes zero, children use unclipped traversal,
including the original bypass of the dynamic support Enabled gate. Normal portal traversal is implemented in
[spVisibilityPortal.cpp](../../../Sparkplug/Code/Sparkplug/spVisibilityPortal.cpp)
as described below. Represented occluders use actual volume geometry through
[spVisibilityOcclusion.cpp](../../../Sparkplug/Code/Sparkplug/spVisibilityOcclusion.cpp).
The older `hasOccluders` flag explicitly denotes an unrepresented list and
still refuses it before frame mutation. Stable borrowed views, finite camera
values and host depth/visit bounds are required; failure after a missing
reachable view or unavailable intersecting-volume geometry may leave a
partially processed frame.

`Analysis/PC/spVisibilityMath.h` contains anonymous mathematical helpers;
exact PC ABI is recorded in `SparkplugAbi.h`. Neither the native constructor
nor allocation-failure behavior is qualified by this traversal interface.

## Occluder collection and point predicates

Normal `46C6B8` checks each volume's C4 stamp before reading geometry. It
classifies the world sphere against the current plane set: inside1 accepts,
outside2 skips, intersecting3 calls `490360(count,packedXYZ)`. A nonzero AL
from this helper rejects the volume; zero accepts it. Append precedes the
accepted stamp write. A rejected volume keeps its previous stamp. Neither
the volume Enabled flag nor the support sphere-culling switch bypasses this
branch. Unclipped `46B9BC` skips the sphere/point predicates and writes the
stamp **before** append. Both paths keep borrowed complete-volume pointers.

The two direct-VM point helpers use an inclusive `±0.001f` band. For one
enabled plane, points below the negative boundary contribute a negative sign;
points above the positive boundary contribute a positive sign. Band points
and unordered NaN comparisons contribute neither. A negative-only plane
rejects a volume through `490360`; a positive-only result on **every** enabled
plane accepts full containment through `490480`. Mixed signs or no signs do
not count as full containment. Consequently, tangent plus inside counts as
contained; tangent plus outside counts as outside; an all-tangent or empty
point set satisfies neither predicate for an enabled plane. These are point
set predicates, distinct from the strict sphere containment helper4903E0.

Both helpers test cached activeCount0 before the vector: outside=false and
contained=true on that branch. Disabled planes are skipped. Their shared
source bodies are in [spVisibilityPointMath.h](../../../Sparkplug/Analysis/PC/spVisibilityPointMath.h),
using integer significands for finite default-nearest x87 arithmetic. NaN
points retain the neutral comparison behavior; consumed Inf values are an
explicit incomplete numeric domain. This is semantic reconstruction from
bounded original query behavior, not a recovered contiguous x86 body for
the VM programs. Qualification of these direct point queries is separate from
the measured [Manager/Scene CPU composition](../scene/scene-world.md#исходник-cpu-фазы-occlusion).

## Portal traversal: source and qualification boundary

Normal traversal processes portals after selected children. A closed portal
or one already marked with the current stamp is skipped. Every other portal
receives the stamp **before** its geometry is tested. A destination root may
therefore be revisited, including `A → B → A`; portal and support stamps
terminate the cycle and prevent duplicate visible supports. The host retains
explicit depth/work bounds. The unclipped `46B870` branch does not process
portals.

The portal test adds the incoming displacement to cached camera position,
spills each component to float, and evaluates the signed distance to the
portal plane. Distance greater than `+.001f` rejects the portal. Below
`-.001f`, the polygon is clipped against each enabled inherited plane with
coplanar retention disabled. Empty output rejects the portal. A successful
descent keeps the first two inherited equations, enables them, and appends
one camera/edge plane per clipped edge in polygon order. The incoming
displacement is preserved for this branch.

The inclusive epsilon band tests all projected polygon edges, including the
distinct summation order of the closing edge. Its source preserves the
observed nearest64 arithmetic and float spill locations through
[spVisibilityPortalMath.h](../../../Sparkplug/Analysis/PC/spVisibilityPortalMath.h).
On acceptance it copies inherited plane values, enables all live planes,
and computes the displaced incoming argument using the original operation
order. The native assignment `45E870` completed independently on nine finite
storage inputs; its vector and outer-count distinction is documented in
[plane storage](visibility-plane-storage.md).

Eighteen fresh original calls completed normally and matched current C++:
far/partial/full clipping, closed/marked/reversed portals, a two-portal cycle,
the inclusive band, both epsilon boundaries and adjacent float values,
ordinary/closing polygon edges, projected outside rejection, a tilted plane,
chained displacement and the Debug gate. Comparison includes ordered root
entries, every plane equation word, enabled bytes, active counts, incoming
displacement and node/portal/support stamps. Each fresh original call ran
the protected assignment preparation itself when reached. Earlier capped
calls remain unaccepted; no stopped guest was resumed and no successful
assignment stub or predecoded body replaced that dependency.

## Borrowed class memberships and host snapshot

[spPartitionMembership.cpp](../../../Sparkplug/Code/Sparkplug/spPartitionMembership.cpp)
contains concrete base PartitionNode dynamic/occluder insertions, both sides'
removal/clear and reciprocal teardown. An insertion appends the opposite link
before its own link and retains duplicates. Remove searches the first matching
pointer, replaces that slot with the last pointer, pops once, then optionally
calls the opposite remove with notification disabled. Missing pointers do not
trigger a callback.

Notifying clear takes the back pointer, calls opposite remove(false), then
pops its own entry. It retains storage. Non-notifying clear releases its own
vector storage and leaves opposite links untouched. This deliberate native
quirk requires callers to repair a deliberately broken graph before peers are
destroyed. These narrow methods do not implement the entire `4269C0` reset,
which also touches collision and portal vectors. Derived spatial insertion
overrides are refused where their placement body is unavailable; they are not
replaced with successful base insertion.

The [VisibilityGraphSnapshot](../../../Sparkplug/Analysis/Host/VisibilityGraphSnapshot.h)
is our lifetime bridge. It follows actual source-class children, Zone roots and
portal destinations and reads actual class-owned payload/static/dynamic/occluder
lists. Current world bounds come from the source objects; model/world producers
must run first. Repeated physical support identities share one snapshot record,
so the concrete Manager deduplicates them through its real stamp logic. After
the concrete Manager/Scene CPU call, the adapter publishes node/support marks
back to source objects. Volumes and portals already receive their marks directly.

One snapshot permits one Build attempt. Source owners and links remain stable
through processing, and the snapshot outlives every use of borrowed Manager
outputs. It creates no native Scene, source object owner or typed Scene registry.
Host vector allocation exceptions are not converted to native failure results:
insertion can retain the first append if the second allocation throws, and a
snapshot Build can propagate an allocation exception. Atomic failure behavior
and native allocator failure paths are not qualified.

The compared finite membership protocol covers duplicate insertion, first-match
removal, both notification modes, storage release and reciprocal object teardown.
The represented Scene comparison also uses actual class-owned memberships and
model/world producers, then builds the snapshot, runs Manager/Scene processing
and publishes marks. It preserves the original ordered visible supports,
occluder queue, plane/cache words and stamps on those inputs. This establishes
the concrete CPU composition through the host lifetime bridge; typed Scene
attachment and derived spatial placement remain separate engine operations.

## Open graph, not a completed visibility subsystem

- Full engine startup/allocator-failure invariants, populated outer-stack
  reserve/relocation and original methods/TU.
- Broader ZoneC4 input domains. Concrete Octree child
  selection/reduction/order and nested frame composition are represented; Base PartitionNode v50
  returns0, so ordinary Base traversal does not descend arbitrary children.
- Portal inputs outside the compared finite domain, including arbitrary
  nonfinite/degenerate polygons and nondefault floating environment.
- Automatic typed Scene attachment, derived dynamic/occluder spatial placement
  and nonpartition rendering. The measured represented-graph Manager/Scene CPU
  composition is described in [Scene](../scene/scene-world.md#исходник-cpu-фазы-occlusion);
  it does not establish full Scene rendering.
  Fresh mesh/shape/world/camera bodies and finite native/current-source
  comparisons are described in [Occlusion runtime](occlusion-runtime.md).
- Scene45EC70 full invocation, ShadowVolumeManager/shader/device dependencies,
  native renderer startup and automatic portable Scene wiring.
