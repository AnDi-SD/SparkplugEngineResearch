# Исходники рендеринга: реализации и границы

Изученное поведение, читаемый исходник и подключённый runtime — разные
результаты. Восстановленный код хранится в `Sparkplug/` и `Winx/`, наш адаптер
Remix — в `tools/WinxRemix/`. Подробная [карта CPU-подготовки](cpu-preparation.md)
связывает входы, выходы и порядок операций.

Здесь «есть исходник» означает тело `.cpp`, inline/template-функции или HLSL
entry point. RTTI/factory, декларация, native listing и статья сами по себе
реализацией соответствующего алгоритма не считаются. Наличие тела также не
означает восстановления всего класса или поведения на произвольных входах.

## PC: общий код и ограниченные модели

| Часть | Где находится реализация | Граница |
| --- | --- | --- |
| Matrix setters, lazy cache, три произведения | [spDXRenderer.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp) | Конкретные CPU-операции; lifecycle находится в отдельном Init TU |
| Device capabilities, CreateDevice, presentation parameters, Reset и cache restore | [spDXRenderer_Init.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Init.cpp) | Сохранены выбор HW/SW/REF, частичные записи, игнорируемые AL/HRESULT и fail-fast resource loops. Полный `4BCF20` и literal selector представлены, включая raw AL и необычный cold cache. COM — внешняя граница; startup выделен в следующий TU |
| Startup, общая CPU-storage и teardown | [spDXRenderer_Startup.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Startup.cpp) | Конкретный `4BD380` связывает caps/device,128 scratch matrices, CPU index/vertex initialization, cache restore и shader startup. Сохранены частичный false startup и отдельная slot70 очистка. Входы globals/OS/SDK явные; полный constructor/destructor и повторный startup не выводятся из этого метода |
| 22 именованных типа shader-констант | [spDXShader.cpp](../../../Sparkplug/Code/SparkplugDX/spDXShader.cpp) | Частичные записи, finite/bounds guards и x87-края описаны отдельно |
| Material install/states, texture/pass binding, light preparation и draw orchestration | [spDXRenderer.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp) | Device operations передаются через явный внешний контракт; это не самостоятельный Direct3D backend |
| Shader key/cache, source composition, compile orchestration и startup | [spPCShaderManager.cpp](../../../Sparkplug/Code/SparkplugPC/spPCShaderManager.cpp), [spDXShaderManager.cpp](../../../Sparkplug/Code/SparkplugDX/spDXShaderManager.cpp), [spPCShaderSource.h](../../../Sparkplug/Code/SparkplugPC/spPCShaderSource.h), [spPCEffectTemplate.cpp](../../../Sparkplug/Code/SparkplugPC/spPCEffectTemplate.cpp) | VS/PS enumeration и `4C97E0` Fixed.rfx flow представлены. [Per-file standalone loader](../../../Sparkplug/Code/SparkplugPC/spPCShaderManager_Standalone.cpp) связывает text reader, реальный annotations parser и shader object. Vertex Create безусловен, F2F5 разрешает только pixel Create; warning остаётся в error queue. OS/SDK compiler/reflection — внешние границы; произвольные шаблоны не квалифицированы |
| RFX parsing/loading | [spPCRFXFileLoader.cpp](../../../Sparkplug/Code/SparkplugPC/spPCRFXFileLoader.cpp) | Наличие parser не закрывает всю startup/ownership цепочку shader manager |
| Material clocks, texture tracks, UV/color controllers | [spRenderController.cpp](../../../Sparkplug/Code/Sparkplug/spRenderController.cpp), [spAnimTexController.cpp](../../../Sparkplug/Code/Sparkplug/spAnimTexController.cpp), [spUVController.cpp](../../../Sparkplug/Code/Sparkplug/spUVController.cpp), [spMaterialColorController.cpp](../../../Sparkplug/Code/Sparkplug/spMaterialColorController.cpp) | Finite операции и codec; exceptional/failure domains и portable clone parity отдельно |
| Alpha/general/nine-bucket queues и renderable callbacks | [spRenderer.cpp](../../../Sparkplug/Code/Sparkplug/spRenderer.cpp), [spRenderable.cpp](../../../Sparkplug/Code/Sparkplug/spRenderable.cpp), [queue math](../../../Sparkplug/Analysis/PC/spRendererQueueMath.h) | Конкретные enqueue/storage/flush/release. Сохранены внутренний general sort, соседние prepare callbacks и PC no-op bucket phases, которые не очищают buckets; branch75F8E8 задаётся явно |
| Сортировка с правилами старой CRT | [LegacySortPolicy.h](../../../Sparkplug/Analysis/Host/LegacySortPolicy.h) | Отдельная host-политика. General использует собственную замену; она не заявляет точную перестановку равных ключей внутреннего compiler sort оригинала |
| Mesh/buffer CPU layout, device wrappers и submission | [spVertexBuffer.cpp](../../../Sparkplug/Code/Sparkplug/spVertexBuffer.cpp), [spIndexBuffer.cpp](../../../Sparkplug/Code/Sparkplug/spIndexBuffer.cpp), [spDXIndexBuffer.cpp](../../../Sparkplug/Code/SparkplugDX/spDXIndexBuffer.cpp), [spDXVertexBuffer.cpp](../../../Sparkplug/Code/SparkplugDX/spDXVertexBuffer.cpp), [spDXRenderer.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp) | CPU shadow и COM resource path разделены. Create сохраняет прямой output slot; index Release очищает pointer только при count0. Все texture/resource families этим не восстановлены |
| Vertex declaration и renderer cache | [spPCVertexDeclaration_Device.cpp](../../../Sparkplug/Code/SparkplugPC/spPCVertexDeclaration_Device.cpp), [spDXRenderer_Declarations.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Declarations.cpp), [контракт](../../reference/classes/sp-vertex-declaration.md) | Miss связывает общий FVF/element builder с Create и вставкой в cache; hit, включая stored null, не вызывает Create. Временный COM output заранее содержит flags, element array освобождается до публикации18, HRESULT игнорируется. Reinit сохраняет native overwrite leak. Unwritten nonzero output даёт unsafe prefix/incomplete; Bind/Release его не используют. CPU-only facade и host shared lifetime отделены от native device state/teardown |
| Texture attachment и обычный/cube render target | [spDXTextureResource.cpp](../../../Sparkplug/Code/SparkplugDX/spDXTextureResource.cpp), [spDXRenderTarget_Device.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderTarget_Device.cpp), [контракт](../../reference/classes/sp-render-target.md) | Create/ReInit/release, формат и metadata, повторные cube attachments и mip loop представлены через COM/D3DX leaf providers. Сохранены игнорируемые HRESULT и Release count. При zero byte count locked-mip цикл вычисляет размер из pitch/height/format. Typed attachment с ненулевой palette выполняет Replace/Register до metadata и использует текущее renderer device отдельно от backing device; порядок сопоставлен с оригиналом. Captured existing objects и явная owner coherence; Legacy bool facade — отдельная host-модель |
| Загрузка готовых native mip-цепочек | [spDXTexture_Native.cpp](../../../Sparkplug/Code/SparkplugDX/spDXTexture_Native.cpp), [контракт](../textures/texture-native-source.md) | `4ABBA0` с nativeFlag1 связывает CreateTexture, surface/LockRect, копирование CPU-строк с сохранением физического pitch padding, Unlock/Release и metadata. Представлены полностью предоставленные raw/DXT1/3/5 цепочки. HRESULT сам по себе не прерывает маршрут; LockRect требует явно инициализированных output/span. Cross-conversion и генерация недостающих mip через внешнюю библиотеку остаются отдельными границами; существующие CPU filter-модели их полную API-цепочку не закрывают |
| Индексы и владение texture palette | [spDXRenderer_Palette.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Palette.cpp), [pool](../../../Sparkplug/Code/SparkplugDX/spDXPalettePool.h), [контракт](../../reference/classes/sp-texture.md) | Register берёт первый свободный индекс и удаляет все его дубликаты, либо увеличивает unsigned counter; Recycle добавляет индекс в хвост. SetPaletteEntries остаётся COM leaf. Replacement освобождает старую palette после recycle новой; порядок и игнорируемый HRESULT сохранены. NULL/alias hazards ограничены явным host-контрактом; whole texture lifetime этим не закрыт |
| Skin palette и ограниченная draw-цепочка | [spSkin.cpp](../../../Sparkplug/Code/Sparkplug/spSkin.cpp) | CPU palette preparation; деформация вершин относится к shader semantics |
| Visibility | [spVisibilityManager.cpp](../../../Sparkplug/Code/Sparkplug/spVisibilityManager.cpp), [portal traversal](../../../Sparkplug/Code/Sparkplug/spVisibilityPortal.cpp), [occluder selection](../../../Sparkplug/Code/Sparkplug/spVisibilityOcclusion.cpp), [point predicates](../../../Sparkplug/Analysis/PC/spVisibilityPointMath.h), [sphere/plane math](../../../Sparkplug/Analysis/PC/spVisibilityMath.h) | Frame/camera/zone/root walk, Base/nested Octree и portal gates/clip/band представлены через borrowed graph views. Nested traversal сохраняет child order и reduced planes; нулевой active count переводит ветвь в unclipped обход, который пропускает Enabled gate. Represented occluders проходят sphere/point rejection, ordered append и marks; normal и unclipped ветви сохраняют различный порядок stamp/append. Непредставленная geometry даёт incomplete. Автоматическая registration остаётся отдельной границей |
| Reciprocal membership и реальный объектный граф | [spPartitionMembership.cpp](../../../Sparkplug/Code/Sparkplug/spPartitionMembership.cpp), [Host snapshot](../../../Sparkplug/Analysis/Host/VisibilityGraphSnapshot.cpp), [контракт](../visibility/visibility-runtime.md) | Конкретные RenderNode/OcclusionVolume/PartitionNode хранят обе стороны membership, duplicates, swap-last removal и notify/clear порядок. Host snapshot заимствует их владельцев и world caches, сохраняет одну запись на объект и публикует Manager/Scene marks обратно. Проход использует реальные model/world producers. Derived spatial insertion, typed Scene construction и автоматические world notifications остаются отдельными; allocation failure не заявляется атомарным |
| Подготовка объёма окклюдера от камеры | [spOcclusionVolumeCamera.cpp](../../../Sparkplug/Code/Sparkplug/spOcclusionVolumeCamera.cpp), [camera math](../../../Sparkplug/Analysis/PC/spOcclusionCameraMath.h) | World/local transform с исходными spill boundaries, epsilon camera cache, face sides, ordered silhouette walk и planes представлены. Scale/rotation/topology cases сопоставлены с оригиналом. Автоматическое Scene wiring этим не закрыто |
| Scene CPU occlusion | [spScene.cpp](../../../Sparkplug/Code/Sparkplug/spScene.cpp), [key math](../../../Sparkplug/Analysis/PC/spOcclusionSceneMath.h), [контракт](../../reference/classes/sp-scene.md) | Физический Named prefix отделён от registration metadata. Реальный CPU phase строит ordering keys, стабильно сортирует до32 записей, готовит volume planes, сохраняет NULL holes и откатывает mark скрытого support на Scene stamp−1. Более32 записей требуют явной host sort policy; native tie order не заявлен. Аналитическое создание не подменяет native factory, clone, четыре подсистемы и автоматическую Scene registration |
| Particle pool, region sampling, producer и frame Update | [spParticleSystemRuntime.cpp](../../../Sparkplug/Code/Sparkplug/spParticleSystemRuntime.cpp) | Общий producer используется Init и emission; физическое кольцо, debit исходного request и ordered modifier calls представлены. Алгоритмы производных modifiers остаются виртуальной границей |
| Обновление существующих particle records | [spParticleCpuUpdate.h](../../../Sparkplug/Analysis/PC/spParticleCpuUpdate.h) | Реальное тело ограниченного алгоритма; не полный интегрированный `spParticleSystem::Update` |
| Ballistic motion, basis, packed quad streams и class dispatch | [spBallisticCpuDraw.h](../../../Sparkplug/Analysis/PC/spBallisticCpuDraw.h), [spParticleSystemRuntime.cpp](../../../Sparkplug/Code/Sparkplug/spParticleSystemRuntime.cpp), [spBallisticColor.h](../../../Sparkplug/Analysis/PC/spBallisticColor.h) | Конкретные24-byte vertices/UInt16 indices, batches, SetWorld и synchronous upload/draw boundary. Raw-input color provider сохраняет narrow extended arithmetic; exceptional x87 domains и GPU execution отдельно |

