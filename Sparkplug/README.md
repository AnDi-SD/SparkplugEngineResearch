# Реконструкция исходников Sparkplug

Эта директория предназначена только для постепенно восстанавливаемого кода
движка. Она повторяет найденный в PC executable корень `Z:\Sparkplug\...`:

```text
Sparkplug/
  Code/
    SparkBase/
    SparkBasePC/
    Sparkplug/
    SparkplugDX/
    SparkplugPC/
```

Имена пяти модулей подтверждены сохранёнными source paths. Папки создаются по
мере появления восстанавливаемого кода; пустое дерево заранее не заполняется.

`wx...`-классы Winx сюда не помещаются: они относятся к application layer, а
точный исходный корень игры пока подтверждён только частичным PDB-путём
`Z:\Winx PS2\CODE\Build\PC\Release\WinxPC.pdb`. Third-party и system-код также
не смешивается с реализацией Sparkplug.

## Статусы путей и имён

Текущая цель — [проверенные части классов для инструментов](../docs/research/tool-driven-research-scope.md).
Полный исходник каждого класса не является условием применения доказанного
метода/поля в софте. [Широкий PC SMO/SAN контракт](../docs/research/pc-smo-san-completion-contract.md)
сохраняется для долгосрочного исследования, отдельно от готовности приложений.
Постоянные правила — [манифест исследования](../docs/research/research-manifesto.md).
Последний итог: [цикл CP99–CP123 до 07:00 МСК 8 сентября](../docs/research/native-cycle-report-2026-09-08-0700.md),
63 CTest suite; PC workflow-v2 45,62%, широкие engine gates 0/7 passed.
[Общий PC loader](../docs/research/native-pc-smo-san-loader.md) уже исполнен на
bbush.san целиком и сопоставлен с portable fields/bindings/PRS. PC FAT58 и RTTI
consumer layouts добавлены в Analysis/PC. [SAN field writer и nested blocks](../docs/research/native-pc-san-writer.md)
восстановлены отдельно. [Portable full-file core для SAN](../docs/research/native-pc-full-loader.md)
уже собран; полный SMO load/save, внешние зависимости и startup ещё открыты.
Это не объявление готовности приложений. Разделы ниже сохраняют хронологию
исследования; числа тестов внутри датированных этапов относятся к тем этапам.

- `exact` — строка пути или имя присутствует в executable;
- `inferred` — рабочий путь/имя, необходимое для сборки, но не найденное
  буквально;
- `analytical` — исследовательское обозначение, которое не становится именем
  исходного API.

Для exact `.cpp` используется найденный относительный путь. Заголовок или build
project разрешается добавить как `inferred`, если без него нельзя собрать
проверочный срез. Этот статус фиксируется рядом с файлом и снимается после
прямого evidence.

## Первый собираемый срез

### PC-анимация: часовой цикл 2026-09-05

Добавлены вычислительный `spTransformTrackEval`, локальный `spNodeController`
и минимальные interfaces `spEvaluator`/`spTransformEval`/`spSubController`.
Названия классов оригинальные; file paths inferred, API `ForAnalysis` —
аналитический. Sampling dependency seam не выдаётся за готовый SAN decoder,
а local PRS update — за полный world/scene updater. В `spSkin` закреплён
проверенный product `inverseBind * boneWorld`. CTest теперь содержит три suites,
включая 32 новых animation assertions.

Read-only evidence и ограниченные native x86 checks:

```powershell
python research\inspect_transform_track_eval.py
python research\inspect_animation_runtime.py
python research\probe_pc_animation.py
```

Команды запускаются из корня repository. Последняя требует MSVC x86 и
использует отдельный процесс с timeout, не запускает игру и не является
OS sandbox. Границы/seams перечислены в
[PC animation runtime pipeline](../docs/research/native-pc-animation-runtime.md).

Продолжение добавило [PC world-update](../docs/research/native-pc-node-world.md):
parent masks, world caches, billboard, affine output и rotation-only invalidation.
На этом checkpoint CTest содержал четыре набора, включая `SparkplugNodeWorldTests`.
На каждый тест установлен timeout 20 секунд.

