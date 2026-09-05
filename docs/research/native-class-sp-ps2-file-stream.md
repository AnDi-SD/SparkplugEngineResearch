# `spPS2FileStream`: PS2-файловый leaf и две I/O-ветки

Статус: глубокий статический срез завершён 4 сентября 2026 года. Доказаны RTTI,
размер `0x114`, полная vtable, embedded `ShellFile`, локальный double buffer и
граница альтернативного shared-async-manager пути. Нативный PS2 SDK/backend на
host не подменяется: точный layout и чистые части state machine вынесены в
`Sparkplug/Analysis/PS2`, а неизвестные platform calls оставлены зависимостями.

## Происхождение и RTTI

Контрольный файл — `local-data/Winx Club the game PS2/SLES_532.19`, SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.
Имя `spPS2FileStream` находится в binary по `0x00459F00`; initializer
`0x00484B70` связывает его с регистрацией:

| Свойство | Значение |
|---|---:|
| class ID | `0x12FDDAB3` |
| base class | `spFileStream / 0x5E0623EC` |
| registration | `0x004AD2D0` |
| vtable | `0x00490FA0` |
| factory | `0x001DC680` |
| constructor | `0x001DC4E0` |
| native size | `0x114` |

Factory прямо выделяет `0x114` байт. Original `.cpp`/header path для этого
класса не найден; `SparkBasePS2` является пока только обоснованной module
границей, а не доказанным путём исходника.

## Layout

Constructor сначала вызывает `spFileStream`, затем обнуляет все leaf-поля,
`ShellFile` размером `0x98` и два descriptor по `0x10`; default mode равен `1`.

| Offset | Размер | Наблюдаемая роль |
|---:|---:|---|
| `+0x00` | `0x1C` | `spFileStream` |
| `+0x1C` | `4` | open mode |
| `+0x20/+0x21` | `1+1` | PCK-resolution и backend/path kind |
| `+0x24` | `4` | физический размер backing file |
| `+0x28` | `1` | open flag |
| `+0x2C` | `4` | logical/PCK-entry size override |
| `+0x30` | `4` | размер одного буфера |
| `+0x34` | `4` | абсолютная позиция |
| `+0x38` | `1` | buffer-initialization state |
| `+0x3C` | `0x98` | embedded `ShellFile` |
| `+0xD4` | `4` | индекс активного буфера `0/1` |
| `+0xD8` | `0x20` | два `{fileOffset,cursor,data,ready}` descriptor |
| `+0xF8` | `1` | выбор shared async-manager path |
| `+0xFC..+0x108` | `0x10` | source kind/handle-or-LSN/origin/physical size |
| `+0x10C/+0x110` | `4+4` | optional owned path strings |

Собственные функции `ShellFile` доказывают его поля: path `[0x80]`, backend
mode `+0x80`, handle `+0x84`, position `+0x88`, size `+0x8C`, sector-like word
`+0x90` и backend kind `+0x94`. Имя типа подтверждается строками самого leaf:
`Loading file %s using ShellFile` и вариантом `from PCK`.

## Vtable

В PS2 ABI перед обычными slots находятся два служебных слова, поэтому offsets
на восемь байт больше PC offsets.

| Slot | Target | Роль |
|---:|---:|---|
| `+0x08` | `0x001DC460` | deleting destructor |
| `+0x0C` | `0x00100810` | inherited notification no-op |
| `+0x10` | `0x001DC580` | clone |
| `+0x14` | `0x00105DC0` | inherited clone-copy |
| `+0x18` | `0x001DA930` | registration getter |
| `+0x1C/+0x20` | `0x00100010/0x00100050` | exact/kind checks |
| `+0x24` | `0x001DC440` | `Open(name) -> Open(1,name)` |
| `+0x28` | `0x001DC020` | `Open(mode,name)` |
| `+0x2C` | `0x001DBB00` | `Close` |
| `+0x30` | `0x001DB7D0` | `Seek` |
| `+0x34` | `0x001DAAC0` | current position |
| `+0x38` | `0x001DB170` | `ReadData` |
| `+0x3C` | `0x001DABC0` | raw `WriteData` |
| `+0x40` | `0x001DAB30` | stream-to-stream write |
| `+0x44` | `0x001DAAF0` | `GetSize` |
| `+0x48` | `0x00114E50` | inherited null `GetBuffer` |

