# spCamera

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spCamera](../../../Sparkplug/Code/Sparkplug/spCamera.h).

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
| --- | ---: | ---: |
| `spCamera` registration | `0x0075E218` | `0x004AB3B0` |
| `spCameraData` factory | protected `0x0041A3F0` | `0x001349B0` |
| platform camera factory | protected `0x004A9120` | `0x001F5FB0` |

Concrete clone сначала создаёт новый объект того же leaf-типа, затем вызывает
унаследованный copy-slot `spNode`. Camera-specific параметры этим copy-slot не
копируются и остаются factory defaults. Это непривычное, но одинаково
наблюдаемое поведение сохранено в portable reconstruction.

## Layout

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

## Camera → renderer

Применение camera state завершает прежний разрыв между сценой и backend:

| Операция | Interface slot | PC body / endpoint | PS2 body |
| --- | ---: | --- | ---: |
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

## Границы описания

Остаются открыты:
