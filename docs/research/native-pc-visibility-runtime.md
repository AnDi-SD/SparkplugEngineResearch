# PC: SceneInit, partition transfer и spVisibilityManager

6 сентября 2026, PC-first checkpoint13 текущего цикла до10:00 МСК.
Продолжение [partition runtime](native-pc-partition-runtime.md) и
[Scene](native-pc-scene-world.md). Только pristine PC SHA из
`inspect_serializer_manager.PC_SHA256`, original VA; PS2 не использовался.

## Граница доказательств

Original `45D850` SceneInit, `45F4D0` partition switch и `46D270` visibility
processing исполнены на настоящих Scene/System/Zone/PartitionNode/Camera/
Static/PCPartitionRenderable/RenderNode/Model объектах. Их factories, graph
ownership и нормальное удаление настоящие. **Whole Visibility factory46D1C0
не завершён**: установленный100k-instruction/2-second cap срабатывает в
protected container helper46C0F0(this+40,32). Лимиты не повышались.

В `VisibilityFixture` это явно **borrowed manager record**, не перехваченный
успешный factory. Исполнены его Base ctor и оба настоящих scratch ctor4914E0/
logical-resize491660(32), real pool allocation/shutdown. C18 уточнил прежнее
аналитическое название reserve: count8 действительно становится32, не capacity.
Операция46C0F0 опущена;
capacity-only против logical-size contract пока unknown. SceneSetPartition
перед использованием очищает plane stack. Поэтому результат не доказывает
полный startup/constructor/allocator invariant менеджера.

Остальные внешние границы: bounded host-memory allocator, recording renderer
leaves, D3D query-type9 с ответом8876086A, imported CRT `memmove` с лимитом4096
bytes/overlap-safe copy/исходным cdecl ABI. Последний импорт потребовался при
очистке уже заполненного output на втором кадре. DLL, GPU, OS и игра не
запускались, EXE и ресурсы не менялись. Guest heap/stack64KiB, child30sec.

## Identity, layout и containers

`spVisibilityManager` **3D7F4387→spBaseObject415352A1**, registration7605E0,
CALL6D3BD0; factory46D1C0 allocates**AC**, resolved ctor4A2B10.
Original source/header path не найден; `Code/Sparkplug` путь inferred.
Primary6E8CDC имеет **7** slots:
46C4B0 /5B7A00 /46D220 /40ECE0 /46C330 /408350 /408370.
Отдельный support6E8CD8→46C340; это не восьмой primary slot.

| Offset | Подтверждённый смысл |
|---|---|
|00..0F|Base object|
|10|secondary vtable|
|14|u32 frame/visibility stamp; ctor0|
|18..27|vector borrowed complete OcclusionVolume pointers|
|28..37|vector borrowed **adjusted render-support** pointers|
|38|borrowed override camera, ctor null|
|3C|sphere-culling switch, ctor1; padding untouched|
|40..4F|vector20-byte plane-set records, allocator word untouched|
|50|current plane-stack index|
|54 /80|two44-byte clipping scratch records|

Plane-set20 = vector16 + activeCount10. Plane20 = four float equation
`dot(n,p)-d`, enabled byte10, padding3. Their original class names unknown;
ABI names `...ObservedLayout` expressly analytical.
Scratch2C: unknown00, generated id04 (global740384), nodeCount08,
circularHead0C, plane10..1F, unknown20, ctor-zero24/28.
Global pool740388 allocates block header8 plus100 nodes×1C=AF0; nodes link
next14/prev18, free link00. Scratch destructor491510 returns nodes;
actual CRT shutdown6D7FA0→491520 frees block data/header. Pool no OS calls.

Clone46D220 statically: new factory, CloneManager Register412F70, source
v0C→inherited40ECE0. Own vectors/stamp/override do not copy. Native clone
whole execution remains blocked by the same constructor boundary; portable
fresh-state clone test is **not** evidence that original factory completed.

## SceneInit и partition replacement

Original Init creates fallback `spPartitionSystem1D8`, `spPartitionNode84`,
`spZoneC8`; attaches System to SceneRoot and Zone to System. Both Scene38/3C
reference System; root belongs directly to System, Zone has two intrusive
references (Node child and root60), Zone B8/BC borrowed root vector contains
the root. Counts: System ref3, root ref0, Zone ref2.

