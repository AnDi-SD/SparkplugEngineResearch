# spApp

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spApp](../../../Sparkplug/Code/SparkBase/spApp.h).

## Identity и необычная двойная иерархия

Обе регистрации передают одинаковые значения:

При этом реальный C++ constructor строит не `spBaseObject`, а
`spCrossPlatform`: PS2 `0x00100230` прямо вызывает
`spCrossPlatform::spCrossPlatform` `0x00105D60`. PC entry `0x004019B0`
перед общим tail передаёт управление соответствующему protected constructor
`0x00417AC0`; tail затем начинает собственные поля с `+0x14`, ровно после
layout `spCrossPlatform`.

Это не повод «исправлять» одну цепочку по другой. Здесь существуют две разные
системы:

```text
C++ layout/lifetime: spNamedObject -> spCrossPlatform -> spApp
engine registration: spBaseObject -> spApp
```

## Общий layout PC/PS2

Обе версии дают размер `0x20` и одинаковые offsets:

```text
+0x00  spCrossPlatform                         0x14
+0x14  secondary singleton-support vptr       0x04
+0x18  byte flag, constructor value 0          0x01
+0x19  alignment padding                       0x03
+0x1c  nullable owned char*                    0x04
```

## Vtables и platform difference

PC primary vtable начинается в `0x006F2F08`:

| Slot | Target | Поведение |
| ---: | ---: | --- |
| `+0x00` | `0x004CA220` | deleting destructor |
| `+0x04` | `0x005B7A00` | inherited no-op notification |
| `+0x08` | `0x004A1BF0` | null clone |
| `+0x0C` | `0x00413120` | inherited `spNamedObject` copy |
| `+0x10` | `0x004CA180` | current registration |
| `+0x14` | `0x00408350` | exact-type check |
| `+0x18` | `0x00408370` | registration-chain check |
| `+0x1C` | `0x004CA080` | вернуть C-строку `""` |
| `+0x20..+0x2C` | `0x0060DB76` | четыре pure-call slots |

Secondary live-object vptr равен `0x006F2F04`; его единственный destructor
thunk `0x004CA190` корректирует `this` на `-0x14`. Base-support destructors —
`0x004CA100/0x004CA120`, полный non-deleting destructor — `0x004CA1A0`.

PS2 primary table `0x0048C580` содержит два ABI-слова перед теми же семью
`spBaseObject` slots. Null clone — отдельный stub `0x001002F0`. В отличие от
PC, empty-string method `0x00100170` находится во secondary table
`0x0048C5A4` после thunk `0x00100300`; четыре PC pure slots в этом PS2 table
не присутствуют. Это реальное platform-surface различие, а не повод создать
универсальную фиктивную ABI-таблицу.

Portable класс реализует общий empty-string результат и четыре lifecycle slots
как abstract contract. Их роли независимо закрыты платформенными наследниками;
исходные имена всё ещё не доказаны, поэтому C++ сохраняет offset-oriented
`vfunc_20_Initialize` и аналогичные имена.

## Связь со `spPCApp`/`spPS2App`

Старая атрибуция `0x004C33C0` к `spPCApp` оказалась ошибочной: getter объекта,
который создаёт этот factory, возвращает registration `0x00764770` класса
`spPCErrorManager`. Соседняя строка `spPCApp` не являлась достаточным
доказательством владельца vtable. Ошибка исправлена до реконструкции
платформенного класса.

Добавлены:

## Открытые вопросы

- original header/translation-unit и namespace;
- исходное имя singleton-support base;
- исходные имена и consumers полей `+0x18/+0x1C`;
- исходное имя/владелец empty-string virtual;
- сигнатуры и роли четырёх PC-only pure slots;
- точное назначение вызываемого destructor-каскада managers;
- исходные signatures/names platform lifecycle hooks и concrete `wx...`
  overrides; подробности вынесены в platform class cards.