Общие `.cpp` входят в engine CMake. Header-only helper подключается через
своих потребителей; наличие такого helper в тесте не означает автоматического
включения в игровой runtime. Выбор проверок в viewing-core CMake и полный
набор optional engine tests также различаются.

Отдельные неполные исходные классы нужно читать именно в их ограниченном
объёме: [spPCRenderer](../../../Sparkplug/Code/SparkplugPC/spPCRenderer.cpp)
содержит identity/clone часть; восстановленный device lifecycle расположен
в `spDXRenderer_Init.cpp`. Bounded startup находится в `spDXRenderer_Startup.cpp`; полный backend из этого не следует.
[spDXAsmShaderParser](../../../Sparkplug/Code/SparkplugDX/spDXAsmShaderParser.cpp)
восстанавливает фактический `4CF950` parser комментариев с register/Component
annotations. Он не был assembler grammar: компиляция инструкций выполнялась
внешним SDK. Его metadata parsing сопоставлено с оригиналом; это не перенос
всего SDK assembler.

## Shader и PS2

| Часть | Исходный материал | Граница |
| --- | --- | --- |
| Известные внешние PC-программы | [7 HLSL-файлов, 11 стадий](../../../Sparkplug/Shaders/README.md) | Читаемые семантические исходники есть. Они не подключены к runtime автоматически и не гарантируют исходный register ABI |
| C++-модели shader expressions | [Skinning](../../../Sparkplug/Analysis/PC/spFixedShaderSkinning.h), [lighting](../../../Sparkplug/Analysis/PC/spFixedShaderLighting.h) | Это модели GPU-выражений, не найденные CPU-методы игры |
| PS2 mesh normalization и пакетная подготовка | [spPS2MeshData.cpp](../../../Sparkplug/Code/Sparkplug/spPS2MeshData.cpp), [normalization](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataNormalization.cpp), [packet builder](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataPacket.cpp), [weight packing](../../../Sparkplug/Analysis/PS2/spPS2MeshWeightPacking.h), [контракт](ps2-mesh-preparation.md) | Raw-copy/default writes и целый Type2 DMA RET/VIF packet представлены, включая поддержанные1..4weights/bone indices. Expanded normalization сохраняет untouched captured allocation bytes. Default owned outer branches и external unweighted packet сопоставлены с оригиналом. В original external weighted interleaved ветви payload совпадает, но declared QWC/RET на1QW короче фактической записи; безопасный source builder не копирует этот undersized header. Стандартный Type3 сохраняет native false |
| PS2 prepopulated strips и compressed Normal | [spPS2MeshStripPreparation.cpp](../../../Sparkplug/Analysis/PS2/spPS2MeshStripPreparation.cpp), [direct packet](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataStripPacket.cpp), [shared emitter](../../../Sparkplug/Code/Sparkplug/spPS2MeshPacketEmitter.cpp), [spPS2MeshNormalPacking.h](../../../Sparkplug/Analysis/PS2/spPS2MeshNormalPacking.h) | Greedy parity joins, ADC и descending sort представлены отдельно от отклоняемого standard Type3. Полный original direct маршрут в конечном unweighted UV0/Color домене сопоставлен по payload/end pointer и освобождению временных списков. Общий writer используется также Type2; ADC8000 попадает только в position.w, direct payload не включает RET или outer ownership. Normal quantization сохраняет Z/Y/X order и unsigned branch через arithmetic provider. Integer-dyadic provider принимает только exactly representable finite операции; этот kernel сопоставлен с original words в ограниченной модели R4000 с explicit rounding-to-zero. Полной EE FPU модели нет |
| PS2 scalar producer `2076E0` | [spPS2PipelineScalarPreparation.h](../../../Sparkplug/Analysis/PS2/spPS2PipelineScalarPreparation.h) | Цикл/dataflow/counter восстановлены; точные `DIV.S/MUL.S` требуют квалифицированного arithmetic provider |
| PS2 renderer | [spPS2Renderer.cpp](../../../Sparkplug/Code/SparkplugPS2/spPS2Renderer.cpp) | Lifecycle/factory/clone часть не реализует весь render backend |
| PS2 Pipeline/Microcode managers | [spPS2PipelineManager.cpp](../../../Sparkplug/Code/SparkplugPS2/spPS2PipelineManager.cpp), [spPS2MicrocodeManager.cpp](../../../Sparkplug/Code/SparkplugPS2/spPS2MicrocodeManager.cpp), [контракт](../../reference/classes/sp-ps2-microcode-manager.md) | Factory/identity/singleton, signed key map, callback dispatch и bounded CALL/CNT/END/MSCAL command words представлены. Native miss содержит unsafe callback; переносимый интерфейс возвращает incomplete. DMA/VU execution не выполняется |
| Четыре PS2 VU upload-образа | [Каталог VU](../../reference/shaders/ps2-vu.md), [отдельный GPL decoder и наш статический анализ](../../../tools/ResearchWorkbench/ps2-vu-disassembler.md) | ISA decode всех4583 pairs и raw roundtrip есть; potential VF/VI/ACC/flag/lane operands и symbolic memory описаны для всех пар. Direct CFG учитывает branch/E delay; отдельная pinned-reference модель задаёт whole-lower discard/old-input priority и stopped-unit continuation entries. Scheduled values, indirect targets, фактические MSCNT/MSCAL, VU unit и численное/runtime исполнение не квалифицированы |

