# План исследования

План построен вокруг проверяемых результатов, а не календарных сроков.

Главная единица исследования теперь — исходная архитектура исполняемых файлов:
оригинальные типы, inheritance, модули, subsystem ownership и точный call graph
от запроса ресурса до runtime object и вывода. Форматы, corpus и поведение игры
остаются обязательными источниками evidence, но больше не задают основной
порядок работ сами по себе. Успешно открытый или видимый файл не доказывает
семантику отдельных полей без найденного consumer в executable.

Правила границы и первая карта исходных модулей находятся в
[реконструкции оригинальной архитектуры](docs/engine/original-architecture.md),
ближайшая очередь классов — в
[плане native-реконструкции](docs/research/native-reconstruction-plan.md),
а активная карта адресов — в
[конвейере ресурсов executable](docs/engine/runtime-resource-pipeline.md).
Локальные эвристики под один известный файл не считаются исследовательским
результатом.

Классы проходят по логическому фронту зависимостей, а не по ожидаемой
сложности: после каждого полного evidence/code/test/doc цикла выбирается direct
узел, закрывающий максимум оставшихся связей текущего end-to-end пути. Пилот и
метрики этого порядка зафиксированы в плане native-реконструкции.

## 0. Исходная архитектура и граница engine/game — главный активный этап

Цель: восстановить Sparkplug с доказанными оригинальными именами классов,
inheritance, модулями и interfaces, а Winx-код держать отдельным application
layer. Неизвестные функции остаются `sub_<VA>`, а аналитические aliases не
становятся именами реконструированного API.

- [x] Восстановить все 733 PC registrations: 329 `sp...`, 404 `wx...`, class/base
  hash, registration object и 42 прямые границы `wx -> sp`.
- [x] Подтвердить исходные модули `SparkBase`, `SparkBasePC`, `Sparkplug`,
  `SparkplugDX`, `SparkplugPC` по 53 source paths и отделить embedded third-party.
- [x] Сопоставить PC с PS2: 231 общий `sp...` и 399 общих `wx...` типов; у всех
  совпадают class hash и base class hash.
- [x] Проверить текущую SMO class-ID table против полного PC graph и исправить
  три неоригинальных shadow-имени.
- [x] Разобрать все 681 MIPS registration PS2: 275 `sp...`, 406 `wx...`,
  registration/base objects, callback arguments и 44 границы `wx -> sp`.
- [x] Создать корень `Sparkplug/` и первый exact translation unit
  `Code/SparkBase/spBaseObject.cpp`: registration, PS2 layout `0x10` и vtable
  addresses без недоказанных имён методов.
- [x] Восстановить общий PC/PS2 registration record размером `0x60`: class/base
  IDs, встроенное имя, base pointer, factory и post-registration property
  callback; подтвердить последний на свойствах `X/Y/Z Value`; блок `+0x50`
  исправить до property group owner/count/first с наследуемым поиском.
- [x] Разобрать PS2 `spNamedObject`: размер `0x14`, shared-name entry, factory,
  clone/copy slots и наследуемые exact/base-chain type checks.
- [x] Собрать первый переносимый `SparkBase` с отдельными ABI evidence-layouts и
  автоматическим тестом object registration, creation, inheritance и clone.
- [ ] Закрыть минимальную связку `spBaseObject -> registration/spRTTIManager ->
  spNamedObject`: reverse-reference list `+0x04`, reference count `+0x08` и PS2
  layouts `spCloneManager`/`spRTTIManager` уже подтверждены; остались
  `+0x0A/+0x0C`, исходные имена singleton-support base/property group,
  PC tree-layout и остаток field schema/callable ABI 0x58-байтной записи
  `spPropertySystem` (name/type и dispatch уже подтверждены).
- [ ] Привязать каждый исследуемый native type/function к module и owner по
  xrefs/constructors, не только по префиксу.
- [ ] Восстановить bootstrap `spApp/spEngineCore -> wx...`, порядок managers и
  callbacks между engine и game.
