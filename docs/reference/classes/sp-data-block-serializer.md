# spDataBlockSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDataBlockSerializer](../../../Sparkplug/Code/Sparkplug/spDataBlockSerializer.h).

PC сохраняет точный путь
`Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp`, PS2 — имя того же
файла. Отдельного RTTI-класса нет: это встраиваемый helper сериализаторов.

## Layout

Обе платформы используют состояние размером `0x28`:

| Offset | Size | Значение |
| ---: | ---: | --- |
| `+0x00` | `0x0C` | platform-specific sequence/list state для stack-а headers |
| `+0x0C` | `0x10` | текущий header |
| `+0x1C` | `0x04` | PC BeginObject сохраняет object pointer; имя member неизвестно |
| `+0x20` | `0x04` | stream writer-а |
| `+0x24` | `0x04` | сохранённый size code для reserved header |

Header одинаков на PC и PS2:

| Offset | Поле |
| ---: | --- |
| `+0x00` | field ID; `0xFFFFFFFF` означает terminator |
| `+0x04` | payload size |
| `+0x08` | позиция начала header |
| `+0x0C` | позиция начала payload |

## Wire grammar

Первый byte делится как `SSSIIIII`: старшие три бита — size code, младшие пять
— field ID. Если ID равен `0x1F`, следующий byte содержит полный ID.

| Code | Payload size |
| ---: | ---: |
| `0` | terminator, ID в runtime заменяется на `0xFFFFFFFF` |
| `1` | `1` |
| `2` | `2` |
| `3` | `4` |
| `4` | `8` |
| `5` | следующий `u8` |
| `6` | следующий little-endian `u16` |
| `7` | следующий little-endian `u32` |

Размеры `1/2/4/8` получают fixed form. Все прочие размеры выбирают минимальный
из `u8/u16/u32`. По грамматике payload размера `0` можно представить как
size code `5` плюс нулевой byte; это **не** terminator. Но original PC
`WriteHeader` для size0 возвращает success вообще без записи. После
`WriteBegin` это оставляет placeholder all-ones. Прямой portable helper
выдаёт корректное пустое поле как явную host policy, не копирует native bug.
Original PC также ошибочно считает ID31 inline, хотя reader требует escape.
Конец секции writer выводит отдельным нулевым byte.

## Native методы

| Операция | PC | PS2 |
| --- | ---: | ---: |
| выбор size code | `0x00472730` | `0x0017E740` |
| terminal byte | `0x00472B00` | `0x0017E7C0` |
| `SkipData` | `0x00472AC0` | `0x0017E830` |
| `ReadHeader` | `0x004728F0` | `0x0017E890` |
| `WriteHeader` | `4727B0 -> 4F5AD0` | `0x0017EAA0` |
| direct field + payload | `0x00472B40` | тот же общий writer graph |

`ReadHeader` сначала сохраняет текущую позицию, читает compact header и size,
затем сохраняет позицию payload. `SkipData` не вычитывает bytes: он делает
`Seek(essStart, dataStreamPosition + payloadSize)`.

У PC внутри `ReadHeader` есть один SecuROM transition, но вся последующая
декодирующая часть и её границы открыты. PS2 даёт независимое полностью
читаемое подтверждение тех же masks, offsets и size cases.

`Sparkplug/Code/Sparkplug/spDataBlockSerializer.*` содержит:

- один универсальный reader и `SkipData`, используемые будущими native
  serializers и platform hooks;
- direct-field writer с нативным выбором size code;
- отдельный terminator writer;
- безопасный отказ при переполнении позиции, ID больше `0xFF`, null payload
  ненулевого размера и ошибках stream-а.

Добавлен PC nested writer: reservation UInt8/16/32, stack, backpatch и
восстановление позиции. Безопасные отказы — явное отличие от мест, где
оригинал полагался на валидный вход. Native имеет ОДИН width на весь stack:
mixed-width nesting повреждает outer header. Portable API отвергает mixed
nesting, ID31 и fixed reservation, пустое/слишком большое поле при End.
Прямой portable writer корректно кодирует ID31/size0 как отдельно указанную
host policy; заявление о побайтном совпадении native на них неверно.
