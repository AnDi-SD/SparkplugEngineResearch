# `spCamera`: projection, viewport и выход в renderer

Дата проверки: 5 сентября 2026 года. Статус: class graph, concrete leaves,
PC observed/PS2 exact layouts, defaults, viewport lifecycle, обе ветви projection,
frustum storage и связь с platform renderer подтверждены. Original header/TU,
часть runtime-полей и camera-manager ownership пока не установлены.

## Иерархия и identity

```text
spNode (0x695C0F65)
└─ spCamera (0x18DF3845, abstract registration)
   ├─ spCameraData (0x24BB4C41)
   ├─ spDXCamera (0x41672E34, PC)
   └─ spPS2Camera (0x055A04E0, PS2)
```

`spCamera` имеет null RTTI factory на обеих платформах. `spCameraData`,
`spDXCamera` и `spPS2Camera` concrete. PC `spCameraData` и `spDXCamera` не
переопределяют camera operations: их vtable отличаются от общей только
destructor/clone/RTTI. PS2 leaf переопределяет configure и применение camera
state, потому что хранит дополнительный platform block.

| Факт | PC | PS2 |
|---|---:|---:|
| `spCamera` registration | `0x0075E218` | `0x004AB3B0` |
| base camera vtable | `0x006DCBC0` | header `0x00490480` |
| `spCameraData` factory | protected `0x0041A3F0` | `0x001349B0` |
| `spCameraData` vtable | `0x006DEA20` | header `0x0048D850` |
| platform camera factory | protected `0x004A9120` | `0x001F5FB0` |
| platform camera vtable | `0x006EF1E0` | header `0x00491890` |

Concrete clone сначала создаёт новый объект того же leaf-типа, затем вызывает
унаследованный copy-slot `spNode`. Camera-specific параметры этим copy-slot не
копируются и остаются factory defaults. Это непривычное, но одинаково
наблюдаемое поведение сохранено в portable reconstruction.

## Layout

PS2 factories дают точный общий размер `0x250`; `spPS2Camera` выделяет `0x340`.
PC factory спрятан за `.rld`, поэтому `0x238` обозначается только как observed
extent, хотя runtime-методы покрывают все байты до последнего поля.

| Поле | PC | PS2 |
|---|---:|---:|
| `spNode` base | `0x000..0x0B3` | `0x000..0x0BF` |
| near / far | `+0xBC / +0xC0` | `+0xC8 / +0xCC` |
| serialized `Is2DMode` | `+0xC8` | `+0xD4` |
| view matrix | `+0xD4` | `+0xE0` |
| projection matrix | `+0x114` | `+0x120` |
| view angle / scaled angle / pixel aspect | `+0x188/+0x18C/+0x190` | `+0x19C/+0x1A0/+0x1A4` |
| six frustum planes | `+0x1C4` | `+0x1D8` |
| dirty flags | `+0x224` | `+0x238` |
| final configure argument | `+0x22C` | `+0x240` |
| viewport active | `+0x230` | `+0x244` |
| projection branch flag | `+0x231` | `+0x245` |
| viewport ratio `height/width` | `+0x234` | `+0x248` |

У `spPS2Camera` common base заканчивается по `+0x250`, дополнительный matrix
block занимает `+0x250..+0x2CF`, platform frustum planes — `+0x2D0`, а
viewport width/height лежат по `+0x330/+0x334`.

Важно: byte projection branch — не сериализованный `Is2DMode`. Setter поля
`Is2DMode` уведомляет renderer, но не пишет этот byte и не добавляет camera
dirty bit. Сливать два флага в один нельзя.

## Defaults и projection

Обе платформы задают:

- near `1.0`, far `10000.0`;
- view angle `0x3F860A92` (`~pi/3`);
- pixel aspect `1.0`;
- начальный dirty mask `2`; view dirty bit равен `1`;
- configure добавляет mask `0x7F`.

Для перспективной ветви:

```text
half = near * tan(viewAngle / 2)
aspectHalf = pixelAspect * (height / width) * half
M00 = near / half
M11 = near / aspectHalf
M22 = far / (far - near)
M23 = 1
M32 = -near * far / (far - near)
M33 = 0
```

Вторая ветвь ставит `M00=2/half`, `M11=2/aspectHalf`,
`M22=1/(far-near)`, `M32=near/(near-far)` и сохраняет identity `M33=1`.
Portable `BuildProjectionMatrixForAnalysis` дополнительно отклоняет
нефинитные/нулевые делители. Это safety guard host-кода, а не найденная native
валидация.

## Camera → renderer

Применение camera state завершает прежний разрыв между сценой и backend:

| Операция | Interface slot | PC body / endpoint | PS2 body |
|---|---:|---|---:|
| configure 2D state | `10` | `0x004AD7A0` | `0x001FB8C0` |
| projection matrix | `12` | `0x004BBAE0`, D3D `SetTransform(3)` | `0x001FBA40` |
| view matrix | `13` | `0x004BBB20`, D3D `SetTransform(2)` | `0x001FB260` |
| world matrix | `14` | `0x004BBB60`, D3D `SetTransform(0x100)` | `0x001F6F50` |
| viewport | `22` | `0x004BBA80`, D3D `SetViewport` | `0x001FAB60` |

PC `spCamera` вызывает сначала view slot `13`, затем projection slot `12`.
PS2 common path делает то же через ABI offsets `+0x3C/+0x38`; два leading
vtable words не являются дополнительными operations. `spPS2Camera` перед этим
строит platform matrices/frustum и передаёт их в те же backend bodies.

Material camera-view/cubemap path одновременно уточнил начало интерфейса.
На PC ordinary/cube bind находятся в slots `1/0`; на PS2 — `0/1`. PS2 slot `1`
печатает `spCubeRenderTarget not supported on PS2` и возвращает false. Поэтому
первые два slot number платформенно различны, несмотря на общую семантику.
Slots `3/4/5` соответствуют begin scene, end scene и clear.

## Проверка и открытая граница

`python -B research\inspect_cameras.py` выполняет `37/37` read-only checks.
`inspect_renderers.py` теперь выполняет `78/78`, включая полные hashes camera,
target, scene и viewport bodies, D3D transform constants и PS2 cube diagnostic.
CTest проверяет IDs/factories, ABI offsets, defaults, обе projection-формулы,
необычный clone и платформенное отображение renderer operations.

Остаются открыты:

1. Original header/TU и настоящие имена camera vtable methods.
2. Роли opaque blocks перед matrices, derived basis и поля рядом с dirty mask.
3. Полный world/view update, frustum culling consumer и camera-manager policy.
4. Direct PC allocations за protected `.rld` entries.
5. Точные сигнатуры 29-slot renderer interface; текущие имена помечены как
   analytical и не выдаются за исходный API.