Следующий [SAN keys checkpoint](../docs/research/native-pc-animation-keys.md)
добавил analytical `spAnimationKeySampling.h`: representations `1..4`, cubic
preparation, quaternion/scalar PRS sampling и связь с evaluator/controller/world.
Это не полный native SAN loader и не новый придуманный engine class.
Теперь CTest содержит **пять** наборов; `SparkplugAnimationKeyTests` — 60 checks.
120 сравнений с original guest instructions проходят без расхождений.

[Object lifecycle checkpoint](../docs/research/native-pc-animation-lifecycle.md)
добавил original `spTrack`, `spAnimTrack`, `spAnimation`. У анимации exact PC
size `0x84` и физический `spNamedObject` base, отличный от engine RTTI chain.
Name-only clone, track ownership, stable tags и explicit host safety deviations
описаны отдельно. На этом checkpoint CTest содержал **6 suites**, object tests **51/51**, original
guest lifecycle **206/206**, hash/identity inspector **20/20**.

[Full SAN reader checkpoint](../docs/research/native-pc-san-reader.md) добавил
`spAnimationSerializer` на существующем `spStream`/`spDataBlockSerializer` core.
Сейчас **7 CTest suites**; reader C++ **84/84**, четыре SAN original reader
**1034 checks**, portable/native reader-to-PRS **2832 comparisons**, original
termination/failure/reuse **137/137**, reader hash anchors **8/8**.
Это field reader, не полный FFPS/FAT loader, registry или готовый writer.

Финальный [actor checkpoint](../docs/research/native-pc-actor-playback.md) добавил
`spActor` scheduler и abstract dependency `spController`: 93 original/portable
сценария, **1857 comparisons**, **61** native lifecycle и **66** C++ assertions.
На actor checkpoint проверено **8 CTest suites**; исходные пути этих TU inferred.

Ночной [manager checkpoint](../docs/research/native-class-sp-animation-manager.md)
добавил `spAnimationManager`: native shared-name IDs/refcounts, intrusive controller
lifetime, frame→actor tick, blank manager clone и controller-only actor clone.
На manager checkpoint **9 CTest suites**. Original manager probe: **326 checks**, два original
SAN/registry lifetime теста: **33 + 69 checks**, hash anchors: **20 checks**.
Full actor setup/tree, native queue, внешний engine frame и portable SAN binding
ownership остаются открытыми, host safety deviations перечислены в карточке.

Следующий [input/start checkpoint](../docs/research/native-pc-actor-binding.md)
добавил portable two-input insert/clear, exact cache/counter semantics и
animation priority group: **10 CTest suites**, **4368 comparisons /156 cases**.
Original actor discovery/binder/start исполняются в bounded probes; unsafe third
input не запускается. Последующие tree/Stop и owned binding работы фиксируются
в [журнале текущего цикла](../journal/2026/2026-09-06-pc-reconstruction-until-1000.md).

[Owned runtime checkpoint](../docs/research/native-pc-actor-owned-runtime.md)
закрыл portable descendant discovery, Start/Rebind/Stop/StopAll и owned Tick.
SAN reader использует освобождаемые name leases того же manager. **11 CTest suites**,
owned actor **81/81**, шесть сквозных original/portable последовательностей —
**5154 comparisons**. Найден и исполнен original matrix static initializer,
от которого зависит PC node constructor. Native event dispatch, внешний engine
frame и upstream overlapping-input/resource-lifetime contracts остаются открытыми.
Host transaction/capacity guards не объявляются поведением original EXE.

[Frame/timer checkpoint](../docs/research/native-pc-engine-frame.md) добавил
original `spTaskTimer` и уточнил exact PC engine layout: два timer members и
две отдельные event queues. **12 CTest suites**, timer30 checks/1968 bit-exact
comparisons, native app/update134 и graphics209 checks. Source timer готов;
portable app/core frame ещё не подключён, GPU/Win32 callbacks остаются seams.

[Scene/world checkpoint](../docs/research/native-pc-scene-world.md) исполнил
original default-scene/camera setup, timer append и app→SAN→owned tree→world
без forced test-side updates. В source исправлены PC camera offsets CC/10C и
angle-setter2D reset, добавлен timer append (теперь34 checks,1968 differential).
`spScene`/`spSceneManager` пока описаны в ABI/probes, **не объявлены готовыми
portable классами**: далее typed scene registrations и camera/render runtime.

