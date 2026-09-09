# Цикл переноса ядер tools — 9 сентября 2026, до 19:00 МСК

Начат в 08:12 МСК от checkpoint `1495999`, Viewer `4a407fd`, TextureTool `1ddb50f`.
Срок следующего отчёта: **2026-09-09 19:00 Europe/Moscow** (`16:00 UTC`).
Статус: в работе. Предыдущий цикл закрыл 17 блоков; все ядра ещё не готовы.

## Уточнение пользователя и границы

Продолжать перенос используемой логики в общие восстановленные классы. Три
ранее обсуждавшихся редакторских случая отложить до плотной работы с LVLcreator:
packed combiner cached byteSize, редкие lossless формы заголовков, обратная
запись world при неравномерном parent scale. Они не задерживают независимую
миграцию. Это не согласование новых редакторских алгоритмов.

В первом случае сохраняется оригинал: в standalone пути три вершины имеют
stride28 и byteSize84; у combiner в служебном cached member наблюдается120
при физической аллокации84. Значение поля не доказывает чтение120 байт из файла.
Не увеличивать чтение/копирование до120 без подтверждённого оригинального вызова.
Проверить потребителя этого поля в нужной софту ветке; не подправлять игру.

Начать с незавершённого StaticRenderObject и необходимых скалярных/ссылочных
readers, затем материалы/skin и оставшиеся ресурсы. Недостающий контракт
исследовать адресно, внедрять один раз. UI вторичен, выпуск не выполняется.
Сохраняются бюджет около1 ГиБ, сборки по2 workers, короткие подходящие выборки,
точечные original-PC сравнения и локальный commit каждого законченного блока.

## Результаты

### Блок1: StaticRenderObject и общий render support

Матрицы читает исходный serializer, C# restrictions/order/inverse classification
удалены. Original5 scalar readers/writers совпали с source/ABI побайтно;3 guards,
C++ Static52/RenderNode34/FullLoader213 прошли. Настоящий класс зарегистрирован
в общем graph. Общий append/sphere support выделен из RenderNode и используется
обоими классами, без второй реализации. Старый inverse editor helper перенесён
без изменения в Editing и явно назван legacy; LVLcreator review отложен пользователем.
Досье: `docs/research/tool-static-render-object-shared-core-2026-09-09.md`.

Проверка текущего потребителя packed mesh byteSize подтвердила: RenderMeshView
получает его из standalone mesh, где native84 и physical84 совпадают.
Combiner cached120 не является указанием увеличить чтение этого пути.
Ветка отложена и не задерживает перенос других readers.

### Блок2: MaterialData reader

C# grammar/state/color/UV parser заменён общим `spMaterialSerializer` через
явную metadata-инспекцию на настоящих DXMaterial/pass/layer/texture-holder.
Ссылки не материализуются подставными объектами; полный reader сохраняет
канонических владельцев. Original9 cases, source/ABI comparisons и3 host guards
прошли. C++ MaterialSerialization554/Controller393/Color225/FullLoader213,
5 выбранных SMO и сборки Viewer/Importer/LVLcreator CoreTests прошли.
Сохранены NULL preserve для controllers, NULL clear для Texture и исходное
UV-zero поведение; сняты лишние C# ограничения на порядок и флаги.
Ограничение старого C# DTO одним стандартным слоем на проход явно отделено
от возможностей общего ядра. Досье: `docs/research/tool-material-reader-shared-core-2026-09-09.md`.

### Блок3: Model/Skin reader

Оба C# decoder используют общие Renderable→Model→Skin readers. Удалены
самостоятельный разбор секций, UInt32, palette/matrices и проверки affine/inverse;
ID инспектируются без выдуманных Node. Общий помощник reference inspection
используется также материалами. Original11/source/ABI совпали,4 host guards,
SkinSerialization119/MaterialSerialization554/FullLoader213 прошли. Material9
captures повторно использованы без новых оригинальных запусков. Выборка5 SMO
и Viewer/Importer/LVLcreator CoreTests builds прошли. Старый DTO требует bound
MeshData; это явно отделено от permissive missing-mesh native reader.
Досье: `docs/research/tool-model-skin-reader-shared-core-2026-09-09.md`.

### Блок4: Animated texture reader и выбор ключа

Общий AnimTexControllerSerializer заменил C# grammar; исходный TextureTrack
выбирает индекс без подставных Texture. Original5/source captures и33 выборов
совпали, inline graph capture переиспользован. C++ Controller488/Material554/
FullLoader213,5 адресно выбранных SMO и3 consumer builds прошли. Два старых
ожидания BloomX исправлены по реальным references и сохранённым BGRA байтам.
Часы UI и эвристики bindings остаются следующей работой.
Досье: `docs/research/tool-animated-texture-reader-shared-core-2026-09-09.md`.

