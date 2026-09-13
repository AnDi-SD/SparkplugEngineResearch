# spResourceFATSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spResourceFATSerializer](../../../Sparkplug/Code/Sparkplug/spResourceFATSerializer.h).

## Область доказательств

PC image содержит exact source path
`Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp`; PS2 — short
filename `spResourceFATSerializer.cpp`. Отдельной RTTI registration и
литерального имени helper type не найдено. Его vtable `0x0048EF60` наследует
RTTI getter `spBaseObject`, а factory создаётся только из manager-а.

## Owner, размер и layout

`spSerializerManager` владеет helper pointer по `+0x28`. PS2 constructor
`0x001801C0` получает allocation `0x64`; destructor entry — `0x0017FDF0`.

| Offset | Extent | Доказанная роль |
| ---: | ---: | --- |
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | `4` | следующий save-side resource ID, default `1` |
| `+0x14` | `0x10` | map file ID -> file entry |
| `+0x24` | `0x10` | map resource ID -> resource entry |
| `+0x34` | `0x10` | map object pointer -> resource entry |
| `+0x44` | `0x10` | ordered file-entry sequence |
| `+0x54` | `0x10` | ordered resource sequence и cursor |

## Entry layouts

Resource entry имеет exact размер `0x24`:

| Offset | Поле/роль |
| ---: | --- |
| `+0x00` | vptr |
| `+0x04` | `m_uID` |
| `+0x08` | semantic file ID; original spelling пока не найден |
| `+0x0C` | owned `m_szName` |
| `+0x10` | `m_ClassID` |
| `+0x14` | `m_uOffset` |
| `+0x18` | `m_uSize` |
| `+0x1C` | one-shot payload-written byte |
| `+0x20` | runtime `spBaseObject*` |

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

Loaded IDs не двигают writer counter `+0x10`; он остаётся равен `1`.
Malformed duplicate ID в native container может привести к overwrite/leak.
Portable loader вместо этого возвращает false и сохраняет память корректной.

## File-index compatibility path

`ReadDiscardedFileIndexForAnalysis` потребляет exact grammar, но временные
данные уничтожает нормально. Наблюдаемая native утечка не считается частью
полезного file contract.

## Lookup, cursor и clear

| PS2 entry | Роль |
| ---: | --- |
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

## Границы описания

Открыты:

- original имя helper type, headers и имена методов;
- полный PC constructor и unknown44; extent58/maps14,20,2C/list48/cursor54 подтверждены;
- transactional/rollback contract при частично повреждённом индексе;
- сохранение offsets/sizes и запись payload;
- cache-miss object materialization и рекурсивный relationship resolver;
- источник ненулевого file ID.
