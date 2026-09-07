# Открытые вопросы native-реконструкции

Статус: текущая очередь актуализирована по PC checkpoints 51–98 и
последующему опыту с heap 128 КиБ, 7 сентября 2026. Ранние заметки ниже исторические:
их формулировки «далее/open» относятся к моменту записи, не отменяют
более поздних проверок. Текущие work items: `research/native-work-items.json`.
Это канонический список неизвестного в исполняемом коде; вопросы SMO
отдельно ведутся в `research/open-questions.md`.

Неизвестность не заменяется удобным именем. Пока original symbol/source string
не найдены, в коде используются `field_<offset>`, `vfunc_<offset>` или
`sub_<VA>`, а предполагаемый смысл хранится отдельно как analytical role.

## Текущие связанные пробелы PC

- [Loader/save](native-pc-full-loader.md): SAN full-file core и отдельные
  object/graph readers/writers подтверждены; полный SMO load/save со всеми
  необходимыми зависимостями, внешними ссылками и отказами остаётся открытым.
- [Геометрия](native-pc-mesh-writer-roundtrip.md): source MeshData writer
  принимается original reader; DX materialization и включение произвольной
  сетки в полную resource/scene цепь ещё требуют работы.
- [Node resource graph](native-pc-node-serialization.md): отдельные graphs,
  serializers и aliases проверены; полнота общих графов и их lifetime не доказана.
- [Shader generation](native-pc-shader-generation.md): generating miss,
  RFX/template и compiler-boundary контракты проверены в выбранных сценариях;
  Parser/EffectTemplate/RFXFileLoader теперь имеют отдельные оценки.
  Live compiler/GPU integration и полнота вариантов остаются открытыми.
- [Свет и Skin](native-pc-skin-selected-light.md): связка decoded SMO light,
  scene cache, SAN/Skin и constants подтверждена. Два first-generation случая
  прошли [при heap 128 КиБ](native-research-strategy-2026-09-07.md);
  queued first-generation и полный PC display этим не закрыты.
- [Controllers](native-pc-material-color-graph.md): UV/Anim/color consumer
  иcodec slices проверены; capped colorfactory/clone не повторять, source
  clone parity иmalformed/full-file lifetime остаются открытыми.
- Conversion/missing mips, full native save/lossless unknowns, protected
  visibility иlive PCdisplay остаются обязательными. PS2 отдельно, вторично.

Полный накопленный список включает исторические неизвестные ниже; текущая
сверка очереди со знаменателем: `python research/audit_native_scope_dependencies.py`.
Пустой результат этой проверки не доказывает полноту call graph.

## Исторические checkpoint-заметки

Checkpoint18 [native-data writer](native-pc-texture-native-writer.md):
CPU TextureData vector6C теперь записывается общим ядром для policies0/1/2,
6 exact cases +3 shared CPU regressions. Полная поддержанная цепочка читается
source DX reader; partial chain остаётся missing-mip boundary. Remaining writer
failures, embedded/external sources и conversion не закрыты.

Checkpoint 17 [palette/lifetime](native-pc-palette-lifetime.md): спPalette
factory/copy constructor/blank virtual clone, renderer register/free-index reuse,
texture setter and destructor, palette codec и пять failure paths подтверждены.
Source palette codec теперь работает, но не имитирует live GPU registration.
Original setter ошибочно unregister-ит новый argument; same-pointer удаляет palette;
DX destructor оставляет palette/index. Failed create и failed mip write могут
вернуть true. Host RAII/проверки ошибок — явное безопасное отличие. Далее native-data
writer, material/controller links, missing mips/conversion/external и live backend.

Checkpoint 16 [native texture source](native-pc-texture-native-source.md):
общий CPU/DX source wrapper и DX native-data reader перенесены; одиннадцать
exact comparisons, включая две рекурсивные секции field 3. Ранее указанный
source-adapter blocker закрыт только для полного поддержанного mip-chain.
Не закрыты native-data writer, cross conversion, missing mips, external source,
palette ownership/renderer registration, material/controller links и live backend.
spPalette factory/getter/dtor подтверждены пятью проверками; это не полный класс.

Checkpoint15 [PC runtime/mip/registry](native-pc-texture-runtime-mips.md):
runtimeDXTexture/sourceclass и flat palette-free codec восстановлены,
nativeDXData→temporaryData→DXTexture copy исполняется на full tiny chains.
Source Dataheader теперь правильныйDXTexture CPU shadow, не unavailable.
Actualstartup всехplatformvariants связан сwire78EA082B, неvirtualID0B1C67BB.
Palette — реальныйspPalette591C0B9F; ownership/rendererregistration ещёopen.
ДалееnativeData/sourcefield3 adapter, материал/ссылки, realassetsslice,
missingmips/conversion/COMerrors и livePCbackend. Дваnative пути заполняют
разныеruntimeполя:4ABBA0 не пишет44/48;4ABAC0 не инициализирует18/1C/flags20.

Checkpoint14 [PC texture codec/boundaries](native-pc-texture-codec-boundaries.md):
CPU source/local/raw reader/writer exact3 rows, actual factory/header/runtime
identity и pre-conversion COM arguments подтверждены. Header42DD10 создаёт
DXTexture, не CPUData; source явно не подменяет runtime object. Открыты
native mips/palette/containers, actual60FDB4/61039A conversion и attribution,
DX source/backend, material texture/controller graph, внешние sources,
lossless unknowns и real PC rendering. Size4AAC50 hidden reset404BC6
подтверждён; первоначальная гипотеза накопления отвергнута оригиналом.

Новый этап без прежнего deadline: [PC SMO/SAN до проверяемого завершения](pc-smo-san-completion-contract.md).
Впереди очереди теперь [FFPS/FAT/SAN loader](native-pc-smo-san-loader.md):
whole422B50 на bbush и повторная загрузка пройдены, registry/FAT/RTTI consumers
получили PC evidence. Открыты startup registration/constructors, directed
remaining materialization/cache/error/external-file branches и whole native save.
[SAN field writer и nested blocks](native-pc-san-writer.md) уже исполнены;
остаются missing descriptors/unknown preservation и часть error paths.
[Общий save-reference protocol](native-pc-save-reference.md) теперь тоже
исполнен и перенесён: indexing/alias/null/SBOO/one-shot, включая native
unchecked final-patch failures. Полная FFPS/FAT save orchestration ещё открыта.
[Read-reference](native-pc-read-reference.md) теперь также исполнен/перенесён:
115 native checks,167 portable,227 runtime comparisons. Actual nonempty PC
MeshData cache hit подтверждён; другие cache cases и cyclic SMO ownership
открыты. Ожидаемый class ID и fresh inline-size original не проверяет,
повторный объект не retain-ит. Общие ошибки не заменяются host rollback.
[Full-file SAN loader](native-pc-full-loader.md) теперь перенесён через прежнее
ядро:227 original/portable whole bbush comparisons,160 full-loader portable
checks,266 reference checks. Outer422940 дополнен42 native checks; DX hook
bit2/exact MeshData selection ещё48. Следующий открытый стык — непустая
DX mesh batch и concrete SMO payload adapters; portable4 SAN/62 tracks не
выдаются за original whole-load evidence на трёх capped файлах.
Cold whole-load bflower/barrel/bw capped100k, профили не повторять; прежние
успешные object-reader/PRS тесты этих файлов остаются действительными.
Unknown44 PC FAT не заполнять именем по симметрии. Старые spatial gaps ниже
сохраняются, но не блокируют независимый serializer/writer research.

