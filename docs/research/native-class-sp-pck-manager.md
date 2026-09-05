# `spPCKManager`: индекс пакетов и точный resource resolver

Статус: реконструирован переносимый поведенческий срез; RTTI, обе vtable,
PS2-layout, формат PCK, порядок приоритетов и семипараметрический resolve
подтверждены независимо на PC и PS2. Исходный `.cpp`/`.h` в бинарниках не
сохранился, поэтому размещение в `Code/SparkBase` и имена методов остаются
`inferred`.

Контрольные файлы:

- PC `local-data/pc-pristine/WinxClub.exe`, SHA-256
  `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`;
- PS2 `local-data/Winx Club the game PS2/SLES_532.19`, SHA-256
  `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`,
  `GP = 0x004A4170`.

## Идентичность и RTTI

| Свойство | PC | PS2 |
|---|---:|---:|
| class name | `spPCKManager` @ `0x006E71EC` | `spPCKManager` @ `0x00444620` |
| class ID | `0x2B9D1649` | `0x2B9D1649` |
| base ID | `spBaseObject`, `0x415352A1` | `spBaseObject`, `0x415352A1` |
| registration | `0x0075FD60` | `0x004A26D0` |
| factory | `0x0045C4E0` | `0x00113E50` |
| primary vtable | `0x006E71C8` | `0x0048CA40` |
| support vtable | `0x006E71C4` | `0x0048CA64` |
| singleton global | `0x0075DB9C` | `0x0049F844` |

PS2 factory `0x00113E50` выделяет ровно `0x34` байта. Конструктор
`0x00113B90` публикует полный объект в global, инициализирует оба контейнера и
резервирует 30 записей пакетов. PC factory закрыт SecuROM, но два независимых
constructor/copy paths по `0x0048E4FC` и `0x004D3200` ставят те же primary и
support vptr. Максимальный наблюдаемый PC offset `+0x38` даёт точный extent не
меньше `0x3C`; прямой `operator new(0x3C)` пока не прочитан.

## Virtual contract

PC primary vtable:

| Slot | Target | Наблюдаемая роль |
|---:|---:|---|
| `+0x00` | `0x0045CA10` | deleting destructor |
| `+0x04` | `0x005B7A00` | пустой notification slot базы |
| `+0x08` | `0x0045C540` | clone construction |
| `+0x0C` | `0x0040ECE0` | пустой base-copy |
| `+0x10` | `0x0045C430` | registration getter |
| `+0x14/+0x18` | `0x00408350/0x00408370` | exact/kind-of |
| `+0x1C` | `0x0045C590` | добавить PCK |
| `+0x20` | `0x0045B8A0` | разрешить имя ресурса |

PS2 ABI содержит два ведущих слова и вставленный support fragment. Поэтому
последние два метода находятся по `+0x30/+0x34`; targets —
`0x00113650/0x00112FF0`. Adjustment destructor support-subobject находится по
`0x001148A0`, PC counterpart — `0x0045C440`.

Clone `0x00113D90` создаёт новый пустой manager, регистрирует пару в
`spCloneManager` и вызывает у source slot `+0x14`. Этот slot равен общему
пустому `spBaseObject` copy `0x00100320`, поэтому подключённые пакеты не
копируются.

## Native layouts

### PS2 object, `0x34`

| Offset | Size | Роль |
|---:|---:|---|
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | 4 | polymorphic singleton-support subobject |
| `+0x14` | 4 | package capacity; constructor reserves `0x1E` |
| `+0x18` | 4 | package count |
| `+0x1C` | 4 | pointer to `0x1C`-byte package records |
| `+0x20` | 4 | unknown |
| `+0x24` | 1 | enables opened-name collection, analytical role |
| `+0x25` | 1 | pauses that collection, analytical role |
| `+0x28` | `0x0C` | capacity/count/pointer of owned name list |

PC uses different compiler containers: package begin/end/capacity-end are at
`+0x18/+0x1C/+0x20`, flags at `+0x28/+0x29`, and opened-name begin/end/capacity
at `+0x30/+0x34/+0x38`. Эти ABI не объединяются в одну native-структуру.

### Mounted package record, `0x1C`, обе платформы

| Offset | Роль |
|---:|---|
| `+0x00` | owned package-name/path string |
| `+0x04` | priority |
| `+0x08` | number of file records |
| `+0x0C` | owned array of file records |
| `+0x10` | owned string block |
| `+0x14` | physical origin when the PCK itself is nested |
| `+0x18` | physical size of the PCK backing stream |

Packages are inserted in descending priority. A new equal-priority package is
inserted before existing equal entries. Duplicate package names are compared
case-sensitively and return success without reopening the file.

Removal (`PS2 0x00113500`) frees the two index allocations and name, copies the
last `0x1C`-byte record over the removed slot, then decrements count. Это не
сохраняет сортировку приоритетов. Destructor `0x00113A50` содержит странный
цикл с одновременно растущим индексом и уменьшающимся count; возможную утечку
оставшихся записей отмечаем как native anomaly и не воспроизводим в переносимом
коде без runtime-подтверждения.

