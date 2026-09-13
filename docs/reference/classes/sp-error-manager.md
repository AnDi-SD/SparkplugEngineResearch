# spErrorManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spErrorManager](../../../Sparkplug/Code/SparkBase/spErrorManager.h), [spPCErrorManager](../../../Sparkplug/Code/SparkplugPC/spPCErrorManager.h), [spPS2ErrorManager](../../../Sparkplug/Code/SparkplugPS2/spPS2ErrorManager.h).

Статус: exact PC source path, identity/base, abstract registration, singleton,
разные exact PC/PS2 layouts, fixed-stack allocator, LIFO chain, threshold и
lazy handler contract подтверждены. Платформенные handler implementations
принадлежат следующим классам `spPCErrorManager`/`spPS2ErrorManager`.

PC сохраняет точный путь
`Z:\Sparkplug\Code\SparkBase\spErrorManager.cpp`; header inferred.

## Identity и lifecycle

Registration имеет null factory/property callback: общий manager абстрактен.
Его третий class-local slot pure на PC (`0x0060DB76`) и null на PS2; оба
platform leaf реализуют его как provider обработчика.

## Platform layouts

Семантика полей одинакова, но встроенная ёмкость различна:

| Роль | PC | PS2 |
| --- | ---: | ---: |
| base | `+0x00..0x0F` | `+0x00..0x0F` |
| support vptr | `+0x10` | `+0x10` |
| message stack | `+0x14`, `0x1000` bytes | `+0x14`, `0x400` bytes |
| used bytes | `+0x1014` | `+0x414` |
| error head | `+0x1018` | `+0x418` |
| handler | `+0x101C` | `+0x41C` |
| handler-resolved byte | `+0x1020` | `+0x420` |
| exact size | `0x1024` | `0x424` |

Поэтому portable implementation параметризует capacity, а ABI не смешивает платформы в фиктивную общую структуру.

## Наблюдаемое поведение

PC `0x004139C0/0x00413A90/0x004137C0/0x004137F0/0x00413A40` и PS2
`0x00107840/0x00107720/0x00107B50` доказывают следующий контракт:

Порог сравнивается с severity головной ошибки. Если она ниже порога, цепочка
остаётся нетронутой. Safe reconstruction сохраняет side effects кроме
завершения процесса и показывает fatal через диагностический флаг.

## Явно открыто

- original names handler provider, formatter/dispatch методов и enum;
- precise native function signatures и calling convention callback;
- смысл padding bytes после `handlerResolved`;
- native overflow-error insertion path и поведение повторного overflow;
- точный PC fatal action и PS2 target `0x00403A88`;
- thread-safety (ни атомарности, ни блокировок пока не доказано);
- platform UI/console policy — предмет следующих двух leaf-классов.
