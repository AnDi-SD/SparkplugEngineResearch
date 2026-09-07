# Конвейер ресурсов в executable

Статус: активная карта реверса. Главная цель проекта на этом этапе —
восстановить не только байтовую структуру отдельных файлов, а точный путь
ресурса через код игры: запрос, поиск, чтение, создание объектов, включение в
сцену и отправку на видеоустройство.

## Что считается точным результатом

Для каждого узла конвейера должны быть зафиксированы:

- конкретный executable и его SHA-256;
- VA/RVA функции и проверяемая машинная сигнатура;
- вызывающие и вызываемые функции;
- соглашение о вызове, аргументы, результат и затронутые поля объектов;
- соответствующий участок PC и, где возможно, PS2;
- runtime-трасса на именованном ресурсе;
- граница уверенности: `located`, `static-mapped`, `runtime-traced` или
  `semantics-confirmed`.

Совпадение файла с нашим parser, успешный возврат `ResourceLoad` и видимый
объект в игре являются разными свидетельствами. Видимость не доказывает, что
каждое сохранённое поле было прочитано или использовано.

## Зафиксированные сборки

Текущая PC-карта ниже относится к PE32 размером 22 065 152 байта и SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
В локальном workspace он лежит в исторически названном каталоге
`local-data/pc-pristine`, но по таблице executable evidence это
`resolution-research`-вариант. Истиной является hash, а не имя каталога.

PS2-опора — `SLES_532.19`, 3 799 600 байт, SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

Адреса нельзя переносить на другую сборку по имени файла. Для остальных PC EXE
нужны masked signatures и отдельная таблица разрешённых RVA.

Read-only сканер `research/inspect_resource_pipeline.py` однозначно нашёл те же
четыре loader anchors и те же D3D endpoints ещё в трёх локальных вариантах:

- `1B04DD9B082087141C840D6546B9B729FD1A3CD1BEBFABC1267035C2B30A4EC3`;
- `21A60A21A5A4232DE0EA7F0019E7D2030A03F3C796BA7EFBBB546E537EF4A193`;
- `C27EA9DB4228781A12A90AE808807D4AF1397A7E40DD8F5FFF28F3C87CC62CDB`.

Во всех четырёх файлах перечисленные ниже RVA совпадают. Это проверка
переносимости конкретных сигнатур, а не утверждение об идентичности всего кода.

## Текущая карта PC

```text
игровой call site
  -> BuildAssetPath
  -> ResourceLoad(path)
      -> создать/открыть file stream
      -> получить spSerializerManager
      -> LoadSceneGraph(stream)
          -> прочитать 28-байтовый FFPS header
          -> проверить FFPS и version 0x26
          -> загрузить FAT index и file index
          -> найти serializer по class ID
          -> создать runtime object
          -> deserialize object / relationships
          -> потребовать spNode для корня scene graph
      -> закрыть stream
  -> сохранить root spNode / найти именованные дочерние nodes
  -> attachment и регистрация в сцене
  -> scene traversal / culling / material setup
  -> spDXRenderer submission
  -> IDirect3DDevice9::DrawPrimitive/DrawIndexedPrimitive
```

Средняя часть теперь **частично подтверждена PC instructions**, а не только
структурой SMO: actual Node attachment/typed Scene registrations/world,
Model/RenderNode ownership и draw callbacks, queues, SceneRender и
Visibility→Partition/Octree/BSP/ZonePortal consumers. См.
[сцену/world](../research/native-pc-scene-world.md),
[render path](../research/native-pc-scene-render-runtime.md),
[BSP](../research/native-pc-bsp-runtime.md) и
[spatial consumers](../research/native-pc-spatial-consumers.md).

Однако это не единая успешно восстановленная `ResourceLoad→pixels` цепочка:
полная loader transaction/FAT fixup, protected Visibility constructor/
plane-copy45E870, Occlusion Init и часть backend/shadow остаются открытыми.
Whole Scene tests используют явные decoded-graph/constructor boundaries,
без запуска игры/GPU; portable Scene pipeline ещё partial. Текущий PC-first
порядок и результаты — [native plan](../research/native-reconstruction-plan.md)
и [журнал цикла](../../journal/2026/2026-09-06-pc-reconstruction-until-1000.md).