[PC DX materialization](native-pc-dx-materialization.md): actual metadata,
first scan, wrapper/combiner ownership и полный native payload helper429A40
теперь исполнены с explicit COM boundary. Исправлен wire order:index codec,
затем vertex codec. Renderer4AE0E0 больше не полностью unknown:actual cache
возвращает spPCVertexDeclaration pointer, map/factory/Init/bind/clear проверены.
Checkpoint7 исполнил **whole4AA870** на triangle/two-mesh/logo-field,91checks;
106 declaration masks/8656 bytes совпали с source, clone/reinit/failure59checks.
Checkpoint8 добавил concrete MeshData/DXMeshData readers через общие codecs,
200 native checks (156 readers+44 whole constructed FFPS),60 exact source/native
fields и131 source checks с двумя одновременно живыми файлами. Все28 CTests
прошли. Deferred source RTTI и lazy target linkage исправлены без ослабления
FAT validation; это host adaptation, не protected native startup evidence.
Открыты реальные SMO scene graphs/остальные concrete adapters, полный writer,
unknown preservation, все lifetime/error variants и GPU validation.
Записи backlog ниже уточняются этой новой карточкой, не требуют повторять
уже исполненные constructors/mapping с нуля.

Перед общим backlog текущий PC-first front идёт по
[доказанной animation-цепочке](native-pc-animation-runtime.md):

| Приоритет | Неизвестно | Уже доказано | Следующая проверка |
|---:|---|---|---|
| P1 | world-update внешние зависимости | protected world/quaternion/builder entries закрыты, 179 guest checks; transform/tree/billboard и rotation-only dirty перенесены | bits `2/4`, collision implementation и frame caller перед `0x0046A240`; [доказательства](native-pc-node-world.md) |
| P0 | полный SAN resource-loader transaction | field reader на четырёх SAN; original и portable owned registry на двух SAN,342 differential lifetime checks; header failure иногда даёт success, payload failure не откатывает target | FFPS/FAT integration, writer, allocation/malformed-key/tag и filled-target lifetime; [reader evidence](native-pc-san-reader.md) |
| P1 | original ownership hazards | incoming ownership governs old release; track-wide bool, stale pointers после release, uninitialized standalone owner, reserve-shrink по capacity | upstream lifecycle invariants в loader/actor; не объявлять host safe guards поведением original EXE |
| P1 | key buffer edge invariants | two-key endpoint preserved; real one-key quaternion and empty packed scale exist; native next-slot reads не всегда bounded | установить allocator padding и upstream guarantees; не объявлять host finite/bounds guards оригинальным поведением |
| P1 | внешний frame и полный lifecycle `spActor` | owned runtime5154 comparisons; actual PCApp→timers→SAN actor→owned scene→world исполнен без forced snapshot update | remaining public controls, borrowed animation lifetime/event reentry; full portable app/core/scene frame и renderer; [scene evidence](native-pc-scene-world.md) |
| P1 | upstream input capacity | insert/clear исполнены и перенесены:4368 comparisons/156 cases; exclusive counter асимметричен, incoming наследует destination cache; Start дошёл до third binder CALL без local2 guard | проверить upstream callers, не постулировать counter balance; third CALL остановлен до вставки, опасный input не запускать |
| P1 | PC matrix startup dependency | node ctor421BA0→4D3740 копирует global7600BC; original initializer исполнен,13 checks; SceneManager→world frame edge теперь найден и исполнен | original math type names и полный startup order; другие globals не объявлять инициализированными по одному matrix helper |
| P1 | engine timers/queues | exact task timers54/90; setup41C300 закрывает append/source и10/34/38; source append34 tests,1968 bit-exact comparisons | native detach/reparent, protected whole core ctor, hardware timer и event queue original type/dispatch |
| P1 | PC graphics frame | core41C460/base begin41C210/end41C2A0 исполнены:209 checks, first-boundary two-phase vector walk, renderer gates/return semantics | concrete engine50/global75DBA0, renderer interface downstream names, scene/camera drawing и native mutation/reentry |
| P0 | PC scene typed registrations | actual Scene54/Manager24 lifecycle; typed registrations; whole SceneInit, Partition switch48E940 RenderNode branch и whole45EC70 с явной Visibility constructor boundary | Occlusion/Collision nonempty switch transfers (не путать с tested tree insertions), unknown58DA4026, full startup/portable Scene; [visibility](native-pc-visibility-runtime.md), [whole render](native-pc-scene-render-runtime.md), [specialized](native-pc-scene-special-managers.md) |
| P0 | PC spatial query/visibility | actual Visibility46D270/46C4D0 selection, root7C/Scene40 stamps, six planes omit cameraNear; support selection/fully-inside source1483 differential fields; Octree queries and clipped portal/cycle tests now confirmed | whole ctor46C0F0 still capped100k/2s; remaining Octree/near-portal-plane45E870, ZoneC4, full portable traversal and occluder overlap490360/490480; [evidence](native-pc-visibility-runtime.md) |
| P0 | PC shared plane storage | [190 native/28 static/17569 differential](native-pc-visibility-plane-storage.md): resize46B720, floor1.5 capacity, raw enabled46ADC0, copy17-of20, release45EA00, separate copy-constructor45E530 and outer append46C350/aliased source; original allocator header path | Assignment45E870 is NOT copy-constructor45E530; ctor preparation46C0F0 reserve-versus-size unknown. Do not resume previous capped calls, use fake copy success or infer original plane class names from allocator path. Interior insert/erase and failure unwinds remain open. |
| P0 | PC SceneRender remainder | original whole45EC70 ordinary/partition perspective+alternate branches executed; actual Shadow3C lifetime/empty passes and nonempty Occlusion on cached input; ignores4702E0 false, consumes previous planes; Debug18 gates notification1A | real occluder Init/full silhouette, shadow light meshes/shaders, full startup and portable Scene; same Visibility ctor gap remains, no GPU image claim; [evidence](native-pc-scene-render-runtime.md) |
| P0 | PC OcclusionVolume remainder | original TU, exact1B8/14 slots, Node-only clone, direct buffers, world/reciprocal links, UInt16 weld/original byte comparator, cached plane builder/Scene84 checks; [evidence](native-pc-occlusion-runtime.md) | full470FE0 topology/silhouette capped100k; actual authored decagon acceptance and reader failure transaction, re-init nonempty edges, original helper names; cached input is not completed Init; full portable class absent |
| P0 | PC Octree remainder | actualC8/eight slots, leaf/mask/plane/ray and registrations43; partial original-named source3104 fields/320cases, actual Debug21 eight-leaf Scene; [evidence](native-pc-octree-runtime.md) | normal traversal and isolated45E870 copy capped100k, remaining64..7C queries/debug/geometry, native setters/full Zone/Scene/source wiring; preserve sphere shortcut F0, don't substitute generic overlap |
| P0 | PC portal/clipping remainder | exact Portal38/NodeC4/native63, clipped whole Scene; now491AA0 classification/alias/ring metadata28 and geometry-only source3842 fields/256cases; [portal](native-pc-zone-portal-runtime.md), [clipping](native-pc-polygon-clipping.md) | unnamed helper identity/opaque20/24/28, upstream polygon<=127 guarantees/pool allocation failure, camera-near-plane45E870, game Open controls/DebugDraw; preserve repeated-first correction and camera-origin near plane |
| P0 | PC BSP remaining consumers | [runtime/source](native-pc-bsp-runtime.md): exactAC/query source; [consumers](native-pc-spatial-consumers.md): BSP/Octree Static/Occlusion native94, Zone asymmetry/owning-vs-borrowed refs/Debug21 draw dedup | nonempty Collision registration, debug/ray consumers64..7C, optional polygon producers, setup/error lifetime; protected recursive45E870/full Visibility ctor остаются open |
| P1 | DX device state cache | actual4B0A90 E4F4+4*index/vE4 ignores HRESULT and writes cache, 960 differential fields/single-entry source | full array extent/default initialization, other texture/sampler/state operations and device backend; do not conflate with common12-wordC868 cache |
| P0 | PC static render placements | actual Static10C/PartitionRenderable8C/support74, shared Matrix4 CRT6D38C0, original model ownership/blank own clones; stored inverse independently reaches renderer; draw failure contracts differ from RenderNode | full serializer->matrix->backend trace, portable classes, native support original name; no invented inverse recomputation or uniform draw failure policy; [evidence](native-pc-partition-runtime.md) |
| P0 | PC SkyBox/Projection/LensFlare remainder | Sky1D4/Manager24, PCProjection24/PCLens38; camera-parent/local-orientation/sky fog release, projection phases и flare capability Init proved | portable classes/world integration, nonempty clone, Projection helper4C51B0/virtual38 и flare query-map/visibility/glare/backend; ordinary sky support no-op нельзя считать отсутствующей геометрией |
| P1 | PC render-node runtime integration | exact1D4/14+6; portable virtual world/getters/caches подключены,2272 comparisons; actual duplicate Model always-clone/append-to-destination29 | automatic Scene/Partition/Occlusion wiring, native individual detach469F50, renderer queue/material consumers; host explicit light-manager binding не равен полной сцене |
| P1 | PC camera runtime | exact238 factories; corrected viewCC/projection10C/basis14C; actual renderer pointer/return gates32 checks, angle clears2D | viewport activation/registry, world/view/frustum/culling, faithful in-place cache transition; fresh utility не равна native cache |
| P1 | `spTransformConstEval` dependency | controller kind `1` создаёт `0x68` object через `0x00601C20` | constructor/evaluate contract, только если нужен текущему PRS пути |
| P1 | skin failure cleanup | inverseBind×world и полный `0x00461D70` builder; renderer count очищается только success branch | error caller recovery; не «исправлять» native поведение догадкой |