- [ ] Выделить полный third-party/system слой: imports, static libraries и DRM.
- [x] Добавить в schema v5 отдельный native catalog: 784 объединённых типа,
  executable/platform presence, inheritance, scope, progress и атомарный
  evidence с provenance; не смешивать его с SMO `classes`.
- [x] Создать каркас `Sparkplug/` и `Winx/` только после доказательства имени
  модуля и класса; signatures и layouts добавляются по мере подтверждения,
  неизвестные поля остаются явными. Текущий срез содержит 112 native-карточек,
  248 файлов реконструкции Sparkplug и шесть файлов application-слоя Winx.
- [x] Ввести воспроизводимую оценку покрытия из SQLite, а не из памяти сессии:
  executable 13,10%, Sparkplug 24,78%, Winx 2,50%, прямые SMO/SAN-типы 44,54%
  на снимке 5 сентября 2026 года.

## 0.1. Точный runtime-конвейер — активный этап

Цель: для PC и PS2 пройти цепь `logical request -> file/PCK -> serializer ->
runtime object -> scene -> renderer -> draw` с адресами, структурами и
воспроизводимой трассой.

- [x] Найти PC `FindMediaPath`, `BuildAssetPath`, общий `ResourceLoad`,
  `spSerializerManager::LoadSceneGraph` и проверки `FFPS/0x26`.
- [x] Независимо сопоставить FFPS/FAT/serializer/root-`spNode` участок в PS2 ELF.
- [x] Восстановить подтверждённый срез `spSerializerManager`: общий PC/PS2
  `0x2C` ABI, ordered platform/operation registry, `0x1C` header validator,
  singleton/clone/teardown и безопасный stream front-end.
- [x] Выделить прямые соседние границы manager-а: PS2 FAT helper `0x64` с
  точными entry layouts/field spellings и cluster `spSerializerHook` с
  платформенными DX/PS2 IDs; недоказанный callback API не добавлять.
- [x] Перенести общий `spSerializerHook` и PS2 leaf до статической границы,
  отдельно зафиксировав закрытое SecuROM тело DX leaf.
- [x] Перенести безопасный FAT-срез: load grammar, RTTI validation,
  ID/object lookup, insertion-order cursor, clear/reset и object indexing.
- [x] Восстановить `spResourceManager` как нативный category+name кэш
  `spTexture`/`spMesh`, включая PC/PS2 ABI, reserve и lifetime hook.
- [x] Найти конечные PC D3D9-вызовы `SetStreamSource`, `SetIndices`,
  `DrawPrimitive` и `DrawIndexedPrimitive`.
- [ ] Восстановить raw-file/PCK resolver и lifetime потока.
- [ ] Продолжить FAT после cache lookup: object materialization,
  payload save и два прохода relationship fixup.
- [ ] Добавить generic serializer-dispatch trace с object index/name/class ID,
  serialized interval, serializer и runtime pointer.
- [ ] Проследить root `spNode` до scene/partition owner, traversal и culling.
- [ ] Связать serialized mesh/material/texture с runtime object и D3D resources.
- [ ] Добавить draw provenance до logical asset и object ID/name.
- [ ] Найти эквивалентный PS2 VIF/GIF/GS output path.
- [ ] Закрыть первым end-to-end маршрутом `Alfea02.smo`, затем character SMO,
  SAN, STX и SPT/SPL-created object.

## 0.2. Исследовательский workspace — готово

- [x] Объединить инструменты через Git submodule.
- [x] Добавить общее решение для семи проектов.
- [x] Отделить документацию и дневник от кода инструментов.
- [x] Исключить игровые файлы и локальные дампы из Git.
- [x] Зафиксировать правила: факт, гипотеза, открытый вопрос.

## 1. FFPS и сериализатор — активно

Цель: устойчиво прочитать контейнер и объектный граф до интерпретации конкретного рендера.