### Вход и общий загрузчик

| Узел | VA / RVA | Статус | Наблюдаемое действие |
|---|---|---|---|
| `FindMediaPath` | `0x00592F70` / `0x00192F70` | `static-mapped`, частично `runtime-traced` | Читает registry, сохраняет корень в игровом `wxAssetManager +0x14`, не в `spResourceManager` |
| `BuildAssetPath` | `0x00592C50` / `0x00192C50` | `runtime-traced` | Собирает путь из `MediaPath`, двух таблиц каталогов, языка и имени |
| общий `ResourceLoad(path)` | `0x00458260` / `0x00058260` | `runtime-traced` | Открывает stream, вызывает serializer manager, возвращает `spNode*` либо `NULL` |
| создание file-stream object | `0x006BD580` / `0x002BD580` | `static-mapped` | Создаёт объект размером `0x20`; точный concrete type ещё нужно подтвердить |
| accessor `spSerializerManager` | `0x00419E10` / `0x00019E10` | `static-mapped` | Лениво создаёт singleton размером `0x2C` через `0x00422E00` |
| `LoadSceneGraph(stream)` | `0x00422550` / `0x00022550` | `static-mapped` | Читает header/FAT, выбирает serializer, создаёт корневой объект и проверяет `spNode` |
| `ValidateFileHeader` | `0x00422260` / `0x00022260` | `static-mapped`, `runtime-traced` | Проверяет `FFPS`, `0x26`, размер потока и platform flags |
| serializer lookup | `0x004224F0` / `0x000224F0` | `located` | Ищет registration по class ID; тело PC защищено/перенаправлено SecuROM |

Подробный PC/PS2 ABI, registration contract и переносимый срез вынесены в
[`spSerializerManager`](../research/native-class-sp-serializer-manager.md).
Manager имеет одинаковый размер `0x2C`, хранит текущие platform/operation masks
по `+0x10/+0x14`, 12-байтный platform-specific список по `+0x1C` и owned FAT
helper по `+0x28`. Registration node равен `0x18`; lookup выбирает первую запись
с совпавшими target ID и пересекающимися platform/operation masks.

`ResourceLoad` получает stream, вызывает его virtual method `+0x20` с
`(1, path)`, затем `spSerializerManager::LoadSceneGraph`. После загрузки вызывает
virtual method stream `+0x24` и release/destructor через slot `0`. Названия
virtual methods пока не считаются доказанными, хотя последовательность
`open -> load -> close -> release` хорошо поддерживается control flow.

`LoadSceneGraph` сначала читает через stream virtual slot `+0x30` ровно `0x1C`
байт. Это семь слов manager-header; `ObjectCount` по file offset `0x1C` уже
читает следующий `FAT::LoadIndex`, а полный фиксированный префикс равен `0x20`.
Затем:

1. `0x00422260` проверяет header;
2. `0x00466B90` выполняет операцию, чья error string называется
   `m_pFAT->LoadIndex(pSource)`;
3. `0x00465CD0` соответствует
   `m_pFAT->LoadFileIndex(pSource)`;
4. `0x00465F00` возвращает FAT entry — точное имя метода открыто;
5. class ID читается из `entry+0x10` и передаётся lookup `0x004224F0`;
6. найденная registration создаёт объект и вызывает его serializer;
7. virtual type-check получает `0x695C0F65`, class ID `spNode`;
8. полученный pointer записывается в `entry+0x20`, после чего
   `0x004671F0(entry+0x0C, object)` переносит FAT name в объект, если он является
   `spNamedObject`.

