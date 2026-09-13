# spLight

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLight](../../../Sparkplug/Code/Sparkplug/spLight.h).

| Платформа | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x72444900 / spNode` | то же |
| Transform update | `0x00428C30` | `0x0016DAC0` |
| Light helper | `0x00428DD0` | `0x0016DB30` |
| Object extent | exact `0xF0` через original `spLightData` factory | exact `0x100` через concrete descendant |

Путь `Code/Sparkplug/spLightDataSerializer.cpp` относится к отдельному serializer, поэтому `Code/Sparkplug/spLight.*` явно считается inferred.

## Layout и значения

Сдвиг всех light-полей между платформами равен `0x0C`: это разница размера
унаследованного `spNode` (`0xB4` PC против `0xC0` PS2).

| Роль | PC | PS2 |
| --- | ---: | ---: |
| embedded scene-light vptr | `+0xB4` | `+0xC0` |
| type | `+0xC0` | `+0xCC` |
| normalized color R,G,B,A | `+0xC4` | `+0xD0` |
| attenuation | `+0xD4` | `+0xE0` |
| intensity | `+0xD8` | `+0xE4` |
| opaque runtime word | `+0xDC` | `+0xE8` |
| range | `+0xE0` | `+0xEC` |
| hotspot / falloff | `+0xE4/+0xE8` | `+0xF0/+0xF4` |
| enabled | `+0xED` | `+0xF9` |

Порядок цветовых float доказан writer-кодом: он собирает little-endian BGRA
payload из `R,G,B,A`, то есть сериализованное 32-битное значение является ARGB.
На PC конструктор читает default `0xFFFFFFFF`; PS2 берёт те же четыре
глобальных byte-компонента после startup-инициализации.

Последний известный byte PC равен `+0xED`; original concrete factory41A330
теперь исполнен и выделяет exact `0xF0`, а не только предполагаемый extent.
Constructor padding и opaqueDC действительно не инициализируются. На PS2
`spLightData` ничего не добавляет и его factory непосредственно выделяет
`0x100`, поэтому размер `spLight` там доказан точно.

## Transform и scene-light граница

PC `0x00428C30` и PS2 `0x0016DAC0` вызывают унаследованный update `spNode`,
проверяют dirty bit `0x8` (PC: current flags OR inherited argument), очищают
его в собственных flags и при ненулевом scene link
передают полный light в manager. Это подтверждает, что light state существует
отдельно от сериализованной секции и обновляется вместе с world transform.

PS2 helper `0x0016DB30` имеет switch по type `0..3`; point/spot пути используют
world position и range, а directional/ambient проходят отдельные ветви.
Функции `0x0016E200`, `0x0016E460` и `0x0016E890` дополнительно отбрасывают
disabled lights, project-shadow lights в обычном проходе и point/spot lights
вне range. Имена manager/container типов и точные сигнатуры этих методов пока
не восстановлены, поэтому renderer API в portable class не выдуман.

## Границы описания

`Sparkplug/Code/Sparkplug/spLight.*` содержит abstract RTTI record, безопасные
semantic getters/setters, constructor defaults и подтверждённый copy. Exact
PC/PS2 bytes живут отдельно в `Analysis/*/SparkplugAbi.h`.

PC scene34 теперь связан с оригинальным `spLightManager`:46ACE0→46AC60
обновляет кэши render nodes/partition payloads; конкретные алгоритмы и
portable list/selection source описаны в отдельной карточке выше.

Открыты: исходное имя embedded support-типа;
назначение opaque word; full portable scene/world integration;
side effects оригинальных property setters; правила
нормализации/валидации type, range и углов вне serializer.