[Render-node checkpoint](../docs/research/native-pc-render-node-runtime.md)
закрыл PC1D4 layout, две таблицы14+6, native intrusive scene list, world/cull/
matrix/dispatch и callback-vector protocol. `Analysis/PC/spRenderNodeMath.h`
содержит проверенный конечный math slice: **1504 comparisons /144 cases**,
**13 CTest suites** на checkpoint7. Продолжение
[checkpoint10](../docs/research/native-pc-model-render-world.md) подключило
virtual world, Model/Mesh getters, native bounds и lazy matrices к классам:
2272 comparisons/32 сценария,34 C++ checks,14 CTest suites. Copy добавляет
независимые Model даже для повторных ссылок, Mesh остаётся shared. Automatic
Scene/Partition/GPU ещё не подключены; LightManager binding явный.

[Checkpoint11](../docs/research/native-pc-renderer-protocol.md) переносит
callback groups/direct phases и raw28 copy в `spRenderable`, добавляет
`Analysis/PC/spRendererQueueMath.h` и record8/20/24 ABI. Native75+64/static28,
portable23/768 differential fields и15 CTest suites. Это не полная portable
renderer queue или GPU implementation; CRT alpha-sort tie order остаётся open.

На Windows с русским MSVC перед **первой конфигурацией нового Ninja build-dir**
выполнить `chcp 65001` в Developer Command Prompt. Иначе CMake может сохранить
повреждённый `/showIncludes` prefix, Ninja перестанет отслеживать заголовки, а
инкрементальная сборка смешает несовместимые object files. При уже повреждённом
prefix нужен новый build-dir; одной повторной сборки недостаточно.

### Базовый срез

`Code/SparkBase/spBaseObject.cpp` — exact translation-unit path. В нём уже
работают переносимые реконструкции `spBaseObject`, `spNamedObject`,
`spCrossPlatform`, абстрактный контракт `spStream`, конкретный
`spMemoryStream`, общий abstract `spFileStream`, Win32 leaf
`spPCFileStream`, exact PS2 layout/state slice `spPS2FileStream`, общий
`spAsyncFileStreamManager`, stateless `spPCAsyncFileStreamManager` и exact
PS2 queue-layout `spPS2AsyncFileStreamManager`, общий `spPCKManager` с
проверенным форматом индекса и семипараметрическим resolver, а также PS2-only
`spPS2Helper` с host/disc path rules, PS2-only `spPS2IOPModuleManager` с
IRX path/list/retry contract и общий `spApp` с точным `0x20` layout,
singleton lifetime и отдельно сохранёнными C++/RTTI иерархиями, а также
платформенные shells `spPCApp`/`spPS2App` с восстановленными lifecycle
contracts, регистрации RTTI и простой
leaf-clone. Inferred-заголовки сохраняют недоказанные виртуальные имена как
`vfunc_<offset>`. Полный native clone-graph с циклами пока остаётся
исследовательской задачей.

Byte-exact PS2 layouts не смешиваются с host-классами и находятся в
`Analysis/PS2/SparkBaseAbi.h`: `spBaseObject = 0x10`, `spNamedObject = 0x14`,
`spCrossPlatform = 0x14`, `spStream = 0x1C`, `spMemoryStream = 0x38`,
`spCloneManager = 0x18`, `spRTTIManager = 0x24`, registration record = `0x60`.
Там же зафиксированы список обратных ссылок базы, singleton-support subobject,
встроенная property group registration и `spPropertySystem = 0x20` с
0x58-байтной записью. Для записи доказаны name/type offsets, parser dispatch,
12-байтный PS2 member-function ABI и положения getter/setter `+0x18/+0x24`;
оставшиеся исходные имена полей пока не найдены. Изолированный CMake-тест
проверяет class IDs, наследование, factory, type checks, клонирование, stream
I/O wrappers, memory-buffer state machine и размеры evidence-структур. Для `spStream` отдельно доказаны
девять pure-virtual операций и non-pure `GetBuffer()`.