PS2 раскрывает неприятную, но важную деталь шага 3: `0x0017F460` читает count
и пары `m_uFileID/m_szFilename`, однако в этой сборке не сохраняет созданные
file entries в переданный FAT helper. Поэтому наличие file-index секции в
потоке ещё не означает, что runtime использует её при загрузке; это похоже на
оставленный compatibility/stub path. PC counterpart уходит в relocated/
obfuscated область и пока не даёт независимой проверки этого различия.

PS2 `0x00182B90` подтверждает отдельный цикл по FAT entries: он пропускает уже
materialized entries, пытается разрешить `spTexture`/`spMesh` через глобальный
`spResourceManager` по category+name, иначе seek-ает к payload, выбирает serializer, создаёт и
читает объект, затем связывает entry ID с object pointer. Точное разделение
inline/external entries, rollback и последующий relationship-fixup pass ещё не
закрыты. Ветка `entry+0x08 != 0` существует и делает lookup file ID, но
`LoadIndex` оставляет это поле нулевым, а PS2 `LoadFileIndex` не сохраняет
прочитанные file entries. Поэтому называть её обычным on-disk external-load
путём пока нельзя: источник ненулевого resource `fileID` в load-сеансе не найден.

### Конкретный call site: `bloom_hair.smo`

Функция `0x005E7710` даёт связный пример от игрового запроса до использования
результата:

| Адрес | Действие |
|---:|---|
| `0x005E774E` | вызывает `BuildAssetPath` для `bloom_hair.smo`, directory indices `2/1` |
| `0x005E776D` | вызывает общий `ResourceLoad` |
| `0x005E779B` | сохраняет root с ref-count в поле consumer `+0x2D0` |
| `0x005E77B2` | вызывает virtual lookup дочернего node `Hair_Master` и сохраняет его в `+0x2D4` |
| `0x00421A60` | позднее получает `Hair_Master` и `Head`; attachment-семантика подтверждается поведением игры |

Этот путь удобен как первый end-to-end trace, но не заменяет путь уровня:
корневой level SMO дополнительно проходит partition, collision, navigation и
gameplay registration.

### Выход в Direct3D 9

Для той же PC-сборки найдены реальные конечные вызовы
`IDirect3DDevice9`. Renderer-interface получает указатель на subobject
`complete spRenderer + 0x18` и читает device по своему `+0xC9D0`, то есть по
адресу полного объекта `+0xC9E8`. Это подтверждённый device slot, но не поле,
начинающее derived-часть `spDXRenderer`: exact common extent равен `0xCA08`.

Camera path теперь даёт прямую проверенную цепочку до этого device. Общий
`spCamera` строит view/projection state, затем вызывает platform interface у
`spRenderer+0x18`: slots `13/12` устанавливают view/projection, slot `14` —
world matrix, slot `22` — viewport. PC bodies доходят соответственно до
`IDirect3DDevice9::SetTransform(2/3/0x100)` и `SetViewport`. Slot `10` собирает
2D matrix state через те же три операции; serialized `Is2DMode` и внутренний
projection-branch byte при этом являются разными полями.

Material render-to-texture добавляет frame boundary: slots `3/4/5` —
begin/end/clear. Target bind нельзя кодировать единым номером: ordinary/cube
равны `1/0` на PC и `0/1` на PS2; PS2 cube slot выдаёт штатную диагностику
unsupported. Это первый подтверждённый platform-dependent порядок методов
внутри общей 29-operation renderer boundary.

| Узел | VA / RVA | Статус |
|---|---|---|
| renderer draw wrapper | `0x004BC290` / `0x000BC290` | `static-mapped`; выбирает indexed/non-indexed ветвь и готовит states |
| `DrawPrimitive` call | `0x004BC3BD` / `0x000BC3BD` | endpoint подтверждён vtable slot `0x144` |
| `DrawIndexedPrimitive` call | `0x004BC3E5` / `0x000BC3E5` | endpoint подтверждён vtable slot `0x148` |
| `spModel` render | `0x00479DC0` / `0x00079DC0` | `static-mapped`; pre-render → renderer slot `9` → post-render |
| renderer slot `9` | `0x004BC670` / `0x000BC670` | `static-mapped`; распаковывает draw-поля `spDXMesh` и вызывает protected bridge |
| protected submission bridge | `0x004BC4A0` / `0x000BC4A0` | SecuROM `.rld` thunk `FF 25 A4 14 3B 01`; внутреннее тело статически не восстановлено |