### Блок5: UV и material-color function readers

Две C# грамматики заменены общими nested serializers. Семь original read-time
state captures совпали с ABI побайтно;4 guards прошли. C++ UV386/Color118/
MaterialColor225/FullLoader213;5 SMO:750/748/802/937/36350 assertions;
3 consumer builds чистые. Удалена ошибочная C# подстановка scale yOffset0.
Protected color ctor не использован: ABI читает настоящие leaf evaluators.
Досье: `docs/research/tool-material-functions-shared-core-2026-09-09.md`.

### Блок6: единая material inspection для трёх потребителей

MaterialData, diffuse и render-state views используют общий reader и один
снимок на неизменяемый документ. Удалены ещё два C# parsers; сохранены последнее
state assignment и authored black. Original9 captures переиспользованы;
C++ Material554/FullLoader213,3 consumer builds и5 SMO прошли. Узкий замер
повторных чтений:5,88→1,41ms, allocations-17%; это не общий FPS/load time.
Выбор материала по физическим соседям остаётся следующей задачей.
Досье: `docs/research/tool-material-inspection-shared-core-2026-09-09.md`.

### Блок7: каталог настоящих Model/Skin resource links

Color/state views и shared-instance metadata используют реальные ссылки,
удалены подбор по соседству и ручной field0 scan активного меша. Каталог хранит
отдельные renderable occurrences; SceneBuilder получает общие Skin DTO/ошибки.
5 SMO:37284/36389/9343/1888/1608 assertions;3 consumer builds проверены.
В Alfea02 обнаружены83 общих mesh с разными material IDs у экземпляров и5 NULL
materials. Texture binding и применение instance material остаются далее.
Native/оригинал повторно не запускались: их код не менялся.
Досье: `docs/research/tool-renderable-references-shared-core-2026-09-09.md`.

### Блок8: пространственные ресурсы общего загрузчика

Семь shared readers и нужные Zone/Partition runtime classes подключены к
ResourceGraph. Сохранены direct ownership, borrowed roots/portals, reciprocal
collision links, реальные RTTI, NULL/repeat семантика и read-time debug colors.
14 original-PC/source states совпали, Spatial74/ReadReference298/FullLoader213/
RenderNode34/Collision54 checks прошли. Alfea02 полностью загружен как4266
ресурсов/489 Node; FAT identities и Node parent/child проверены по исходному
индексу. Это не Scene initialization/visibility. Host limit4096 мешал валидному
файлу; после измерения47MiB peak предел ResourceGraph повышен до8192.
5 managed samples:37284/36389/9343/3071/1608;3 consumer builds прошли.
Native spatial inspectors в C# и material/texture runtime integration остаются
следующими шагами; Octree reader ещё отсутствует. Релиз не собирался.
Досье: `docs/research/tool-spatial-readers-shared-core-2026-09-09.md`.

### Блок9: material/texture bindings загруженного графа

Удалён старый1154-строчный texture resolver с подбором по именам/соседству и
равномерной анимацией. ResourceGraph отдаёт все actual passes/layers,
canonical references, raw colors/UV и texture keys. BGRA приходит из CPU
texture, выбранной loader; снимок кешируется, native graph затем освобождается.
Каждый существующий scene instance получает собственный material; Model не
наследует чужую Skin palette. Пять файлов:2282 Model/Skin,168 uploads,
30432 snapshot/scene assertions и31 ABI guard. Native554/488/213 прошли.
Многопроходный frontend/UV/точный clock ещё не подключены: данные сохранены,
старую имитацию не подставляем. Пользователь уведомлён о потере части эффектов.
Полный draw list/placement и renderer states остаются далее.
Досье: `docs/research/tool-loaded-materials-shared-core-2026-09-09.md`.

### Блок 10: настоящие Node/Skin и render support

Runtime сохраняет загруженные Node с реальными классами и Skin bone bindings;
отдельный C# граф базовых Node и inverse-bind подстановка начальной позы удалены.
Снимок включает ordered support membership с повторами и исходными матрицами.
Пять файлов: 1985 Node, 2039 supports, 2380 members, 6078 managed checks;
Python сверил 1296 palette matrices, пять SAN кадров и 32 ABI guards.
Native 32/86/213/34/52/74 passed; три consumer builds чистые.
Пойман и исправлен неверный перенос transform PartitionSystem на baked room:
compatibility placement теперь использует фактический support Model.
Пять общих SMO regressions прошли. PS2-tagged menu runtime явно остановлен
на известном неподтверждённом material-color factory (53 контроллера);
metadata inspection не выдаётся за полноценный runtime. Пользователь уведомлён.
Полный Scene occurrence list, visibility и material runtime остаются далее.
Досье: `docs/research/tool-loaded-scene-shared-core-2026-09-09.md`.