`spTransformTrackEval` caller и input fields больше не считаются полностью
неизвестными; `spNodeController` PRS-slice восстановлен. Original source/API
names остаются открытыми даже там, где поведение уже проверено. PS2 deferred.

| Приоритет | Неизвестно | Что уже доказано | Следующий источник evidence |
|---:|---|---|---|
| P0 | `spBaseObject +0x0C` | обе платформы обнуляют слово; несколько PS2/PC copy/assignment paths переносят его вместе с base storage, но поведенческих чтений в `spBaseObject.cpp` и надёжного отдельного consumer пока нет; `+0x0A/+0x0B` закрыты как padding | искать setter/consumer `+0x0C` по производным классам, не смешивая с обычными offset `+0x0C` чужих структур |
| P0 | original type/member names для `+0x04/+0x08` | `+0x04` — optional list обратных ссылок; `+0x08` — `uint16` intrusive reference count | RTTI/property metadata, assert strings, соседние source paths и одинаковые PC/PS2 accessors |
| P0 | original type name singleton-support base трёх систем | subobject находится по `+0x10`; constructor публикует полный объект в global instance, destructor сбрасывает его, thunk корректирует `this-0x10`; роль доказана для `spCloneManager`, `spRTTIManager`, `spPropertySystem` | искать template/header evidence в других платформах и соседних исходных строках; отсутствие имени в PC/PS2 strings и доступном RTTI уже проверено |
| P0 | PC-tail `spRTTIManager` | factory выделяет `0x20`; PS2 layout равен `0x24`, tree начинается по `+0x14` | обойти SecuROM trampoline через callers, dtor и отдельные tree operations |
| P1 | exact tree/container type managers | известны размеры, init/destroy/search/insert и key class ID | восстановить node layout и allocator contract по helpers |
| P1 | остаток field schema property record | доказаны размер `0x58`, comparison vptr `+0x00`, name/type `+0x04/+0x08`, name-argument flag `+0x14`, точный 12-байтный PS2 callable ABI и getter/setter `+0x18/+0x24`; `+0x30` является type-specific union, `+0x50` — type-specific payload | определить общую семантику `+0x30/+0x3C/+0x48/+0x4C`, исходные enum/type/descriptor names и сверить ABI с PC |
| P1 | original type name встроенной property group | registration `+0x50` точно содержит owner/count/first record; append, indexed lookup, inherited name lookup и stride `0x58` восстановлены | source/header strings либо повторно используемый helper в соседних модулях |
| P1 | clone ownership общих ссылок и циклов | PC412BE0 always-clone;412C40→4D3810 map-aware; actual map startup/overwrite/root cleanup и RenderNode duplicated Model проверены29 checks | remaining derived callers/cycles; не обещать alias preservation всем классам, не путать unrelated4D3800 с lookup |
| P2 | исходные имена virtual slots, notification record и header/namespace | addresses и поведение базовых slots доказаны на PC/PS2; notification record имеет точный PS2 layout `0x20`, путь `.cpp` exact | PDB/source strings, exports, RTTI/assert messages; не выводить имена только из поведения |
| P2 | точное размещение managers по original source modules | class names/IDs и регистрация доказаны, но отдельные source paths не найдены | registration initializer neighborhoods и PC source-path anchors |
| P1 | `spStream` original mode enum и stream-to-stream write slot | mode передаётся первым аргументом второй перегрузки `Open`; основные биты проверяются с приоритетом read `1`, write `2`, read/write `4`, append `8` служит модификатором; безымянный слот переносит заданное число байт из другого `spStream`; на PC это `+0x34`, на PS2 `+0x40`, поскольку он меняется местами с raw `WriteData` | искать original enumerator/method names в call-site strings, header fragments или ещё одной platform build; до этого сохранять analytical `vfunc_WriteFromStream` и разные ABI-таблицы |
| P1 | `spStream` typed overload types | доказана последовательность размеров `0x10/0x08/0x0C/0x40/0x24/0x04/0x02/0x01`, scalar и string behavior | сопоставлять с восстанавливаемыми math classes по call sites, не назначать тип только по размеру |
| P2 | original declaration/TU `spCrossPlatform` | PC/PS2 registration и vtables доказаны; общий layout точно `0x14`, класс не добавляет полей, factory отсутствует, clone возвращает null | найти header/source evidence; не объявлять отсутствие factory доказательством pure virtual |
| P2 | original header/TU, member names и `GetBuffer` pointer type `spStream` | общий PC/PS2 layout `0x1C`, роли `+0x14/+0x18`, обе vtables и чистая абстрактность девяти slots доказаны | искать другие executables/SDK fragments; текущие `spStream.h/.cpp` явно inferred |
| P2 | полная stream-error schema | найдены сообщения и связь owned name `+0x18` с `spStreamError`; отдельно доказаны причины error-объекта `4..8` и глобальные operation codes `0x00020002..0x00020006` | установить точное соответствие двух шкал и platform OS codes при будущем разборе `spErrorManager`/`spStreamError` |
| P1 | `spMemoryStream` nonvirtual API | layout `0x38`, все virtual operations, default `5000`, growth/ownership state machine, exact-size allocation и PC helpers передачи ownership/reset доказаны; portable names трёх helpers аналитические | найти header fragments либо caller assertions с original spellings; проверить, были ли PS2 release/reset inline или discarded |
| P2 | `spMemoryStream` PC factory prologue | class ID/registration/vtable и все offsets совпадают с PS2; PS2 factory прямо выделяет `0x38`, PC entry защищён SecuROM | снять runtime trampoline либо найти ещё один PC build для прямой инструкции allocation size |
| P2 | `spFileStream` original declaration/TU | class/base IDs, layout `0x1C`, null factory/clone и единственный concrete `Open(name) -> Open(1,name)` доказаны на PC/PS2 | искать header/source fragments; не относить соседний exact `spPCFileStream.cpp` к общему классу без evidence |
| P2 | остаток `spPCKManager` | class/base ID, singleton, PS2 size `0x34`, PC extent `0x3C`, два container ABI, package record `0x1C`, file record `0x14`, Add/Remove, priority и все семь resolver outputs доказаны; 78 PCK/12 229 records проходят независимый index scan | найти original method/member/source names, прямой PC allocation size, роль PS2 `+0x20`/PC `+0x24`, consumer opened-name list и проверить destructor anomaly |
| P1 | остаток `spPS2Helper` | class `spPS2Helper`, ID `0x7E3C519B`, layout `0x11C`, singleton, clone и полный host/disc path transform доказаны; PC-пары нет, а прежний кандидат `0x004C33C0` окончательно исправлен на factory `spPCErrorManager` по его registration getter | установить original method/field names, роль `+0x18` и Sony SDK entry points hardware-reset метода `0x001E93B0` |
| P2 | остаток `spPS2IOPModuleManager` | class/ID, exact `0x28` layout, singleton, root, owned linked list, duplicate/force semantics и 102-attempt bound доказаны | найти original names, consumer one-way flag `+0x18`, SDK-имя loader и объяснить one-byte name allocation anomaly |
| P1 | остаток `spApp` | общий class ID `0x391B146A`, registration base `spBaseObject`, фактический C++ base `spCrossPlatform`, PC/PS2 layout `0x20`, singleton, null clone, owned `char* +0x1C` и empty-string virtual доказаны; оба platform app inheritance edge подтверждены | найти original TU/header, имена/consumers `+0x18/+0x1C` и сигнатуры PC pure slots |
| P1 | остаток `spPCApp` | exact `.cpp`, class/base IDs, обе vtable, lifecycle sequence/message loop и используемый prefix до `+0x83` доказаны; `0x004C33C0` исключён как factory соседнего `spPCErrorManager` | провести границу base/derived по constructor `wxPCApp`, доказать полный `sizeof`, роли `+0x28..+0x57` и сигнатуры hooks `+0x30..+0x48` |
| P1 | остаток `spPS2App` | class/base IDs, exact `0x20` layout, обе vtable, null clone и generic update loop доказаны | установить original source path/names, precise secondary-this adjustment и concrete `wxPS2App` bootstrap callbacks |
| P1 | остаток `wxPCApp` | game/engine boundary, class/base IDs, exact allocation `0x2CC`, vtables, window overrides и lifecycle order доказаны | обойти protected constructor `0x00528280`, назвать manager globals только после разбора их registrations и уточнить base/derived boundary `+0x84` |
| P1 | остаток `wxPS2App` | class/base IDs, exact `0x48` allocation, четыре pointer fields, vtables, title и lifecycle overrides доказаны | сопоставить GP-relative managers с классами и выяснить tail `+0x30..+0x47`; original TU отсутствует в strings |
| P0 | остаток `spEngineCore` | exact PC `.cpp`, общий class/base ID, factory/clone, singleton, размеры PC `0x158` и PS2 `0x150`, разные container offsets, обе vtable по 18 local slots, four-stage initialize, default scene/camera и reverse teardown доказаны; `wxEngineCore` заменяет только ordinal 10 | сопоставлять каждый manager global с registration, искать original имена методов/containers и runtime PC constructor за `.rld` trampoline |
| P1 | остаток `wxEngineCore` | общий ID/base, обе vtable, clone, единственный override ordinal 10 и exact PS2 size `0x150` без derived storage доказаны; на PC подтверждён inherited prefix `0x158` | найти original TU/header и имя override, обойти PC `.rld` factory, сопоставить game globals/states `0x36/0x4E` и renderer field `+0x28` с concrete classes |
| P1 | остаток `spError` | общий ID/base, PS2 exact size `0x28`, совпадающий PC prefix, поля code/severity/source/line/message/next, четыре formatter-роли и blank-clone contract доказаны | найти original header/TU и enum names; сопоставлять derived error classes и их code spaces |
| P1 | остаток `spErrorManager` | exact PC `.cpp`, общий ID/base, abstract provider, singleton, PC `0x1024` против PS2 `0x424`, fixed message stack, LIFO chain, threshold и lazy handler доказаны | разобрать оба platform leaf, native callback ABI, overflow insertion, fatal target и возможную синхронизацию |
| P2 | остаток `spPCErrorManager` | ID/base, allocation `0x1024`, stateless leaf, обе vtable, clone, provider и точная MessageBox/console severity map доказаны | найти original TU/header, callback typedef, разобрать common console route `0x00413500` и imported UI owner |
| P2 | остаток `spPS2ErrorManager` | ID/base, allocation `0x424`, stateless leaf, обе vtable, clone, provider и пустой retail callback доказаны | назвать support adapter `0x0020DA40`, проверить debug/другую PS2 build и common fatal target `0x00403A88` |
| P1 | остаток `spSubscriptionManager` | общий ID/base, exact size `0x20`, singleton, 12-byte tree tail, subscribe/unsubscribe/dispatch, duplicate suppression и вызов object notification slot доказаны | найти original names/TU, notification hierarchy/key type, точный PC subscribe entry и mutation/order semantics callbacks |
| P1 | остаток `spFontManager` | общий ID/base, singleton, null common factory/clone, PC `0x3C` против PS2 `0x38`, intrusive font array, first-name lookup, две default references и initialization доказаны; оба platform leaf сопоставлены | найти common TU/header/API и container typedef, точный comparator, роли оставшихся state words и concrete `spFont`/renderer resource graph |
| P2 | остаток `spPCFontManager` / `spPS2FontManager` | exact PC `.cpp`, IDs/bases/factories/vtables, storage-free leaves и blank clone доказаны; только PC добавляет backend-buffer stage | назвать PC helper/type и ownership `+0x28/+0x2C`; найти PS2 source evidence и объяснить inherited-only initialization |
| P1 | остаток `spInputManager` | общий ID/base, null factory/clone, singleton и PS2 third-interface/ready-byte prefix доказаны; platform leaves разделены | найти common TU/header, точный PC common tail и original interface/method/event names |
| P1 | остаток `spDXInputManager` | ID/base, three vtables, DirectInput/keyboard/mouse, capacity 4, count `+0x3C`, controller array `+0x40`, init/poll/destruction and observed extent `0x50` доказаны | обойти `.rld` factory для direct sizeof, назвать COM interfaces/options/status enum и восстановить exact polling state |
| P1 | остаток `spPS2InputManager` | ID/base, exact `0x48`, three vtables, two controllers, two auxiliary devices, 14 device slots and lifecycle доказаны | назвать PS2 SDK/device classes, `+0x28..+0x3F`, button mask enum and all 14 operations |
| P1 | остаток `spDebugManager` | общий ID/base, factory/clone, singleton и exact `0x38` доказаны; PS2 tail содержит 20-entry cursor, 12 flags и пять owned resources, PC destructor releases two globals | найти original TU/header/API, exact PC tail, renderer resource identities, table/sentinel type and meanings of all flags; не смешивать с direct-base game class `wxDebugManager` |
| P1 | остаток `spEntityManager` | общий ID/base, factory/blank clone, singleton storage, PS2 exact `0x20`, PC observed prefix `0x20`, list add/remove/clear и notification dispatch доказаны | найти original TU/header/API и direct PC sizeof; назвать STL/support types, event protocol `0x1C/0x1E`, callback mutation semantics и concrete owned entity graph |
| P1 | остаток `spTemplateManager` | общий ID/base, singleton, factory/blank clone, PS2 exact `0x24`, PC observed prefix `0x24`, normalized find, append/refcount and clear/release доказаны | найти original manager TU/header/API, direct PC sizeof/add entry, `+0x20`, normalization rules and exact `spTemplateObject` ownership contract |
| P0 | остаток `spTemplateInstance` | ID/base, exact `0x28`, platform list ABI, fresh named `spNode` root, notification forwarding, destruction and blank-runtime clone доказаны на PC/PS2; portable root seam заменён настоящим `spNode` | найти original declaration path, `+0x14`, element/key contracts и relationship algorithms |
| P0 | остаток `spTemplateObject` | exact source path, ID/base, exact `0x1B0`, layout, uninitialized serializer state/path, four-way dispatch, cleanup and selective clone доказаны на PC/PS2 | назвать states/fields, закрыть ownership `+0x14/+0x1C/+0x24/+0x28`, opaque blocks/defaults и entity/behavior creation contract |
| P0 | остаток `spTemplateSerializer` | exact source path, ID/base, exact `0x1F4`, descriptor fields/XML vocabulary, target/input and owned-output boundaries, blank clone доказаны на PC/PS2 | закрыть exact methods, Type/status enums, binary/read-write grammar, child recursion, ID resolution and property-stream output |
| P0 | остаток `spGameLevel` | ID/base, exact `0x2C`, platform list ABI, owned `spTemplateInstance` elements, cleanup and name-only clone доказаны на PC/PS2 | найти original TU/API, trailing `+0x20/+0x24/+0x28`, PC list operations and per-instance progress contract |
| P0 | остаток `spGameLevelSerializer` | exact source path, ID/base, `0x184`, binary magic/header boundary, `0x168` record and level/template/instance flow доказаны на PC/PS2 | закрыть full header/XML vocabulary, path/load flags, partial-insert rollback, write format and progress callback |
| P1 | остаток `spIndexBuffer` | общий ID/base, exact `0x28`, полный field layout, четыре count transforms, 16/32-bit payload, stream grammar, release, blank RTTI clone и отдельный deep copy доказаны на PC/PS2 | найти original TU/header/API и enumerators `eIndexBufferType`, семантику flags выше bit 0, прямую PC-пару deep-copy helper и error-code schema |
| P1 | остаток `spVertexBuffer` | общий ID/direct base, exact `0x5C`, 22 offset fields, component-mask widths, allocation/release, stream grammar, blank RTTI clone и отдельный deep copy доказаны на PC/PS2 | найти original TU/header/API и enum spellings, назвать редкие component bits, `m_uFlags`, raw/external init contracts и PS2 deep-copy boolean |
| P1 | остаток `spResource` | общий ID/base, concrete factory, storage-free exact `0x14`, name-only clone и destructor-to-resource-manager remove-first hook доказаны на PC/PS2 | найти original TU/header/API; portable teardown намеренно не создаёт manager из destructor, как делает PS2 |
| P0 | остаток `spResourceManager` | общий ID/direct base, singleton, exact PS2 `0x2C`/PC `0x30`, entry `0x08`, reserve/category/name/cache/load edges; PC lazy factory/empty teardown исполнены | original header/TU/method names, роли `+0x14/+0x1C`, nonempty PC cache, `.stx` helper, async ABI; FAT cache-hit/miss materialization |
| P0 | остаток `spMesh` | общий ID/base, null factory/clone, exact `0x50`, dual-vtable prefix, bounds flag/min/max, indexed bounds pass и trailing component/primitive/vertex counts `+0x44/+0x48/+0x4C` доказаны PC/PS2 | назвать secondary interface/support object, init API, PC constructor, bounding-sphere/min-max synchronization и связь с 32-bit indices/vertex formats |
| P0 | остаток `spDXVertexBuffer` / `spDXIndexBuffer` | PC-only IDs/direct base, exact `0x20/0x1C`, vtables, blank clones, COM lifetime, D3D9 create signatures и combiner-параметры доказаны; safe host storage и тесты готовы | найти original headers/TU/method names, назвать `vertex +0x18` и `index +0x14`, извлечь protected constructor/factory bodies, device-loss/reset policy и связь с `spDXVertexDeclaration` |
| P0 | остаток `spDXMeshCombiner` | exact `SparkplugDX/spDXMesh.cpp`, PC-only `0x2C`, весь state, lifetime, единственный vslot, dynamic INDEX16/VB init, lock/commit/unlock и active global доказаны; safe portable helper подключён к `spDXMesh` | найти original header/method names и тип пятого неиспользуемого аргумента, восстановить native failure rollback и полный serializer dispatch |
| P0 | остаток `spDXSharedMeshData` + serializer | CP104: exact factory sizes `0x1C/0x14`,110 точных wire bytes, Init/copy/cleanup, cursor8, read/write errors; настоящий COM Create failure даёт null read в4C2AB6; safe bounded source readers | назвать `+0x10`, original API, combined-VB ownership, shared inline framing в целом save/load graph, другие Init failures и device-reset lifetime |
| P0 | остаток `spDXMesh` | PC-only ID/base, observed `0x88`, две vtable, lifetime, common metadata, standalone CPU/GPU, combined/shared paths, packed expansion и exact FVF map доказаны | direct sizeof/protected bodies, original API, renderer mapping `0x004AE0E0`, device reset, exact rollback и draw consumer |
| P0 | остаток `spDXMeshSerializer` и optimizer materializer | PC-only ID/direct base/target, observed `0x14`, обе vtable, полный scalar wire order, shared relationship ID, read materialization и write/index optimizer calls доказаны; safe codec round-trip готов | назвать interface/header и optimizer types; разобрать `0x004BEDF0` и `0x004C07E0`, common relationship framing, shared-buffer ownership/deduplication и partial-read rollback |
| P0 | остаток `spDXCombinedVB` / `spDXSceneGraphOptimizer` | combined-VB ID/direct base, observed `0x3C`, vtable/lifetime, list+map ownership, raw payload, five-word mesh association и save-side роль доказаны; common traversal подтверждён; DX prefix уточнён до `0x54`, разделены lookup/range/CombineData VM-entry | восстановить DX ownership/singleton, protected add/group/lookup bodies, physical record layouts, grouping/dedup, failure rollback и original paths/API; определить PS2 судьбу common implementation |
| P0 | остаток `spRenderer` / platform leaves | RTTI/layout/29 interface slots; PC native alpha2048×24/general20/bucket8 queues, sort/flush/failure/no-ref; PC typed pass slots no-op; math192 comparisons | source queue/Scene/backend wiring, global75F8E8 startup, alpha CRT tie order, protected ctor100k cap, remaining platform operations/device ownership/reset; PS2 deferred |
| P0 | остаток `spRenderTarget` / cube / manager | три RTTI-ветви, common/cube layouts, PC surfaces, exact PS2 размеры, format matrices, reset path и три manager lists доказаны; renderer slots теперь связаны: PC ordinary/cube `1/0`, PS2 `0/1`, причём PS2 cube диагностически unsupported | назвать остальные target-interface slots, закрыть `.rld` PC allocation sizes, backing ownership и failure rollback; не считать concrete PS2 cube factory аппаратной поддержкой |
| P0 | остаток `spMaterialTexture` / render-target leaves | RTTI/layout/target/fallback/manager/recursion/camera/cube state и serializer fields доказаны; render methods теперь связаны с target bind, begin/end/clear, camera render и PS2 unsupported cube path | восстановить original signatures двух leaf slots, intrusive ownership, protected PC bodies и movie backend; проследить draw provenance внутри camera traversal |
| P0 | остаток `spCamera` / concrete leaves | common/concrete IDs, PC observed `0x238`, PS2 exact `0x250/0x340`, defaults, viewport, dirty masks, projection/frustum state, clone и renderer matrix/viewport operations доказаны; portable code и scanner 37/37 готовы | original paths/API, opaque state blocks, full view/world update, frustum consumers, camera-manager ownership и direct PC allocations; не сливать serialized Is2D с отдельным projection-branch byte |
| P0 | остаток `spRenderNode` | PC exact1D4/14+6, source virtual world/getters/lazy caches, original copy append/independent Model/spheres/control fields, source light binding;2272 comparisons | original header/TU/API/support type,118/11C, automatic Scene/Partition/Occlusion and renderer; native detach469F50; callbacks external lifetime |
| P0 | остаток `spRenderable` | PC exact58/raw28, nonempty cdecl5 groups/erase/ordinal, direct AL gates, shared material save/failure paths; source callback phases и576 state comparisons | original TU/API/field names, full source pre/post/renderer wiring, current-material cache consumers, upstream valid pass/reentry invariants; PS2 deferred |
| P0 | остаток `spModel` | actual PC exact60/default3, Mesh getters, clone/shared mesh, full nonempty callback/alpha queue pre/mesh/post protocol; source mode hook/raw28 copy fixed | original draw/bounds names, projection enum, full source draw/backend wiring; PS2 classifier not imposed on PC |
| P0 | остаток `spMeshData` | PC exact58 ctor independently run, uninitialized50/54/minmax, sphere0/validfalse; explicit cold-owner-zero teardown through ResourceMgr, source safe-null divergence | original TU/API, upstream Init/rollback lifetime invariant, secondary interface, full original buffer materialization; cold teardown is not Init |
| P0 | остаток `spDXMeshData` | общий ID/base, owning-buffer prefix, PS2 exact `0x44`, conversion из `spMeshData`, destruction, blank clone и serializer boundary доказаны | получить прямой PC `sizeof`/constructor, назвать `+0x14` и PS2 tail `+0x20..+0x43`, восстановить original API и cross-platform header words |
| P0 | остаток `spPS2MeshData` | общий ID/base, PS2 exact `0x100`, PC observed extent `0x100`, constructor tables, packet lifetime, component normalization, planning helpers и serializer connection доказаны; `+0x30/+0x34` определены как emitted vertex/primitive counts | получить прямой PC `sizeof`/constructor, original API/enums и имена счётчиков, полностью описать descriptor formats, `+0xF8` и binary/external packet grammar |
| P0 | остаток `spPS2Mesh` | PS2-only ID/direct `spRenderMesh` base, exact `0x58`, обе vtable, owned prepared data/helper, conversion/attach/release, blank clone, перенос counts и draw-preparation consumer доказаны | найти original header/TU/API, тип и manager packet-emitter-а, сигнатуры/семантику трёх draw operations и полный failure rollback |
| P0 | остаток `spTexture` | ID, разные C++/registration base, PS2 exact `0x38`, PC observed extent, `spITexture`, null clone, Init-state и dimension normalization доказаны | восстановить четыре точные сигнатуры `spITexture`, имена `+0x18/+0x1C/+0x31/+0x34`, buffer wrapper и backend lifetime через `spTextureData`/platform leaves |
| P0 | остаток `spTextureBuffer` | общий ID/base, exact `0x30`, default format `6`, ownership `+0x1C/+0x24`, blank clone, Init и pixel-size map доказаны на PC/PS2 | найти original TU/header, enum spellings, имя третьего `u16`, тип auxiliary object `+0x24`, deep-copy API и platform-upload contract |
| P0 | остаток `spTextureData` | общий ID/base, exact `0x4A0/0x498`, embedded buffer, container ABI split, name-only clone, copy и selected payload cleanup доказаны | назвать flags/record types, разобрать `0x410`, четыре interface slots, serializer-to-record mapping, rollback и platform upload lifetime |
| P0 | остаток `spNode` | общий ID/base/factory, exact PC `0xB4` и PS2 `0xC0`, local matrix/PRS, flags, parent/child lifetime, root walk, recursive mask `0x200`, destruction и deep child clone доказаны | найти original header/TU/API, тип scene link `+0x3C`, dirty/cache masks, world-transform/bounds/name-search slots, collision API и legacy animated reader semantics |
| P0 | остаток `spLight` | общий ID/base, null factory/clone, PC exactF0/PS2 exact100, PC copy intensity omission, previousB8/nextBC scene list и dirty8→LightManager refresh executed | original TU/API/support type, opaqueDC, property-setter invalidation, full portable virtual world/scene/backend wiring |
| P0 | остаток `spLightData` | exact PC original factoryF0, concrete clone/copy независимо подтверждают intensity1, actual scene reparent/world consumption и девять serializer offsets | original constructor symbol, native root clone transaction, full portable Scene/renderer integration; serializer lifecycle остаётся самостоятельным |
| P0 | остаток `spLightManager` | original24 factory/lifetime, borrowed lists, scene cache propagation, helper28 with8 ordinary+first ambient;90 native/22 static и1800 portable comparisons | original support/cache names, concrete partition payload type, full SceneInit/wiring, backend light upload; ctor owner20 untouched, не использовать standalone refresh как initialized scene |
| P0 | остаток `spSerializer` | общий ID/direct base, null factory/clone, dual-vptr `+0x10`, PS2 exact `0x14`, PC observed `0x14`, identity class-ID hook и центральные `SBOO` load/save entry доказаны; object-header reader фактически не проверяет marker, strict helper отделён; PS2 relationship resolver рекурсивно materialize-ит inline entry без отдельного глобального fixup | назвать secondary interface/callbacks, полностью перенести relationship resolver/ownership/rollback и получить прямой PC sizeof |
| P0 | остаток `spSerializerManager` / FAT helper | manager и основной PS2 FAT-срез перенесены: exact `0x2C/0x64`, registry/header, index grammar, RTTI validation, lookup/cursor/clear, object indexing и safe file-index compatibility path; уточнено, что LoadIndex не инициализирует byte `+0x1C` | установить original identity helper-а `+0x28`, enum/name политики `+0x18`, save-side происхождение `fileID +0x08`, PC container ABI, payload writer, object materialization, fixup и rollback contract |
| P0 | остаток `spSerializerHook` / platform hooks | общий base и PS2 leaf перенесены; exact PS2 slot обеспечивает lazy manager и больше не использует FAT/stream, все 25 инструкций закреплены; PC factory — доказанный SecuROM thunk `FF 25 98 2D 3B 01` | назвать slot `+0x24`, извлечь настоящее PC body/signature/TU и объяснить пустую PS2 platform branch; не подменять DX leaf догадочным no-op |
| P0 | остаток `spNodeSerializer` | общий ID/direct registered base, C++ base, factory/blank clone, PS2 exact и PC observed `0x14`, target `spNode`, field IDs, read/write order, defaults и relationship traversal доказаны | original header/interface names, direct PC sizeof, stream/status types, quaternion codec, `spCollisionInfo`, forward fixup/rollback и legacy field 8 |
| P0 | остаток `spRenderNodeSerializer` | общий ID/direct `spNodeSerializer`, target, factory/clone, PC observed и PS2 exact `0x14`, повторяемый field `0`, `spRenderable` validation, attach/index/write order и code `7` доказаны; portable plan/attach seam готов | original header/interface name, direct PC sizeof, common relationship framing/fixup, stream status, partial-read rollback и original enum spelling кода `7` |
| P0 | остаток `spLightDataSerializer` | общий ID, flattened registered base, C++ `spNodeSerializer` base, PS2 exact/PC observed `0x14`, factory/clone/target и node+light writer plans доказаны | header/interface names, PC sizeof, absent-enabled loader lifecycle, exact ARGB conversion, stream status и rollback |
| P0 | остаток `spLightSerializer` | общий ID, direct C++/RTTI base `spNodeSerializer`, PS2 exact/PC observed `0x14`, factory/clone, target `spLight` и node+light writer plans доказаны | original paths/interface names, PC sizeof, absent-enabled loader lifecycle, exact ARGB conversion, stream status и rollback |
| P0 | остаток `spRenderableSerializer` | общий ID/direct `spSerializer` base, PS2 exact/PC observed `0x14`, factory/clone, target `spRenderable`, resource indexing и четыре field contracts доказаны | original paths/interface names, PC sizeof, concrete material/fog types, stream framing/status и relationship fixup/rollback |
| P0 | остаток `spModelSerializer` | filename, общий ID/direct `spRenderableSerializer` base, PS2 exact/PC observed `0x14`, factory/clone, target `spModel`, indexing и два own-field contracts доказаны; mesh type исправлен на `spMesh` | full source path/header/interface names, PC sizeof, non-null mesh validation stage, projection enum/default, stream/fixup/rollback |
| P0 | остаток `spMeshDataSerializer` | exact PC source path, общий ID/direct `spSerializer` base, PS2 exact/PC observed `0x14`, target, factory/clone, mode gate и index-then-vertex payload доказаны | header/interface names, PC sizeof/helper address, selector enum, stream framing/status и rollback |
| P0 | остаток `spPS2MeshDataSerializer` | exact PC source path, общий ID/direct `spMeshDataSerializer` base, PS2 exact/PC observed `0x14`, target/factory/clone, три fields, writer order и platform-разный native reader mask доказаны | header/interface/config names, direct PC sizeof, descriptor field names, DMA/VIF/GIF packet grammar, alignment/status/rollback |
| P0 | остаток `spDXMeshDataSerializer` | exact PC source path, общий ID/direct `spMeshDataSerializer` base, PS2 exact/PC observed `0x14`, target/factory/clone, два fields, reader masks и FVF/size header доказаны | header/interface/config names, direct PC sizeof, full FVF mapping, Direct3D buffer codec/device ownership, status/rollback |
| P0 | остаток `spTextureDataSerializer` | exact PC source path, общий ID/direct `spSerializer` base, PS2 exact/PC observed `0x14`, target/factory/clone, source fields `2/3/4`, local fields `6/0`, mode gate и raw size доказаны | header/interface/config names, direct PC sizeof, attached-source API, canonical reference resolver, container mapping и status/rollback |
| P2 | `spPCFileStream` original header/member spelling | exact `.cpp`, class/base ID, размер `0x20`, единственный `HANDLE +0x1C`, все virtual methods и Win32 state machine доказаны | искать header fragments/другую PC build; до этого host x64 class остаётся portable, а точный layout — в Analysis/PC |
| P0 | `spPS2AsyncFileStreamManager` queue producer | class/base IDs, layout `0x37A0`, 50 records по `0x11C`, synchronous public request и update/RPC consumer доказаны; ни одного CPU writer/increment для `queuedCount +0x3794` в ELF нет | искать overlay/IOP modules и условные другие builds; не объявлять dormant queue рабочей без producer evidence |
| P1 | `spAsyncFileStreamManager` source/support type | общий class ID/base, layout `0x18`, global lifetime и request/update contract доказаны на PC/PS2; `+0x14` является полиморфным singleton/dispatch support, но original type name неизвестен | искать header fragments/template RTTI; проверить точный return type request и original callback typedef |
| P2 | `spPCAsyncFileStreamManager` error sentinel | stateless layout `0x18`, factory/clone/vtable и полностью synchronous request path доказаны; failed `GetSize` оставляет local равным `0xBB40E64E` из `0x00744230` | проверить значение в другой PC build/runtime и callers; portable code намеренно использует safe zero, чтобы не воспроизводить runaway allocation |
| P1 | `spPS2FileStream` platform types/names | class/base IDs, vtable, layout `0x114`, embedded `ShellFile 0x98`, normal buffering/seek и граница async path доказаны; source path и original field spellings отсутствуют | искать SDK/library symbols или ещё одну PS2 build; связать resolver outputs с PCK/FAT и уточнить `+0x21/+0xFC/+0x90` |

