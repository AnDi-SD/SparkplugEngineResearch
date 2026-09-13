# spFileStream

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFileStream](../../../Sparkplug/Code/SparkBase/spFileStream.h).

## Идентичность и layout

| Свойство | PC | PS2 |
| --- | ---: | ---: |
| class ID | `0x5E0623EC` | `0x5E0623EC` |
| base | `spStream / 0x6CC80D8A` | `spStream / 0x6CC80D8A` |
| размер | `0x1C` | `0x1C` |

PS2 constructor `0x00112200` вызывает `spStream` constructor и меняет только
vptr. PC constructor `0x006BE780` делает то же. Ни одна операция класса не
обращается за пределы `spStream +0x18`, поэтому новых полей нет.

## Vtable и поведение

| Роль | Результат |
| --- | --- |
| `Open(name)` | вызвать virtual `Open(1, name)` |
| `Open(mode,name)` и следующие 7 операций | abstract |
| `GetBuffer` | null |

На PC pure slots занимают `+0x20..+0x3C`, на PS2 — `+0x28..+0x44` после двух
служебных ABI-слов. Режим `1` совпадает с уже доказанным stream read flag.
Результат platform `Open` возвращается без преобразования.

Важно: строка `Z:\Sparkplug\Code\SparkBasePC\spPCFileStream.cpp` относится к
следующему PC leaf-классу. Приписывать её общему `spFileStream` оснований нет.
В PS2 функции общего класса лежат рядом с другими SparkBase stream methods, но
это также не доказывает имя translation unit.

## Открытые вопросы

1. Original header/TU path и source-level namespace.
2. Original enum type и имя read enumerator; доказано только значение `1`.
3. Был ли one-argument `Open` объявлен inline в header либо находился в общем
   `.cpp`; linked code не различает эти варианты.
4. Точные qualifiers чистых virtual declarations наследуются как открытый
   вопрос `spStream`.
