# Нативный класс `spLight`

Статус: RTTI, C++-база, конструктор, layout, значения по умолчанию, inherited
copy, transform-update и граница scene-light helper подтверждены на PC и PS2.
Класс абстрактен для RTTI: factory отсутствует, а clone базового `spLight`
возвращает null. Переносимый срез восстановлен; собственно регистрация света в
scene/render backend оставлена evidence-only.

| Платформа | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x72444900 / spNode` | то же |
| Registration / initializer | `0x0075E278 / 0x006D2AE0` | `0x004A9550 / 0x00482A20` |
| Constructor | `0x00428CA0` | `0x0016DE40` |
| Destructor | body `0x00428DB0`, deleting `0x00428F70` | `0x0016DDD0` |
| Clone / copy / getter | protected `0x004A1BF0` / protected `0x00428EB0` / `0x00428D90` | `0x0016E000` / `0x0016D9F0` / `0x0016D9E0` |
| Transform update | `0x00428C30` | `0x0016DAC0` |
| Light helper | `0x00428DD0` | `0x0016DB30` |
| Primary / support vtable | `0x006DCC18 / 0x006DE98C` | headers `0x0048E7E0 / 0x0048E820` |
| Object extent | exact `0xF0` через original `spLightData` factory | exact `0x100` через concrete descendant |

Исходный путь самого класса не найден. Путь
`Code/Sparkplug/spLightDataSerializer.cpp` относится к отдельному serializer,
поэтому `Code/Sparkplug/spLight.*` явно считается inferred.

## Layout и значения

Сдвиг всех light-полей между платформами равен `0x0C`: это разница размера
унаследованного `spNode` (`0xB4` PC против `0xC0` PS2).

| Роль | PC | PS2 | Constructor default |
|---|---:|---:|---:|
| embedded scene-light vptr | `+0xB4` | `+0xC0` | platform vtable |
| PC scene-list previous/next (PS2 роль отдельно не перепроверена) | `+0xB8/+0xBC` | `+0xC4/+0xC8` | `0, 0` |
| type | `+0xC0` | `+0xCC` | `0`, directional |
| normalized color R,G,B,A | `+0xC4` | `+0xD0` | white |
| attenuation | `+0xD4` | `+0xE0` | false |
| intensity | `+0xD8` | `+0xE4` | `1.0` |
| opaque runtime word | `+0xDC` | `+0xE8` | **не инициализируется** |
| range | `+0xE0` | `+0xEC` | `200.0` |
| hotspot / falloff | `+0xE4/+0xE8` | `+0xF0/+0xF4` | `0.0 / 0.0` |
| project shadow volume | `+0xEC` | `+0xF8` | false |
| enabled | `+0xED` | `+0xF9` | true |

Порядок цветовых float доказан writer-кодом: он собирает little-endian BGRA
payload из `R,G,B,A`, то есть сериализованное 32-битное значение является ARGB.
На PC конструктор читает default `0xFFFFFFFF`; PS2 берёт те же четыре
глобальных byte-компонента после startup-инициализации.

Последний известный byte PC равен `+0xED`; original concrete factory41A330
теперь исполнен и выделяет exact `0xF0`, а не только предполагаемый extent.
Constructor padding и opaqueDC действительно не инициализируются. На PS2
`spLightData` ничего не добавляет и его factory непосредственно выделяет
`0x100`, поэтому размер `spLight` там доказан точно.

## Copy и подтверждённая аномалия intensity

PS2 `0x0016D9F0` сначала вызывает `spNode` copy, затем переносит type, четыре
цветовых float, attenuation, opaque `+0xE8`, range, оба угла, project-shadow и
enabled. Единственное сериализуемое поле, которое он пропускает, — intensity
`+0xE4`. Поэтому свежий clone `spLightData` сохраняет constructor default
`1.0`, даже если source имел другую интенсивность.

Это выглядит как ошибка оригинала, но реконструкция не подменяет факт более
удобным поведением. Portable `vfunc_14` повторяет доказанный PS2 contract и тест
явно закрепляет потерю intensity. PC copy `428EB0 → 505CB0` теперь независимо
исполнен: existing destination9.5 сохраняется при source3.25, actual fresh
LightData clone остаётся1.0. См. [PC light manager checkpoint](native-class-sp-light-manager.md).

Opaque word — обратный случай: constructor его не пишет, serializer его не
читает и не записывает, но copy переносит как 32 bits. Host-реконструкция
безопасно инициализирует его нулём и сохраняет raw bits при copy; смысл и
нативный тип не заявляются.

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

## Переносимый срез и открытые вопросы

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
