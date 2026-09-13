# spPS2AsyncFileStreamManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

## Идентичность

| Свойство | Значение |
| --- | ---: |
| class ID | `0x57746EF7` |
| base | `spAsyncFileStreamManager / 0x7EA51364` |
| RTTI factory | `0x001DA890` |
| allocation size | `0x37A0` |

Initializer `0x00484B30` даёт имя/IDs/base/factory. Отдельного original source
path не найден. Factory, helper `0x001DA800` и clone `0x001DA6F0` независимо
выделяют `0x37A0` и повторяют один inlined constructor body; самостоятельный
constructor symbol/code range не выделен.

## Vtable и lifetime

| Primary slot | Target | Роль |
| ---: | ---: | --- |
| `+0x08` | `0x001DA680` | deleting destructor |
| `+0x0C` | `0x00100810` | notification no-op |
| `+0x10` | `0x001DA6F0` | clone |
| `+0x14` | `0x00105DC0` | inherited clone-copy |
| `+0x18` | `0x001DA380` | registration getter |
| `+0x30` | `0x001DA4B0` | request/load |
| `+0x34` | `0x001DA390` | update/pump |

Между common slots и leaf operations расположен support-vtable fragment;
adjustment destructor `0x001DA920` вычитает `0x14`. Destructor не освобождает
отдельный heap payload: он восстанавливает leaf vptrs и вызывает common manager
destruction. Весь большой queue block встроен непосредственно в объект.

## Exact layout

```text
+0x0000  spAsyncFileStreamManager (0x18)
+0x0018  constructed byte
+0x0019  padding[3]
+0x001C  request[50], 50 * 0x11C = 0x3778
+0x3794  queuedCount               (analytical name)
+0x3798  completionWatchdog
+0x379C  readyState                (analytical name)
+0x379D  padding[3]
          total 0x37A0
```

Каждая запись имеет доказанную границу `0x11C`:

| Record offset | Размер | Роль |
| ---: | ---: | --- |
| `+0x000` | `0x100` | stream name buffer |
| `+0x100` | `4` | platform command handle/result |
| `+0x104` | `4` | `spMemoryStream* destination` |
| `+0x108` | `4` | completion callback |
| `+0x10C` | `4` | callback context |
| `+0x110` | `4` | неизвестно; update не читает |
| `+0x114` | `4` | LSN/logical sector |
| `+0x118` | `4` | byte count |

Constructor обнуляет `0x3778` bytes records одним `memset`, обнуляет оба
tail-счётчика, ставит `readyState = 1` и после завершения initialization —
`constructed = 1`.

## Public request на `0x001DA4B0`

1. создать `spPS2FileStream` через `0x001DC640`;
2. открыть `name` через virtual one-argument `Open`;
3. получить size;
4. присвоить destination диагностическое имя через `spStream` helper
   `0x001148C0`;
5. вызвать `spMemoryStream::ResizeAndSetSize` (`0x00112260`);
6. вызвать stream-to-stream write destination slot `+0x40`;
7. без null-check вызвать callback(context);
8. закрыть и удалить source;
9. вернуть true.

Результаты Open/GetSize/resize/copy/Close не влияют на return. Request не
обращается к queue block и не увеличивает `queuedCount`. Поэтому имя класса не
означает, что этот public путь действительно отложен.

## `Update` и platform transfer

```text
Started streaming file %s, LSN = %d, SIZE = %dkb
Finished streaming file %s
```

После завершения вызывается callback/context текущей записи. Затем `memmove`
сдвигает `request[1..49]` в `request[0..48]` на `0x365C` bytes и счётчик
уменьшается. Для следующей записи:

- destination capacity = `(byteCount & ~0x7FF) + 0x1000`;
- число читаемых секторов = `(byteCount >> 11) + 1`;
- destination buffer получается через virtual `GetBuffer +0x48`;
- `0x001DCAE0(LSN, sectors, buffer)` отправляет platform RPC/stream command и
  возвращает handle в record `+0x100`.

Обе формулы намеренно дают дополнительный сектор и для exact multiple `0x800`;
это не обычный ceiling. Helper пишет параметры в uncached alias `0x204AD600`,
вызывает platform RPC `0x00415FE8` и ждёт ответ в `0x204AD640`. Byte flag по
GP offset `-0x4734` читается update и сбрасывается перед новым command; иных CPU
writes к нему нет, поэтому завершение, вероятно, приходит от IOP/RPC side.

## Недостижимая очередь

- три inlined constructor body;
- чтение/decrement/update в `0x001DA390`.

Нет CPU-кода, который увеличивает `queuedCount` или заполняет indexed record.
Public request полностью синхронен. Возможные объяснения пока равноправны:

1. producer был условно скомпилирован из этой game build;
2. generic engine оставил неиспользуемый consumer;
3. producer находился в overlay/module, которого нет в основном ELF;
4. часть состояния заполнялась внешним IOP protocol — менее вероятно для
   object-relative heap address, но без IOP module исключать нельзя.

В `Analysis/PS2/SparkBaseAbi.h` добавлены exact record/manager layouts,
class/registration/vtable/function addresses. В отдельном state helper
зафиксированы только доказанные чистые capacity/sector formulas. Tests проверяют
`0x11C`, 50 записей, offsets tail, итоговый размер `0x37A0` и граничные значения
формул (`0`, `0x800`, `0x801`). Нативный RPC не запускается на host.

## Открытые вопросы

1. Где producer queue и достижим ли update consumer в shipped игре.
2. Original source/header, имена record и tail flags.
3. Роль record `+0x110`.
4. Точный IOP/RPC module и семантика command handle `+0x100`.
5. Почему `host0:` и disc branches request скомпилированы в одинаковый
   synchronous path.
6. Нужно найти/разобрать IOP modules прежде, чем считать transfer completion
   protocol закрытым.