## Дополнение: `spPS2TextureDataSerializer`

Подтверждены exact PC source path, Class ID/direct base, PS2 exact/PC observed
`0x14`, target ID, source-wrapper, field order, platform type `8/9`, разные
native-load masks PC `0x02` / PS2 `0x08`, palette sizes и 20-байтная mip-запись.
Открыты original header/interface/config names, direct PC `sizeof`, имена
`auxiliaryValue` и четырёх descriptor words, полная pixel-format/swizzle table,
ownership/alignment, status/rollback и полный layout target `spPS2TextureData`.

## Дополнение: `spDXTextureDataSerializer`

Подтверждены exact PC source path, Class ID/direct base, PS2 exact/PC observed `0x14`,
target ID, source-wrapper, field order, platform type `6/7`, native-load masks PC
`0x02` / PS2 `0x08`, field IDs `0/1`, 16-байтная in-memory mip-запись и wire-size
`rowStride * height`. Открыты original header/interface/config names, direct PC
`sizeof`, pixel-format mapping, семантика byte-флагов, Direct3D creation/ownership,
alignment/status/rollback и полный layout target `spDXTextureData`.

Отдельная RTTI-регистрация/строка `spDXTextureData` не найдена ни в PC, ни в PS2;
пока известен только target ID `0x0B1C67BB`. Не объявлять portable target до появления
factory/layout/lifetime evidence.