Подробные карточки находятся в
[`docs/research/native-class-sp-base-object.md`](../docs/research/native-class-sp-base-object.md),
[`docs/research/native-class-sp-cross-platform.md`](../docs/research/native-class-sp-cross-platform.md)
и
[`docs/research/native-class-sp-stream.md`](../docs/research/native-class-sp-stream.md),
[`docs/research/native-class-sp-memory-stream.md`](../docs/research/native-class-sp-memory-stream.md),
[`docs/research/native-class-sp-file-stream.md`](../docs/research/native-class-sp-file-stream.md),
[`docs/research/native-class-sp-pc-file-stream.md`](../docs/research/native-class-sp-pc-file-stream.md),
[`docs/research/native-class-sp-pck-manager.md`](../docs/research/native-class-sp-pck-manager.md),
[`docs/research/native-class-sp-ps2-helper.md`](../docs/research/native-class-sp-ps2-helper.md),
[`docs/research/native-class-sp-ps2-iop-module-manager.md`](../docs/research/native-class-sp-ps2-iop-module-manager.md),
[`docs/research/native-class-sp-app.md`](../docs/research/native-class-sp-app.md),
[`docs/research/native-class-sp-pc-app.md`](../docs/research/native-class-sp-pc-app.md),
[`docs/research/native-class-sp-ps2-app.md`](../docs/research/native-class-sp-ps2-app.md),
а порядок дальнейшей работы — в
[`docs/research/native-reconstruction-plan.md`](../docs/research/native-reconstruction-plan.md).

## Срез `SparkplugEngine`

Отдельный target сохраняет найденную границу `Code/Sparkplug`. В нём уже
собираются `spEngineCore`, manager/template/game-level цепочка и первая
renderer/resource вертикаль:

```text
spIndexBuffer
spVertexBuffer
spNamedObject -> spResource -> spMesh -> spMeshData
                                     -> spRenderMesh -> spDXMesh
                                                     -> spPS2Mesh
spNamedObject -> spPlatformSpecificMeshData -> spDXMeshData
                                           -> spPS2MeshData
spNamedObject -> spRenderable -> spModel
spBaseObject -> spMaterial -> spMaterialData
                         -> spPS2Material
spNamedObject -> spResource -> spTexture   (C++ lifetime graph)
spNamedObject -------------> spTexture     (registration graph)
spBaseObject -> spTextureBuffer
spNamedObject -> spResource -> spTexture -> spTextureData
spNamedObject -> spNode -> spLight -> spLightData
spBaseObject -> spDXVertexBuffer / spDXIndexBuffer -> spDXMeshCombiner
```

