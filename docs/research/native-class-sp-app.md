# `spApp`: общий application-lifetime объект

Статус: собираемая реконструкция подтверждённой общей части. Имя, class ID,
registration, отсутствие factory/property callback, singleton lifetime, размер,
поля, обе vtable и constructor/destructor проверены на PC и PS2. Исходный путь,
имена двух полей и исходное имя единственного concrete string-method не найдены.

## Контрольные бинарники

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

Исполняемые файлы не изменялись. Проверка проводилась статически, а portable
срез собирался и тестировался отдельно.

## Identity и необычная двойная иерархия

Обе регистрации передают одинаковые значения:

| Поле | PC | PS2 |
|---|---:|---:|
| Class ID | `0x391B146A` | `0x391B146A` |
| Зарегистрированный base ID | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration | `0x00764E30` | `0x0049FF00` |
| Initializer | RVA `0x002D5AC0` | `0x0047F180` |
| Getter | `0x004CA180` | `0x00100000` |
| Factory/property callback | `0 / 0` | `0 / 0` |

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

Portable C++ поэтому наследуется от `spCrossPlatform`, но `StaticRTTI()` хранит
base `spBaseObject`. В результате C++ `dynamic_cast` видит CrossPlatform, а
движковый `IsKindOf(spCrossPlatform)` — нет; именно это проверяет автотест.

## Общий layout PC/PS2

Обе версии дают размер `0x20` и одинаковые offsets:

```text
+0x00  spCrossPlatform                         0x14
+0x14  secondary singleton-support vptr       0x04
+0x18  byte flag, constructor value 0          0x01
+0x19  alignment padding                       0x03
+0x1c  nullable owned char*                    0x04
```

PS2 constructor полностью виден: публикует complete-object pointer в
`0x0049F800`, устанавливает primary/support vptr, пишет ноль в `+0x18` и
`+0x1C`. PC readable tail делает те же записи и публикует pointer в
`0x00764708`. Оба destructors освобождают ненулевую строку `+0x1C`, очищают
singleton и вызывают общий shutdown глобально принадлежащих engine services.
Последний каскад пока не переносится в `spApp`: соответствующие managers ещё не
реконструированы и подменять их фиктивными объектами нельзя.

Роль байта `+0x18` и назначение текста `+0x1C` не доказаны. В portable классе
они называются `stateFlag_`/`ownedText_` только аналитически. Безопасные
`ForAnalysis` wrappers существуют для теста ownership и не выдаются за native
API.

## Vtables и platform difference

PC primary vtable начинается в `0x006F2F08`:

| Slot | Target | Поведение |
|---:|---:|---|
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

Последующая независимая проверка подтвердила оба inheritance edge. PC
registration `0x00764710` хранит class/base IDs
`0x7635EFDE/0x391B146A`, а constructor `0x00447E50` вызывает protected
`spApp` constructor и возвращает secondary vptr на `+0x14`. PS2 registration
`0x004B0EC0` аналогично хранит `0x354B1350/0x391B146A`, а constructor
`0x001E7730` прямо вызывает `0x00100230`.

Старая атрибуция `0x004C33C0` к `spPCApp` оказалась ошибочной: getter объекта,
который создаёт этот factory, возвращает registration `0x00764770` класса
`spPCErrorManager`. Соседняя строка `spPCApp` не являлась достаточным
доказательством владельца vtable. Ошибка исправлена до реконструкции
платформенного класса.

## Состояние реконструкции

Добавлены:

- `Sparkplug/Code/SparkBase/spApp.h/.cpp` — inferred path, portable behavior;
- `Sparkplug/Code/SparkplugPC/spPCApp.h/.cpp` и inferred
  `Sparkplug/Code/SparkplugPS2/spPS2App.h/.cpp`;
- `spAppLayout` в отдельных PC/PS2 ABI evidence headers;
- регистрация, singleton/null-clone/empty-string/field-ownership tests;
- проверка различия C++ hierarchy и native RTTI hierarchy.

Изолированная Windows x64 сборка и `SparkBaseTests` проходят полностью.

## Открытые вопросы

- original header/translation-unit и namespace;
- исходное имя singleton-support base;
- исходные имена и consumers полей `+0x18/+0x1C`;
- исходное имя/владелец empty-string virtual;
- сигнатуры и роли четырёх PC-only pure slots;
- точное назначение вызываемого destructor-каскада managers;
- исходные signatures/names platform lifecycle hooks и concrete `wx...`
  overrides; подробности вынесены в platform class cards.

Эти вопросы не скрыты удобными догадками и переносятся в следующий bootstrap
срез.
