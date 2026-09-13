# spSerializerManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spNode](../../../Sparkplug/Code/Sparkplug/spNode.h), [spPCKManager](../../../Sparkplug/Code/SparkBase/spPCKManager.h), [spSerializer](../../../Sparkplug/Code/Sparkplug/spSerializer.h), [spSerializerManager](../../../Sparkplug/Code/Sparkplug/spSerializerManager.h), [spStream](../../../Sparkplug/Code/SparkBase/spStream.h).

## Область доказательств

В PS2 ELF сохранено имя `spSerializerManager.cpp`.

## Identity, lifetime и clone

| Свойство | Вывод |
| --- | --- |
| Class ID | подтверждено независимо |
| registered base ID | прямой `spBaseObject` |
| singleton | один process-global instance |

PS2 constructor даёт наиболее прозрачную опору: после `spBaseObject` он
ставит vtable, пишет `0`, `1`, `2` по `+0x10/+0x14/+0x18`, инициализирует
список по `+0x1C`, создаёт owned helper размером `0x64` через `0x001801C0`,
сохраняет его по `+0x28` и публикует singleton. Точное имя типа helper-а пока
не доказано, поэтому код не называет его `spResourceFAT` только по поведению.

## Layout manager-а

Обе платформы имеют общий размер `0x2C` и одинаковые offsets логических полей:

| Offset | Size | Смысл | Уверенность |
| ---: | ---: | --- | --- |
| `+0x00` | `0x10` | `spBaseObject` | подтверждено |
| `+0x10` | `4` | mask платформ текущего файла | подтверждено lookup/load paths |
| `+0x14` | `4` | направление: load `1`, save `2` | подтверждено registration lookup |
| `+0x18` | `4` | политика записи полей; default `2`, значения `0/2` включают optional/default fields | роль consumer-ов подтверждена, original enum неизвестен |
| `+0x1C` | `0x0C` | registration list | подтверждено |
| `+0x28` | `4` | owned FAT helper pointer | поведение подтверждено, type name остаётся открыт |

Содержимое 12-байтного списка platform-specific. PC использует старое MSVC
представление `allocator word / allocated sentinel / size`. PS2 хранит
`size / inline sentinel next / inline sentinel previous`. Эти ABI не
объединяются в одну native-структуру; portable код использует `std::list`.

Поле `+0x18` имеет внешних consumer-ов, поэтому прежняя пометка «нет чтений»
исправлена. В PS2 writer-функциях `spDXMeshDataSerializerWrite`
`0x00161DA0`, `spMeshDataSerializerWrite` `0x00162510` и
`spPS2MeshDataSerializerWrite` `0x00162E10` загружается
`spSerializerManager::singleton + 0x18`. Значения `0` и `2` проходят одну
ветвь и разрешают запись optional/default-полей; остальные значения её
подавляют. Ещё пять аналогичных чтений находятся около `0x001758F0`,
`0x00175974`, `0x00176710`, `0x00176794` и `0x0017874C`. Семантическая роль
поля теперь доказана, но исходное имя enum и смысл остальных значений — нет.

## Registration node и dispatch

Native registration node имеет `0x18` байт на обеих платформах:

| Offset | Значение |
| ---: | --- |
| `+0x00/+0x04` | next/previous links |
| `+0x08` | target object Class ID |
| `+0x0C` | platform mask |
| `+0x10` | operation mask |
| `+0x14` | `spSerializer*`; ownership сгруппировано по уникальному pointer |

PS2 `0x00182070` и PC counterpart `0x00422D90` добавляют запись в конец.
В зафиксированных binaries найдено 67 прямых вызовов registration на PS2 и
78 на PC. Разница реальна для этих двух сборок и запрещает считать список
serializer-ов платформенно одинаковым.
Все 67 PS2 tuples декодируются статически. Распределение `(platform/operation)`:
`01/01=3`, `01/02=3`, `06/01=3`, `06/02=3`, `08/01=3`, `08/02=3`,
`08/03=1`, `FF/01=1`, `FF/02=1`, `FF/03=46`. Значение `3` означает обе
операции по bitmask; `0xFF` — все platform bits. Имя platform bit `0x04` не
выдумывается, хотя он входит в mask `0x06`.
Повторный target ID разрешён: как минимум один PS2 initializer регистрирует
один target `0x33C34CF0` парами `(platform=6, operation=2)`, `(6,1)` и
`(8,2)`. Lookup идёт в порядке вставки и возвращает первый элемент, для
которого одновременно выполняются:

```text
node.targetClassID == requestedClassID
&& (manager.platformMask & node.platformMask) != 0
&& (manager.operationMask & node.operationMask) != 0
```

## Точная граница заголовка

Manager читает через stream ровно семь `uint32`, то есть `0x1C` байт:

| File offset | Поле |
| ---: | --- |
| `0x00` | signature `FFPS` / `0x53504646` |
| `0x04` | version `0x26` |
| `0x08` | export/session tag, смысл открыт |
| `0x0C` | declared file size |
| `0x10` | platform mask |
| `0x14` | data offset |
| `0x18` | data size |

`ObjectCount` по file offset `0x1C` не входит в этот header. Его следующим
читает FAT `LoadIndex`; поэтому полный фиксированный префикс обычного SMO равен
`0x20`, но граница `spSerializerManager` заканчивается на `0x1C`.

Validator расположен по PC `0x00422260` и PS2 `0x00181D80`. Порядок проверок:

1. signature;
2. version;
3. получение фактического stream size;
4. platform acceptance: PC `(mask & 0x03) != 0`, PS2 `(mask & 0x09) != 0`;
5. строгая граница `dataOffset < actualStreamSize`.