System RTTI directly derives Node even though physical prefix is RenderNode:
Scene ordinary-render count stays0. Init sets LightManager1C=&Scene18 itself;
the older fixture's prewritten link is explicitly cleared before this call.
Actual Scene destructor tears down the whole fallback graph.

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
|---|---|---|
|mark already current|skip|skip|
|Enabled bit200|not tested|skip if disabled|
|manager3C true|sphere/plane test|sphere test unless bypass130|
|manager3C false|no sphere rejection|no sphere rejection; Enabled still applies|
|DebugManager21|unclipped46B870|unclipped; Enabled not tested here|

Later RenderNode drawing can still reject Disabled; do not conflate visibility
and draw predicates. Stamp wrapFFFFFFFF→0 is not guarded: zero-mark supports
are skipped on that frame; following stamp1 works normally. No corrected or
"safer" original behavior is invented in the source slice.

Дополнительный original test с **двумя Zone roots**: обычный46C4D0 путь
посещает оба и выдаёт оба Static supports. Debug21 loop46D470..49B проверяет
длину Zone vector, но передаёт46B870 снова **selectedSystem root**, а не
очередной элемент. Поэтому второй самостоятельный root не посещается.
Это отдельная подтверждённая особенность EXE, не правило обычного просмотра.

## Sphere helpers and portable slice

4902D0 tests activeCount10==0 first. Otherwise enabled planes reject on
`ny*y + nx*x + z*nz - d < -radius`, strict comparison; tangent survives.
490500 does **not** gate by activeCount; returns1 inside,2 outside,3 intersecting,
including both tangent boundaries. Unordered NaN comparison does not reject
in4902D0 and takes intersecting branch in490500. Numeric source uses double
intermediates to approximate x87 extended work; no universal bit-exact claim.

`Sparkplug/Code/Sparkplug/spVisibilityManager.{h,cpp}` implements only original
identity/Base-only clone and borrowed **record-oriented selection**: frame
publish, duplicate stamps, above per-kind predicates. Callers supply already
computed world spheres/planes and payload→static→dynamic order. It does not
walk a portable Scene, create a fake completed native constructor or silently
substitute portal/occluder logic. `Analysis/PC/spVisibilityMath.h` contains the
anonymous mathematical helpers; exact PC ABI in `SparkplugAbi.h`.

## Open graph, not a completed visibility subsystem

- Whole factory/46C0F0 protected container preparation and original methods/TU.
- ZoneC4; concrete Octree child selection and packed child order/clipping.
  Base PartitionNode v50 returns0, so arbitrary child arrays do not prove
  ordinary visibility descent. Debug/unclipped is a separate route.
- Portals use stamp34, enabled20, plane24 and clipping scratch54/80. Their
  polygon/zone transform and epsilon branches are not fully executed here.
- Occluder mesh/plane generation, volume-vs-support tests and Scene removal
  from visible output; helper490360/490480 and actual opaque geometry next.
- Scene45EC70 full invocation, ShadowVolumeManager/shader/device dependencies,
  native renderer startup and automatic portable Scene wiring.

Reproducible probes: `probe_pc_scene_partition_init.py`,
`probe_pc_visibility_runtime.py`, `inspect_pc_visibility_runtime.py`,
`compare_pc_visibility_runtime.py`, `SparkplugVisibilityTests`. Final counts
and immutable manifest are recorded in the cycle journal, not inferred from
the length of a disassembly dump.

Следующий [checkpoint14](native-pc-scene-render-runtime.md) уже исполнил целый
SceneRender с actual empty Shadow/Lens phases. Это закрывает whole-call
границу на описанных fixtures, но не nonempty occlusion/portal/GPU и не
прежний protected Visibility constructor.

Цикл до12:00 добавил [точный surrounding plane-storage contract](native-pc-visibility-plane-storage.md):
resize46B720 и release45EA00 не меняют outer activeCount, all-enable46ADC0
пересчитывает его и присваивает raw byte всем live planes. Дополнительно
copy-constructor45E530 и outer-stack append46C350: Native190,
17569 differential fields/576 операций. Это не исполнение45E870/46C0F0.