- [x] Заголовок `FFPS`, каталог объектов и перевод logical offset в physical offset.
- [x] Заголовок объекта `typeHash + SBOO`.
- [x] Базовый разбор `spDataBlockSerializer` и расширенных размеров.
- [x] Реестр известных class ID.
- [x] Строгие диагностические коды без эвристического «исправления» входа.
- [x] Найти эквивалентные проверки `FFPS`/`0x26` в runtime-коде PC и PS2.
- [ ] Восстановить схему serializer по xrefs и строкам полей PC/PS2 executable.
- [ ] Установить смысл полей заголовка `0x08` и `0x10`.
- [ ] Разделить повреждение каталога и допустимые платформенные варианты.
- [ ] Добавить стабильный JSON-экспорт графа объектов.

## 2. Статические меши — следующий приоритет

Цель: одинаково воспроизводимый mesh decode на чистых PC- и PS2-корпусах.

- [x] Подтвердить варианты `E0`/`E1` и primitive type `3` (triangle strip).
- [x] Отделить serialized stride от runtime stride.
- [x] Показать подтверждённые позиции в просмотрщике.
- [ ] Разобрать primitive type `2`.
- [ ] Разобрать PS2-варианты `E1` и правила границ блоков.
- [ ] Описать все встреченные vertex format/FVF: normal, color, UV и веса.
- [ ] Подтвердить winding, handedness, оси и единицы измерения.
- [ ] Восстановить transform hierarchy и владение transform-данными.

## 3. Материалы и текстуры

Цель: перенести подтверждённые знания `SMOTextureTool` в общий объектный граф и добиться визуально корректного рендера.

- [x] Экспорт и замена известных текстурных раскладок.
- [x] Round-trip repack без замены данных.
- [x] Экспериментальная замена на 1024/2048 с проверкой в игре.
- [x] Связать подтверждённый one-to-one путь `spMaterialData` → `spTextureData` с sibling mesh через каталог.
- [ ] Разобрать несколько material/texture layers и неоднозначных sibling mesh.
- [ ] Назвать и проверить render/texture states вместо хранения «магических» индексов.
- [ ] Воспроизвести vertex diffuse modulation, alpha и blend operations.
- [ ] Реализовать catalog-safe repack с пересчётом всех затронутых offsets/sizes.
- [x] Добавить в просмотрщик подтверждённые BGRA textures и UV0 layout `0x940`.
- [ ] Расширить texture render на остальные подтверждённые vertex layouts и material passes.

## 4. Сцена, skin и анимация

- [x] Описать `spNode`, `spSkin`, bone palette, vertex weights и логические связи `esfNodeChild` для PC-корпуса.
- [ ] Восстановить runtime skinning formula и pose update.
- [ ] Разобрать GUI semantics `spTextNode` / `spTextRenderable` / `spFont`.
- [ ] Исследовать `ANM` как таблицу состояний/ссылок.
- [ ] Исследовать `SAN` как FFPS-анимационный ресурс.
- [ ] Добавить проигрывание анимации после стабилизации статической сцены.

## 5. Мир и collision

- [ ] Связать `SPT`, `SPL` и SMO-ресурсы уровня.
- [ ] Описать `spCollisionInfo`, `spMeshBV` и пространственные структуры.
- [ ] Отделить сериализацию движка от игровых компонентов Winx Club.
- [ ] Добавить просмотр сцены/уровня только после документирования ссылок.

## 6. Воспроизводимость

- [ ] Получить чистые PC- и PS2-корпусы и создать локальные SHA-256 manifests.
- [ ] Разделить pristine, modified и extracted наборы.
- [ ] Добавить обезличенные regression fixtures, которые можно законно публиковать.
- [ ] Сравнивать parser output между ревизиями в CI.
- [ ] Связать каждое утверждение документации с кодом, manifest или записью эксперимента.

## 7. Экспорт и контролируемый импорт