Clone создаёт новый default leaf и копирует только inherited base state. Он не
делит открытый file, буферы или platform-manager state.

## Обычный `ShellFile` path

`Open` не проверяет, открыт ли объект уже. Для exact read mode `1` сначала
выполняются resolver/cache calls; PCK success даёт физический path, logical
origin и logical size. Префиксы `host0:` и `atfile` влияют на backend kind.
Mode bits проверяются с приоритетом `1`, затем `2`, `4`, `8` и превращаются в
backend modes `1/2/3/3`.

После успешного `ShellFile` open leaf:

- сохраняет physical size и open flag;
- выделяет один 64-byte-aligned блок `0x8000`;
- делит его на два буфера по `0x4000`;
- делает seek к началу либо к концу при bit `8`;
- сохраняет диагностическое имя stream.

Обычные read/write используют два descriptor. Seek внутри готового окна только
меняет cursor; переход в готовое соседнее окно меняет active index; дальний seek
выравнивает file position вниз до 64 байт, читает текущий buffer и планирует
следующий. Read возвращает true без отдельной проверки open/EOF/полноты запроса.
Raw write расширяет physical size до конечной позиции, обслуживает границы
буферов и также не передаёт наружу ошибки нижнего I/O.

Непосредственно перед buffer state machine `Seek` имеет необычный контракт:

- `essStart (1)`: `logicalOrigin + offset`, верхняя граница проверяется только
  при ненулевом physical size;
- `essEnd (2)`: `physicalSize - offset`, без проверки границ;
- `essCurrent (4)`: отрицательный результат запрещён, верхняя граница снова
  отключена при physical size `0`;
- неизвестное значение оставляет позицию прежней и в обычном случае возвращает
  true; точная конечная позиция равна размеру и разрешена.

`GetCurrentPosition` у закрытого объекта записывает `0` и возвращает false; у
открытого вычитает logical origin. `GetSize` аналогично обнуляет output при
закрытом stream, иначе предпочитает ненулевой logical-size override физическому
размеру.

Stream-to-stream write всегда выделяет aligned temporary требуемого размера,
игнорирует результаты source read и destination write, освобождает блок и
возвращает true. Это отличается от PC leaf, который сначала пробует direct
`GetBuffer()`.

`Close` всегда возвращает true. Обычная ветка ждёт и закрывает `ShellFile`,
освобождает локальный aligned block; альтернативная освобождает owned strings.
После синхронизации global manager весь leaf state сбрасывается к constructor
defaults.

## Shared async-manager path

Под platform condition read-mode `Open` сначала пробует `0x001DBC10`. При
успехе `+0xF8` переключает Read/Seek на `0x001DAED0/0x001DB350`, размер буфера
становится `0x20000`, а память и асинхронные операции принадлежат глобальному
manager. В binary подтверждён отдельный класс `spPS2AsyncFileStreamManager`:

| Свойство | Значение |
|---|---:|
| class ID | `0x57746EF7` |
| base ID | `0x7EA51364` (`spAsyncFileStreamManager`) |
| factory | `0x001DA890` |
| allocation size | `0x37A0` |

Полный manager protocol не включён в leaf-реконструкцию: он станет следующим
зависимым классом. Поля `+0xFC..+0x108` потому пока имеют analytical, а не
original имена.

## Код и проверка

`Sparkplug/Analysis/PS2/SparkBaseAbi.h` хранит byte-exact layout и адреса.
`spPS2FileStreamState.h` воспроизводит только чистую, независимо тестируемую
часть normal seek/position/size contract. Автотест проверяет offsets, wraparound,
нулевой unknown-size, exact EOF, некорректный seek source и PCK-size override.

PS2 class намеренно не помещён в host `Code/` с фиктивным файловым API: до
реконструкции `ShellFile`, resolver и async manager такая реализация скрыла бы
неизвестное за новым интерфейсом и уже не соответствовала бы исходному движку.

## Открытые вопросы

1. Original header/TU path и spellings всех leaf members.
2. Точные типы `ShellFile`, mode/seek enums и ownership результата resolver.
3. Семантика `startSector +0x90`, path kind `+0x21` и source kind `+0xFC`.
4. Полный shared-buffer ownership/synchronization protocol manager.
5. Почему public `Open` разрешает повторный вызов и какие callers гарантируют
   предварительный `Close`.
6. Ошибки/short reads на реальном PS2 и роль нижних функций, результаты которых
   leaf игнорирует.