Отдельная serialization-ветка начата с
[`spSerializer`](../docs/research/native-class-sp-serializer.md): восстановлены
его прямой RTTI-base, dual-vptr префикс и identity class-ID hook; manager-driven
field framing теперь опирается на
[`spDataBlockSerializer`](../docs/research/native-class-sp-data-block-serializer.md):
перенесены общий PC/PS2 compact header, read/skip, прямой writer и terminal.
Manager-driven
load/save теперь продолжены в
[`spSerializerManager`](../docs/research/native-class-sp-serializer-manager.md):
восстановлены singleton, ordered platform/direction registry, exact `0x2C`
PC/PS2 ABI, семисловный FFPS header validator и безопасный stream front-end до
FAT boundary. Прямой edge
[`spSerializerHook`](../docs/research/native-class-sp-serializer-hook.md) закрыт
для общего abstract ABI и PS2 leaf; у PC DX leaf защищён только factory, а
открытые vtable/destructor/clone, mesh metadata parser и `<0x4E20` batch plan
перенесены из точного `SparkplugDX/spDXMesh.cpp`. GPU materialization tail
пока явно открыт. Следующий manager-owned узел
[`spResourceFATSerializer.cpp`](../docs/research/native-class-sp-resource-fat-serializer.md)
уже имеет переносимый index/lookup/cursor/clear/object-index срез. Полные
SMO materialization и remaining concrete payload adapters ещё открыты;
[полный PC SAN loader](../docs/research/native-pc-full-loader.md) уже собирает
существующие FAT/RTTI/serializer/name-binding в переносимую цепочку.
Whole bbush совпал с original по227 значениям, portable4 SAN/62 tracks
сохранили прежний field-core результат. Generic read/write reference также
перенесены; whole native FFPS save и arbitrary cyclic ownership ещё открыты.
Следующий join-узел
[`spResourceManager`](../docs/research/native-class-sp-resource-manager.md)
закрыт как non-owning category+name кэш только для `spTexture`/`spMesh`, с
раздельным `0x30/0x2C` PC/PS2 ABI и destructor hook из `spResource`.
[`spNodeSerializer`](../docs/research/native-class-sp-node-serializer.md)
продолжает эту ветку concrete factory/clone, target ID `spNode`, девятью field
IDs и доказанным порядком/default suppression writer. Потоковый codec и
collision relationships остаются evidence-only.
[`spLightDataSerializer`](../docs/research/native-class-sp-light-data-serializer.md)
наследует его на уровне C++, но регистрируется напрямую от `spSerializer`;
восстановленный write plan сохраняет отдельные node/light sections и все девять
light defaults.
[`spLightSerializer`](../docs/research/native-class-sp-light-serializer.md)
остаётся отдельным direct-потомком `spNodeSerializer`, нацеленным на базовый
`spLight`; совпадающие light field IDs не превращены в ложное наследование от
data serializer.
[`spRenderableSerializer`](../docs/research/native-class-sp-renderable-serializer.md)
начинает соседнюю ветку напрямую от `spSerializer`: target `spRenderable`, две
необязательные resource relationships и два всегда записываемых alpha-sort
поля.
[`spModelSerializer`](../docs/research/native-class-sp-model-serializer.md)
продолжает её через target `spModel`, базовую `spMesh` relationship и всегда
записываемую projection group.
[`spMeshDataSerializer`](../docs/research/native-class-sp-mesh-data-serializer.md)
отдельно восстанавливает прямой index-buffer/vertex-buffer payload и
подтверждённый gate неизвестного native serialization mode.
[`spPS2MeshDataSerializer`](../docs/research/native-class-sp-ps2-mesh-data-serializer.md)
добавляет platform-specific packet и bounding box, не смешивая разные PC/PS2
reader flag masks.
[`spDXMeshDataSerializer`](../docs/research/native-class-sp-dx-mesh-data-serializer.md)
восстанавливает парную DX-ветку, два field и арифметику backend payload header.
[`spTextureDataSerializer`](../docs/research/native-class-sp-texture-data-serializer.md)
начинает texture-serializer ветку: direct `spSerializer`, три варианта source,
локальный field 0 и проверяемая арифметика raw texture payload сохранены без
подмены настоящего stream/resource resolver.
[`spPS2TextureDataSerializer`](../docs/research/native-class-sp-ps2-texture-data-serializer.md)
продолжает эту ветку прямым потомком: сохранены target, source termination,
порядок platform type/cross/native полей, разные PC/PS2 reader masks и каркас
PS2 palette/mip payload без выдуманного stream codec.
[`spDXTextureDataSerializer`](../docs/research/native-class-sp-dx-texture-data-serializer.md)
закрывает парную DX-ветку: те же source/reader правила, platform type `6/7` и
подтверждённый `width/rowStride/height/raw bytes` mip-каркас сохранены без подмены
на настоящий Direct3D loader.
[`spMaterialSerializer`](../docs/research/native-class-sp-material-serializer.md)
начинает material-ветку: сохранены embedded helper boundary, стандартный pass/layer
write plan и порядок resource indexing; редкие platform/render-target layers явно
отклоняются до полного доказательства их grammar.