## Дополнение: `spMaterialSerializer`

Подтверждены exact PC source path, Class ID/direct `spSerializer` base, PS2 exact/PC
observed `0x3C`, embedded data-block state `+0x14`, lifetime, стандартная field grammar,
legacy/current texture-state IDs, relationship index order и все редкие layer branches.
Fields `7`, `13..16`, шесть wrapper class ID, common render-target payload и clamp
cubemap `1..6` теперь доказаны. Открыты original header/helper type name, direct PC
allocation/secondary vtable, enum names, status/rollback, ownership/fixup и game
mutation test для отсутствующих в корпусе ветвей.

## Дополнение: `spMaterialDataSerializer`

Подтверждены Class ID/direct `spMaterialSerializer` base, target `spMaterialData`,
PS2 exact/PC observed `0x3C`, отсутствие derived state, lifetime, thin wrappers и
null-target read guard. Открыты original header/source path и interface/signature names,
direct PC allocation, reader status/rollback и in-game mutation test. Target
`spMaterialData` теперь отдельно закрыт до PC-observed/PS2-exact layout, defaults,
accessors и blank-clone contract; у общего `spMaterial` остаются неизвестные поля и
точный pass state-application dispatch. Runtime pass/layer ownership, capacity `8`,
standard nested texture и deep clone уже закрыты отдельной карточкой.

