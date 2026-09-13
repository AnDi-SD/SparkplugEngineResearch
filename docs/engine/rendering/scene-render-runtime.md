# PC: целый SceneRender, Shadow manager и DX state cache

## Scene selection, phases и ошибки

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
| --- | --- |
| camera view returns0 | false; projection/sky/visibility ещё не вызваны |
| camera projection returns0 | false после view; downstream отсутствует |
| RenderNode matrix returns0, immediate | false; no mesh/shadow/later phases |
| RenderNode Model/mesh returns0, immediate | true; node игнорирует Model result |
| Static matrix returns0, immediate | true; Static продолжает Model |
| Static Model/mesh returns0, immediate | false; Scene останавливается |
| Static Model/mesh returns0 внутри general flush | true; flush result/Model failure ignored |

QueueingC050 сбрасывается до generalFlush; очередь очищается логически с
сохранением capacity. Это не универсальная транзакция с rollback. Зависимость
поведения от kind/phase сохранена, не выровнена под придуманное единое правило.

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
| --- | --- |
| 00..13 | Cross/Named prefix |
| 14 | secondary singleton/deleting support |
| 18 /19 | enabled1 / optional extra volume pass0; padding untouched |
| 1C /20 /24 | ctor0; full runtime meanings open |
| 28 | ctor0; Init shader lookup result |
| 2C | ctor0; Init view_proj_matrix handle |
| 30 /34 | constructor leaves untouched; Init LightPos / Range handles |
| 38 | ctor1, full meaning open |

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

## DX cached state helper and source

Original **4B0A90(renderer,index,value)** compares word at
`renderer+E4F4+4*index`. If equal, AL1 without device access. Otherwise actual
`deviceC9E8->vE4(device,index,value)` stdcall, then **unconditionally updates the
cached word** and returns AL1, irrespective of HRESULT. Failure therefore
suppresses retries for the same value on the next call.

## Reproduce / remaining work

Next same graph: actual OcclusionVolume mesh/plane construction and support
rejection, Octree/portal walk, remaining shadow-light geometry and shader
resources. New source here is DX single-state operation plus Shadow ABI,
**not portable Scene or a full Shadow class**. No game/assets/apps/publication.
