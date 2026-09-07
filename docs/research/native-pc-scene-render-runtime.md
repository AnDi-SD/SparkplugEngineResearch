# PC: целый SceneRender, Shadow manager и DX state cache

6 сентября 2026, checkpoint14 цикла до10:00 МСК. Продолжение
[Visibility/SceneInit](native-pc-visibility-runtime.md),
[renderer queues](native-pc-renderer-protocol.md) и
[static supports](native-pc-partition-runtime.md). Original VA pristine PC;
PS2 не исследуется этим срезом.

## Что теперь исполнено целиком

Original **45EC70(Scene,camera)** до нормального возврата, не до остановочного
адреса: actual camera application, Sky manager, Visibility processing,
adjusted render supports, Model/mesh boundary, general queue flush,
**actual spDXShadowVolumeManager**, alpha flush, empty LensFlare/glare и
optional debug notification. Проверены perspective и camera231 alternate
ветви, последующие кадры и propagation/ignoring ошибок.

Это **не полноценное игровое изображение**: модель/узлы/менеджеры actual native,
но mesh payload borrowed; renderer memory, его device leaves и COM slotE4
явно recording fixtures. Visibility manager имеет прежний documented borrowed
constructor gap46C0F0. Shader resources, nonempty shadow-volume rendering,
occluders/portals, полная инициализация renderer/app и GPU не выдаются за готовые.

Ограничения прежние:100k instructions/2sec на native call, child30sec.
Суммарная первая batch попытка из нескольких свежих fixtures достигла30sec;
**независимые сценарии разделены на пять отдельных ограниченных children**.
Ни одна capped native функция не продолжалась частями и лимит не увеличивался.
Все normal native allocations освобождены original teardown.

## Scene selection, phases и ошибки

Fixture явно выбирает global74024C=0 для immediate и=1 для queued path;
75F8E8=0 означает проверенную general queue. Raw PE global bytes не называются
результатом полного application startup.

Успешная initialized partition scene с RenderNode и пустыми special lists:
camera view→projection, Sky→alphaFlush(empty), Visibility, support draw,
generalFlush, Shadow, alphaFlush, Lens→glare. SceneInit включает Sky/Lens/
Projection managers; пустые native PC phases тоже исполняются.

Отдельная constructed Scene с null partition выбирает ordinary intrusive
RenderNode list (без whole Init claim для этой ветки). Camera actual attached
to SceneRoot, SceneManager выполняет world-update; camera.scene3C настоящая.
Partition path передаёт support draw **force1**, ordinary — **force0**.
Spherez=.25/r=.125 перед near1 доходит до mesh boundary в partition path,
но native ordinary RenderNode frustum rejects её. GPU clipping этим не отменён.

| Контролируемая ошибка | Наблюдаемый whole-Scene результат |
|---|---|
|camera view returns0|false; projection/sky/visibility ещё не вызваны|
|camera projection returns0|false после view; downstream отсутствует|
|RenderNode matrix returns0, immediate|false; no mesh/shadow/later phases|
|RenderNode Model/mesh returns0, immediate|true; node игнорирует Model result|
|Static matrix returns0, immediate|true; Static продолжает Model|
|Static Model/mesh returns0, immediate|false; Scene останавливается|
|Static Model/mesh returns0 внутри general flush|true; flush result/Model failure ignored|

QueueingC050 сбрасывается до generalFlush; очередь очищается логически с
сохранением capacity. Это не универсальная транзакция с rollback. Зависимость
поведения от kind/phase сохранена, не выровнена под придуманное единое правило.

### Исправленная трактовка notification1A

**Это не безусловное end-of-frame событие.** После Lens Scene читает
DebugManager18. При false45F194 прыгает сразу45F2C6 к успешному возврату и
пропускает весь debug tail, включая40F9A0(Scene,1A,0,0).
Отдельный пустой-scene test включает только Debug18: original notification
тогда действительно вызывается. Оно не заменено recording-success seam;
observer только фиксирует вход в исходную функцию.

## spShadowVolumeManager / spDXShadowVolumeManager

