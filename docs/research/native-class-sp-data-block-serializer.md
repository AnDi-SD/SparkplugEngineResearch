# `spDataBlockSerializer`: универсальный codec полей SMO

Статус: wire grammar, read/skip и прямой writer восстановлены и проверены на
PC/PS2. Вложенный `WriteBegin`/`WriteEnd` и точные имена вспомогательных типов
ещё требуют отдельного прохода.

## Доказательства

| Платформа | Образ | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

PC сохраняет точный путь
`Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp`, PS2 — имя того же
файла. Отдельного RTTI-класса нет: это встраиваемый helper сериализаторов.

## Layout

Обе платформы используют состояние размером `0x28`:

| Offset | Size | Значение |
|---:|---:|---|
| `+0x00` | `0x0C` | platform-specific sequence/list state для stack-а headers |
| `+0x0C` | `0x10` | текущий header |
| `+0x1C` | `0x04` | writer state, точное имя пока неизвестно |
| `+0x20` | `0x04` | stream writer-а |
| `+0x24` | `0x04` | сохранённый size code для reserved header |

Header одинаков на PC и PS2:

| Offset | Поле |
|---:|---|
| `+0x00` | field ID; `0xFFFFFFFF` означает terminator |
| `+0x04` | payload size |
| `+0x08` | позиция начала header |
| `+0x0C` | позиция начала payload |

Оригинальное имя header-типа не найдено, поэтому переносимый код использует
`spDataBlockHeaderForAnalysis`.

## Wire grammar

Первый byte делится как `SSSIIIII`: старшие три бита — size code, младшие пять
— field ID. Если ID равен `0x1F`, следующий byte содержит полный ID.

| Code | Payload size |
|---:|---:|
| `0` | terminator, ID в runtime заменяется на `0xFFFFFFFF` |
| `1` | `1` |
| `2` | `2` |
| `3` | `4` |
| `4` | `8` |
| `5` | следующий `u8` |
| `6` | следующий little-endian `u16` |
| `7` | следующий little-endian `u32` |

Размеры `1/2/4/8` получают fixed form. Все прочие размеры выбирают минимальный
из `u8/u16/u32`. Поэтому payload размера `0` кодируется как size code `5` плюс
нулевой byte и **не** является terminator. Конец секции writer выводит отдельным
нулевым byte.

## Native методы

| Операция | PC | PS2 |
|---|---:|---:|
| выбор size code | `0x00472730` | `0x0017E740` |
| terminal byte | `0x00472B00` | `0x0017E7C0` |
| `SkipData` | `0x00472AC0` | `0x0017E830` |
| `ReadHeader` | `0x004728F0` | `0x0017E890` |
| `WriteHeader` | protected fragment | `0x0017EAA0` |
| direct field + payload | `0x00472B40` | тот же общий writer graph |

`ReadHeader` сначала сохраняет текущую позицию, читает compact header и size,
затем сохраняет позицию payload. `SkipData` не вычитывает bytes: он делает
`Seek(essStart, dataStreamPosition + payloadSize)`.

У PC внутри `ReadHeader` есть один SecuROM transition, но вся последующая
декодирующая часть и её границы открыты. PS2 даёт независимое полностью
читаемое подтверждение тех же masks, offsets и size cases.

## Реализованный срез

`Sparkplug/Code/Sparkplug/spDataBlockSerializer.*` содержит:

- один универсальный reader и `SkipData`, используемые будущими native
  serializers и platform hooks;
- direct-field writer с нативным выбором size code;
- отдельный terminator writer;
- безопасный отказ при переполнении позиции, ID больше `0xFF`, null payload
  ненулевого размера и ошибках stream-а.

Безопасные отказы — явное отличие от мест, где оригинал полагался на валидный
вход и 32-битную арифметику. Wire bytes для допустимого ввода совпадают.

`research/inspect_data_block_serializer.py` фиксирует 36 PC/PS2 проверок,
включая hashes полных тел, source path, masks, layout stores и абсолютный seek.
CTest проверяет round-trip фиксированного/переменного/extended/empty поля и
terminator.

## Открыто

- оригинальное имя header-типа и public header path;
- семантика sequence state `+0x00..+0x0B`;
- полные контракты nested `WriteBegin`/`WriteEnd`, включая выбор заранее
  зарезервированной ширины размера и patch-back;
- native error propagation/частично изменённое состояние на повреждённом
  потоке;
- перенос всех concrete serializer payload methods на этот codec.
