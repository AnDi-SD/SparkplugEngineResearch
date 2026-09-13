# spCrossPlatform

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

```text
spBaseObject (0x10)
  -> spNamedObject (0x14)
       -> spCrossPlatform (0x14 на PS2)
            -> spStream / spRenderer / managers / platform interfaces
```

`spCrossPlatform` не добавляет полей, не регистрирует собственные свойства и не
имеет RTTI factory. При этом он не является простым alias: у него собственные
vtable, constructor/destructor, registration getter и отдельный контракт
клонирования. Поэтому класс достаточно мал для второго полного пилота и
одновременно открывает путь к ресурсной вертикали через `spStream`.

Контрольные бинарники:

## RTTI и registration

Обе версии дают один контракт:

PS2 initializer `0x0047F480` передаёт в общий registration constructor
`0x001115B0` адрес записи, оба ID, строку `spCrossPlatform`, registration
`spNamedObject` и два нулевых callback. PC initializer делает то же через
`0x00412FF0`. Следовательно, тип известен native RTTI, но не создаётся напрямую
через `spRTTIManager::Create` и не имеет собственной группы свойств.

Нулевой factory сам по себе не доказывает C++ `abstract`: access modifiers и
наличие pure virtual declarations из бинарника пока не восстановлены. Он
доказывает только запрет прямого создания через runtime registration.

## Layout PS2

Constructor `0x00105D60` вызывает constructor `spNamedObject` `0x00105F60`,
после чего заменяет только vptr на `0x0048C660`. Записей в новые instance
offsets нет. Независимое подтверждение границы даёт constructor
`spMasterTimer` `0x00114F50`: после вызова `spCrossPlatform` он начинает
собственные поля с `+0x14`.

```text
+0x00  spBaseObject
+0x10  spNamedObject::sharedNameEntry
+0x14  конец spCrossPlatform / начало данных производного класса
```

Для PC class ID, inheritance и vtable подтверждены непосредственно. Равный
32-битный layout очень хорошо согласуется с кодом и ABI, но отдельный PC size
claim пока не вводится: нужные constructor bodies в дисковом образе затронуты
SecuROM transformations.

## Constructor, destructor и clone

PS2 constructor:

1. вызывает `spNamedObject` constructor;
2. устанавливает vtable `0x0048C660`;
3. не создаёт новых полей.

PS2 deleting destructor `0x00105D00` сначала возвращает vptr класса, затем
вызывает `spNamedObject` destructor `0x00105EE0` и при положительном delete-флаге
освобождает объект. Это обычная цепочка производного класса без собственного
ресурса.

Clone slot намеренно возвращает null. На PS2 для него выпущен отдельный stub
`0x00105DA0`; на PC vtable повторно использует root implementation
`0x004A1BF0` с тем же результатом. Copy slot остаётся унаследованным от
`spNamedObject`, поэтому производные классы могут использовать его в своих
clone implementations, но прямой `spCrossPlatform::Clone` результата не даёт.

## Vtables

PS2 vtable начинается с двух ABI-слов; функциональные slots идут с `+0x08`:

| Slot | Target | Наблюдаемая роль |
| ---: | ---: | --- |
| `+0x08` | `0x00105D00` | deleting destructor |
| `+0x0C` | `0x00100810` | inherited empty notification handler |
| `+0x10` | `0x00105DA0` | null clone |
| `+0x14` | `0x00105DC0` | inherited `spNamedObject` copy |
| `+0x18` | `0x00105CF0` | вернуть registration `0x004A0260` |
| `+0x1C` | `0x00100010` | inherited exact-type check |
| `+0x20` | `0x00100050` | inherited base-chain check |

PC vtable `0x006DB8FC` не имеет двух начальных PS2 ABI-слов:

| Slot | Target | Соответствие PS2 |
| ---: | ---: | --- |
| `+0x00` | `0x00417B00` | deleting destructor |
| `+0x04` | `0x005B7A00` | empty notification handler |
| `+0x08` | `0x004A1BF0` | null clone |
| `+0x0C` | `0x00413120` | `spNamedObject` copy |
| `+0x10` | `0x00417AF0` | registration getter |
| `+0x14` | `0x00408350` | exact-type check |
| `+0x18` | `0x00408370` | base-chain check |

## Архитектурная роль

В PC registration graph у класса 20 прямых наследников, в PS2 — 13. Общая
часть включает `spStream`, `spRenderer`, `spInputDevice`, `spInputManager`,
`spFontManager`, `spParticleFX`, projection/render-target managers и
`spVideoStream`. PC добавляет DX/network/thread/shadow ветви.

Это позволяет описать `spCrossPlatform` как nominal boundary для объектов,
которые имеют общий интерфейс/имя, но получают реализацию ниже по платформенной
ветке. Наиболее полезная следующая цепочка:

```text
spCrossPlatform
  -> spStream
       -> spFileStream
            -> spPCFileStream / spPS2FileStream
       -> spMemoryStream
```

Собираемая реализация сохраняет подтверждённые свойства:

- class/base IDs и RTTI chain;
- отсутствие factory и property callback;
- унаследованное имя;
- null clone;
- отдельный current-registration getter.

## Что остаётся неизвестным

- оригинальные имена virtual methods и access modifiers;
- был ли класс source-level abstract или только runtime non-creatable;
- точный original header/translation unit;
- непосредственно читаемые PC constructor/non-deleting-destructor bodies;
- отдельное формальное доказательство PC instance size.

Эти неизвестные не мешают считать малый класс практически закрытым: его
наблюдаемое состояние исчерпывается базой `spNamedObject`, а runtime-контракт
совпадает на двух платформах.
