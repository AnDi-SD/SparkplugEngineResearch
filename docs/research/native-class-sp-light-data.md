# Нативный класс `spLightData`

Статус: concrete RTTI/factory, storage-free наследование от `spLight`,
constructor/destructor, clone и serializer-to-runtime offsets подтверждены на
обеих платформах. PC protected factory теперь исполнен: allocation exactF0;
copy/clone независимо подтвердили пропуск intensity, ранее известный по PS2.

| Платформа | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x5E6402DF / spLight` | то же |
| Registration / initializer | `0x0075D4E8 / 0x006D1D60` | `0x004A8260 / 0x00480E9C` |
| Factory / allocation | original protected `0x0041A330 / 0xF0` исполнен | `0x00134BB0 / 0x100` |
| Constructor | protected/unresolved entry | `0x0016ECC0` |
| Destructor | body `0x00435410`, deleting `0x00435430` | deleting `0x0016EC50` |
| Clone / getter | `0x0041ACA0 / 0x00435400` | `0x00134AF0 / 0x00135B00` |
| Primary / support vtable | `0x006DE990 / 0x006DE98C` | headers `0x0048D930 / 0x0048D970` |
| Layout | exact `0xF0` | exact `0x100` |

PS2 constructor вызывает только `spLight` constructor и заменяет primary и
embedded-support vtable. Собственных полей нет. Destructor аналогично
возвращает vtable в состояние `spLightData` перед вызовом base destructor; ни
одного derived resource cleanup не выполняется. PC destructor и все обращения
к полям подтверждают ту же storage-free форму.

## Concrete clone

На обеих платформах clone:

1. создаёт новый `spLightData` через concrete factory;
2. регистрирует source/destination в clone manager;
3. вызывает inherited `spLight::copy` virtual slot;
4. уничтожает destination и возвращает null при неуспехе copy.

Вследствие доказанной особенности `spLight::copy` PC и PS2 clone переносят все
light-параметры, кроме intensity: destination остаётся `1.0`. Portable
`spLightData::vfunc_10` воспроизводит именно этот порядок. Name, transform,
flags и дочерняя node-иерархия проходят через уже восстановленный `spNode`.

## Связь serializer field с runtime offset

Структурная карточка
[`smo-class-sp-light-data.md`](smo-class-sp-light-data.md) описывает sparse
SMO-секцию из девяти optional fields. Нативный writer теперь связывает их с
объектом без предположений:

| Field | Семантика | PC | PS2 | Default writer |
|---:|---|---:|---:|---:|
| 0 | type | `+0xC0` | `+0xCC` | directional `0` |
| 1 | project shadow volume | `+0xEC` | `+0xF8` | false |
| 2 | ARGB color | floats `+0xC4` | floats `+0xD0` | `0xFFFFFFFF` |
| 3 | attenuation | `+0xD4` | `+0xE0` | false |
| 4 | intensity | `+0xD8` | `+0xE4` | `1.0` |
| 5 | range | `+0xE0` | `+0xEC` | `200.0` |
| 6 | hotspot angle | `+0xE4` | `+0xF0` | `0.0` |
| 7 | falloff angle | `+0xE8` | `+0xF4` | `0.0` |
| 8 | enabled | `+0xED` | `+0xF9` | false при отсутствии field |

Последняя строка подчёркивает разницу constructor и serialized default:
constructor включает light (`true`), но decoder отсутствующего field 8 обязан
дать `false`. Реальные 1 482 наблюдения корпуса явно записывают `true`, поэтому
противоречия в shipped assets нет.

Writer PS2 `0x00191FC0..0x001926E0` и PC
`0x004401F3..0x004405D4` не обращаются к opaque runtime word
`+0xDC/+0xE8`. Этот word принадлежит `spLight`, а не скрытому десятому полю
формата.

## Переносимый срез и границы

`Sparkplug/Code/Sparkplug/spLightData.*` добавляет concrete factory/RTTI и
clone поверх `spLight`; дублирующего storage нет. Тесты проверяют hierarchy,
defaults, все переносимые light-поля, exact PS2/PC ABI и
intensity-аномалию clone.

Original PC allocation/copy/clone и сценовая регистрация теперь проверены в
[`spLightManager`](native-class-sp-light-manager.md):90 native checks и1800
selection/cache comparisons. Clone-pair там recording seam; complete native
root-clone transaction не объявлен готовым. Portable Scene wiring остаётся
отдельным. Serializer writer/reader и renderer/shadow-volume backend также
не включаются в data-класс; existing `spLightDataSerializer` имеет свою карточку.
