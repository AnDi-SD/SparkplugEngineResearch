# `spLightDataSerializer`: сериализация данных света

Статус: PC/PS2 layout, две разные цепочки наследования, lifetime, target ID и
writer/default contract подтверждены. Потоковый codec остаётся evidence-only.

Точный исходный translation unit:

`Z:\Sparkplug\Code\Sparkplug\spLightDataSerializer.cpp`

## Две цепочки наследования

Это важный случай, где нельзя выводить C++-иерархию только из RTTI движка:

- constructor/destructor и вызовы base read/write доказывают C++-цепочку
  `spLightDataSerializer -> spNodeSerializer -> spSerializer`;
- registration обеих платформ задаёт base ID `0x42429877`, то есть напрямую
  `spSerializer`, пропуская `spNodeSerializer`.

Portable класс наследует `spNodeSerializer`, но его `spRTTIRecord::base`
указывает на `spSerializer`. Таким образом не потеряна ни одна из двух истин.

| Свойство | PC | PS2 |
|---|---:|---:|
| Class ID | `0x33EC2F8E` | `0x33EC2F8E` |
| Registration | `0x0075ED10` | `0x004AAFF0` |
| Initializer | `0x006D3000` | `0x00483CD0` |
| Factory | `0x0043FFD0` (protected) | `0x001A4FE0` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006E1974` | header `0x00490110` |
| Secondary vtable | `0x006E1968` | header `0x00490134` |

PS2 factory вызывает `spNodeSerializer` constructor `0x00197540` и лишь меняет
оба vptr. Собственного storage нет. PC destructor `0x0043FFA0` ставит те же
derived vptr и переходит в `spNodeSerializer` destructor `0x004638C0`, что
независимо подтверждает source-level base даже при защищённом factory.

## Methods

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0043FF90` | `0x001A4170` |
| write | `0x00440110` | `0x001A4180` |
| read | `0x00440640` | `0x001A48C0` |
| SBOO load override | `0x004400B0` | `0x001A4E00` |
| target class ID | `0x0043FFC0` | `0x001A4E80` |
| deleting destructor | `0x00440090` | `0x001A4E90` |
| blank clone | `0x00440040` | `0x001A4F00` |

Target slot возвращает `spLightData::ClassID` (`0x5E6402DF`). Clone создаёт
новый serializer через собственный factory и вызывает общий no-payload copy.
PS2 secondary thunks `0x001A5050/0x001A5060` корректируют `this` на `-0x10`
перед read/write.

## Writer contract

Сначала вызывается `spNodeSerializer::write`, затем открывается собственный
data block света. Поля идут строго по ID:

| ID | Значение | Условие записи |
|---:|---|---|
| 0 | type | не directional (`0`) |
| 1 | project shadow volume | `true` |
| 2 | ARGB color | не белый с допуском `0.001` на RGBA components |
| 3 | attenuation | `true` |
| 4 | intensity | не `1.0` |
| 5 | range | не `200.0` |
| 6 | hotspot angle | не `0.0` |
| 7 | falloff angle | не `0.0` |
| 8 | enabled | `true` |

Числа 4–7 сравниваются точно. Цвет сравнивается покомпонентно с native
константой `0x3A83126F` (`~0.001`), после чего writer квантует RGBA в 8-битный
ARGB payload. Opaque runtime word `+0xDC/+0xE8` не читается и не пишется.

Field 8 показывает реальное различие defaults: serializer подавляет `false`,
но constructor `spLight` создаёт включённый свет. В shipped corpus поле явно
равно `true` у всех объектов. Поведение отсутствующего field 8 при разных путях
создания объекта остаётся вопросом lifecycle, а не поводом переписать доказанный
writer.

## Portable срез

`BuildKnownWritePlanForAnalysis` возвращает два списка: inherited node section и
собственную light section. Это сохраняет повторяющиеся field IDs разных data
blocks и не выдаёт плоский список за настоящий SMO stream. RTTI/factory, blank
clone и target ID также восстановлены.

Не реализованы data-block framing, status/error enum, byte-exact ARGB conversion,
read rollback и общий object relationship context. Эти части нельзя безопасно
додумывать только по таблице полей.

## Проверка

Сериализованная сборка `ninja -j1` и два CTest-набора проходят. Тесты отдельно
проверяют flattened engine RTTI, C++-доступность через `spNodeSerializer`, exact
PS2/observed PC `0x14`, target ID, default-план `{node: IsAnimated,
light: Enabled}`, полный порядок изменённых light fields и blank clone.

## Открытые вопросы

1. Original header, namespace и secondary-interface names.
2. Прямое PC allocation proof вместо protected-factory observed extent.
3. Инициализация отсутствующего field 8 на всех loader paths.
4. Byte-exact float-to-ARGB rounding/clamping для нештатных значений.
5. Stream status enum, block rollback и диагностический контракт.


## CP88 update, 2026-09-07

The earlier portable-plan-only and protected-factory gaps are superseded by
[complete PC codec evidence](native-pc-light-serialization.md): actual factories,
19 exact184 native assertions,4 additional native boundary cases,source
read/write and fresh-object round trips. Remaining limits are stated there.