- [x] Добавить отдельный `SmoExporter.Core` поверх общего strict decoder.
- [x] Экспортировать meshes, normals, UV0/UV1, vertex colors, материалы и текстуры в самодостаточный GLB.
- [x] Добавить обязательный compatibility export OBJ/MTL/PNG.
- [x] Проверить GLB и OBJ импортом в Blender 4.5.
- [x] Сохранить importer metadata в GLB и проверять неизменность исходного SMO.
- [x] Реализовать topology-safe in-place replacement vertex records без изменения FFPS-каталога.
- [x] Реализовать rigid replacement через один существующий palette slot без изменения skeleton/object graph.
- [x] Добавить детерминированное разбиение целой OBJ/GLB-сцены по triangles с настраиваемыми лимитами vertices/indices/triangles.
- [x] Реализовать экспериментальный catalog-safe repack существующих mesh slots для изменения vertex/index count и topology.
- [x] Проверить первый whole-model SMO в игре: triangle-list загружается, но распределение по старым skin slots разрывает модель при анимации.
- [x] Проверить в игре single-slot rigid вариант с нулевыми mesh slots: игра завершается аварийно.
- [x] Проверить диагностический single-slot rigid v2: игра загружается, вырожденные primitives устраняют crash.
- [x] Добавить single-slot rigid v2 и автоматическую подгонку размера/центра в GUI.
- [x] Добавить ручной выбор rigid palette bone и замену основного character atlas из PNG/JPEG.
- [x] Извлекать embedded GLB base-color textures и записывать их безопасным fixed-size RGB writer без repack; сохранять исходный Alpha и структуру SMO.
- [x] Исследовать пересчёт FFPS directory offsets/sizes после texture resize; путь признан небезопасным и отключён после игровых crash.
- [x] Ограничить ручной bone picker реально использованными host-mesh palette slots после crash на неподтверждённом slot 8.
- [x] Проверить пределы 512/1024/2048/4096; размерный repack признан неподтверждённым и полностью отключён.
- [x] Сделать замену текстуры явной и необязательной; по умолчанию сохранять исходный SMO atlas.
- [x] Запретить resize texture без подтверждённого material owner; разрешать только замену пикселей исходного размера.
- [x] Зафиксировать whole-model body atlas в pixel-only режиме 256×256 из-за нестабильной owner association.
- [x] Подтвердить texture mutability однобайтовыми raw RGB pixel probes без repack и изменений headers (проверено в игре 10.08.2026).
- [x] Подтвердить в игре полную замену RGB атласа `256×256` при побайтовом сохранении Alpha, headers, offsets и размера файла.
- [x] Восстановить texture writer в GUI только в подтверждённом fixed-size RGB режиме; resize/repack `SMOTextureTool` не использовать.
- [x] Добавить замену встроенной текстуры из GLB/PNG с проверкой BGRA layout, marker `00` на `+0x3C` и fixed-size записью только RGB.
- [x] Добавить SMO → SMO visual transplant: сохранить служебный object graph/materials/collision target, заменить meshes и textures данными донора и пересчитать FFPS offsets/sizes.
- [x] Разрешить различающееся число SMO meshes/palettes: перераспределять donor triangles по доступным 16-bone target slots и безопасно гасить лишние slots.
- [x] Добавить отчёт bone mapping и безопасный fallback дополнительных donor bones на shared ancestor/bind-nearest bone; отдельно предупреждать о target bones без donor weights.
- [x] Сравнивать deform hierarchy по ближайшим weighted parents, отдельно показывая обойдённые helper/control paths.
- [x] Конвертировать подтверждённые skinned vertex layouts при SMO-трансплантации, включая генерацию normals для `0x093E → 0x097E`.
- [x] Проверять для SMO-донора platform/serializer, точные bone names и hierarchy; различие bind pose показывать как предупреждение.
- [x] Отключать в GUI подгонку, rigid bone, atlas replacement и повторную нарезку при выборе SMO-донора.
- [x] Импортировать подготовленный GLB/FBX skin по точным bone names и bind pose с пересборкой 16-bone palettes; FBX конвертировать через Blender в общий GLB pipeline.
- [x] Нативным trace локализовать RGBA-crash и исправить off-by-one marker/payload (`+0x3C`/`+0x3D`); corrected donor Alpha проходит native load, production ждёт широкой визуальной проверки.
- [ ] Добавить редактор соответствий и контролируемую генерацию weights для модели без костей или с неверным skeleton.