## Load paths

Scene loader: PC `0x00422550`, PS2 `0x00182150..0x00182624`.
Общий resource variant на PS2: `0x00182640..0x001829A8`.

Подтверждённый общий префикс обоих путей:

```text
очистить временное FAT-состояние
read(header, 0x1C)
ValidateFileHeader(header, stream.GetSize())
manager.platformMask = header.platformMask
manager.operationMask = 1
FAT.LoadIndex(stream)
FAT.LoadFileIndex(stream)
require stream.GetCurrentPosition() == header.dataOffset
добавить dataOffset к logical stream origin
```

После этого scene loader берёт первую FAT entry, читает Class ID по `entry+0x10`,
находит serializer, создаёт объект через основной serializer-interface slot и
десериализует через secondary interface. Результат обязан быть `spNode`
(`0x695C0F65`) и записывается в `entry+0x20`.

## Платформенный serializer hook в общем load-path

| Класс | Class ID | Direct base | Registration/init | Доказанный размер |
| --- | ---: | --- | --- | ---: |
| `spSerializerHook` | `0x18092F8D` | `spBaseObject` | PC `0x00763D20/0x006D5280`; PS2 `0x004A9D90/0x00483010` | PS2 `0x10` |
| `spDXSerializerHook` | `0x0D832A30` | `spSerializerHook` | PC `0x007631B0/0x006D4D10` | observed prefix `0x1C` |
| `spPS2SerializerHook` | `0x1C0E0F30` | `spSerializerHook` | PS2 `0x004B83B0/0x00485730` | `0x10` |

PS2 base vtable `0x0048EF30` оставляет slot `+0x24` pure/null; subclass vtable
`0x00491BB0` ставит туда `0x00208E70`. Реализация не обращается к переданным
FAT и stream: она гарантирует наличие singleton `spSerializerManager`, читает
его platform field `+0x10` и имеет пустые сходящиеся ветви вокруг значения
`8`. Caller не использует возвращаемое значение. Это может быть урезанный
platform hook или остаток условной инициализации, но точное имя и исходная
сигнатура метода пока не доказаны. Поэтому код manager-а не получает
придуманного callback API, а ABI-факт сохранён отдельно.

## Предварительно закрытая форма FAT helper-а

PS2 constructor `0x001801C0` создаёт direct-`spBaseObject` объект размером
`0x64` со следующим доказанным каркасом:

| Offset | Extent | Наблюдаемая роль |
| ---: | ---: | --- |
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | `4` | next resource ID, default `1` |
| `+0x14` | `0x10` | file-entry map keyed by file ID |
| `+0x24` | `0x10` | resource-entry map keyed by resource ID |
| `+0x34` | `0x10` | resource-entry map keyed by object pointer |
| `+0x44` | `0x10` | ordered file-entry sequence |
| `+0x54` | `0x10` | ordered resource-entry sequence/current cursor |

Resource entry имеет exact PS2 extent `0x24`: `m_uID +0x04`, semantic
`fileID +0x08`, owned `m_szName +0x0C`, `m_ClassID +0x10`, `m_uOffset +0x14`,
`m_uSize +0x18`, one-shot save guard `+0x1C` и object pointer `+0x20`.
Имена пяти stream-полей подтверждены буквально сохранившимися diagnostics;
имя `fileID` по `+0x08` пока аналитическое, хотя его роль подтверждена lookup-ом.
`LoadIndex` `0x0017F570` читает из stream только
`m_uID/m_szName/m_ClassID/m_uOffset/m_uSize`; поля `+0x08/+0x20`
конструктор обнуляет, но byte `+0x1C` явно не инициализирует. Portable helper
задаёт ему безопасное значение `false`. File entry равен `0x0C` и содержит подтверждённые
`m_uFileID +0x04` и `m_szFilename +0x08`.

Vtable pointer `0x0048EF60` заменяет destructor на `0x0017FDF0`, но RTTI slot
оставляет унаследованный `spBaseObject` getter `0x00102830`. Отдельной RTTI
registration helper-а не найдено. Поэтому его identity принципиально нельзя
получить тем же способом, что identity публичных `sp...` классов; exact TU и
поведение здесь сильнее, чем RTTI, но всё ещё не доказывают spelling type-а.

## Что восстановлено в коде

- RTTI, factory, singleton lifetime и blank-clone semantics;
- owned ordered registry и оба lookup-варианта;
- platform/operation dispatch context;
- точная 0x1C header structure и порядок validator-а;
- безопасный stream front-end до FAT boundary;
- owned FAT helper, index grammar/RTTI validation, lookup/cursor/clear и
  save-side object indexing.

Byte-exact PC/PS2 layouts и addresses отделены в соответствующие
`Analysis/*/SparkplugAbi.h`. Host-layout не выдаётся за 32-bit native ABI.

## Границы описания

- исходное имя и полный constructor owned helper-а `+0x28`; PC extent58 и container consumers уже подтверждены;
- original enum/name политики `+0x18` и смысл значений, отличных от `0/2`;
- original enum/type names platform, operation и header result;
- save pipeline и использование `declaredFileSize/dataSize`;
- оставшееся PC unknown44 и save-side происхождение resource `fileID`;
- cache-miss materialization, ownership и rollback;
- рекурсивный relationship resolver и момент удаления временного FAT state;
- original имя/signature hook slot (PS2 `+0x24`, PC `+0x1C`) и полный PC mesh-containing body.

Связанная end-to-end карта находится в
[`runtime-resource-pipeline.md`](../../engine/resources/pipeline.md).
