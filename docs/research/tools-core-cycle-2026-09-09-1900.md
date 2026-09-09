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
