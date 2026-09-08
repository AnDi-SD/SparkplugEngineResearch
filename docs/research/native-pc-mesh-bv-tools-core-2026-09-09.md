# MeshBV и данные граней для общих ядер tools

Цикл до 07:30 МСК 9 сентября. Нужный срез — загрузка, сохранение и просмотр
collision-геометрии меню/уровней. Реализация общая в `Sparkplug/` и `Winx/`;
межъязыковой resource graph регистрирует эти реальные классы. Игровые запросы
столкновений, OPCODE tree и navigation queries не входят в доступное API.

## Подтверждённые классы и методы

| Класс | ID | Нужная ответственность |
|---|---|---|
| spMeshBV | 3F453DE7 | Владелец collision mesh, bounds, связь с CollisionInfo |
| spCollisionMesh | 36432CFF | Прямое владение index/vertex buffers и face container |
| spMeshBVSerializer | 6C662708 | Два поля geometry/face data, reader/writer |
| spFaceDataContainer | 6E954A6A | Типизированный массив объектов extension data |
| spExtensionData | 6A1D0D2E | Абстрактный stream/ownership контракт |
| spCustomAppData | 76181F6B | Абстрактная база игровых extension data, copy dispatch |
| wxFaceData | 313C4C17 | Surface type UInt8, flags UInt16, surface ID UInt8 |

Имена и bases подтверждены PC initializers `6D4090`, `6D40C0`, `6D40F0`,
`6D6550`, `6D65B0`, MeshBV `6D3F40`, serializer `6D2D60`. Пути новых headers
предполагаемые: оригинальные имена файлов этих классов не найдены.

`spMeshBV` factory `47AFF0` выделяет 0x50 bytes; default sphere/data нулевые.
Reader `438490` создаёт `spVertexBuffer` (0x5C), `spIndexBuffer` (0x28) и
`spCollisionMesh` (0x1C), читает стандартные buffer streams, передаёт владение
MeshBV. Writer `438680` вызывает **те же общие buffer writers** `45FC90` /
`460400`. Первое UInt32 geometry — **index-buffer type 2**, а не версия.
Третье UInt32 — index format flags; header после индексов — обычный vertex
componentFlags/count/flags. Второго декодера этих буферов не добавлено.

`47A210` удаляет прежний data owner **до** проверки нового. Для ненулевого
data принимает index type2, UInt16 indices (flags bit0 clear), vertex
componentFlags0; рассчитывает sphere через `468370 → 468000`, затем запускает
OPCODE build. Null replacement сохраняет прежнюю sphere. Восстановленный
`SetDataAndBoundsForAnalysis` прямо обозначает только первые операции:
**его результат не является результатом построения query tree**. Приложениям
не предоставлен метод queries или успешная заглушка OPCODE.

Sphere producer вынесен из `spDXMesh.cpp` в общий `Analysis/PC/spVertexBounds.h`
без изменения алгоритма и промежуточных float stores. Render mesh и MeshBV
используют одну реализацию. Она сканирует все вершины, включая неиспользованные
индексами. Отдельный специфичный PC render-mesh AABB остаётся прежним.

Collision transform slot `47A180` — ровно `RET 16`: P/R/S/CollisionInfo и сам
MeshBV не меняются. В отличие от OBB, дополнительного преобразования здесь нет.
MeshBV clone использует inherited named copy `413120`, оставляет default geometry.

`spCollisionMesh` destructor `47D700` прямо удаляет три владельца. Face array
`47D5E0` читает class ID/count, создаёт каждый элемент через **original RTTI
factory** и вызывает его virtual reader. Writer `47D530` пишет те же сведения
и вызывает virtual writer каждого элемента. Count не подменяется числом граней:
совпадение в корпусе само по себе не является отдельной проверкой оригинала.