## Дополнение: platform material-data serializers

Для `spDXMaterialDataSerializer` и `spPS2MaterialDataSerializer` подтверждены общие
direct base, размер и inherited material contract, но раздельные RTTI/lifetime/vtables.
Открыты original header/source path, interface/signature names, direct PC allocations,
точный механизм выбора serializer-а и platform conversion в уже подтверждённый
`spPS2Material`, а также game mutation test. Отдельный target-hook не добавлять: в обеих vtable он
отсутствует.

`spPS2Material` закрыт до PS2-only identity/direct `spMaterial` base, exact `0xD0`,
обеих vtable, color/power offsets, полноценного common+tail copy/clone и pass-update
consumer. `0x001700B0` раскрыт до per-layer stage selection и renderer
texture-transform slot `23`. Открыты original path/API, native default `+0xC0`,
имена generation/update и остальные texture/render-state operations.

## Дополнение: `spCameraSerializer`

Подтверждены direct node-serializer base, target, размер и wire fields `0/1`.
Runtime layout/projection/viewport вынесены в отдельную карточку `spCamera`.
Открыты original paths/names, manager-driven load initialization, datablock
encoding enum, protected PC paths, status/rollback и game mutation test.

## Дополнение: `spCameraDataSerializer`

Подтверждено, что serializer с таким именем реально targets `spCamera`, а не
`spCameraData`. Открыты причина registry design, original paths/signatures, load-helper
семантика, direct PC allocation и runtime dispatch между двумя camera records.

