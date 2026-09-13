# spLightData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLight](../../../Sparkplug/Code/Sparkplug/spLight.h), [spLightData](../../../Sparkplug/Code/Sparkplug/spLightData.h).

## Объект и поведение

Статус: concrete RTTI/factory, storage-free наследование от `spLight`,
constructor/destructor, clone и serializer-to-runtime offsets подтверждены на
обеих платформах. PC protected factory теперь исполнен: allocation exactF0;
copy/clone независимо подтвердили пропуск intensity, ранее известный по PS2.

| Платформа | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x5E6402DF / spLight` | то же |
| Layout | exact `0xF0` | exact `0x100` |

PS2 constructor вызывает только `spLight` constructor и заменяет primary и
embedded-support vtable. Собственных полей нет. Destructor аналогично
возвращает vtable в состояние `spLightData` перед вызовом base destructor; ни
одного derived resource cleanup не выполняется. PC destructor и все обращения
к полям подтверждают ту же storage-free форму.

### Concrete clone

На обеих платформах clone:

1. создаёт новый `spLightData` через concrete factory;
2. регистрирует source/destination в clone manager;
3. вызывает inherited `spLight::copy` virtual slot;
4. уничтожает destination и возвращает null при неуспехе copy.

### Связь serializer field с runtime offset

Структурная карточка
[`smo-class-sp-light-data.md`](sp-light-data.md) описывает sparse
SMO-секцию из девяти optional fields. Нативный writer теперь связывает их с
объектом без предположений:

| Field | Семантика | Default writer |
| ---: | --- | ---: |
| 0 | type | directional `0` |
| 1 | project shadow volume | false |
| 2 | ARGB color | `0xFFFFFFFF` |
| 3 | attenuation | false |
| 4 | intensity | `1.0` |
| 5 | range | `200.0` |
| 6 | hotspot angle | `0.0` |
| 7 | falloff angle | `0.0` |
| 8 | enabled | false при отсутствии field |

Writer PS2 `0x00191FC0..0x001926E0` и PC
`0x004401F3..0x004405D4` не обращаются к opaque runtime word
`+0xDC/+0xE8`. Этот word принадлежит `spLight`, а не скрытому десятому полю
формата.

### Собственная serializer-секция

Каждое поле необязательно и записывается только при отличии от default.

| Field | Ключ | Тип | Default | Наблюдения PC + PC + PS2 |
| ---: | --- | --- | --- | ---: |
| 0 | `light.type` | `UInt32 enum` | `0` — directional | 1 339 |
| 1 | `light.project_shadow` | `Boolean byte` | `false` | 0 |
| 2 | `light.color` | `ARGB UInt32` | `0xFFFFFFFF` | 1 247 |
| 3 | `light.attenuation` | `Boolean byte` | `false` | 0 |
| 4 | `light.intensity` | `Single` | `1.0` | 34 |
| 5 | `light.range` | `Single` | `200.0` | 1 101 |
| 6 | `light.hotspot_angle` | `Single`, радианы | `0.0` | 1 170 |
| 7 | `light.falloff_angle` | `Single`, радианы | `0.0` | 1 170 |
| 8 | `light.enabled` | `Boolean byte` | `false` | 1 482 |

Field 8 физически присутствует во всех объектах и всегда равен `true`. Поэтому
отсутствующее поле означает выключенный свет, а не включённый. Цвет хранится как
ARGB-число; байты в SMO видны в little-endian порядке BGRA.

Hotspot и falloff являются сырыми углами в радианах. Значения вроде `π/4` и
`-π/180` подтверждают единицу измерения, но в исходных ресурсах встречаются и очень
большие ненормализованные углы. Decoder намеренно не приводит их к диапазону
`0..2π`.

### Типы света

PC-функция визуализации helper-геометрии содержит явный switch:

| Значение | Тип | PC helper |
| ---: | --- | --- |
| 0 | directional | `Directional Light` / `Arrow Model` |
| 1 | point | `Point Light` / `Sphere Model` |
| 2 | spot | `Spot Light` / `Outer Cone Model` |
| 3 | ambient | helper отсутствует; все записи называются `Ambient01` |

| Directional | Point | Spot | Ambient |
| ---: | ---: | ---: | ---: |
| 48 | 348 | 0 | 118 |
| 48 | 348 | 0 | 118 |
| 47 | 331 | 0 | 76 |

Field 0 у directional отсутствует, потому что значение 0 является default. Spot —
реальный поддерживаемый движком подтип, хотя ни один доступный SMO его не использует.
Десять полных размеров/форм PC-объекта и девять PS2-форм — это комбинации пропущенных
default-полей, а не дополнительные типы света.

### Viewer и база

`SmoLightDataDecoder` читает только последнюю, собственную секцию и применяет
подтверждённые defaults. Так номера полей не смешиваются с одноимёнными ID в
унаследованной секции `spNode`/`spLight`. Read-only inspector показывает enum-имя
типа, цвет, intensity, range, углы и булевы значения; field 4 больше не остаётся hex-only.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spLightData
```

### Дополнение: нативный runtime layout

Последующий разбор классов связал все поля этой секции с `spLight` storage.
На PC offsets равны: type `+0xC0`, color `+0xC4`, attenuation `+0xD4`, intensity
`+0xD8`, range `+0xE0`, hotspot/falloff `+0xE4/+0xE8`, project shadow
`+0xEC`, enabled `+0xED`. На PS2 каждый из них сдвинут на `+0x0C` из-за
большего `spNode`, то есть начинается с type `+0xCC` и заканчивается enabled
`+0xF9`.
