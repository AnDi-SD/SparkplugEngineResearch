# spLightDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLightDataSerializer](../../../Sparkplug/Code/Sparkplug/spLightDataSerializer.h).

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

PS2 factory вызывает `spNodeSerializer` constructor `0x00197540` и лишь меняет оба vptr. Собственного storage нет.

## Methods

Target slot возвращает `spLightData::ClassID` (`0x5E6402DF`). Clone создаёт
новый serializer через собственный factory и вызывает общий no-payload copy.
PS2 secondary thunks `0x001A5050/0x001A5060` корректируют `this` на `-0x10`
перед read/write.

## Writer contract

Сначала вызывается `spNodeSerializer::write`, затем открывается собственный
data block света. Поля идут строго по ID:

| ID | Значение | Условие записи |
| ---: | --- | --- |
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
но constructor `spLight` создаёт включённый свет. в известных данных поле явно
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

## Открытые вопросы

1. Original header, namespace и secondary-interface names.
2. Прямое PC allocation proof вместо protected-factory observed extent.
3. Инициализация отсутствующего field 8 на всех loader paths.
4. Byte-exact float-to-ARGB rounding/clamping для нештатных значений.
5. Stream status enum, block rollback и диагностический контракт.