Для `spRenderable` layouts намеренно различаются (`0x58` PC, `0x50` PS2), а
`spModel` соответственно занимает `0x60/0x58`. Общий `spVertexBuffer` закрыт
как exact `0x5C` с component-layout и stream grammar, а `spMeshData` соединяет
его с `spIndexBuffer` через доказанный two-owner/deep-copy/bounds contract.
Актуальные
карточки: [`spIndexBuffer`](../docs/research/native-class-sp-index-buffer.md),
[`spVertexBuffer`](../docs/research/native-class-sp-vertex-buffer.md),
[`spMesh`](../docs/research/native-class-sp-mesh.md),
[`spRenderMesh`](../docs/research/native-class-sp-render-mesh.md),
[`spPS2Mesh`](../docs/research/native-class-sp-ps2-mesh.md),
[`spPS2Material`](../docs/research/native-class-sp-ps2-material.md),
[`spRenderable`](../docs/research/native-class-sp-renderable.md),
[`spModel`](../docs/research/native-class-sp-model.md) и
[`spMeshData`](../docs/research/native-class-sp-mesh-data.md), а также
[`spPlatformSpecificMeshData`](../docs/research/native-class-sp-platform-specific-mesh-data.md)
и [`spDXMeshData`](../docs/research/native-class-sp-dx-mesh-data.md),
[`spPS2MeshData`](../docs/research/native-class-sp-ps2-mesh-data.md).
PC-only GPU-граница
[`spDXVertexBuffer`/`spDXIndexBuffer`](../docs/research/native-class-sp-dx-buffers.md)
теперь также восстановлена: exact `0x20/0x1C`, D3D9 creation order, COM
lifetime и связь с `spDXMeshCombiner` подтверждены; переносимый слой сохраняет
эти контракты без зависимости от Direct3D.
Непосредственный [`spDXMeshCombiner`](../docs/research/native-class-sp-dx-mesh-combiner.md)
закрыт следующим: exact source TU и `0x2C` layout, shared dynamic buffers,
lock/cursor/commit/unlock и временный global hook-а теперь имеют executable-backed
реализацию. Полный второй serializer-pass всё ещё не объявляется готовым.
Следующая renderer/resource граница —
[`spTexture`](../docs/research/native-class-sp-texture.md): PS2 exact `0x38`,
PC observed extent `0x38`, общий Init-state и отдельный abstract `spITexture`.
Его CPU-зависимость [`spTextureBuffer`](../docs/research/native-class-sp-texture-buffer.md)
закрыта как общий exact `0x30`: concrete factory, ownership, форматная таблица
и blank clone совпали на PC/PS2.
Следующий concrete [`spTextureData`](../docs/research/native-class-sp-texture-data.md)
имеет exact `0x4A0/0x498`, встроенный buffer по `+0x38`, name-only clone и
подтверждённый CPU-payload copy; platform containers оставлены раздельными.
[`spNode`](../docs/research/native-class-sp-node.md) добавляет exact `0xB4/0xC0`,
local transform, runtime flags и parent/child lifetime. Его реконструкция также
заменила временный `spNamedObject`-корень `spTemplateInstance` настоящим узлом.
[`spLight`](../docs/research/native-class-sp-light.md) продолжает node-ветку
abstract light state, раздельным PC/PS2 support ABI и scene-update границей;
[`spLightData`](../docs/research/native-class-sp-light-data.md) — storage-free
concrete leaf с factory/clone и прямой связью девяти SMO fields с runtime offsets.

[`spLightManager`](../docs/research/native-class-sp-light-manager.md) добавляет
PC borrowed list/selection срез: восемь обычных источников, первый ambient,
native swap-last/stale-tail cache и eligibility по hierarchy100/lightEnabled.
Original PC copy/clone Light независимо подтверждены; Node получил recursive100
helper. Проверки:90 original,22 static,29 C++,1800 differential/540 cases,
CTest14/14. Полное portable Scene/partition/world/backend wiring пока открыто.

Следующий [PC specialized scene checkpoint](../docs/research/native-pc-scene-special-managers.md)
добавляет exact ABI SkyBox1D4/SkyManager24/PCProjection24/PCLensFlare38 и
native probes81+39+43/static25. Исправлено различие wire node-section и actual
SkyBox→RenderNode inheritance; camera-follow идёт через parent DefaultCamera.
Здесь добавлены ABI/доказательства, **не готовые portable manager classes**.

[PC partition/static checkpoint](../docs/research/native-pc-partition-runtime.md)
добавляет exact ABI PartitionNode84/ZoneC8/System1D8/Static10C/PCPartition8C
и общий anonymous render support74. Native38+38/static30, CTest15/15.
Подтверждены reciprocal registration ownership и разные render failure
contracts. Здесь также пока **ABI, не полные portable spatial classes**.

