# `spResourceFATSerializer.cpp`: helper таблицы ресурсов

Статус: основные PS2 layouts, index readers, lookup/cursor/clear и первичная
save-side индексация восстановлены; object materialization, payload save,
relationship fixup и PC container ABI ещё открыты. Имя самого C++ типа не
найдено, поэтому переносимый класс называется
`spResourceFATHelperForAnalysis`, а не выдаётся за оригинальную декларацию.

## Область доказательств

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

PC image содержит exact source path
`Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp`; PS2 — short
filename `spResourceFATSerializer.cpp`. Отдельной RTTI registration и
литерального имени helper type не найдено. Его vtable `0x0048EF60` наследует
RTTI getter `spBaseObject`, а factory создаётся только из manager-а.

## Owner, размер и layout

`spSerializerManager` владеет helper pointer по `+0x28`. PS2 constructor
`0x001801C0` получает allocation `0x64`; destructor entry — `0x0017FDF0`.

| Offset | Extent | Доказанная роль |
|---:|---:|---|
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | `4` | следующий save-side resource ID, default `1` |
| `+0x14` | `0x10` | map file ID -> file entry |
| `+0x24` | `0x10` | map resource ID -> resource entry |
| `+0x34` | `0x10` | map object pointer -> resource entry |
| `+0x44` | `0x10` | ordered file-entry sequence |
| `+0x54` | `0x10` | ordered resource sequence и cursor |

PS2 container layouts не переносятся byte-for-byte на host: reconstruction
использует `std::map`, `std::list` и `unique_ptr`, сохраняя ключи, порядок и
ownership, но не заявляя native ABI.

## Entry layouts

Resource entry имеет exact размер `0x24`:

| Offset | Поле/роль |
|---:|---|
| `+0x00` | vptr |
| `+0x04` | `m_uID` |
| `+0x08` | semantic file ID; original spelling пока не найден |
| `+0x0C` | owned `m_szName` |
| `+0x10` | `m_ClassID` |
| `+0x14` | `m_uOffset` |
| `+0x18` | `m_uSize` |
| `+0x1C` | one-shot payload-written byte |
| `+0x20` | runtime `spBaseObject*` |

File entry имеет exact размер `0x0C`: vptr, `m_uFileID +0x04`, owned
`m_szFilename +0x08`. Имена семи `m_*` stream-полей подтверждены assertion
strings в PS2 binary.

## Index load

PS2 `0x0017F570` читает:

```text
uint32 count
repeat count:
    uint32 m_uID
    string m_szName
    uint32 m_ClassID
    uint32 m_uOffset
    uint32 m_uSize
```

После чтения Class ID проверяется через global RTTI manager, затем entry
добавляется в ID map и ordered sequence. `fileID +0x08` и object `+0x20`
конструктор обнуляет. Важное уточнение: load constructor не инициализирует
байт `+0x1C` явно. Portable boundary задаёт ему `false`, чтобы не переносить
неопределённое чтение из native heap.

Loaded IDs не двигают writer counter `+0x10`; он остаётся равен `1`.
Malformed duplicate ID в native container может привести к overwrite/leak.
Portable loader вместо этого возвращает false и сохраняет память корректной.

## File-index compatibility path

PS2 `0x0017F460` читает count и для каждой записи `m_uFileID/m_szFilename`,
но не использует переданный helper, ничего не вставляет и не освобождает
выделенную запись. Это доказано только для данной PS2-сборки; PC counterpart
закрыт relocated/obfuscated переходом.

`ReadDiscardedFileIndexForAnalysis` потребляет exact grammar, но временные
данные уничтожает нормально. Наблюдаемая native утечка не считается частью
полезного file contract.

## Lookup, cursor и clear

| PS2 entry | Роль |
|---:|---|
| `0x0017F8A0` | установить cursor на первый ordered resource и вернуть entry |
| `0x0017F840` | продвинуть cursor и вернуть следующий entry |
| `0x0017F930` | lookup resource по object pointer |
| `0x0017F990` | lookup resource по ID |
| `0x0017FA10` | lookup file по file ID |
| `0x0017FA90` | удалить file entries и очистить file containers |
| `0x0017FB70` | удалить resource entries, очистить три resource containers и сбросить next ID в `1` |

Порядок iteration — порядок вставки, а не сортировка ID map.

## Save-side `IndexObject` `0x0017FC60`

Функция отклоняет уже индексированный object, выдаёт ID из `+0x10`, затем
увеличивает counter. Новая entry получает `fileID=0`, Class ID, zero
offset/size, `payloadWritten=false` и object pointer. Для `spNamedObject`
копируется непустое имя. Entry входит одновременно в ID map, object map и
ordered sequence.

Ненулевой file ID не появляется ни здесь, ни в обычном LoadIndex. Его
save/legacy происхождение остаётся отдельным вопросом и не моделируется.

## Что перенесено

`Sparkplug/Code/Sparkplug/spResourceFATSerializer.*` содержит безопасный
аналитический helper с:

- точной load grammar и RTTI validation;
- ID/object/file lookup;
- insertion-order cursor;
- раздельным clear и reset next ID;
- save-side object indexing и переносом `spNamedObject` name;
- cache-hit pass через `spResourceManager` для inline texture/mesh entries;
- безопасным потреблением PS2 compatibility file index.

Helper теперь создаётся и уничтожается вместе с переносимым
`spSerializerManager`, как в native lifetime.

## Проверки и открытые границы

`research/inspect_serializer_manager.py` выполняет 67 read-only проверок,
включая exact layouts/field strings, constructor stages, обе grammar,
различие инициализации `+0x1C` и reset ID. CTest проверяет round-trip индекса,
порядок cursor, lookup, clear, object indexing, duplicate rejection, unknown
RTTI ID и file-index consumption.

Открыты:

- original имя helper type, headers и имена методов;
- exact PC container/object ABI;
- transactional/rollback contract при частично повреждённом индексе;
- сохранение offsets/sizes и запись payload;
- cache-miss object materialization и рекурсивный relationship resolver;
- источник ненулевого file ID.