Pointer provenance основного model path теперь доказан: PS2
`spRenderNode::0x001AA810` после frustum culling вызывает чистый render-slot
`spRenderable`; `spModel` закрывает его и передаёт собственный `baseMesh` в
общий renderer slot `9`. PC slot читает подтверждённые `spDXMesh` offsets
`+0x44..+0x84` и достигает защищённого bridge, а открытый downstream wrapper
достигает фактических D3D draw endpoints. Внутренние `SetIndices`/
`SetStreamSource` адреса защищённого тела больше не заявляются по статическим
байтам pristine executable.

Сканер также находит отдельный non-indexed D3D path с `DrawPrimitive` по
`0x0060589E` внутри функции около `0x006057E0`. Его владелец пока не установлен;
он не объединяется с основным mesh path только из-за совпавшего API endpoint.

## Независимая PS2-опора

PS2 повторяет ту же высокоуровневую последовательность другим ABI:

| Узел | VA | Статус | Сопоставление с PC |
|---|---:|---|---|
| `ValidateFileHeader` | `0x00181D80..0x00182064` | `static-mapped` | Проверяет `FFPS`, `0x26`, flags и размер |
| scene-graph load | `0x00182150..0x00182624` | `static-mapped` | Аналог PC `0x00422550`; читает `0x1C` байт и требует корень `spNode` |
| второй resource-load variant | `0x00182640..0x00182990` | `located` | Почти тот же FAT/serializer path без уже доказанной scene-root семантики |
| registration append | `0x00182070` | `static-mapped` | Добавляет owned 0x18-byte serializer record; 67 static call sites |
| lookup по object / Class ID | `0x001829E0` / `0x00182AC0` | `static-mapped` | Stable first-match по target/platform/operation |
| цикл materialization всех entries | `0x00182B90` | `static-mapped` | Создаёт ещё не materialized FAT objects и выполняет binding |
| FAT load index | `0x0017F570` | `static-mapped` | Аналог PC `0x00466B90` по control flow и error strings |
| FAT load file index | `0x0017F460` | `semantics-confirmed` | Читает count и `m_uFileID/m_szFilename`, но PS2 body не сохраняет entries; PC `0x00465CD0` закрыт relocated thunk |
| FAT entry selection | `0x0017F8A0` | `located` | Аналог PC `0x00465F00`; точное имя открыто |
| FAT-name application | `0x001810F0` | `semantics-confirmed` | Проверяет `spNamedObject` и вызывает его name setter с `entry+0x0C` |
| render-node culling/dispatch | `0x001AA810` | `static-mapped` | sphere/frustum gate и вызов render-slot каждого `spRenderable` |
| `spModel` render | `0x0015A640` | `static-mapped` | pre-render → renderer slot `9` → post-render |
| PS2 renderer slot `9` | `0x001FF6A0` | `static-mapped` | `mesh+0x50 -> payload+0x40`, VIF/DMA/GS preparation и buffer binding |

В PS2 scene-graph loader созданный объект также проверяется class ID
`0x695C0F65`. Это независимо подтверждает роль `spNode`, FAT и registration
lookup. PS2 теперь независимо закрывает высокоуровневый mesh endpoint до
VIF/DMA/GS-подготовки. Формат самих packet descriptors и точные GS register
семантики остаются следующей низкоуровневой границей.

## Главные разрывы цепи

Приоритет определяется разрывами call graph, а не расширениями файлов:

1. **File/PCK resolver.** Разделить raw file stream и PCK path, восстановить
   владельца `loadFromPCK`, cache и lifetime stream.
2. **FAT/object lifecycle.** Найти точный цикл entries, оба прохода создания и
   чтения, порядок ID-only/sized/inline relationships и момент fixup.
