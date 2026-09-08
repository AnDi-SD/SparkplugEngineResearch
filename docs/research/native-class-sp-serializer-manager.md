# `spSerializerManager`: реестр сериализаторов и вход в FAT

Статус на8 сентября: manager, основной FAT index и bounded whole-file
object materialization перенесены. Полный startup и все failure/fixup branches
остаются открытыми. Класс выбран не по
простоте, а как ближайший узел, соединяющий
уже разобранные `spStream`, `spPCKManager`, `spSerializer`, `spNode` и
конкретные serializers.

## Область доказательств

Дополнение 6 сентября: [PC loader checkpoint](native-pc-smo-san-loader.md)
исполнил общий `422B50` целиком на `bbush.san`, включая actual factory,
serializer reader, shared-name registry, повторную загрузку и FAT clear.
Начальный manager/FAT/RTTI state пока явно задан fixture-ом; полный startup,
SMO mesh hook, все failure/fixup branches и writer не закрыты.

Дополнение8 сентября: [whole-file profile CP101](native-pc-whole-file-profile.md)
расширил реальные SAN/SMO; [CP114](native-pc-whole-uv-scene.md) проверил целый
SMO с DX mesh hook, двумя textures и обновлением UV после загрузки. Managers,
RTTI и COM остаются явно заданными. [Whole SAN host roundtrip](native-pc-san-file-roundtrip.md)
собирает файл из проверенных частей; original outer save entry пока не найден.

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

В PC executable буквально присутствует исходный путь
`Z:\Sparkplug\Code\Sparkplug\spSerializerManager.cpp`. В PS2 ELF сохранено
имя `spSerializerManager.cpp`. Заголовок реконструкции inferred: его исходное
имя и декларации в поставленных файлах не найдены.

## Identity, lifetime и clone

| Свойство | PC | PS2 | Вывод |
|---|---:|---:|---|
| Class ID | `0xE422E9EB` | `0xE422E9EB` | подтверждено независимо |
| registered base ID | `0x415352A1` | `0x415352A1` | прямой `spBaseObject` |
| registration | `0x0075DDF0` | `0x004A9E50` | static RTTI record |
| singleton | `0x0075DDE8` | `0x0049F9DC` | один process-global instance |
| constructor | `0x00422E00` protected entry | `0x001830E0` | размер `0x2C`, состояние `0/1/2` |
| destructor | `0x00422EB0` | `0x00182FE0` | удаляет FAT helper и serializers, очищает singleton |
| clone | `0x00422FA0` | `0x00183150` | создаёт чистый manager, runtime registry не копирует |
| factory | `0x00422F40` | `0x00183260` | concrete class, allocation `0x2C` |

PS2 constructor даёт наиболее прозрачную опору: после `spBaseObject` он
ставит vtable, пишет `0`, `1`, `2` по `+0x10/+0x14/+0x18`, инициализирует
список по `+0x1C`, создаёт owned helper размером `0x64` через `0x001801C0`,
сохраняет его по `+0x28` и публикует singleton. Точное имя типа helper-а пока
не доказано, поэтому код не называет его `spResourceFAT` только по поведению.

Clone не является исключением из singleton-правила: PS2 `0x00183150`
конструирует чистый manager и тем самым переписывает global pointer на clone;
его destructor затем безусловно обнуляет pointer, даже если исходный manager
ещё жив. Переносимый код и тест сохраняют это странное, но наблюдаемое
поведение вместо введения более удобной несуществующей guard-семантики.

## Layout manager-а

Обе платформы имеют общий размер `0x2C` и одинаковые offsets логических полей:

| Offset | Size | Смысл | Уверенность |
|---:|---:|---|---|
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
|---:|---|
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

PS2 entry `0x001829E0` получает ID из RTTI переданного объекта, а
`0x00182AC0` принимает Class ID непосредственно. Поэтому registry — это не
словарь `Class ID -> serializer`: platform/direction являются частью ключа,
а порядок регистрации наблюдаемо значим. При teardown native manager удаляет
первый serializer pointer и затем стирает все registration nodes с тем же
pointer. Следовательно, несколько masks могут ссылаться на один экземпляр;
в переносимом срезе это выражено `shared_ptr`, а не независимым `unique_ptr` в
каждой записи.

