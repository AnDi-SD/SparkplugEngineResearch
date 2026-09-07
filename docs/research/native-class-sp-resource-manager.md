# `spResourceManager`: нативный кэш мешей и текстур

Статус: identity, прямой base, singleton/lifetime, exact PS2 `0x2C`, полный
PC exact allocation `0x30`, cache entry `0x08`, reserve-настройка,
классификация, register/remove/find и синхронные load-ветви подтверждены.
Имена исходных методов, header и translation unit не восстановлены.

## Область доказательств

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

Строка класса `spResourceManager` и Class ID `0xA4B9923B` присутствуют в
обоих executable. Исходный путь или хотя бы имя `spResourceManager.cpp` в них
не найдено, поэтому расположение реконструкции под `Code/Sparkplug` inferred.

Это не объект с `MediaPath` и таблицами игровых каталогов. Тот PC-объект
принадлежит игровому asset layer (`wxAssetManager` по текущей атрибуции).
Нативный `spResourceManager` существенно меньше и обслуживает только кэш
именованных графических ресурсов `spTexture`/`spMesh`.

## Identity и lifetime

| Свойство | PC | PS2 |
|---|---:|---:|
| Class ID / direct base | `0xA4B9923B / spBaseObject` | same |
| Registration / initializer | `0x0075FB78 / 0x006D3640` | `0x004A9D30 / 0x00482FD0` |
| Factory | protected thunk `0x00458D00` | `0x0017DD80` |
| Destructor / clone | `0x00458AB0 / 0x00458D60` | `0x0017DB10 / 0x0017DC40` |
| RTTI getter | `0x00458B00` | `0x0017D0B0` |
| Main / support vtable | `0x006E703C / 0x006E7038` | `0x0048EEF0 / 0x0048EF14` |
| Singleton | `0x0075DB78` | `0x0049F868` |
| Size | exact allocation `0x30` | exact allocation `0x2C` |

PS2 factory вызывает `operator new(0x2C)`, устанавливает обе vtable,
публикует singleton и задаёт `+0x15=false`, `+0x18=0`, `+0x1C=-1`.
Clone создаёт новый чистый manager и вызывает только storage-free copy base-а;
кэш и настройки не копируются. Как и у других native singleton-классов, новый
экземпляр без guard-а заменяет global pointer.

## Layout и platform split

Общие смысловые поля:

| Offset | Size | Роль |
|---:|---:|---|
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | `4` | singleton-support subobject vptr |
| `+0x14` | `1` | пока неизвестно |
| `+0x15` | `1` | включён reserve/preallocation |
| `+0x18` | `4` | запрошенное число cache entries |
| `+0x1C` | `4` | default `-1`, роль пока неизвестна |
| `+0x20` | platform-specific | последовательность `{kind, spResource*}` |

Entry равен `0x08`: category по `+0`, non-owning resource pointer по `+4`.
PS2 container занимает `0x0C` (`count +0x24`, storage `+0x28`), поэтому весь
manager равен `0x2C`. Старый MSVC vector занимает `0x10`
(`begin/end/capacityEnd +0x24/+0x28/+0x2C` плюс allocator word), поэтому PC
extent равен `0x30`. Эти ABI намеренно не сведены к одной byte-layout.

PS2 `0x0017D360` сохраняет flag/count и при true резервирует container.
Единственный прямой caller в игре по `0x00277AF4` передаёт `true, 0x9C4`
(2500 ресурсов).

## Ключ кэша

PS2 `Find` `0x0017D480` и `Register` `0x0017D670` независимо раскрывают две
категории:

| RTTI-ветка | Class ID boundary | Category |
|---|---:|---:|
| `spTexture` или наследник | `0x2F281E13` | `1` |
| `spMesh` или наследник | `0x3F077B6C` | `2` |
| всё остальное | — | `0x10` / reject |

Register требует существующее name storage, принимает только категории `1/2`
и добавляет `{category, pointer}` в конец. Он не владеет объектом, не повышает
reference count и не удаляет дубликаты.

Find классифицирует запрошенный Class ID тем же RTTI-графом, затем возвращает
первую запись с совпавшими category и точной строкой name. Поэтому запрос
`spTextureData` не требует exact runtime type: он может вернуть любой ранее
закэшированный наследник `spTexture` с таким именем. Для мешей правило то же.
Порядок вставки наблюдаемо важен.

`Remove` `0x0017D3A0` удаляет первую запись с данным pointer. PS2 destructor
`spResource` `0x0017CEB0` вызывает этот метод. Если singleton отсутствует,
native код перед удалением лениво создаёт manager — странное, но точное
поведение конкретной сборки.

Portable destructor снимает ресурс только с уже существующего manager и не
создаёт singleton во время глобального/static teardown. Это явно отмеченная
защитная граница host-реконструкции, а не попытка выдать удобное поведение за
байт-точное native.

## Load-ветви и связь с FAT

PS2 `0x0017D7E0` отклоняет пустой filename. Ветка `.stx` использует отдельный
texture-data loader; обычный путь открывает platform stream в режиме `1` и
передаёт его в `spSerializerManager::LoadResources` `0x00182640`. Stream в
обоих исходах закрывается и уничтожается.

`0x0017DA20` аналогично открывает stream и вызывает
`spSerializerManager::LoadSceneGraph` `0x00182150`; неудача сопровождается
строкой `Can't open file: %s`. Отдельные функции `0x0017D0C0/0x0017D1D0`
собирают `0x1C`-байтный async job и проводят его через async stream manager.
Полные callback typedef и ownership результата ещё открыты.

Внутри FAT materialization `spSerializerManager::LoadAllFATEntries`
`0x00182B90` сначала вызывает `spResourceManager::Find(classID, name)` для
entry с `fileID==0`. Если кэш дал объект, payload повторно не создаётся.
Именно это ребро соединяет native FFPS object table с ранним
mesh/texture reuse и должно войти в будущий importer/exporter core.

## Что перенесено и проверено

`Sparkplug/Code/Sparkplug/spResourceManager.*` содержит переносимую модель
подтверждённых операций: singleton/defaults, RTTI classification,
non-owning register/remove/find, reserve и blank clone. `spResource`
синхронизирован с этим lifetime hook.

`research/inspect_resource_manager.py` выполняет 50 read-only проверок двух
контрольных binaries: hashes, RTTI initializer, vtables, allocation/defaults,
layout stores, startup reserve, call counts, обе classification branches,
destructor hook и serializer load edges. CTest отдельно проверяет category/name
lookup, leaf/base lookup, duplicate order, rejection и автоматическое remove.

PC [checkpoint10](native-pc-model-render-world.md) исполнил lazy factory
`458D00` при actual MeshData→Mesh→Resource teardown: exact30, обе vtable,
global75DB78 и actual empty deleting destructor подтверждены. Это не
исполнение nonempty PC cache или полного loader transaction.

[PC read-reference checkpoint](native-pc-read-reference.md) теперь дополнительно
исполняет nonempty register/find/remove через один named MeshData: actual
register добавляет8-byte entry, resolver переиспользует его без фабрики и
даже без зарегистрированного serializer-а, teardown освобождает все allocations.
Это не закрывает остальные cache categories/duplicates/error/async ветви.

Открыты:

- original header/TU и имена всех методов/полей;
- роль bytes `+0x14` и word `+0x1C`;
- остальные nonempty PC cache cases и полный loader transaction;
- `.stx` helper и точный тип возвращаемого texture resource;
- async callback/job ABI и error propagation;
- полная FAT materialization/fixup ownership после cache hit/miss;
- controlled runtime trace, связывающий cache hit с конкретным SMO entry.
