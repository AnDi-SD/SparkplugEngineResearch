# spDXIndexBuffer / spDXVertexBuffer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXIndexBuffer](../../../Sparkplug/Code/SparkplugDX/spDXIndexBuffer.h), [spDXMesh](../../../Sparkplug/Code/SparkplugDX/spDXMesh.h), [spDXVertexBuffer](../../../Sparkplug/Code/SparkplugDX/spDXVertexBuffer.h), [spIndexBuffer](../../../Sparkplug/Code/Sparkplug/spIndexBuffer.h), [spVertexBuffer](../../../Sparkplug/Code/Sparkplug/spVertexBuffer.h).

Статус: PC-only классы, их RTTI, прямой base, размеры, vtable, lifetime и
параметры создания Direct3D-ресурсов подтверждены. Это не общие CPU-классы
`spVertexBuffer`/`spIndexBuffer`, а следующий backend-слой между загруженными
данными меша и `spDXMesh`.

| Факт | `spDXVertexBuffer` | `spDXIndexBuffer` |
| --- | ---: | ---: |
| Class ID | `0x37036C17` | `0x23022413` |
| Direct base | `spBaseObject` | `spBaseObject` |
| GPU initialize | `0x004B1F60` | `0x004B2140` |
| Размер | `0x20` | `0x1C` |

В PS2 executable обе строки классов и обе регистрации отсутствуют. Это
доказанная платформенная граница, поэтому для них не создаётся фиктивный PS2
ABI.

## Layout

После `spBaseObject` размером `0x10` vertex-оболочка содержит:

| Offset | Наблюдаемое значение |
| ---: | --- |
| `+0x10` | `IDirect3DVertexBuffer9*`, освобождается через COM `Release` |
| `+0x14` | FVF, записывается аргументом `Initialize` |
| `+0x18` | неизвестное слово |
| `+0x1C` | размер буфера в байтах |

Index-оболочка содержит:

| Offset | Наблюдаемое значение |
| ---: | --- |
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

Открыто: original header/TU и имена методов, роли `vertex +0x18` и
`index +0x14`, точная политика device
loss/reset и связь с `spDXVertexDeclaration`. Следующий обязательный узел —
`spDXMeshCombiner`, после него можно восстанавливать полный `spDXMesh`.
