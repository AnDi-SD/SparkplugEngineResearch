# spPCFileStream

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPCFileStream](../../../Sparkplug/Code/SparkBasePC/spPCFileStream.h).

```text
Z:\Sparkplug\Code\SparkBasePC\spPCFileStream.cpp
```

| Свойство | Значение |
| --- | ---: |
| class ID | `0x5EDF341C` |
| base class | `spFileStream / 0x5E0623EC` |
| native size | `0x20` |

Factory выделяет ровно `0x20` байт, вызывает `spFileStream` constructor
`0x006BE780`, ставит leaf vtable и обнуляет единственное новое поле:

| Offset | Размер | Роль |
| ---: | ---: | --- |
| `+0x00` | `0x1C` | `spFileStream` |
| `+0x1C` | `4` | Win32 `HANDLE`; `0` означает закрытый stream |

## Vtable и функции

| Slot | Target | Наблюдаемая роль |
| ---: | ---: | --- |
| `+0x00` | `0x006BE2A0` | deleting destructor |
| `+0x04` | `0x005B7A00` | inherited notification no-op |
| `+0x08` | `0x006BD5F0` | clone |
| `+0x0C` | `0x00413120` | inherited clone-copy |
| `+0x10` | `0x006BD570` | registration getter |
| `+0x1C` | `0x006BE7C0` | inherited `Open(name) -> Open(1,name)` |
| `+0x20` | `0x006BD710` | `Open(mode,name)` |
| `+0x24` | `0x006BD980` | `Close` |
| `+0x28` | `0x006BDA00` | `Seek` |
| `+0x2C` | `0x006BE100` | current position |
| `+0x30` | `0x006BDB60` | `ReadData` |
| `+0x34` | `0x006BDE30` | stream-to-stream write |
| `+0x38` | `0x006BDCF0` | raw `WriteData` |
| `+0x3C` | `0x006BDFC0` | `GetSize` |
| `+0x40` | `0x004A1BF0` | inherited null `GetBuffer` |

## `Open` и mode bits

| Первый установленный bit | access | disposition | attributes |
| ---: | --- | --- | --- |
| `1` | `GENERIC_READ` | `OPEN_EXISTING` | `FILE_ATTRIBUTE_READONLY` |
| `2` | `GENERIC_WRITE` | `CREATE_ALWAYS`; с bit `8` — `OPEN_ALWAYS` | `FILE_ATTRIBUTE_NORMAL` |

Во всех случаях share mode равен `FILE_SHARE_READ | FILE_SHARE_WRITE`. Bit `8`
после успешного открытия дополнительно вызывает `Seek(essEnd, 0)`. Неизвестный
mode оставляет нулевые access/disposition и закономерно приводит к ошибке.

При неудаче результат `CreateFileA`, то есть `INVALID_HANDLE_VALUE`, остаётся в
поле. Поэтому следующий `Open` всё ещё заблокирован, а `Close` пытается закрыть
невалидный ненулевой handle, игнорирует результат `CloseHandle`, обнуляет поле и
возвращает true. `Close` уже закрытого stream возвращает false.

### Resource-manager ветка

Имена manager, структуры семи output arguments и смысл остальных outputs пока
не доказаны. Реконструкция реализует подтверждённый прямой Win32 backend;
resource lookup будет подключён после отдельного разбора его настоящего класса.

## I/O semantics

`Seek` переводит значения `1/2/4` в `FILE_BEGIN/FILE_END/FILE_CURRENT`.
Start-offset предварительно увеличивается на `spStream +0x14`. Как и оригинал,
метод считает любой результат `SetFilePointer == 0xFFFFFFFF` ошибкой, не
проверяя `GetLastError` для допустимой позиции `0xFFFFFFFF`.

`GetCurrentPosition` вызывает `SetFilePointer(0, FILE_CURRENT)` и вычитает
logical origin. `GetSize` возвращает low word `GetFileSize(handle, nullptr)` без
поправки на origin и так же без правильного различения допустимого
`0xFFFFFFFF`. На ошибочных ветках output caller-а не обнуляется.

`ReadData` возвращает true после успешного `ReadFile`, только если фактически
прочитан хотя бы один байт. Поэтому:

- короткое ненулевое чтение у EOF считается успехом;
- нулевой запрос и обычное чтение уже на EOF считаются failure;
- полное совпадение `bytesRead == requested` не требуется.

Stream-to-stream overload сначала спрашивает `source->GetBuffer()`:

1. при null выделяет временные `byteCount` байт, вызывает `ReadData`, **игнорирует
   его результат**, пишет temporary и освобождает его;
2. при ненулевом buffer вызывает `source->Seek(essCurrent, byteCount)`, игнорирует
   результат, но передаёт в `WriteFile` начало buffer, а не `buffer + position`;
3. возвращает только результат destination raw write.

- factory/inheritance/class ID и закрытый clone;
- create/truncate, read, append, seek, position, size и close;
- отказ повторного `Open`;
- short EOF read, zero-byte read/write и invalid-handle state;
- необычную direct-buffer ветку stream-to-stream write.

Host x64 object намеренно не выдаётся за 32-bit ABI-exact: точная структура
`0x20` хранится отдельно в `Analysis/PC/SparkBaseAbi.h`.

## Открытые вопросы

1. Original header path и spelling единственного поля.
2. Original enum type/names для mode и seek source.
3. Класс и точный семипараметрический контракт resource resolver.
4. Является ли logical origin смещением PCK-entry, filesystem mount либо иной
   разновидностью виртуального файла; полное поведение resolver неизвестно.
5. Полная связь Win32 errors с `spStreamError` reason/operation code.
