# spMaterial / spMaterialData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMaterial](../../../Sparkplug/Code/Sparkplug/spMaterial.h), [spMaterialData](../../../Sparkplug/Code/Sparkplug/spMaterialData.h).

## Идентичность и lifetime

`spMaterial` имеет Class ID `0x5C0314C5`, в engine RTTI наследует `spBaseObject` и
регистрируется как абстрактный тип с null factory. `spMaterialData` имеет Class
ID `0x6160348B`, напрямую наследует `spMaterial` и является concrete leaf.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| `spMaterialData` factory | protected `0x0041A390` | `0x00134AB0` |
| размер `spMaterial` prefix | exact observed `0x78` | exact `0x80` |
| размер `spMaterialData` | actual allocation `0xBC` | exact aligned allocation `0xD0` |

Полные исходные paths не сохранились.

## Layout `spMaterial`

| Offset | Представление | Подтверждённая роль |
| ---: | --- | --- |
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

После общего prefix расположены четыре RGBA-вектора и один float.
Следующая таблица — **PS2**. PC offsets соответственно78/88/98/A8/B8,
secondary getter ECX=complete+14; defaults совпадают.

| Offset | Поле | Default |
| ---: | --- | --- |
| `0x80` | diffuse RGBA | white `(1,1,1,1)` |
| `0x90` | ambient RGBA | black `(0,0,0,1)` |
| `0xA0` | specular RGBA | white `(1,1,1,1)` |
| `0xB0` | emissive RGBA | black `(0,0,0,1)` |
| `0xC0` | specular power | `0.0` |

PS2 оставляет alignment padding `0xC4..0xCF`. Defaults независимо связаны с
глобальными black/white constants, которые инициализируются кодом по
`0x0047F2A0`. Secondary accessors `0x0016F7B0..0x0016F960` подтверждают
смещения getters/setters, включая specular power.
