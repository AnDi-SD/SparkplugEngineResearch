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
materialization, save payload и relationship fixup всё ещё evidence-only.
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

Игровые бинарники, декомпилированные листинги и полные дампы в эту директорию не
добавляются.
