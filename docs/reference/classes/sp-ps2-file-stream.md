# spPS2FileStream

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

| Свойство | Значение |
| --- | ---: |
| class ID | `0x12FDDAB3` |
| base class | `spFileStream / 0x5E0623EC` |
| native size | `0x114` |

Factory прямо выделяет `0x114` байт. Original `.cpp`/header path для этого
класса не найден; `SparkBasePS2` является пока только обоснованной module
границей, а не доказанным путём исходника.

## Layout

Constructor сначала вызывает `spFileStream`, затем обнуляет все leaf-поля,
`ShellFile` размером `0x98` и два descriptor по `0x10`; default mode равен `1`.

| Offset | Размер | Наблюдаемая роль |
| ---: | ---: | --- |
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
| ---: | ---: | --- |
| `+0x08` | `0x001DC460` | deleting destructor |
| `+0x0C` | `0x00100810` | inherited notification no-op |
| `+0x10` | `0x001DC580` | clone |
| `+0x14` | `0x00105DC0` | inherited clone-copy |
| `+0x18` | `0x001DA930` | registration getter |
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

После успешного `ShellFile` open leaf:

- сохраняет physical size и open flag;
- выделяет один 64-byte-aligned блок `0x8000`;
- делит его на два буфера по `0x4000`;
- делает seek к началу либо к концу при bit `8`;
- сохраняет диагностическое имя stream.

Непосредственно перед buffer state machine `Seek` имеет необычный контракт:

`GetCurrentPosition` у закрытого объекта записывает `0` и возвращает false; у
открытого вычитает logical origin. `GetSize` аналогично обнуляет output при
закрытом stream, иначе предпочитает ненулевой logical-size override физическому
размеру.

`Close` всегда возвращает true. Обычная ветка ждёт и закрывает `ShellFile`,
освобождает локальный aligned block; альтернативная освобождает owned strings.
После синхронизации global manager весь leaf state сбрасывается к constructor
defaults.

## Shared async-manager path

| Свойство | Значение |
| --- | ---: |
| class ID | `0x57746EF7` |
| base ID | `0x7EA51364` (`spAsyncFileStreamManager`) |
| allocation size | `0x37A0` |

Полный manager protocol не включён в leaf-реконструкцию: он станет следующим
зависимым классом. Поля `+0xFC..+0x108` потому пока имеют analytical, а не
original имена.

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