Игровые бинарники, декомпилированные листинги и полные дампы в эту директорию не
добавляются.

[`spVisibilityManager`](../docs/research/native-pc-visibility-runtime.md)
добавляет original-named **частичный record-oriented selection** source:
frame stamps, borrowed support deduplication, dynamic Enabled/bypass gates.
Plane/sphere helpers в Analysis/PC, exact managerAC/scratch/plane ABI отдельно.
SceneInit/partition transfer и native Visibility исполнены с явно borrowed
manager record: whole protected constructor не завершён, порталы/окклюдеры/
GPU и portable Scene traversal пока не подменены этой узкой реализацией.

[Whole SceneRender checkpoint](../docs/research/native-pc-scene-render-runtime.md)
добавляет actual native whole-path tests67/Shadow lifetime14/static23 и
`spDXRenderer` single-entry state-cache source (960 differential/6 C++).
Shadow manager3C сохранён как exact ABI; portable Shadow/Scene classes ещё
не реализованы. Native constructor/device seams остаются явно описанными.

[Occlusion checkpoint](../docs/research/native-pc-occlusion-runtime.md)
добавляет exact1B8/face/edge ABI и fully-inside plane/sphere math helper;
native84/static24, обновлённые1483 differential checks и CTest17.
Original `Code/Sparkplug/spOcclusionVolume.cpp` path теперь доказан, но полной
portable реализации класса пока нет. Cached silhouette используется как
явный вход native Scene test; full protected Init остаётся capped.

[Octree checkpoint](../docs/research/native-pc-octree-runtime.md) добавляет
`Code/Sparkplug/spPartitionNode.*` и `spOctreeNode.*` — original-named
**частичный child/query source**, не все33 native methods. Native43/static24,
source28/3104 differential fields и CTest18. ExactC8/ray scratch ABI отдельно;
explicit geometry/Zone-presence inputs и host safety guards описаны. Normal
Visibility plane-copy45E870 остаётся capped; Debug21 test его не закрывает.

[Portal checkpoint](../docs/research/native-pc-zone-portal-runtime.md) добавляет
частичные original-named `spZonePortal/spZonePortalNode`: geometry/borrowed
references/inherited-only clones и пять getter names из diagnostics. Пути
inferred, host bounds/finite guards явно отделены. Native63/static32/source19,
plane1024 differential fields/256cases и CTest19. Actual whole Scene clipped
portal/partial aperture/cycle tests прошли; native near-plane45E870 branch и
полный portable Scene traversal всё ещё открыты, no GPU/image claim.

[Polygon clipping checkpoint](../docs/research/native-pc-polygon-clipping.md)
добавляет **geometry-only** `Analysis/PC/spPolygonClip.h`, не выдуманную native
class identity. Native resize/copy/alias metadata28/static22/source11,
3842 differential fields/256cases и CTest20. Original repeated-first correction
сохранена; host127 bound выведен из local arrays, native128 не запускался.
Scratch metadata/pool и полная Visibility интеграция не подменяются vector API.

[BSP checkpoint](../docs/research/native-pc-bsp-runtime.md) добавляет частичный
original-named `Code/Sparkplug/spBSPNode.*`: two children, raw split plane,
independent optional polygon, inherited-only clone, leaf/masks/packed visible
children и ray queries. ExactAC/ray8 ABI; getter names original, paths inferred.
Native48/static17/source17/2046 differential fields256cases, **CTest21/21**.
Whole original Scene camera-Zone selection подтверждён отдельно; portable
Scene/registration/debug implementation и protected45E870 не объявлены готовыми.

[Plane-storage checkpoint](../docs/research/native-pc-visibility-plane-storage.md)
добавляет analytical `Analysis/PC/spVisibilityPlaneStorage.h`: resize/reuse,
полуторный capacity growth, fieldwise copy без padding, all-enabled и release.
Это не найденный original class name и не замена protected45E870/46C0F0.
Fresh-copy45E530 моделируется отдельно от assignment. Native190/static28,
17569 differential fields/64 cases/576 операций;
host padding initialization и bounded exceptions отделены от native semantics.
