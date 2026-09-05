# `spDXCombinedVB`: сериализуемый общий vertex/index aggregate

Статус: PC-only identity, direct base, registration, vtable, destructor,
serializer-visible payload и роль в mesh writer подтверждены. Конструктор,
factory и два ключевых lookup helpers защищены SecuROM, поэтому runtime build
algorithm пока остаётся явной границей.

## Идентичность и ABI

- class ID `0x4B18E622`;
- direct base `spBaseObject`;
- registration `0x00764408`, initializer `0x006D5550`;
- vtable `0x006F1E70`;
- registration getter `0x004C0D50`;
- destructor `0x004C0C50`, deleting destructor `0x004C0DF0`;
- blank clone `0x004C0E70`;
- protected constructor/factory thunks `0x004C0D60/0x004C0E10`.

Original header и однозначный `.cpp` не найдены. Близость адресов к DX renderer
не используется как доказательство исходного пути.

Factory не раскрывает allocation size. Destructor и
`spDXSharedMeshDataSerializer` вместе доказывают prefix `0x3C`:

| Offset | Наблюдаемая роль |
|---:|---|
| `+0x00..+0x0F` | `spBaseObject` |
| `+0x10..+0x18` | old-MSVC list state; nodes владеют payload по `node+0x08` |
| `+0x1C..+0x24` | old-MSVC tree/map state |
| `+0x28` | неизвестное слово |
| `+0x2C` | owned raw vertex bytes |
| `+0x30` | owned raw index bytes |
| `+0x34` | vertex byte size |
| `+0x38` | index byte size |

Destructor сначала удаляет payload каждого list node, затем оба raw-буфера,
рекурсивно очищает map, освобождает list и вызывает `spBaseObject` destructor.

## Место в save/load-конвейере

`spDXMeshSerializer` получает этот объект из `spDXSceneGraphOptimizer` helper-а
`0x004BEDF0(mesh)`. Helper `0x004C07E0` на найденном combined VB возвращает пять
параметров диапазона конкретного `spDXMesh`. Затем relationship serializer
выбирает `spDXSharedMeshDataSerializer` по source class ID `0x4B18E622`.

Writer shared-data serializer-а выдаёт:

```text
u32 indexByteSize
u32 vertexByteSize
u8  indexData[indexByteSize]
u8  vertexData[vertexByteSize]
```

На загрузке override class-ID remap заменяет `spDXCombinedVB` на
`spDXSharedMeshData`, а `spDXMeshSerializer` привязывает к созданной паре
GPU-буферов сохранённый диапазон. Таким образом source и load target намеренно
имеют разные native классы.

## Portable boundary и проверка

Portable `spDXCombinedVB` хранит четыре полностью доказанных payload-значения,
атомарно принимает bytes и создаёт пустой RTTI clone. Дополнительно его
безопасный analysis-facade теперь хранит не владеющее мешом соответствие ровно
пяти значений, возвращаемых `0x004C07E0`: `indexBegin`, `vertexBegin`,
`indexCount`, `vertexCount`, `vertexStride`. Диапазон принимается только если
индексы (16-bit native aggregate) и вершины целиком помещаются в текущие raw
payload. Это не объявляется восстановленным STL layout или исходным именем
record-а, но исправляет важную семантику writer-а: пять слов берутся из map
combined-VB, а не из полей runtime `spDXMesh`.

`research/inspect_dx_combined_vb.py` выполняет 27 read-only checks: SHA-256,
PC-only identity, hashes семи bodies, registration, vtable, destructor ownership
и все четыре serializer offsets. CTest проверяет payload, его использование
shared-data writer-ом, source-to-target remap и blank clone.

Открыты exact `sizeof`, original source/header/API, роль `+0x28`, физический
node/map record layout, dedup/grouping policy, автоматическое построение
aggregate, failure rollback и device-reset lifetime. Контракт mesh-range lookup
и его save-side потребитель уже закрыты; следующий узел — владеющий этими
объектами `spDXSceneGraphOptimizer`.
