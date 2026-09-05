# `spDXMesh`: PC runtime-меш и D3D9 FVF bridge

Статус: PC class/base IDs, две vtable, полный наблюдаемый prefix `0x88`, lifetime,
оба пути инициализации, batch ranges, копирование index/vertex payload и точное
преобразование component flags в FVF подтверждены. Класс отсутствует на PS2;
там зарегистрирован отдельный `spPS2Mesh`.

## Идентичность

- class ID `0x193B2671`;
- direct base `spRenderMesh` (`0x67974A9C`);
- registration `0x00763150`, initializer `0x006D4CB0`;
- primary vtable `0x006EF334`, secondary vtable `0x006EF32C`;
- exact implementation path `Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp`.

Constructor и factory являются переходами в protected region. Поэтому конец
`0x88` следует считать доказанным наблюдаемым prefix, а не direct allocation size.

## Layout

Разбор одновременно уточнил последние три слова общего `spMesh`:

| Offset | Роль |
|---:|---|
| `+0x44` | vertex component flags |
| `+0x48` | primitive count |
| `+0x4C` | vertex count |

PC leaf продолжает layout:

| Offset | Роль |
|---:|---|
| `+0x50` | `spIndexBuffer::eIndexBufferType` |
| `+0x54/+0x58` | intrusive DX index/vertex wrappers |
| `+0x5C` | intrusive `spDXSharedMeshData*` |
| `+0x60/+0x64` | index/vertex byte sizes |
| `+0x68/+0x6C` | optional CPU index/vertex staging pointers |
| `+0x70` | D3D9 FVF |
| `+0x74` | runtime vertex stride |
| `+0x78/+0x7C` | base index/base vertex в общем buffer-е |
| `+0x80` | число texture-coordinate sets, выведенное из component bits |
| `+0x84` | renderer-owned vertex-format code |

Cross-platform подтверждение полей общего base независимо даёт `spPS2Mesh`:
он переносит primitive/vertex counts из `spPS2MeshData` в `+0x48/+0x4C`.

## Пути создания

Secondary method `0x004AA000` принимает обычные `spIndexBuffer` и
`spVertexBuffer`. Без активного `spDXMeshCombiner` он создаёт отдельные wrappers
либо CPU staging. С активным global `0x00763148` он копирует байты в текущие
cursors общего buffer-а, запоминает прежние index/vertex counters как диапазон и
делает commit.

Component bit `0x20` имеет специальную конверсию: четыре `u8` по offset-у
`componentOffsets[6]` становятся четырьмя ненормализованными `float`; stride и
общий byte size увеличиваются на 12 на вершину. Это не цветовая нормализация.

Method `0x004A9CC0` создаёт render mesh как диапазон уже готового
`spDXSharedMeshData`. Он записывает FVF, stride, starts/counts, вычисляет primitive
count по topology, переносит четырёхкомпонентную bounding sphere и получает
`+0x84` от renderer-а.

## FVF

Функция `0x004B21E0` восстановлена целиком. Она выбирает базу `0x02/0x12`,
добавляет normal/diffuse/specular/position-weight flags, кодирует от одной до
восьми texture-coordinate sets и для component `0x20` ставит `0x1000`.
Portable `ComponentFlagsToFVFForAnalysis` повторяет приоритеты ветвей буквально.

Значение `+0x84` пока не эмулируется: короткий helper `0x004AE0E0` уходит в
protected renderer import. Portable code оставляет явный ноль, а не выдаёт FVF
за другой renderer handle.

## Проверка и открытое

`research/inspect_dx_mesh.py` выполняет 40 read-only checks: hashes всех известных
bodies, RTTI, две vtable, offsets, active combiner, packed expansion и FVF map.
CTest проверяет standalone GPU, CPU staging, packed conversion, два диапазона
общего batch-а, shared payload, ABI и blank clone.

Открыты direct `sizeof`, protected constructor/factory, original header и имена
двух interface methods, renderer mapping `0x004AE0E0`, device loss/reset, точные
ошибки/rollback и непосредственный draw consumer. Serializer, создающий этот
класс из shared-buffer relationship, теперь закрыт отдельно; следующий узел —
его optimizer materializer `0x004BEDF0/0x004C07E0`.