### Блок 11: все поддержанные render occurrences

SceneBuilder использует actual container/member slots, сохраняет повторы и
собственный material/Skin каждого Model, декодирует общий Mesh один раз.
Alfea01: 1050→1141, Alfea02: 954→1008; оба состава дошли до LVLcreator editable
документа. PC menu: 99→207 слотов на 110 Model. Пять файлов: 18057 checks,
2379 C ABI comparisons, 21 guards. Три Workspace cases: 4726 checks.
Native SkinRender 237, SkinSerialization/FullLoader passed; четыре consumer
builds чистые. Original proofs переиспользованы, UI визуально не проверялся.
Новый отложенный случай: редактор склеивал повторные slots; Workspace теперь
их хранит, command model явно отклоняет REPEATED_RENDERABLE_AUTHORING до
согласованной работы над адресацией таких ссылок. Пользователь уведомлён.
Досье: `docs/research/tool-render-occurrences-shared-core-2026-09-09.md`.

### Блок 12: Exporter использует actual Node pose

Удалён второй C# FK, inverse-bind подстановка начальной позы и physical parent
fallback. Exporter получает world/parents всех actual loaded Node; только
target coordinate/local conversion остаётся в адаптере. Пять GLB readbacks:
1985 Node, 37720 assertions, max linear error 5.364418e-7, translation
0.00048828125. Три consumer builds чистые; native/оригинал не менялись.
Дальше остаются полный occurrence/material export и material runtime.
Досье: `docs/research/tool-exporter-node-pose-shared-core-2026-09-09.md`.

### Блок 13: Exporter/Importer используют все actual occurrences

Второй сборщик физических meshes/Static instances удалён. Exporter использует
общую Scene, различает Mesh geometry / Model variant / support slot и сохраняет
собственные material/Skin references. GLB/FBX переиспользуют rigid geometry;
FBX export protocol v4 отделяет транспортные ordinals от реальных file IDs,
v3 остаётся читаемым. Split/selection/CLI/GUI подключены к variant/slot keys.
SMO importer разворачивает все actual placements со своими world matrices.
Пять GLB/SDK FBX readbacks: 2379 placements, 2282 variants, 75534 checks;
selection/baked/OBJ/guards 60, importer 5349, v3 compatibility 4. Пять consumer
builds и native FBX прошли. Alfea01 GLB geometry 5,09 вместо 9,05 MB при копиях.
Static selection Skin и importer multipass явно отклоняются, ограничения
зафиксированы. Full material runtime/target shader projection остаются далее.
Native game DLL не менялась, original probes не повторялись, релиза нет.
Досье: `docs/research/tool-export-occurrences-shared-core-2026-09-09.md`.

### Блок 14: живой material runtime общей сцены

Materials API сохраняет тот же ResourceGraph, вызывает actual RenderController
Apply, material pass update и DXMaterial color frame method. Все UV submissions
и texture selection приходят из общего кода, projector общий со static snapshot.
Удалены два старых uniform texture clocks в frontend; новый автоматический
renderer frame ещё не подключён. Пять файлов: 2191 material, 88 controllers,
86 изменённых UV layers, 13787 checks. Python: 14 animation events, 73 checks,
37 ABI guards. Static snapshot regression 30938; три native suites и пять
consumer builds прошли. Original proofs переиспользованы, алгоритмы не менялись.
MaterialColorController factory и full scheduler/shader остаются открытыми.
Досье: `docs/research/tool-material-runtime-shared-core-2026-09-09.md`.

### Блок 15: общий Octree reader

Восстановлен spOctreeNodeSerializer, metadata inspector вызывает тот же native
reader. Четыре original/source совпадения, raw/partial/omitted cases; полный
тестовый граф 9 объектов, 15 ABI checks / 11 guards. Четыре уровня: 245 Octree,
984 checks; их полные графы требуют ещё SkyBox 7A7124AF. FAT сообщает конкретный
отсутствующий класс. Две native suites и пять managed builds прошли. Нет
подмены SkyBox, видимость и authoring writer остаются открытыми.
Досье: `docs/research/tool-octree-reader-shared-core-2026-09-09.md`.

### Блок 16: SkyBox и общая регистрация RenderNodeSerializer

Оригинал регистрирует существующий RenderNodeSerializer для SkyBox; ошибочная
exact-class проверка реконструкции исправлена по original initializer и трём
reader/index/writer executions. Два world PRS captures совпали побайтно.
Собственный C# SkyBox parser заменён общим metadata projector. Две реальные
сцены загрузили 3399 объектов / 979 occurrences, 18 + 7350 checks. Native
четыре suites / пять consumer builds прошли. SkyBox помечен отдельным pass,
обычный GL model pass его пропускает с SKY_PASS_PENDING. Camera-follow и
frontend pass ещё не подключены. Следующие blockers: OcclusionVolume,
NavigationGraph; LensFlare встречен в дополнительном контрольном уровне.
Досье: `docs/research/tool-skybox-shared-core-2026-09-09.md`.