Portable метод `RegisterForAnalysis` дополнительно отклоняет null pointer. Это
защитная граница исследовательской реализации, а не утверждение, что native
функция делала ту же проверку.

## Точная граница заголовка

Manager читает через stream ровно семь `uint32`, то есть `0x1C` байт:

| File offset | Поле |
|---:|---|
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

Именно эта функция не сравнивает `declaredFileSize` и `dataSize` с реальным
размером. Такие проверки нельзя приписывать manager-у без отдельного consumer
evidence.

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

Последующий вызов `0x004671F0` PC / `0x001810F0` PS2 ранее был помечен как
неизвестный binding. PS2 тело уточняет его роль: оно проверяет object на
`spNamedObject` (`0x44DE07FD`) и вызывает `sub_00105E80`, то есть переносит
`entry.m_szName` из `entry+0x0C` в runtime object. ID/object maps принадлежат
FAT helper-у и обновляются другими функциями; этот вызов не является binding.

## Платформенный serializer hook в общем load-path

Generic loader PS2 `0x00182640` после проверки FAT/data-offset создаёт объект
через `0x00209010`, вызывает его последний virtual slot `+0x24` с аргументами
`(manager.m_pFAT, source stream)` и сразу уничтожает. RTTI закрывает ранее
безымянную зависимость:

| Класс | Class ID | Direct base | Registration/init | Доказанный размер |
|---|---:|---|---|---:|
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

PC counterpart имеет точное имя `spDXSerializerHook`; защищён только factory
`0x004AA430`. Vtable `0x006EF3C8`, destructor/clone и slot `0x004AAB80`
открыты. Slot вызывает `0x004AA870`: дважды обходит FAT, извлекает field `1`
mesh metadata через `spDataBlockSerializer`, группирует одинаковый FVF при
сумме vertices `< 0x4E20`, создаёт общий DX buffer object и во втором проходе
материализует entries. Portable reconstruction уже переносит parser/batch
plan, но оставляет GPU-container/load tail явно незавершённым.

## Предварительно закрытая форма FAT helper-а

Exact PC translation unit для соседнего кода —
`Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp`. Литеральное имя
самого C++ type в бинарниках пока не найдено, поэтому `spResourceFATSerializer`
не переносится в production namespace только на основании имени файла.

PS2 constructor `0x001801C0` создаёт direct-`spBaseObject` объект размером
`0x64` со следующим доказанным каркасом:

| Offset | Extent | Наблюдаемая роль |
|---:|---:|---|
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

Save-side index helper `0x0017FC60` также создаёт resource entry размером
`0x24`, выдаёт последовательный `m_uID` из helper `+0x10`, сохраняет
`m_ClassID` и object pointer, добавляет запись в ID/object maps и ordered
sequence — но тоже явно пишет `fileID = 0`. Следовательно, источник
ненулевого file ID не находится ни в обычном load index, ни в основном
object-index path этой PS2-сборки.

`LoadFileIndex` `0x0017F460` сначала читает count, а затем для каждой записи
выделяет `0x0C` байт и читает `m_uFileID/m_szFilename`. Однако PS2 тело не
обращается к переданному helper-у, не вставляет запись ни в map `+0x14`, ни в
sequence `+0x44` и не освобождает её на этом пути. Это наблюдаемое поведение
данной сборки, похожее на оставленный compatibility/stub path, а не основание
повторять утечку в переносимом коде. PC entry `0x00465CD0` закрыт переходом в
relocated/obfuscated область, но теперь `465CD0 ->13BCA90` исполнен независимо
и подтвердил то же consume-without-store поведение. Старое PS2-only ограничение
этого утверждения снято; native утечка в portable helper не переносится.

Полный разбор вынесен в отдельную карточку
[`native-class-sp-resource-fat-serializer.md`](native-class-sp-resource-fat-serializer.md).
Переносимый основной срез уже существует, но PC контейнерный ABI отличается,
save-side происхождение resource `fileID` не замкнуто, а original public names
не доказаны.

Vtable pointer `0x0048EF60` заменяет destructor на `0x0017FDF0`, но RTTI slot
оставляет унаследованный `spBaseObject` getter `0x00102830`. Отдельной RTTI
registration helper-а не найдено. Поэтому его identity принципиально нельзя
получить тем же способом, что identity публичных `sp...` классов; exact TU и
поведение здесь сильнее, чем RTTI, но всё ещё не доказывают spelling type-а.