3. **Serializer dispatch.** Для каждой runtime registration получить class ID,
   factory, read method, объект на входе/выходе и наследование serializer.
4. **Scene ownership.** Проследить root `spNode*` из `ResourceLoad` до
   partition/scene manager, attachment, ref-count и удаления.
5. **Traversal и culling.** Update/render entries и значительная часть
   `spNode`/Static/Partition/portal selection теперь исполнены отдельно.
   Соединить их с реальным loader graph и закрыть protected normal plane-copy,
   Occlusion Init/silhouette и portable traversal, не выдавая Debug21 за normal.
6. **Material и GPU resources.** Связать object IDs mesh/material/texture с
   созданными D3D vertex/index buffers, textures и pass/state setup.
7. **Draw provenance.** Для каждого draw получить root asset, object ID/name,
   class ID, mesh/material pointer, buffer pointer и primitive arguments.
8. **PS2 output — вторая очередь.** Сопоставить тот же logical mesh с VIF/GIF
   packet и GS draw, когда это нужно для незакрытого PC доказательства либо
   после приоритетного PC пути; не отвлекать текущий цикл без необходимости.

## Первый контрольный маршрут

Первым следует разобрать `Alfea02.smo`, потому что он одновременно проверяет
loader, level scene, partition и спорные размещения:

1. без изменения файла записать `BuildAssetPath`, `ResourceLoad` и root pointer;
2. на входе scene-graph loader снять 28-байтовый header и FAT metadata;
3. на каждом serializer dispatch писать FAT index, object name, class ID,
   serialized interval, serializer pointer и созданный object pointer;
4. отдельно проследить `Object_2_lvl` и `Object_3_lvl` до scene registration;
5. поставить read watchpoints либо узкие breakpoints на consumer к полям
   `Transform`/`InvTransform` и установить, читается ли field 2, заменяется ли
   оно или используется позднее;
6. связать их mesh/buffer pointers с `SetStreamSource`, `SetIndices` и
   `DrawIndexedPrimitive`;
7. повторить только после baseline trace на контролируемой копии с одной
   length-preserving mutation.

До пункта 5 корректный рендер объектов с математическим inverse означает лишь
runtime-совместимость файла. Он не доказывает, что loader поддерживает две
формулы `InvTransform`.

## Порядок работы и артефакты

Каждый закрытый участок должен давать три сохраняемых результата:

1. **Статическая карта:** сигнатура, VA/RVA, callers/callees и псевдокод.
2. **Runtime trace:** JSONL с thread, call depth, asset, object/FAT identity,
   pointers, arguments и return value.
3. **База знаний:** связь executable symbol/call site с logical asset,
   serialized locator и runtime object/GPU resource.

Текущие anchors и конечные D3D slots воспроизводятся без изменения бинарника:

```powershell
python -B research\inspect_resource_pipeline.py local-data\pc-pristine\WinxClub.exe
python -B research\inspect_resource_pipeline.py "local-data\Winx Club the game PS2\SLES_532.19"
python -B research\inspect_serializer_manager.py
python -B research\inspect_renderers.py
python -B research\inspect_render_targets.py
python -B research\inspect_material_render_target_textures.py
```

Корпус файлов является набором входных векторов и проверкой покрытия. Direct
SMO/SAN classes из базы получают приоритет, а **порядок внутри этого фронта**
определяется runtime-зависимостями. Корпус помогает выбирать варианты, но не
подменяет доказательство actual loader/processing/render поведения.

## Критерий завершения end-to-end пути

Путь одного ресурса считается восстановленным только если одна трасса позволяет
без догадки пройти:

```text
logical asset name
  -> physical file/PCK interval
  -> FAT entry и serialized object
  -> serializer registration и runtime object
  -> scene owner/instance
  -> renderable/mesh/material
  -> GPU buffer/texture
  -> конкретный draw call
```

Следующий обязательный контроль — повторить такой путь для character SMO,
level SMO, SAN animation, STX texture и одного SPT/SPL-created объекта. Только
после этого общие правила можно переносить на остальные ресурсы.
