# spEngineCore

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spEngineCore](../../../Sparkplug/Code/Sparkplug/spEngineCore.h).

Статус: identity, exact source path на PC, inheritance, factory/clone,
singleton lifetime, размеры и layout обеих платформ, обе vtable и все 18
class-local targets подтверждены. Главная последовательность initialize/shutdown,
default scene/camera и две callback-роли восстановлены по поведению. Исходные
имена 18 методов и большинства manager-полей не найдены, поэтому адресные
слоты остаются каноническими.

PC сохраняет точный путь
`Z:\Sparkplug\Code\Sparkplug\spEngineCore.cpp`. На PS2 найдено только имя
`spEngineCore.cpp`; inferred header размещён рядом с доказанным `.cpp`.

## Identity и создание

PC factory выделяет ровно `0x158`, PS2 factory — `0x150`. PS2 constructor
`0x00133380` прямо вызывает `spBaseObject::spBaseObject` `0x00102BF0`.
PC factory передаёт объект в entry `0x0041C7E0`, но это защитный trampoline
через pointer `0x013B2028` в `.rld`, а не статически читаемое тело. Поэтому
детали PC constructor берутся только из независимо видимых destructor/method
accesses и не дорисовываются по PS2.

Clone `0x0041CB90`/`0x00133500` создаёт новый core через тот же factory,
регистрирует пару в clone manager и вызывает у источника inherited copy slot.
Manager graph и runtime state не копируются. Это объясняет, почему portable
`vfunc_14` делегирует только `spBaseObject`.

## Singleton и lifetime

PS2 destructor сначала вызывает последний virtual shutdown slot, уничтожает
два inline container-блока, освобождает storage контейнера `+0x20`, отпускает
intrusive `+0x1C`, восстанавливает base support-vptr и вызывает destructor
`spBaseObject`. PC выполняет тот же порядок с compiler-specific offsets и
двумя container destructors от `+0xCC` и `+0x54`.

## Layout: реальная platform difference

Общий семантический prefix:

```text
+0x00  spBaseObject                              0x10
+0x10  singleton/support vptr                    0x04
+0x14  initialized byte                          0x01
+0x15  alignment padding                         0x03
+0x18  owned default-scene object                0x04
+0x1c  intrusive default-camera reference        0x04
```

После него ABI контейнера расходится:

| Роль | PC | PS2 |
| --- | ---: | ---: |
| первый inline container | `+0x20`, `0x10` bytes | `+0x20`, `0x0C` bytes |
| callback 1 | `+0x30` | `+0x2C` |
| callback 2 | `+0x34` | `+0x30` |
| manager/opaque fields | `+0x38..+0x53` | `+0x34..+0x4B` |
| область inline-подсистем | `+0x54..+0x153` | `+0x4C..+0x14B` |
| final owned manager/object | `+0x154` | `+0x14C` |
| exact size | `0x158` | `0x150` |

PS2 constructor дополнительно доказывает self-links `+0x104 = this+0x4C` и
`+0x148 = this+0x88`, а также helper-конструкцию двух двухэлементных контейнеров
со strides `0x3C` и `0x44`. PC destructor вызывает аналоги с теми же strides,
но по сдвинутым началам `+0x54/+0xCC`. Один общий C struct здесь был бы
фактически неверен; поэтому `Analysis/PC/SparkplugAbi.h` и
`Analysis/PS2/SparkplugAbi.h` содержат разные exact layouts.

## Vtables

PC primary table `0x006DC318` содержит семь стандартных slots `spBaseObject`,
затем 18 class-local slots `+0x1C..+0x60`. Secondary support table начинается
в `0x006DC310`, thunk `0x0041C5E0` корректирует `this-0x10`.

```text
PC targets:
41C110 41B420 41CBE0 41B3B0 41B6B0 41BE40
41C300 41B5F0 41BDF0 41C210 41C2A0 4D74A0
41BD60 41B3E0 41B900 41BA70 4D74A0 41C0B0
```

PS2 standard slots находятся в primary table `0x0048D340`, а 18 операций —
в support table `0x0048D364` после двух ABI words и thunk `0x00133A30`:

```text
PS2 targets:
131E30 131C90 1318B0 132F80 132B60 131F90
133100 132FD0 132360 1317E0 131750 132A70
132490 132430 132A80 132580 132A60 131EE0
```

Порядок первых двух targets между платформами не сводится простым добавлением
двух PS2 ABI words: PC `0x0041C110` и PS2 `0x00131C90` — main initialize,
а PC `0x0041B420` и PS2 `0x00131E30` — wrapper, собирающий временную request
record. Это зафиксировано как platform surface difference, а не «исправлено»
перестановкой адресов.

Два PC slots используют один stub `0x004D74A0`, возвращающий true с очисткой
двух stack arguments. PS2 компилятор испускает для этих ролей два отдельных
true-stub `0x00132A70/0x00132A60`.

## Initialize, scene и shutdown

PC `0x0041C110` и PS2 `0x00131C90`:

1. печатают/подготавливают engine banner (строки явно сохранены на PS2);
2. инициализируют корневую platform/renderer boundary;
3. последовательно вызывают четыре virtual stage;
4. немедленно возвращают false при первой ошибке;
5. только после всех успехов пишут `1` в `+0x14`.

PC `0x0041B6B0` последовательно поднимает network, utility, special-fx, input,
sound и graphics managers. `0x0041B900` продолжает shadow volume, particle,
bloom и связанные graphics groups. `0x0041BA70` поднимает physics, font,
resource, serializer, render-target и GUI managers. Названия групп опираются
на точные error strings `0x006DBFEC..0x006DC244`; concrete class каждого
global пока не назначается без разбора его registration.

PC `0x0041C300`/PS2 `0x00133100` создаёт default scene в `+0x18`, затем default
camera в `+0x1C` с intrusive retain. PC `0x0041B5F0` связывает camera с scene.
PC `0x0041BDF0`/PS2 `0x00132360` освобождает обе ссылки.

PC shutdown `0x0041C0B0`/PS2 `0x00131EE0` освобождает final owned object,
вызывает две teardown-стадии, отпускает корневой manager и в самом конце
обнуляет `+0x14`. Большой teardown `0x0041BE40`/PS2 `0x00131F90` освобождает
manager globals в обратной зависимости. Portable срез не создаёт фиктивные
manager classes: он воспроизводит доказанный short-circuit/state contract через
явный `InitializeForAnalysis` и оставляет реальное подключение следующим
классам.

Добавлены:

## Явно открыто

- original header, namespace и имена всех 18 virtual methods;
- скрытое PC constructor body за `.rld` trampoline;
- native camera-vector mutation/ownership и original type name двух event queues;
- original names и concrete class IDs manager globals;
- точные request/event structs для wrapper/dispatch slots;
- renderer reset и frame begin/end signatures;
- роль поля PS2 `+0x34`/примерного PC аналога;
- связь последних derived overrides с `wxEngineCore`.

Следующий класс по принятой bootstrap-очереди — `wxEngineCore`. Он должен
закрыть derived overrides и отделить Winx-specific stages от общего manager
bootstrap.
