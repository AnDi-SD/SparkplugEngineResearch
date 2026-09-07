# Нативный `spFog` и его renderer binding

PC checkpoint11: [actual codec/ownership/blank clone](native-pc-fog-serialization.md).
Factory28 теперь исполнена прямо. После clear sole-owned Fog native FAT
сохраняет freed pointer; его повторное использование не запускалось. Host context
удерживает безопасного shared owner. Это не новый in-game/renderer proof.

Дата проверки: 5 сентября 2026 года. Статус: Class ID, direct base, полный
payload/layout, defaults, lifetime и renderer operation подтверждены на PC и
PS2. Имена enum восстановлены только как аналитическое отображение поведения.

## Идентичность и layout

`spFog` имеет Class ID `0x7AC95AEC`, в engine RTTI напрямую наследует `spBaseObject` и
создаётся собственной factory.

| Факт | PC | PS2 |
|---|---:|---:|
| registration / initializer | `0x0075CF48 / 0x006D1A90` | `0x004A7CC0 / 0x00480B5C` |
| factory | protected `0x00419E90` | `0x00135820` |
| vtable | `0x006DBD04` | header `0x0048DCF0` |
| clone | `0x0041A8E0` | `0x00135720` |
| размер | observed `0x28` | exact `0x28` |

PC checkpoint12: физический prefix является `spNamedObject`: factory обнуляет
name `+0x10`, copy `413120` удерживает имя, destructor вызывает `413090`.
Это не меняет RTTI: common helper `4671F0` проверяет NamedObject ancestry и
**не присваивает Fog имя из FAT**, даже если там есть строка. Это проверено
оригинальными инструкциями и source; PS2 name-prefix здесь не переисследовался.

После физического name `+0x10` находятся type `+0x14`, ARGB
color `+0x18`, start `+0x1C`, end `+0x20` и density `+0x24`. Defaults равны
`type=0`, `color=0xFF000000`, `start=0.0`, `end=1.0`, `density=1.0`.
Wire-порядок полностью совпадает с
[`spFogSerializer`](native-class-sp-fog-serializer.md).

Как и у material leaf, native clone создаёт default object и вызывает общий
named-copy, копирующий имя, но не fog payload. Portable реализация сохраняет этот
неожиданный blank-clone contract.

## Renderer operation `SetFog`

Call sites сопоставляют fog с callable renderer interface slot `26`:

| Платформа | Body | Результат |
|---|---:|---|
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

## Проверка и открытые вопросы

`research/inspect_material_runtime.py` проверяет RTTI/lifetime/layout/defaults,
field reads, точную PC D3D state sequence и PS2 binding. CTest проверяет payload,
blank clone и одинаковое аналитическое сопоставление slot `26`.

Открыты exact original paths/method spelling, полный native NameManager lifetime, детали
PS2 GS/VIF downstream реализации, serializer rollback и контролируемый
in-game mutation test.
