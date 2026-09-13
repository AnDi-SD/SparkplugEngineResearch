# MeshBV и данные граней для общих ядер tools

## классы и методы

| Класс | ID | Нужная ответственность |
| --- | --- | --- |
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

Sphere producer вынесен из `spDXMesh.cpp` в общий `Analysis/PC/spVertexBounds.h`
без изменения алгоритма и промежуточных float stores. Render mesh и MeshBV
используют одну реализацию. Она сканирует все вершины, включая неиспользованные
индексами. Отдельный специфичный PC render-mesh AABB остаётся прежним.

Collision transform slot `47A180` — ровно `RET 16`: P/R/S/CollisionInfo и сам
MeshBV не меняются. В отличие от OBB, дополнительного преобразования здесь нет.
MeshBV clone использует inherited named copy `413120`, оставляет default geometry.

## Границы описания

Host guards явно строже повреждённого оригинала: byte/count limits, finite
positions/sphere, индексы внутри vertex array, отказ на пустой geometry,
face-before-geometry, повторное чтение populated face container и small-int
overflow. Известные wxFaceData fields читаются своей фактической шириной;
advertised sizes и неизвестные поля ограничены доступным внешним payload.
Повторный face field безопасно заменяет owner, не воспроизводит native leak.
Сохранение объекта без geometry отклоняется до native null dereference.

`WinxGameCore` — отдельная библиотека игровых данных, без bootstrap/OS startup. Sparkplug не зависит от Winx: face factory регистрирует приложение.
