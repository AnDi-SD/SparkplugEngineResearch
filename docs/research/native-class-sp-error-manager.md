# `spErrorManager`: общий стек сообщений и маршрутизация ошибок

Статус: exact PC source path, identity/base, abstract registration, singleton,
разные exact PC/PS2 layouts, fixed-stack allocator, LIFO chain, threshold и
lazy handler contract подтверждены. Платформенные handler implementations
принадлежат следующим классам `spPCErrorManager`/`spPS2ErrorManager`.

## Контрольные бинарники

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

PC сохраняет точный путь
`Z:\Sparkplug\Code\SparkBase\spErrorManager.cpp`; header inferred.

## Identity и lifecycle

| Поле | PC | PS2 |
|---|---:|---:|
| Class ID | `0x660E40D8` | `0x660E40D8` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration | `0x0075A428` | `0x004A08C0` |
| Initializer | `0x006D1560` | `0x0047F540` |
| Getter | `0x004136E0` | `0x00107710` |
| Constructor | protected entry `0x00413680` | `0x00107CB0` |
| Destructor | `0x004136F0` | `0x00107C20` |
| Primary vtable | `0x006DB63C` | `0x0048C720` |
| Support vtable | `0x006DB638` | `0x0048C744` |
| Singleton global | `0x00755264` | `0x0049F80C` |

Registration имеет null factory/property callback: общий manager абстрактен.
Его третий class-local slot pure на PC (`0x0060DB76`) и null на PS2; оба
platform leaf реализуют его как provider обработчика.

## Platform layouts

Семантика полей одинакова, но встроенная ёмкость различна:

| Роль | PC | PS2 |
|---|---:|---:|
| base | `+0x00..0x0F` | `+0x00..0x0F` |
| support vptr | `+0x10` | `+0x10` |
| message stack | `+0x14`, `0x1000` bytes | `+0x14`, `0x400` bytes |
| used bytes | `+0x1014` | `+0x414` |
| error head | `+0x1018` | `+0x418` |
| handler | `+0x101C` | `+0x41C` |
| handler-resolved byte | `+0x1020` | `+0x420` |
| exact size | `0x1024` | `0x424` |

Размеры прямо подтверждаются allocations platform factories (`0x1024` у PC,
`0x424` у PS2). Поэтому portable implementation параметризует capacity, а ABI
не смешивает платформы в фиктивную общую структуру.

## Наблюдаемое поведение

PC `0x004139C0/0x00413A90/0x004137C0/0x004137F0/0x00413A40` и PS2
`0x00107840/0x00107720/0x00107B50` доказывают следующий контракт:

1. строки последовательно копируются в inline stack; новая граница должна быть
   строго меньше capacity;
2. ошибки связываются через `spError +0x24`, newest-first;
3. цепочка форматируется через virtual formatter каждой ошибки;
4. handler разрешается через platform virtual provider лениво и кешируется;
5. dispatch получает текст, severity, source и line головной ошибки;
6. после принятого dispatch цепочка размыкается и used сбрасывается;
7. severity не ниже `3` идёт в fatal path (PC/PS2 terminal action пока не
   воспроизводится переносимым кодом).

Порог сравнивается с severity головной ошибки. Если она ниже порога, цепочка
остаётся нетронутой. Safe reconstruction сохраняет side effects кроме
завершения процесса и показывает fatal через диагностический флаг.

## Реконструкция и проверка

Добавлены `Sparkplug/Code/SparkBase/spErrorManager.h/.cpp`, два exact ABI
layout, registration records и тестовый derived handler. Тесты проверяют
singleton, null factory/clone, stack boundary, LIFO formatting, threshold,
одноразовое разрешение handler и cleanup. Изолированная Windows x64 сборка
проходит `SparkBaseTests` и `SparkplugEngineTests` (2/2).

## Явно открыто

- original names handler provider, formatter/dispatch методов и enum;
- precise native function signatures и calling convention callback;
- смысл padding bytes после `handlerResolved`;
- native overflow-error insertion path и поведение повторного overflow;
- точный PC fatal action и PS2 target `0x00403A88`;
- thread-safety (ни атомарности, ни блокировок пока не доказано);
- platform UI/console policy — предмет следующих двух leaf-классов.