## Дополнение: `spFogSerializer`

Подтверждены class/base/target IDs, обязательный field 0, порядок всех пяти
значений, PS2 exact/PC observed размер serializer-а, lifetime и vtables. Сам
`spFog` теперь закрыт до полного `0x28` layout/defaults/blank clone, а renderer
slot `26` доказывает карту disabled/exp/exp2/linear. Открыты точные source/header
paths и имена интерфейса, смысл success-only serializer slot, rollback частичного
чтения, PS2 downstream GS/VIF детали и контролируемый in-game mutation test.

## Дополнение: `spMatColorControllerSerializer`

Подтверждены обе платформы, class/base/target, `0x14`, пять evaluator roles и их
одинаковые target offsets. Открыты исходные пути/имена, layouts и формулы
`spColorFuncEval`/`spFunctionEval`, смысл success/finalize slot, rollback
частичной вложенной секции, причина отсутствия объектов в PS2-корпусе и runtime
mutation test.

## Дополнение: `spLightControllerSerializer`

Подтверждены class/base/target, `0x14`, полная схема ID `0..8`, offsets, wire kinds,
default suppression, вычисление reciprocal frequency и отдельный relationship pass
на `spLight`. Отдельно подтверждено различие default ARGB: `0xFF000000` на PC и
`0x00000000` на PS2. Открыты исходные пути/имена, полный `spLightController`, enum и
runtime-формула, причина цветового различия, zero/NaN semantics, ownership/fixup,
rollback повреждённых данных и контролируемый in-game mutation test.

