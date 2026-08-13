# План исследования

План построен вокруг проверяемых результатов, а не календарных сроков. Порядок может меняться после получения чистых PC/PS2-сборок.

## 0. Исследовательский workspace — готово

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
- [x] Добавить в просмотрщик подтверждённые ABGR/BGRA textures и UV0 layout `0x940`.
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
- [x] Добавить замену встроенной текстуры из GLB/PNG с проверкой ABGR layout и fixed-size записью только RGB.
- [x] Добавить SMO → SMO visual transplant: сохранить служебный object graph/materials/collision target, заменить meshes и textures данными донора и пересчитать FFPS offsets/sizes.
- [x] Разрешить различающееся число SMO meshes/palettes: перераспределять donor triangles по доступным 16-bone target slots и безопасно гасить лишние slots.
- [x] Добавить отчёт bone mapping и безопасный fallback дополнительных donor bones на shared ancestor/bind-nearest bone; отдельно предупреждать о target bones без donor weights.
- [x] Сравнивать deform hierarchy по ближайшим weighted parents, отдельно показывая обойдённые helper/control paths.
- [x] Конвертировать подтверждённые skinned vertex layouts при SMO-трансплантации, включая генерацию normals для `0x093E → 0x097E`.
- [x] Проверять для SMO-донора platform/serializer, точные bone names и hierarchy; различие bind pose показывать как предупреждение.
- [x] Отключать в GUI подгонку, rigid bone, atlas replacement и повторную нарезку при выборе SMO-донора.
- [x] Импортировать подготовленный GLB/FBX skin по точным bone names и bind pose с пересборкой 16-bone palettes; FBX конвертировать через Blender в общий GLB pipeline.
- [x] Изолированным игровым тестом подтвердить, что внешний Alpha в fixed-size ABGR вызывает crash; production writer сохраняет Alpha target и меняет только RGB.
- [ ] Добавить редактор соответствий и контролируемую генерацию weights для модели без костей или с неверным skeleton.
