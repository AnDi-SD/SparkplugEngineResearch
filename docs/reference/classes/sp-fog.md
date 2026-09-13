# spFog

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFog](../../../Sparkplug/Code/Sparkplug/spFog.h).

### Идентичность и layout

`spFog` имеет Class ID `0x7AC95AEC`, в engine RTTI напрямую наследует `spBaseObject` и
создаётся собственной factory.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| размер | observed `0x28` | exact `0x28` |

После физического name `+0x10` находятся type `+0x14`, ARGB
color `+0x18`, start `+0x1C`, end `+0x20` и density `+0x24`. Defaults равны
`type=0`, `color=0xFF000000`, `start=0.0`, `end=1.0`, `density=1.0`.
Wire-порядок полностью совпадает с
[`spFogSerializer`](sp-fog-serializer.md).

Как и у material leaf, native clone создаёт default object и вызывает общий
named-copy, копирующий имя, но не fog payload. Portable реализация сохраняет этот
неожиданный blank-clone contract.

### Renderer operation `SetFog`

Call sites сопоставляют fog с callable renderer interface slot `26`:

| Платформа | Body | Результат |
| --- | ---: | --- |
| PC | `0x004AD390` | переводит payload в D3D9 fog render states |
| PS2 | `0x001FB1F0` | сохраняет fog pointer и отмечает linear-path |

PC body читает все пять полей и выставляет `D3DRS_FOGENABLE` (`0x1C`),
`D3DRS_FOGCOLOR` (`0x22`), `D3DRS_FOGTABLEMODE` (`0x23`),
`D3DRS_FOGSTART` (`0x24`), `D3DRS_FOGEND` (`0x25`) и
`D3DRS_FOGDENSITY` (`0x26`). Поведение даёт аналитическую карту:
`0=disabled`, `1=exp`, `2=exp2`, `3=linear`.

Важная ABI-поправка: PS2 vptr указывает на двухсловный GCC vtable header.
Поэтому встречающийся в caller byte-offset `+0x70` означает callable slot
`(0x70-8)/4 = 26`, а не slot `28`. Slot `28` — отдельная shutdown operation и
не должен использоваться для fog.

### Границы описания

Открыты exact original paths/method spelling, полный native NameManager lifetime, детали
PS2 GS/VIF downstream реализации, serializer rollback и контролируемый
in-game mutation test.

### Байтовая структура

Размер каждого объекта равен 31 байту, сигнатура базы —
`s0:f0:20|s0:end`.

| Смещение | Размер | Значение |
| ---: | ---: | --- |
| `0x00` | 4 | class ID `0x7AC95AEC` |
| `0x04` | 4 | `SBOO` |
| `0x08` | 2 | field 0: header `0xA0`, UInt8 payload size `0x14` |
| `0x0A` | 4 | `UInt32 type` |
| `0x0E` | 4 | `ARGB UInt32 color` |
| `0x12` | 4 | `Single start` |
| `0x16` | 4 | `Single end` |
| `0x1A` | 4 | `Single density` |
| `0x1E` | 1 | terminator собственной serializer-секции `0x00` |

Все числа little-endian. Цвет сохраняется целиком как `uColor`; диагностический
runtime-текст выводит только RGB, поэтому влияние старшего alpha-байта отдельно
не доказано.

### Подтипы и наблюдаемые значения

Первый `UInt32` является настоящим discriminator, а не частью цвета:

| Type | Интерпретация | PS2 | Всего |
| ---: | --- | ---: | ---: |
| 0 | `none`, fog отключён | 290 | 1 060 |
| 3 | `linear` в render enum | 24 | 80 |

PC runtime по `0x004DF0B9` прямо пропускает объект, если член type `+0x14` равен
нулю, поэтому смысл `none` подтверждён поведением. Значение 3 соответствует
`linear` в передаваемом renderer fog enum. Значения 1 (`exponential`) и 2
(`exponential-squared`) в доступных SMO не встречаются и пока не являются
наблюдаемыми подтипами. Для вариантов базы созданы только `type 0` и `type 3`.

По всем 1 140 объектам:

- 22 различных 20-байтовых payload и 17 цветов;
- `start` лежит в диапазоне `0..5000`;
- `end` лежит в диапазоне `1000..60000`;
- `density` всегда равен `0`;
- все три `Single` конечны, NaN/Infinity не обнаружены.
