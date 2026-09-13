# spPCKManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPCKManager](../../../Sparkplug/Code/SparkBase/spPCKManager.h).

Контрольные файлы:

## Идентичность и RTTI

PS2 factory `0x00113E50` выделяет ровно `0x34` байта. Конструктор
`0x00113B90` публикует полный объект в global, инициализирует оба контейнера и
резервирует 30 записей пакетов. PC factory закрыт SecuROM, но два независимых
constructor/copy paths по `0x0048E4FC` и `0x004D3200` ставят те же primary и
support vptr. Максимальный наблюдаемый PC offset `+0x38` даёт точный extent не
меньше `0x3C`; прямой `operator new(0x3C)` пока не прочитан.

## Virtual contract

PC primary vtable:

| Slot | Target | Наблюдаемая роль |
| ---: | ---: | --- |
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

### PS2 object, `0x34`

| Offset | Size | Роль |
| ---: | ---: | --- |
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
| ---: | --- |
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
| ---: | --- |
| `+0x00` | offset каталога в `strings` |
| `+0x04` | offset basename в `strings` |
| `+0x08` | logical sector number (LSN) |
| `+0x0C` | byte offset payload |
| `+0x10` | byte count payload |

- 78 из 78 валидных PCK;
- 12 229 file records;
- ни одного нарушения string bounds, payload bounds, basename ordering или
  LSN/offset invariant;
- `INIT.PCK:data/menus/4kids.stx` — LSN `0x4B5`, offset `0x25A800`, size
  `0x4043D`.

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

На non-Windows `vfunc_AddPackage` пока не подменяется `std::ifstream`: до runnable `spPS2FileStream` используется `AddPackageImage`, чтобы не выдавать host I/O за восстановленный platform leaf.
