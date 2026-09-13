# spResourceManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spResourceManager](../../../Sparkplug/Code/Sparkplug/spResourceManager.h).

Статус: identity, прямой base, singleton/lifetime, exact PS2 `0x2C`, полный
PC exact allocation `0x30`, cache entry `0x08`, reserve-настройка,
классификация, register/remove/find и синхронные load-ветви подтверждены.
Имена исходных методов, header и translation unit не восстановлены.

## Область доказательств

Строка класса `spResourceManager` и Class ID `0xA4B9923B` присутствуют в обоих executable.

Это не объект с `MediaPath` и таблицами игровых каталогов. Тот PC-объект
принадлежит игровому asset layer (`wxAssetManager` по текущей атрибуции).
Нативный `spResourceManager` существенно меньше и обслуживает только кэш
именованных графических ресурсов `spTexture`/`spMesh`.

## Identity и lifetime

PS2 factory вызывает `operator new(0x2C)`, устанавливает обе vtable,
публикует singleton и задаёт `+0x15=false`, `+0x18=0`, `+0x1C=-1`.
Clone создаёт новый чистый manager и вызывает только storage-free copy base-а;
кэш и настройки не копируются. Как и у других native singleton-классов, новый
экземпляр без guard-а заменяет global pointer.

## Layout и platform split

Общие смысловые поля:

| Offset | Size | Роль |
| ---: | ---: | --- |
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
| --- | ---: | ---: |
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

Открыты:

- original header/TU и имена всех методов/полей;
- роль bytes `+0x14` и word `+0x1C`;
- остальные nonempty PC cache cases и полный loader transaction;
- `.stx` helper и точный тип возвращаемого texture resource;
- async callback/job ABI и error propagation;
- полная FAT materialization/fixup ownership после cache hit/miss;
- controlled runtime trace, связывающий cache hit с конкретным SMO entry.