## Долгосрочный ориентир: от файлов к модели игры

Этот раздел фиксирует направление, а не обещание построить большой редактор и не
календарный план. Ближайшими приоритетами остаются исследование форматов,
воспроизводимость и стабилизация уже существующих инструментов.

Целевая зависимость слоёв:

`Research → общие библиотеки/SDK → специализированные инструменты → модель игры`

Creation Kit используется только как ориентир модели данных: базовые сущности,
их экземпляры в сценах, ссылки, зависимости и отдельный набор изменений поверх
оригинальной игры. Его конкретный интерфейс и файловая система плагинов не
копируются.

### 8. Исследовательские сборки и граница ответственности

- [ ] Называть публичные сборки стадии `0.x` **Research Preview** или
  **Experimental Build**, не обещая стабильность ещё исследуемого pipeline.
- [ ] Для руководств и диагностических отчётов всегда указывать точную версию
  инструмента; сохранять старые сборки вместо преждевременной совместимости со
  всеми проектами `0.x`.
- [ ] Для breaking changes кратко описывать изменившееся понимание формата,
  затронутые результаты и путь повторной сборки.
- [ ] Отделять демонстрационный best case после ручной доводки от
  воспроизводимого результата автоматического импорта.
- [ ] Выпускать новую пользовательскую сборку по накопленному проверяемому
  результату, а не по каждой отдельной находке.

### 9. Общая база ресурсов и зависимостей

Цель: научиться открывать не только один файл, но и логическую сущность игры,
собранную runtime из нескольких ресурсов.

- [x] Ввести read-only `Game Database`: индекс поддерживаемой установки игры,
  hashes, типы ресурсов, логические имена и известные ссылки между ними.
  Основа реализована в research schema v5 для PC/PS2: все файлы, EXE/ELF,
  directory/PCK, свойства, символы, resolved/ambiguous/unresolved dependencies
  и отдельный накопительный слой native research.
  Следующий слой — пользовательский asset browser поверх этого индекса.
- [ ] Добавить общий `Asset Resolver`, которым пользуются Viewer, Importer и
  Level Creator вместо собственных правил поиска файлов.
- [ ] Различать asset, логическую игровую сущность и её экземпляр на уровне;
  сохранять источник и provenance каждого разрешённого объекта.
- [ ] Представлять неизвестную или отсутствующую зависимость явным proxy, а не
  угадывать её и не повреждать исходный ресурс.
- [ ] Построить проверяемый dependency graph для цепочек вроде
  `EXE → ANM → SAN → track → SMO node` и составных персонажей из body, hair,
  skeleton и animation set.
- [ ] Последовательно закрыть частично известные и inventory-only форматы по
  [очереди анализа ресурсов](docs/research/game-resource-analysis-priority.md),
  сохраняя для каждого structure, semantics, dependencies, runtime и write
  statuses раздельно.

### 10. Level Creator как первый game-level полигон

- [ ] Сначала стабилизировать текущие open/save, project journal, Undo/Redo,
  viewport, build и проверки в игре; не заменять работающий редактор сразу новой
  оболочкой.
- [ ] Постепенно отделить `LevelDocument` от одного `SMO`: SMO остаётся одним из
  источников, рядом могут разрешаться внешние персонажи, анимации и метаданные.
- [ ] Разделить **Source Scene** (что действительно хранится в SMO) и
  **Resolved Scene** (что собирает и показывает игра).
- [ ] Показывать внешних персонажей и эффекты как linked/read-only instances,
  визуально отличать их в иерархии и никогда не сериализовать их внутрь level SMO
  без отдельной явной операции.