## Дополнение: `spAnimTexControllerSerializer`

Подтверждены class/base/target, `0x14`, обязательный field 0, порядок
`count -> float times -> texture relationships`, target offsets `+0x3C/+0x40/+0x44`
и relationship gate `spTexture`, а не только concrete `spTextureData`. Открыты
original paths/names, полный controller lifecycle, ownership/refcount различия,
frame-count limits и overflow, runtime validation времени, playback semantics,
rollback inline textures и контролируемый in-game mutation test.

## Дополнение: `spUVControllerSerializer`

Подтверждены class/base/target, `0x14`, обязательный field 0, embedded transform
по `spUVController + 0x4C`, порядок семи functional evaluator-ов и двух `Vector3`,
а также отсутствие relationship pass. Открыты original paths/names, полный
controller layout, интерфейс временного `spTransFunctionEvalSerializer`, enum и
runtime-математика функций, time/wrap/NaN semantics, error rollback и
контролируемый in-game mutation test.

## Дополнение: `spTransFunctionEvalSerializer`

Подтверждены отдельная RTTI identity, direct base, target, `0x14`, field 0 типа 5,
порядок и offsets семи evaluator-ов и двух векторов, отсутствие relationships.
Открыты original paths/names, остаток target layout, original field/getter names,
`FunctionType` и runtime-математика, default/NaN/time/wrap semantics, error
behavior и контролируемый in-game mutation test. PC reader/writer защищены
непрямыми entry-переходами, поэтому их содержательное подтверждение пока опирается
на vtable identity, независимый PS2 body и совпадающий корпус.

## Дополнение: `spFunctionEvalSerializer`

Подтверждены native class name, class/base/target IDs, `0x14`, шесть scalar fields,
их offsets/default suppression и reciprocal Frequency. Открыты original paths,
точное RTTI-имя target-а `0x9450E590`, target lifecycle/layout, enum и runtime-
формулы, zero/NaN/Inf semantics, time/wrap behavior и in-game mutation test.

## Дополнение: `spColorFuncEvalSerializer`

Подтверждены native class/target names, class/base/target IDs, `0x14`, восемь
scalar fields, offsets/default suppression, reciprocal Frequency и отдельные
platform default colors. Открыты original paths, target lifecycle/layout,
причина цветового различия, enum и runtime-интерполяция, zero/NaN/Inf semantics
и контролируемый in-game mutation test.

## Дополнение: `spSphereBVSerializer`

Подтверждены class/base IDs, `0x14`, position/radius fields, epsilon `0.001`,
writer source offsets и reader mirror offsets. Target ID `0x390946D2` доказан
корпусом, но не отдельным serializer virtual hook. Открыты original paths/names,
полный target inheritance/layout, смысл и синхронизация зеркал, invalid-radius
semantics и контролируемый in-game mutation test.

## Дополнение: `spBoxBVSerializer`

Подтверждены class/base IDs, `0x14`, position/full-size fields, epsilon, mirror
position и вычисление half-extents/bounding sphere. Открыты original paths/names,
полный target layout, invalid-size semantics, runtime resynchronization и in-game
mutation test. Target ID `0x7B4C0876` остаётся corpus-связью без virtual hook.

## Дополнение: `spOBBBVSerializer`

Подтверждены class/base IDs, `0x14`, position/full-size/rotation fields, epsilon,
matrix-to-quaternion write path, quaternion-to-matrix read path и derived sphere.
Открыты original paths/names, полный target layout, точная quaternion convention,
normalization/invalid-value semantics, runtime resynchronization и in-game
mutation test. Target ID `0x4DA04889` остаётся corpus-связью без virtual hook.

## Постоянная проверка по ходу исследования

При разборе каждого следующего класса или subsystem дополнительно проверяются:

1. доступы к ещё неизвестным offsets уже восстановленных base-классов;
2. совпадения constructor/destructor helpers и secondary vtables;
3. callers неизвестных функций из этой таблицы и новые строки/asserts рядом;
4. различия PC/PS2, которые нельзя автоматически сводить к одному layout;
5. связи с resource loader, serializer, scene ownership и renderer.

Находка считается закрывающей вопрос только после записи адреса, платформы,
контрольного executable и наблюдаемого consumer. Отрицательный поиск также
фиксируется в карточке класса, если он ограничивает область дальнейшего поиска.

## PC Node serialization checkpoint9

[Node field/ref/write evidence](native-pc-node-serialization.md): factory14,
scalar rules and owning children now have original/source exact comparisons;
real object.smo passes staged-native outer versus source whole loader. Full
Node/Save is not closed. Collision/reparent/scene registration/lossless/error
remain open. Constructed whole Node422B50@88FDE3 and gameover outer@89A355
hit100k and are disabled; no retry/resume/cap increases. Second stop occurs
during third Node factory, before attach; underlying cause remains unknown.

## Очередь после SparkBase

Актуализация CP19–29: nonempty Texture/Anim/UV и prebound Color relationships
проходят общий reference core; scalar/color/PRS evaluators и finite controller
consumers имеют source/native сравнения. Полный Color factory остаётся capped.
DXShaderLayer имеет actual factory/lifetime/copy, но его корректный RTTI
отклоняется оригинальными common layer helpers до shader-specific tail.
Ниже сохранены исторические checkpoint-ы; текущие проценты брать из platform
ledger/goal report, а не складывать старые локальные статусы.

Checkpoint13 [PC standard material graph](native-pc-material-standard-graph.md):
header MaterialData/DXData42F4C0 создаёт runtime spDXMaterial797B39EC, а не
файловый6160348B. Actual Model→Material→Pass→Std read/index/write замкнут на
четырёх bounded graphs. Pass→Layer→MaterialTexture — direct-delete, не
intrusive; UV9 имеет40bytes, zeroflag не сбрасывает старую матрицу.
Открыты nonempty Texture/Color/UV/Anim links, DXShaderLayer71643E66,
полный native clone graph, render application и whole Save. DX power+B8
ctor не инициализирует; host writer требует явно заданного значения.

Checkpoint12 [PC Material](native-pc-material-scalar.md): прежний PC prefix80 /
MaterialDataC4 оказался PS2-derived предположением. Actual PC78/BC и interface
6DE9D0 подтверждены; word10 Fog/Material — physical NamedObject при direct
BaseObject RTTI. Common4671F0 всё равно не переносит FAT-name в эти типы.
Material scalars/empty passes замкнуты, full layers/ColorController423650,
base-copy policy и renderer остаются открыты. Whole logo-field native scout
достиг32KiB allocation guard и отключён, безповторов/увеличениялимитов;
точный failing request/PC не записан, причина не объявлена доказанной.

Checkpoint11 [Fog codec/owner](native-pc-fog-serialization.md) замкнул
nonempty Model/Fog. Новый конкретный lifetime риск: clear удаляетsole-owned
Fog, но native FAT entry.object остаётся freedaddress. Ни повторный resolve,
ни UAF не запускались. Host context pinning — безопасное отличие, не доказанная
native очистка. Native partial-word read mutation также явно отделена отhost.

Checkpoint10 [scene sections](native-pc-scene-serialization.md) добавил
native/source RenderNode→Model read/index/write и concrete adapters. Nonempty
Model mesh, Renderable material/fog, Skin и textures ещё требуют замыкания.
Whole RenderNode NULL diagnostic4169DF→0033D03C не исполнена доreturn;
pre-diagnostic stop469288 — отдельная ограниченная проверка, не fake-success.

- bootstrap и ownership `spApp/spEngineCore -> wx...`;
- raw-file/PCK resolver и stream lifetime;
- FAT object creation и два relationship-fixup прохода;
- serializer dispatch до runtime object;
- scene/partition ownership, culling и draw provenance.

При переходе к этим вертикалям активные вопросы SparkBase не закрываются
формально: найденные по пути xrefs сначала проверяются против таблицы выше.
