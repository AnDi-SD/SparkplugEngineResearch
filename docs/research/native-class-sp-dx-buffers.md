# `spDXVertexBuffer` и `spDXIndexBuffer`: D3D9-оболочки GPU-буферов

Статус: PC-only классы, их RTTI, прямой base, размеры, vtable, lifetime и
параметры создания Direct3D-ресурсов подтверждены. Это не общие CPU-классы
`spVertexBuffer`/`spIndexBuffer`, а следующий backend-слой между загруженными
данными меша и `spDXMesh`.

| Факт | `spDXVertexBuffer` | `spDXIndexBuffer` |
|---|---:|---:|
| Class ID | `0x37036C17` | `0x23022413` |
| Direct base | `spBaseObject` | `spBaseObject` |
| Registration | `0x00763C00` | `0x00763C60` |
| Registration initializer | `0x006D51F0` | `0x006D5220` |
| Constructor entry | `0x004B1E00` | `0x004B1FC0` |
| Factory entry | `0x004B1E30` | `0x004B2010` |
| Clone | `0x004B1EA0` | `0x004B2080` |
| Destructor | `0x004B1EF0` | `0x004B20D0` |
| GPU initialize | `0x004B1F60` | `0x004B2140` |
| Deleting destructor | `0x004B1FA0` | `0x004B2180` |
| Vtable | `0x006F05C4` | `0x006F05F8` |
| Размер | `0x20` | `0x1C` |

В PS2 executable обе строки классов и обе регистрации отсутствуют. Это
доказанная платформенная граница, поэтому для них не создаётся фиктивный PS2
ABI.

## Layout

После `spBaseObject` размером `0x10` vertex-оболочка содержит:

| Offset | Наблюдаемое значение |
|---:|---|
| `+0x10` | `IDirect3DVertexBuffer9*`, освобождается через COM `Release` |
| `+0x14` | FVF, записывается аргументом `Initialize` |
| `+0x18` | неизвестное слово |
| `+0x1C` | размер буфера в байтах |

Index-оболочка содержит:

| Offset | Наблюдаемое значение |
|---:|---|
| `+0x10` | `IDirect3DIndexBuffer9*`, освобождается через COM `Release` |
| `+0x14` | неизвестное слово |
| `+0x18` | размер буфера в байтах |

Размеры не выведены из соседства символов. Пять независимых callers выделяют
ровно `0x20` байт перед vertex-конструктором и ещё пять — ровно `0x1C` перед
index-конструктором. Деструкторы и все наблюдаемые обращения согласуются с
этими границами.

Оба RTTI clone создают новый объект через factory, регистрируют пару в clone
manager и вызывают только корневой copy-slot `spBaseObject`. GPU-ресурс и
локальные поля не копируются: результат является пустой оболочкой.

## Direct3D contract

`spDXVertexBuffer::Initialize` получает четыре 32-битных аргумента и передаёт
их в `IDirect3DDevice9::CreateVertexBuffer` без перестановки:

```text
byteSize, usage, fvfCode, pool, &field10, nullptr
```

Вызов идёт через смещение `+0x68` vtable D3D9 device. После него сохраняются
FVF в `+0x14` и byte size в `+0x1C`.

`spDXIndexBuffer::Initialize` аналогично вызывает device slot `+0x6C`, то есть
`CreateIndexBuffer`:

```text
byteSize, usage, format, pool, &field10, nullptr
```

Размер сохраняется в `+0x18`. Обе native-функции игнорируют `HRESULT` и
возвращают `true`; это описывается как факт оригинала, но не переносится в
безопасный host-код. Переносимые `InitializeForAnalysis` возвращают `false` при
ошибке выделения памяти и только после успеха заменяют состояние объекта.

Отдельная функция `spDXIndexBuffer` `0x004B1FF0`, имеющая одиннадцать прямых
callers, вызывает COM `Release` и обнуляет `+0x10`, если счётчик D3D-объекта
дошёл до нуля. При этом размер `+0x18` не сбрасывается. Деструкторы обоих
классов выполняют ту же release-проверку для собственного GPU pointer, затем
переходят в destructor `spBaseObject`.

## Связь с `spDXMeshCombiner`

`spDXMeshCombiner::Initialize` создаёт по одной оболочке и использует
фиксированные параметры:

```text
index:  byteSize, usage=8, format=0x65 (D3DFMT_INDEX16), pool=1
vertex: byteSize, usage=8, fvfCode,                         pool=1
```

Затем он напрямую вызывает `Lock` у D3D-объектов через COM slot `+0x2C` и
хранит два write cursor. Когда записано целевое число вершин, оба буфера
разблокируются через slot `+0x30`. Готовый `spDXMesh` получает ссылки на эти же
оболочки и отдельно хранит base vertex/base index внутри объединённого
буфера. Тем самым batch plan `spDXSerializerHook` теперь связан с конкретным
GPU ownership-механизмом, а не только с размерными метаданными.

## Восстановленная граница

Inferred-файлы `Code/SparkplugDX/spDXVertexBuffer.*` и
`Code/SparkplugDX/spDXIndexBuffer.*` реализуют RTTI/factory/blank clone и
безопасный host storage с тем же порядком аргументов. Это не попытка заменить
Direct3D renderer: COM pointer и устройство остаются в byte-exact PC evidence,
а host storage нужен для изолированных тестов следующего `spDXMeshCombiner`.

`research/inspect_dx_buffers.py` проверяет SHA обоих контрольных executable,
отсутствие классов на PS2, bodies, RTTI registrations, vtables, число прямых
callers, размеры allocations и параметры, с которыми wrappers создаются
комбайнером. CTest отдельно проверяет layout/ID, factory, blank clone,
инициализацию и release-семантику переносимого слоя.

6 сентября: actual protected factories/constructors обоих wrappers и combiner
исполнены:97 checks/6 cases, включая unchecked Create/Lock/Unlock HRESULT.
Vertex ctor оставляет byteSize1C нетронутым, но обнуляет10/14/18; index
обнуляет10/14/18. Это consumer/lifetime evidence без настоящего device.
[Детали](native-pc-dx-materialization.md).

Открыто: original header/TU и имена методов, роли `vertex +0x18` и
`index +0x14`, точная политика device
loss/reset и связь с `spDXVertexDeclaration`. Следующий обязательный узел —
`spDXMeshCombiner`, после него можно восстанавливать полный `spDXMesh`.
