# Нативные `spMaterial` и `spMaterialData`

Дата проверки: 5 сентября 2026 года. Статус: identity, наследование,
PC/PS2 layout, defaults, material-state/pass prefix и concrete color payload
подтверждены. Неизвестные поля и pass/layer ownership оставлены непрозрачными.

## Идентичность и lifetime

`spMaterial` имеет Class ID `0x5C0314C5`, напрямую наследует `spBaseObject` и
регистрируется как абстрактный тип с null factory. `spMaterialData` имеет Class
ID `0x6160348B`, напрямую наследует `spMaterial` и является concrete leaf.

| Факт | PC | PS2 |
|---|---:|---:|
| `spMaterial` registration / initializer | `0x0075DFD0 / 0x006D2930` | `0x004A9610 / 0x00482B10` |
| `spMaterial` primary vtable | `0x006DC984` | header `0x0048E870` |
| `spMaterialData` registration / initializer | `0x0075D548 / 0x006D1D90` | `0x004A82C0 / 0x00480ED4` |
| `spMaterialData` factory | protected `0x0041A390` | `0x00134AB0` |
| `spMaterialData` primary/interface vtables | `0x006DE9FC / 0x006DEA20` | headers `0x0048D8A0 / 0x0048D8C4` |
| размер `spMaterial` | observed `0x80` | exact `0x80` |
| размер `spMaterialData` | observed `0xC4` | exact aligned allocation `0xD0` |

Полные исходные paths не сохранились. Поэтому reconstructed files размещены в
подтверждённом общем модуле `Code/Sparkplug`, но путь помечен inferred.

## Layout `spMaterial`

Обе платформы подтверждают общий prefix до `0x80`:

| Offset | Представление | Подтверждённая роль |
|---:|---|---|
| `0x00..0x0F` | `spBaseObject` | базовый object prefix |
| `0x10` | `u32` | пока не названо |
| `0x14` | pointer/vptr | secondary material interface |
| `0x18..0x1F` | 8 bytes | пока не названо |
| `0x20..0x48` | `11 × u32` | render-state values |
| `0x4C` | `u32` | пока не названо |
| `0x50` | `u32` | число material passes |
| `0x54..0x70` | `8 × pointer` | фиксированное хранилище pass relationships |
| `0x74` | byte | render override flag |
| `0x75` | byte | vertex-alpha flag |
| `0x76..0x77` | 2 bytes | padding |
| `0x78` | `u32` | непрозрачное runtime state |
| `0x7C` | pointer | `spMaterialColorController` relationship |

Native default render-state vector равен
`[0, 0, 1, 2, 1, 1, 3, 0, 4, 1, 6]`. Проверяемый portable API не даёт
выходить за 11 состояний и восстанавливает pass capacity `8`; исходные enum и
названия методов пока не выдумываются.

## Concrete payload `spMaterialData`

После общего prefix расположены четыре RGBA-вектора и один float:

| Offset | Поле | Default |
|---:|---|---|
| `0x80` | diffuse RGBA | white `(1,1,1,1)` |
| `0x90` | ambient RGBA | black `(0,0,0,1)` |
| `0xA0` | specular RGBA | white `(1,1,1,1)` |
| `0xB0` | emissive RGBA | black `(0,0,0,1)` |
| `0xC0` | specular power | `0.0` |

PS2 оставляет alignment padding `0xC4..0xCF`. Defaults независимо связаны с
глобальными black/white constants, которые инициализируются кодом по
`0x0047F2A0`. Secondary accessors `0x0016F7B0..0x0016F960` подтверждают
смещения getters/setters, включая specular power.

## Неочевидная clone-семантика

Нативный clone создаёт новый default object и вызывает copy slot. У
`spMaterialData` этот slot является success-only no-op (`0x005A7DB0` PC,
`0x0016F950` PS2), поэтому colors, power, passes и relationships намеренно не
переносятся. Portable clone повторяет это наблюдаемое поведение, хотя обычный
пользовательский deep copy выглядел бы логичнее.

## Проверка и оставшаяся граница

`python research/inspect_material_runtime.py` выполняет 42 read-only проверки:
хэши executable, RTTI/vtables, factories, точные PS2 constructors, defaults,
accessors, blank clone/copy и material-to-fog renderer path. CTest фиксирует
RTTI, defaults, bounds, clone-семантику и platform ABI `static_assert`.

Открыты original paths и spelling API, роли `+0x10/+0x18/+0x4C/+0x78`, exact
PC allocations, exact роли некоторых полей и контролируемый in-game mutation
test. Pass/layer ownership теперь закрыт в
[`native-class-sp-material-layers.md`](native-class-sp-material-layers.md);
следующая зависимость — фактический state-application/render dispatch.

PS2-only concrete sibling [`spPS2Material`](native-class-sp-ps2-material.md)
теперь разобран отдельно: он также занимает exact `0xD0`, использует те же
color/power offsets, но, в отличие от `spMaterialData`, полноценно копирует
общий material state и весь tail. Его update является первым подтверждённым
consumer-ом pass-графа.
