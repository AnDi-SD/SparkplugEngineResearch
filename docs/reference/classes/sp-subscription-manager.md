# spSubscriptionManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSubscriptionManager](../../../Sparkplug/Code/SparkBase/spSubscriptionManager.h).

Статус: identity/base, factory, exact size/layout, singleton, vtables,
clone/lifetime и три операции subscribe/unsubscribe/dispatch подтверждены на
PC/PS2. Original TU/header и container typedef не найдены; путь
`Code/SparkBase` inferred по окружению common object system.

| Платформа | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0xE4567D00` | `0xE4567D00` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Initializer | `0x006D15F0` | `0x0047F730` |
| Singleton global | `0x0075537C` | `0x0049F808` |

PC factory и PS2 factory независимо выделяют `0x20`. Общий layout:

```text
+0x00  spBaseObject                    0x10
+0x10  singleton-support vptr          0x04
+0x14  subscription tree/container    0x0c
sizeof                                0x20
```

## Три операции

PS2 даёт прямые тела:

- subscribe `0x0010E3B0` принимает manager, integer key и `spBaseObject*`;
- unsubscribe `0x0010E210` принимает ту же пару key/object;
- dispatch `0x0010DE90` читает key из notification `+0x0C`, находит группу и
  вызывает notification virtual каждого объекта.

Контейнер имеет два уровня: ordered key map и ordered set object pointers. Subscribe
сначала ищет уже существующий object и не добавляет duplicate; unsubscribe
удаляет пустую группу. Hundreds of constructor/destructor callers используют
эту пару симметрично (например key `5` у одного из PC game objects).

## Явно открыто

- original TU/header, method names и container typedef;
- точный source type ключа и notification base class;
- полная byte-layout notification variants (известный `+0x0C` — лишь key);
- mutation/reentry semantics внутри callback; порядок подтверждён только
  при неизменяемой во время рассылки группе, безопасное удаление текущего
  элемента или всей группы не заявляется;
- owner lifetime: manager хранит сырые object pointers, но механизм
  обязательной destructor-unsubscribe ещё не выражен типом.
