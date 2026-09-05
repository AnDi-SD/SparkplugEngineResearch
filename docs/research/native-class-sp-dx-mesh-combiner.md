# `spDXMeshCombiner`: общий динамический буфер партии мешей

Статус: имя, exact translation unit, полный layout `0x2C`, constructor,
destructor, `IsFull`, `Initialize`, commit, единственный virtual slot и связь с
`spDXSerializerHook`/`spDXMesh` подтверждены. Класс не участвует в Sparkplug
RTTI и существует только в PC Direct3D-ветке.

Исходный файл доказан диагностическими строками:
`Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp`. Header не найден, поэтому
`Code/SparkplugDX/spDXMesh.h` остаётся inferred.

## Layout

| Offset | Наблюдаемая роль |
|---:|---|
| `+0x00` | vptr `0x006EF294`; один deleting destructor `0x004A9F30` |
| `+0x04` | целевое число вершин |
| `+0x08` | уже записанное число вершин |
| `+0x0C` | FVF |
| `+0x10` | размер общего vertex buffer в байтах |
| `+0x14` | размер общего index buffer в байтах |
| `+0x18` | intrusive `spDXVertexBuffer*` |
| `+0x1C` | intrusive `spDXIndexBuffer*` |
| `+0x20` | текущий write cursor vertex buffer |
| `+0x24` | текущий write cursor index buffer |
| `+0x28` | уже записанное число индексов |

Constructor `0x004A9610` ставит vptr и обнуляет все слова от `+0x08` до
`+0x28`, но оставляет `+0x04` нетронутым до `Initialize`. Это ещё один случай,
где оригинал полагается на обязательный порядок вызовов. Portable constructor
обнуляет весь state как безопасное, явно отделённое расхождение.

Destructor `0x004A9640` отпускает intrusive ссылки сначала на index wrapper,
затем на vertex wrapper. Class ID, factory и clone отсутствуют: это локальный
служебный C++-класс, а не сериализуемый `spBaseObject`.

## Методы

`IsFull` `0x004A95E0` возвращает строгое равенство:

```text
writtenVertexCount == targetVertexCount
```

`Initialize` `0x004A96C0` имеет пять аргументов:

```text
fvfCode, targetVertexCount, vertexByteSize, indexByteSize, ignored
```

Последний аргумент в доступном теле не читается. Функция создаёт
`spDXIndexBuffer(0x1C)` и `spDXVertexBuffer(0x20)`, передаёт им динамические
D3D9 параметры `usage=8`, `pool=1`, для индексов выбирает
`D3DFMT_INDEX16 (0x65)`, после чего lock-ает оба ресурса целиком. Полученные
адреса становятся курсорами `+0x20/+0x24`.

Commit `0x004A98E0` получает:

```text
vertexCount, vertexByteCount, indexCount, indexByteCount
```

Он увеличивает два счётчика и два курсора. Когда новое число вершин точно
равно цели, вызывает `Unlock` у обоих D3D buffers. Native-функция не проверяет
переполнение и capacity; portable `CommitForAnalysis` выполняет эти проверки
атомарно, чтобы исследовательский тест не мог повредить память.

## Место в loader-е

У каждого метода ровно один прямой caller. `spDXSerializerHook`:

1. собирает batch одного FVF с числом вершин строго меньше `0x4E20`;
2. выделяет ровно `0x2C` и вызывает constructor/Initialize;
3. публикует helper в global `0x00763148`;
4. materialize-ит выбранные `spMeshData` вторым проходом;
5. serializer `spDXMesh` копирует payload в текущие cursors и вызывает commit;
6. после партии hook обнуляет global.

В самом `spDXMesh` текущие `writtenVertexCount/writtenIndexCount` до копирования
становятся base vertex/base index. Затем mesh получает intrusive ссылки на
общие wrappers. Это объясняет, почему batch ограничивается 16-битным индексным
пространством, а отдельные mesh сохраняют собственные диапазоны внутри общих
GPU-буферов.

## Восстановленная граница и проверки

Portable `spDXMeshCombiner` создаёт уже восстановленные host-аналоги DX
wrappers, сохраняет точный порядок параметров и cursor/count state. Он не
создаёт D3D device и не притворяется полным вторым проходом serializer hook.

`research/inspect_dx_mesh_combiner.py` выполняет 41 read-only проверку:
контрольные SHA, source/name evidence, полные тела функций, один vtable slot,
число callers, все offsets, allocation `0x2C`, lock/unlock и active global.
CTest проверяет initialization, native buffer flags, атомарное отклонение
overflow, частичный commit и exact-full unlock.

Открыто: исходные имена методов/header, C++ тип пятого аргумента, native error
rollback при неудаче второго buffer-а и полный serializer dispatch второго
прохода. Следующий узел по этому же ребру — concrete `spDXMesh`.