Original IDs/base:
`spDXShadowVolumeManager04680BC1→spShadowVolumeManager63FEA321→`
`spCrossPlatform20A72504→spNamedObject44DE07FD`.
Base has null registration factory; PC leaf4A9030 exact**3C**, global75DB7C.
Original source/header path для Shadow не найден; ABI names confirmed,
пути будущих declarations inferred.

Primary6EF138 nine slots:
4A9010 /5B7A00 /4A90B0 /413120 /4A7C60 /408350 /408370 /4A8570 /4A85F0.
Support6EF134→4A7C70 adjusts this−14 before deleting destructor.

| Offset | Наблюдение |
|---|---|
|00..13|Cross/Named prefix|
|14|secondary singleton/deleting support|
|18 /19|enabled1 / optional extra volume pass0; padding untouched|
|1C /20 /24|ctor0; full runtime meanings open|
|28|ctor0; Init shader lookup result|
|2C|ctor0; Init view_proj_matrix handle|
|30 /34|constructor leaves untouched; Init LightPos / Range handles|
|38|ctor1, full meaning open|

Whole original ctor, RTTI ancestry, direct clone and both deletion adapters
исполнены. Clone only inherited Named portion; controls/handles remain fresh
ctor values, including untouched30/34. Nonempty-name sharing не заявлено этим
test. Creating clone overwrites global singleton; deleting **any** instance
unconditionally clears global even if another instance exists. Destructor
4A7C30→417AE0 does not own/release literal handles1C..38 in these tests.

Init4A8570 statically looks up `ShadowVolumePoint` through global763024,
factory4C9680; when found, parameters `view_proj_matrix`, `LightPos`, `Range`.
Missing shader logs warning and **still returns true**; actual lookup/backend
and nonempty Init are not executed here, no inferred full shader success.

### Real empty shadow passes

Scene calls4A85F0 only if enabled18. Camera byte231 nonzero causes native
early true with no state/device calls. Perspective branch uses cached camera
basis/scene light list; empty light list still sets/restores D3D states and
clears renderer currentLightCacheC190 when !C9C4.

With explicit zero state-cache fixture, first changed COM calls `(index,value)`:
`(53,1),(55,1),(54,1),(58,FFFFFFFF),(52,1),(59,FFFFFFFF),(174,1),`
`(52,0),(174,0)`. Repeated frame needs only52/174 on/off. Requested equal-zero
states are suppressed by the real state helper. HRESULT failure for every
COM call does not change successful empty-pass result.

## DX cached state helper and source

Original **4B0A90(renderer,index,value)** compares word at
`renderer+E4F4+4*index`. If equal, AL1 without device access. Otherwise actual
`deviceC9E8->vE4(device,index,value)` stdcall, then **unconditionally updates the
cached word** and returns AL1, irrespective of HRESULT. Failure therefore
suppresses retries for the same value on the next call.

This device cache is distinct from common spRenderer's12-word logical cache
atC868. The complete E4F4 array extent and constructor invalidation remain
unknown; safe fixture indices≤255 do not prove native capacity256.
`spDXRenderer::ApplyRenderStateCacheEntryForAnalysis` implements the verified
single-entry operation with explicit callback/context. No backend, default
array values or GPU startup is invented. Changed-value/null callback rejection
is marked host-only; native dereferences a device instead.

## Reproduce / remaining work

- `probe_pc_scene_render_runtime.py`: **67 checks** across five independent
  bounded scenarios (21+17+8+19+2).
- `probe_pc_shadow_manager_runtime.py`: **14/14**.
- `inspect_pc_scene_render_runtime.py`: **23/23** SHA/RTTI/table/CALL/gates.
- `compare_pc_dx_render_state.py`: **960/960** fields,96 two-call sequences.
- `SparkplugDXRenderStateTests`: **6/6**; full CTest **17/17**.

Next same graph: actual OcclusionVolume mesh/plane construction and support
rejection, Octree/portal walk, remaining shadow-light geometry and shader
resources. New source here is DX single-state operation plus Shadow ABI,
**not portable Scene or a full Shadow class**. No game/assets/apps/publication.