PS2 `0x00182B90` уже показывает цикл по FAT entries: готовые entries
пропускаются, некоторые объекты могут разрешаться из глобального resource
manager, для остальных stream переводится к entry payload, serializer создаёт
и читает объект, после чего entry связывается с результатом. Промежуточный
объект generic path теперь идентифицирован как `spPS2SerializerHook`; открытыми
остаются точные имена FAT/hook методов, rollback и отдельный
relationship-fixup pass. Они не входят в переносимый код manager-а.

У цикла есть ветка `fileID != 0`, вызывающая file-ID lookup, но результат
lookup не записывается в `entry.object`. Сразу загруженные `LoadIndex` entries
имеют `fileID == 0`, а `LoadFileIndex` не наполняет file map. Поэтому
семантика этой ветки считается save/legacy boundary, а не доказанной загрузкой
внешнего файла.

## Что восстановлено в коде

В `Sparkplug/Code/Sparkplug/spSerializerManager.*` перенесены только части с
закрытым contract:

- RTTI, factory, singleton lifetime и blank-clone semantics;
- owned ordered registry и оба lookup-варианта;
- platform/operation dispatch context;
- точная 0x1C header structure и порядок validator-а;
- безопасный stream front-end до FAT boundary;
- owned FAT helper, index grammar/RTTI validation, lookup/cursor/clear и
  save-side object indexing.

Byte-exact PC/PS2 layouts и addresses отделены в соответствующие
`Analysis/*/SparkplugAbi.h`. Host-layout не выдаётся за 32-bit native ABI.

Тесты проверяют identity/base/factory, размеры manager/node/header/FAT/hook, состояние
`0/1/2`, ownership, platform/load/save dispatch, first-match при duplicates,
lookup по object RTTI, все ветви header validator-а и чтение header из
`spMemoryStream`, а также FAT grammar, lookup, cursor, clear, object index и
ошибку неизвестного RTTI ID. Отдельный lifetime probe фиксирует teardown по группам первого
появления serializer pointer (`A, B, alias A -> destroy A, затем B`).

Read-only regression check повторно проверяет hashes, literal source paths,
RTTI initializers manager/hook, числа registration calls, три вызова FAT
constructor-а, точные FAT field spellings, формы обоих index readers и
различие инициализации byte `+0x1C`, а также отсутствие фактического чтения
`SBOO` marker после object-header read — всего 67 проверок — без
запуска игры и без изменения binaries:

```powershell
python -B research\inspect_serializer_manager.py
```

## Открытые вопросы и следующий логический узел

PC checkpoint5: [whole portable SAN loader](native-pc-full-loader.md) уже
реализован через existing core; outer422940 имеет42 directed native checks,
inline reference —115. Это закрывает прежний общий gap для подтверждённого
SAN subset, но не SMO mesh batches, whole save или general cyclic ownership.
Последующие пункты относятся к оставшимся ветвям, а не отсутствию loader-а.

- исходное имя и полный constructor owned helper-а `+0x28`; PC extent58 и container consumers уже подтверждены;
- original enum/name политики `+0x18` и смысл значений, отличных от `0/2`;
- original enum/type names platform, operation и header result;
- save pipeline и использование `declaredFileSize/dataSize`;
- оставшееся PC unknown44 и save-side происхождение resource `fileID`;
- cache-miss materialization, ownership и rollback;
- рекурсивный relationship resolver и момент удаления временного FAT state;
- original имя/signature hook slot (PS2 `+0x24`, PC `+0x1C`) и полный PC mesh-containing body.

Hook, основной FAT index-срез и `spResourceManager` category+name cache теперь
закрыты отдельными code/evidence/test/doc циклами. Следующий прямой join-узел —
cache-miss materialization и рекурсивный relationship resolver внутри
`0x00182B90/0x00181150`, затем save payload. Это
одновременно продвигает native importer/exporter и связывает уже разобранные
mesh/material/texture serializers с runtime resource ownership.

Связанная end-to-end карта находится в
[`runtime-resource-pipeline.md`](../engine/runtime-resource-pipeline.md).