`wxFaceData` factory `5A4E30` выделяет 0x1C bytes, default fields нулевые.
Reader `5A5320` сбрасывает поля перед чтением, использует base128 small-int
`5A4F70`, пропускает неизвестные ID и применяет повторные поля по порядку.
Writer `5A4FE0` независимо опускает каждое нулевое поле, пишет остальные в порядке
1/2/3, затем terminator0. Copy `5A4E10` переносит ровно три числовых поля.
Значения flags и surface ID не получили вымышленных игровых enum.

## Проверки и границы

[`probe_pc_mesh_bv_core.py`](../../research/probe_pc_mesh_bv_core.py): свежие
bounded original-PC micro cases `defaults`, `geometry`, `faces`, `face-defaults`,
`faces-unknown`. Выполняются оригинальные constructors/readers/writers,
sphere/OPCODE build, transform slot и destructors. Последний case дополнительно
проверяет unknown ID300, repeated fields и настоящий clone `wxFaceData`.
Геометрия: P=(0,0,0),(2,0,0),(0,4,0), индексы0/1/2; sphere=(1,2,0,sqrt(5)).
Не используются seams внутри алгоритмов или RTTI lookup: для face cases
подготовлен явно обозначенный точный однозаписный startup registry.

Результаты micro: 2/2, 10/10, 14/14, 14/14 и 18/18 native allocations
освобождены. Максимальная arena reservation944 bytes; самый длинный reader
16 742 instructions. Input/output bytes и SHA256 находятся в локальных JSON
`local-data/results/tools-core-cycle-20260909-0730/mesh-bv/`.

Один ранний стенд передал transform slot три аргумента вместо четырёх и был
остановлен проверкой calling convention. Оригинальный `RET16` и call site
CollisionInfo доказали ошибку fixture; восстановленный алгоритм не менялся.
Первичное сомнение о null-local при face read снято: запись `[esp+14]` следует
за push аргумента и действительно обновляет исходный local `[esp+10]`.

Host guards явно строже повреждённого оригинала: byte/count limits, finite
positions/sphere, индексы внутри vertex array, отказ на пустой geometry,
face-before-geometry, повторное чтение populated face container и small-int
overflow. Известные wxFaceData fields читаются своей фактической шириной;
advertised sizes и неизвестные поля ограничены доступным внешним payload.
Повторный face field безопасно заменяет owner, не воспроизводит native leak.
Сохранение объекта без geometry отклоняется до native null dereference.

`WinxGameCore` — отдельная библиотека игровых данных, без bootstrap/OS startup.
Sparkplug не зависит от Winx: face factory регистрирует приложение. Новый
`RegisterCloneForAnalysis` — только публичный alias уже восстановленной
операции clone-map, а не вторая реализация клонирования.

Release DLL и семь выбранных C++ suites прошли; новый `ViewerMeshBVCoreChecks`
проверяет 76 условий, включая точные original writer bytes, defaults, clone,
неизвестные/повторные face fields и явные отказы на повреждённых входах.
[`validate_shared_vertex_sphere.py`](../../research/validate_shared_vertex_sphere.py)
сверил sphere **побитно** с оригиналом на семи fresh micro cases: triangle,
five, seven, u32, rounding, zero-primitives и point. Полный корпус не запускался.

Общий resource graph прочитал три выбранных SMO с проверкой всех FAT entries
против сохранённой базы для точно совпадающих SHA256, Node parent/child consistency
и finite transforms: `Menus/igmenu_opt_pc.smo` — 1 225 objects/835 nodes;
`SFX/tile_bad.smo` — 127/35; `Characters/Bloom/bloom_jeans.smo` — 121/98.
Первые два содержат 33+15 MeshBV, у tile_bad есть wxFaceData.
У tile_bad понадобилась регистрация уже восстановленного `spParticleSystem`:
изменений его игрового алгоритма не было. Это дополнительная проверка интеграции,
а не исполнение целого SMO оригинальной игрой.
После обновления DLL также прошли все37 коротких тестов SanToVmd.
