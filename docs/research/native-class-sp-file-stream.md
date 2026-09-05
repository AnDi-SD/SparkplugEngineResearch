# `spFileStream`: общая абстрактная файловая граница

Статус: PC/PS2 class contract разобран и перенесён 4 сентября 2026 года.
Это малый промежуточный класс: он не добавляет instance fields и реализует
только default-read перегрузку `Open`. Отдельный original source/header path не
найден, поэтому оба portable-пути помечены inferred.

## Идентичность и layout

| Свойство | PC | PS2 |
|---|---:|---:|
| class ID | `0x5E0623EC` | `0x5E0623EC` |
| base | `spStream / 0x6CC80D8A` | `spStream / 0x6CC80D8A` |
| registration | `0x0084D590` | `0x004A2610` |
| vtable | `0x00729270` | `0x0048C9A0` |
| factory | null | null |
| размер | `0x1C` | `0x1C` |

PS2 constructor `0x00112200` вызывает `spStream` constructor и меняет только
vptr. PC constructor `0x006BE780` делает то же. Ни одна операция класса не
обращается за пределы `spStream +0x18`, поэтому новых полей нет.

## Vtable и поведение

| Роль | PC | PS2 | Результат |
|---|---:|---:|---|
| deleting destructor | `0x006BE7D0` | `0x001121A0` | разрушить base и при необходимости объект |
| clone | общий null target | `0x00112240` | null |
| registration getter | `0x006BE7A0` | `0x00112170` | registration выше |
| `Open(name)` | `0x006BE7C0` | `0x00112180` | вызвать virtual `Open(1, name)` |
| `Open(mode,name)` и следующие 7 операций | `_purecall` | null slots | abstract |
| `GetBuffer` | общий null target | `spStream::GetBuffer` | null |

На PC pure slots занимают `+0x20..+0x3C`, на PS2 — `+0x28..+0x44` после двух
служебных ABI-слов. Режим `1` совпадает с уже доказанным stream read flag.
Результат platform `Open` возвращается без преобразования.

Важно: строка `Z:\Sparkplug\Code\SparkBasePC\spPCFileStream.cpp` относится к
следующему PC leaf-классу. Приписывать её общему `spFileStream` оснований нет.
В PS2 функции общего класса лежат рядом с другими SparkBase stream methods, но
это также не доказывает имя translation unit.

## Реализация и тест

Добавлены inferred `Sparkplug/Code/SparkBase/spFileStream.h/.cpp`, RTTI record и
две evidence-layout структуры без новых полей. Тестовый platform leaf проверяет,
что `Open(name)`:

- передаёт mode `1` и исходное имя;
- сохраняет virtual dispatch;
- возвращает false от platform implementation без подмены;
- оставляет clone null и наследует null `GetBuffer`.

## Открытые вопросы

1. Original header/TU path и source-level namespace.
2. Original enum type и имя read enumerator; доказано только значение `1`.
3. Был ли one-argument `Open` объявлен inline в header либо находился в общем
   `.cpp`; linked code не различает эти варианты.
4. Точные qualifiers чистых virtual declarations наследуются как открытый
   вопрос `spStream`.