- [ ] Предусмотреть режимы авторской структуры и игрового preview: collision,
  navigation и markers для исследования; разрешённые actors, animation и effects
  для оценки итоговой сцены.

### 11. Общая оболочка со специализированными редакторами

Универсальной должна стать среда, а не центральный редактор каждого формата.
Переход к ней начинается только тогда, когда несколько программ действительно
повторяют один и тот же устойчивый компонент.

- [ ] Выделять общие document model, property system, selection, diagnostics,
  validation, asset browser и build pipeline без копирования Core-кода в GUI.
- [ ] Сохранить специализированные контексты: Level, Character/Skeleton,
  Model, Material/Texture, Animation, Collision/Navigation и другие только по
  мере подтверждения соответствующих данных.
- [ ] Разрешить нескольким представлениям работать с одной document model:
  изменение материала, scene graph или skin должно быть видно во всех вкладках.
- [ ] Сохранить **Advanced/Research View**: object/class ID, relationships,
  logical/physical offsets, raw bytes и статусы
  `подтверждено / гипотеза / неизвестно`.
- [ ] Показывать возможности операции явно: `view`, `edit`, `experimental` или
  `unsupported`; GUI не должен изображать поддержку, которой нет у writer-а.

### 12. Семантические операции над игрой

Цель: пользователь меняет «костюм Блум» или «персонажа на уровне», а не вручную
координирует несколько несвязанных файлов и патчеров.

- [ ] Ввести game-level команды и транзакции, которые могут атомарно обращаться
  к нескольким backend: SMO, textures, animations и runtime patches.
- [ ] Не связывать одну кнопку с одним физическим файлом. Например, замена
  внешнего вида персонажа может включать body SMO, hair mapping, texture и
  проверку animation bindings.
- [ ] Рассматривать нынешние SmoImporter, SmoLVLcreator, SMOTextureTool,
  WinxHairPatcher и Viewer как проверочные приложения и возможные будущие backend,
  а не как код, который требуется выбросить ради монолита.
- [ ] Отделить анатомические эвристики режима **Человек** от универсального
  bone mapper для произвольных скелетов.

### 13. Недеструктивные проекты и пакеты модов

Рабочий проект редактора и устанавливаемый мод — разные сущности.
`.smolvlproj` остаётся промежуточным представлением для безопасной разборки и
повторной сборки SMO; он не становится универсальным описанием мода.

- [ ] Зафиксировать отдельную модель mod project: новые assets, семантические
  операции, зависимости, совместимость и конфликты. Рабочее имя пакета `.winx`
  не считать контрактом до проектирования manifest.
- [ ] Считать исходную установку read-only и проверять известную версию по
  hashes; никогда не распространять и не изменять оригинальный EXE напрямую.
- [ ] Сначала реализовать собираемые профили в отдельной staging-папке с
  copy-on-write для изменяемых файлов; runtime loader/hooks рассматривать только
  если простой внешний build действительно станет недостаточен.
- [ ] Определять конфликты по логическому свойству игры, а не только по факту
  изменения одного физического SMO или EXE.
- [ ] Применять runtime patches только к копии подтверждённой версии executable
  и собирать один итог из совместимых семантических операций.
- [ ] Отделить Editor, создающий mod package, от Mod Manager, который разрешает
  зависимости, строит профиль и запускает игру.

### 14. Вход для контрибьюторов и критерий остановки

- [ ] Описать границы модулей и небольшой путь сборки/теста без необходимости
  сначала изучить весь SMO и Sparkplug.
- [ ] Поддерживать ограниченные задачи для UI, документации, installer-а,
  thumbnails, fixtures и независимых экспортных путей; помечать подходящие
  issues как `good first issue`.
- [ ] Не считать open source гарантией появления команды и не превращать
  интерес пользователей в обязательство автора поддерживать бессрочный продукт.
- [ ] Любой завершённый слой — документация, corpus, библиотеки или отдельный
  рабочий инструмент — считать самостоятельным полезным результатом. Проект
  можно замедлить или отложить без искусственного достижения «финального mod kit».
