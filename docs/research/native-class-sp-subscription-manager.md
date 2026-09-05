# `spSubscriptionManager`: маршрутизация object notifications

Статус: identity/base, factory, exact size/layout, singleton, vtables,
clone/lifetime и три операции subscribe/unsubscribe/dispatch подтверждены на
PC/PS2. Original TU/header и container typedef не найдены; путь
`Code/SparkBase` inferred по окружению common object system.

## Контрольные бинарники и identity

| Платформа | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0xE4567D00` | `0xE4567D00` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration | `0x0075A548` | `0x004A2470` |
| Initializer | `0x006D15F0` | `0x0047F730` |
| Primary vtable | `0x006DB788` | `0x0048C860` |
| Support vtable | `0x006DB784` | `0x0048C884` |
|Clone | `0x00416610` | `0x0010E630` |
| Factory | `0x004165B0` | `0x0010E750` |
| Singleton global | `0x0075537C` | `0x0049F808` |

PC factory и PS2 factory независимо выделяют `0x20`. Общий layout:

```text
+0x00  spBaseObject                    0x10
+0x10  singleton-support vptr          0x04
+0x14  subscription tree/container    0x0c
sizeof                                0x20
```

PC constructor/destructor bodies `0x00416500/0x00416450` закрыты `.rld`
trampolines, но allocation, field accesses, vtables и PS2 direct construction
независимо подтверждают layout. Clone создаёт пустой manager и вызывает
inherited empty copy, то есть runtime subscriptions не копируются.

## Три операции

PS2 даёт прямые тела:

- subscribe `0x0010E210` принимает manager, integer key и `spBaseObject*`;
- unsubscribe `0x0010E3B0` принимает ту же пару key/object;
- dispatch `0x0010DE90` читает key из notification `+0x0C`, находит группу и
  вызывает notification virtual каждого объекта.

PC dispatch `0x00415A20` делает тот же поиск и вызывает target по vtable
`+0x04`; это тот же source-level slot, что PS2 target по `+0x0C` после двух
ABI words. PC unsubscribe `0x004163A0` подтверждён 150 destructor callers.

Контейнер имеет два уровня: ordered key map и группа object pointers. Subscribe
сначала ищет уже существующий object и не добавляет duplicate; unsubscribe
удаляет пустую группу. Hundreds of constructor/destructor callers используют
эту пару симметрично (например key `5` у одного из PC game objects).

## Реконструкция и проверка

Добавлены `Sparkplug/Code/SparkBase/spSubscriptionManager.h/.cpp`, exact PC/PS2
ABI layouts/anchors и registration. Portable seam сохраняет integer grouping,
duplicate suppression, cleanup пустой группы и вызов `spBaseObject::vfunc_0C`.
Тест проверяет две группы, два подписчика, неизвестный key, duplicate,
unsubscribe и empty clone. Изолированная Windows x64 сборка проходит 2/2.

## Явно открыто

- original TU/header, method names и container typedef;
- точный source type ключа и notification base class;
- полная byte-layout notification variants (известный `+0x0C` — лишь key);
- порядок callbacks при одинаковом key и mutation semantics внутри callback;
- точный адрес PC subscribe body за защищёнными helper paths;
- owner lifetime: manager хранит сырые object pointers, но механизм
  обязательной destructor-unsubscribe ещё не выражен типом.