## Формат PCK

`AddPackage` читает из `spFileStream`:

```text
u32 stringByteCount
u8  strings[stringByteCount]
u32 fileCount
FileRecord files[fileCount]  // stride 0x14
... sector-aligned payloads
```

Serialized file record:

| Offset | Роль |
|---:|---|
| `+0x00` | offset каталога в `strings` |
| `+0x04` | offset basename в `strings` |
| `+0x08` | logical sector number (LSN) |
| `+0x0C` | byte offset payload |
| `+0x10` | byte count payload |

После чтения `AddPackage` прибавляет адрес string block к первым двум словам,
превращая offsets в C-string pointers. Для каждой проверенной записи выполняется
точное равенство `LSN * 0x800 == byteOffset`. Records отсортированы по basename;
одинаковые basename из разных каталогов образуют соседнюю группу.

Безопасный read-only прогон `research/inspect_pck.py` по
`local-data/Winx Club the game PS2/DATA/PCK` дал:

- 78 из 78 валидных PCK;
- 12 229 file records;
- ни одного нарушения string bounds, payload bounds, basename ordering или
  LSN/offset invariant;
- `INIT.PCK:data/menus/4kids.stx` — LSN `0x4B5`, offset `0x25A800`, size
  `0x4043D`.

Скрипт читает только заголовок, string block и таблицу записей; payloads в
память не загружаются. Воспроизводимый запуск из корня репозитория:

```text
cd local-data\Winx Club the game PS2
python ..\..\research\inspect_pck.py DATA\PCK --lookup data\menus\4kids.stx
```

## Разрешение ресурса

Подтверждённый virtual contract имеет семь явных параметров после `this`:

```text
bool Resolve(
    resourceName,
    packageNameOut,
    packagePhysicalSizeOut,
    packagePhysicalOriginOut,
    logicalSectorOut,
    byteOffsetOut,
    byteCountOut)
```

PC `0x0045B8A0` и PS2 `0x00112FF0` независимо показывают одинаковый порядок.
Внутренние matchers — `0x0045B1D0` и `0x00113120`.

Алгоритм:

1. удалить только начальный `./` или `.\`;
2. заменить `\` на `/`;
3. перевести ASCII uppercase в lowercase (`PS2 0x0040AC40`);
4. разделить строку по последнему `/` на directory и basename;
5. обходить пакеты в текущем порядке приоритетов;
6. binary-search basename, затем просмотреть всю соседнюю группу одинаковых
   basename и сравнить directory;
7. вернуть package path, два свойства backing package и три свойства файла.

`spPS2FileStream::Open` передаёт семь output addresses. При успехе он отмечает
PCK-режим, кладёт `byteCount` в `+0x2C`, а `byteOffset` добавляет к
`spStream::logicalOrigin +0x14`. LSN нужен fast/async path. Это закрывает
прежний P0-вопрос о resolver перед `spPCFileStream::Open`: менеджер, аргументы и
семантика всех outputs теперь известны.

## Дополнительный список имён

`PS2 0x00112FC0` / `PC 0x0045B660` возвращает true только при первом флаге 1 и
втором 0. Тогда file-stream open вызывает `PS2 0x00112EE0` / PC counterpart и
добавляет ещё не встречавшееся имя в отдельный owned-string vector. Consumers,
которые выгружают этот список, и исходные имена флагов пока не установлены.

## Реконструкция и проверки

- `Sparkplug/Code/SparkBase/spPCKManager.h/.cpp` — RTTI, singleton lifetime,
  priority ordering, exact resolver, removal и безопасный parser seam;
- `Sparkplug/Analysis/PC/SparkBaseAbi.h` — PC observed layout и адреса;
- `Sparkplug/Analysis/PS2/SparkBaseAbi.h` — exact `0x34` layout и адреса;
- `Sparkplug/Tests/spBaseObjectTests.cpp` — synthetic PCK, duplicate basename,
  normalization, priority fallback, семь outputs, clone и malformed bounds;
- `research/inspect_pck.py` — независимая corpus-проверка.

Portable parser добавляет bounds/order checks, которых native код явно не
делает. На non-Windows `vfunc_AddPackage` пока не подменяется `std::ifstream`:
до runnable `spPS2FileStream` используется `AddPackageImage`, чтобы не выдавать
host I/O за восстановленный platform leaf.

## Открыто

1. Original method/member/container names и точный source/header path.
2. Прямой PC factory allocation size; сейчас доказан observed extent `0x3C`.
3. Роль слова PS2 `+0x20` / PC `+0x24`.
4. Кто включает, приостанавливает и потребляет opened-name collection.
5. Runtime-проверка destructor anomaly и порядка после удаления не-последнего
   приоритетного пакета.
