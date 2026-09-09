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