### Блок 17: общая навигация и удаление C# readers

Восстановлены actual NavigationGraph/NavigationSet/MeshNavigationSet/Portal и
четыре serializers. Шесть scalar, три relationship и два MeshBV binding
original-PC captures совпали с C++. Сохранены byte indices, borrowed repeated
refs, raw enabled, двухбитная matrix и исходные правила bounds/sphere.
Три C# reader заменены проекторами того же ResourceGraph. Gardenia02/RedF01/
battle_02: 10 228 объектов, 13 sets, 10 portals, 20 623 cells / 21 533 checks.
Три native suites и пять managed builds прошли. Routing и writer не заявлены.
Повторная замена непустой Graph table явно отклоняется после original duplicate
release; пользователь уведомлён. PC-only test_world_navmesh упёрся в старый
source-only TextureData, который исследуется следующим. Occlusion Init остаётся
неподтверждённым; capped вызов не повторялся и не заменялся заглушкой.
Досье: `docs/research/tool-navigation-readers-shared-core-2026-09-09.md`.

### Блок 18: LensFlare reader, spQuad и PS2 ownership

Field1 оказался counted array, field0 — одним primary element. Ошибочная
C# трактовка удалена; actual LensFlare/Quad/serializer используются общим graph.
Пять PC captures совпали с C++, отдельный PC Quad destructor подтвердил refs2→1.
Полный PC LensFlare destructor capped и не повторялся; PS2 static destructor
подтвердил array→Quad→Renderable ownership. ABI21/15guards, managed20,
LensFlare15/FullLoader213, пять builds. Два реальных уровня требуют Occlusion;
их успешная загрузка не заявляется. UI только подключение исправленных DTO.
Старый source-less TextureData — отдельный незакрытый whole-file случай CP116.
Досье: `docs/research/tool-lens-flare-shared-core-2026-09-09.md`.

### Блок 19: необходимые методы Occlusion topology

Пять readable PC методов перенесены в actual spOcclusionVolume;15 original
state/results совпали с C++. Конструктор полей и Node-only clone подтверждены;
OcclusionTopology38/FullLoader213 passed. PS2 дала порядок полной подготовки
геометрии. Full Init, weld representative order и connectivity ещё открыты,
класс не включён в ResourceGraph. Новые уровни не заявлены. C# не менялся,
полный корпус и managed builds не повторялись. Старые capped entries не трогались.
Досье: `docs/research/tool-occlusion-topology-shared-core-2026-09-09.md`.

### Блок 20: ParticleSystem из общего ResourceGraph

Удалён C# particle reader. Actual параметры/defaults/region/pool/RenderNode
проецируются через ABI; inherited ссылки берутся из общего Renderable API.
Инспектор исправил порядок height/radius для cylinder/cone и сохраняет raw
mode bytes. Десять архивных PC input/state/writer captures совпали с текущим
C++/ABI/managed; новых original runs не было. Native46/213, ABI48/5guards,
managed67 и отдельный region-order test, пять builds passed. Два SFX графа
с тремя emitters плюс Bloom control: 254 объекта. Looping simulation, GPU draw
и unsupported whole graphs остаются открыты; исторические 1745 не пересчитаны
и не заявлены как новое покрытие. Game source не менялся.
Досье: `docs/research/tool-particle-reader-shared-core-2026-09-09.md`.

### Блок 21: исходящие связи Occlusion edges

PS2 counterpart помог найти readable PC4705A0. Метод перенесён в actual class;
12 original/source states/results совпали, включая рекурсию, epsilon, порядок
ссылок и partial-failure→repeat-success. Вся original память освобождена,
max arena57168. OcclusionTopology63/FullLoader213 passed. C ABI/C# не менялись;
full Init и новые уровни не заявлены. Protected capped entries не повторялись.
Досье: `docs/research/tool-occlusion-connectivity-shared-core-2026-09-09.md`.

### Блок 22: инспекция Fog без C# payload reader

Inspector/Corpus используют actual Fog serializer через bounded field bridge.
Три original-PC captures совпали с ABI/managed, включая raw enum/NaN/negative
zero;7 ABI guards,15 managed checks. Настоящее поле logo_screen.smo проверено
в прежнем inspector. FullLoader213 и пять builds passed. Game source не менялся,
whole surrounding graph или GPU fog не заявлены; полный корпус не запускался.
Досье: `docs/research/tool-fog-inspection-shared-core-2026-09-09.md`.