## Наш адаптер Remix и внешние исходники

У адаптера есть конкретные C++-реализации по подсистемам:

- [Proxy и runtime](../../../tools/WinxRemix/src/runtime/winx_d3d9_probe.cpp),
  [API session](../../../tools/WinxRemix/src/runtime/winx_remix_api_session.h),
  [resource registry](../../../tools/WinxRemix/src/runtime/winx_remix_resource_registry.h)
  и [frame scheduler](../../../tools/WinxRemix/src/runtime/winx_frame_scheduler.h).
- [Камера](../../../tools/WinxRemix/src/native/winx_native_camera_source.h),
  [геометрия](../../../tools/WinxRemix/src/surface/winx_rigid_geometry_packet.h),
  [материал](../../../tools/WinxRemix/src/materials/winx_material_recipe_source.h)
  и [Skin](../../../tools/WinxRemix/src/skin/winx_skin_gpu_submit.h).
- [Свет](../../../tools/WinxRemix/src/lighting/winx_light_payload_source.h),
  [texture snapshot](../../../tools/WinxRemix/src/textures/winx_texture_mip_snapshot.h)
  и [content identity](../../../tools/WinxRemix/src/runtime/winx_resource_identity.h).

`Build-Probe.ps1` собирает основной `.cpp`, который включает header modules.
Readers, observers, converters и API-подача являются кодом адаптера; они
используют работающую оригинальную игру и не заменяют все её CPU producers.
Поддержанные варианты и пробелы заданы в
[rendering contract](../../../tools/WinxRemix/docs/rendering-contract.md).

Remix SDK/renderer/bridge и зависимости — внешние исходники закреплённой
версии. Их источник, hashes и подготовка заданы в
[dependencies.json](../../../tools/WinxRemix/dependencies.json) и
[инструкции адаптера](../../../tools/WinxRemix/README.md). Наши изменения
bridge/renderer хранятся как source patches в `tools/WinxRemix/patches/`.
Сторонний исходник, патч и установленная совместимая DLL — разные сущности;
наличие первых двух не квалифицирует третью.
