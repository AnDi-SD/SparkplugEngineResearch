# Открытые вопросы SMO

Статус: актуализировано 29 августа 2026 года после завершения полного
структурного/read-only разбора 36/36 классов, наблюдаемых в корпусах
`pc-pristine`, `pc-working` и `ps2-pristine`.

Этот файл является каноническим списком нерешённых вопросов. Подробный порядок
быстрых игровых проверок вынесен в
[`../docs/research/smo-runtime-validation-plan.md`](../docs/research/smo-runtime-validation-plan.md).
Долгие задачи, которые разумнее выполнять вместе с разработкой writer/importer,
отделены от текущего runtime-этапа.

Главный порядок текущего исследования задаёт не этот список полей, а разрывы в
[`runtime resource pipeline`](../docs/engine/runtime-resource-pipeline.md).
Визуальные проверки ниже используются после локализации соответствующего
consumer в executable либо как явно ограниченный разведочный тест.

Активный pre-release scope теперь содержит только вопросы, обязательные для
SmoLVLcreator и production model import; его gate и критерии отказа записаны в
[`../docs/research/smo-lvlcreator-import-mvp-plan.md`](../docs/research/smo-lvlcreator-import-mvp-plan.md).
Остальные P0/P1-вопросы ниже не удалены, но считаются post-MVP backlog.

## Подтверждённая исходная точка

- Проиндексированы 1 149 SMO-копий: по 416 в двух PC-корпусах и 317 уникальных
  PS2 SMO; все они структурно разобраны без ошибок.
- Все 36 встреченных class ID известны и имеют строгий read-only decoder.
- Неизвестных границ наблюдаемых объектов и необъяснённых хвостов payload не
  осталось. Неизвестность ниже означает семантику, runtime-поведение,
  ненаблюдавшуюся serializer-ветвь или безопасность записи.
- Primitive type `2` подтверждён как triangle list.
- Прежние группы «494 PS2 preamble» и «137 PS2 boundary» являются настоящими
  native PS2 mesh в PC-файлах `Menus/*_ps2.smo`, а не повреждениями.
- Наблюдаемый material/layer/texture graph, PC skinning, PC SAN timing и
  interpolation, collision geometry и navigation topology структурно разобраны.
- Связь анимаций имеет вид `EXE -> ANM -> SAN -> track name -> SMO node name`.
  Это строковый lookup, а не сырой указатель.
- Native-validator теперь принудительно переводит все наблюдаемые
  `BuildAssetPath` в `Media` рядом с выбранным executable. Неизменённые pristine
  controls `RT-SMO-LOADER-000` и `RT-SMO-NAME-000` прошли; Bloom contextual
  baseline на `startLevel=2` запросил 2 Bloom ANM и 111 различных Bloom SAN.
- `RT-SMO-NAME-CASE-001` подтвердил регистрозависимость PC lookup: one-byte
  `R_Ankle -> r_Ankle` mutation загрузилась, но новый ключ не получил ни exact,
  ни cross-case равенства в нативном `char_traits<char>::compare`-дереве; pristine
  дал два точных совпадения. Импортированные case-insensitive comparators эту
  связь не обслуживают.
- Missing parent/leaf и duplicate toes в обоих порядках не отвергают модель.
  Binding-write probe показал, что `Z_Ankle/Z_Toe` получают новый отдельный слот
  `0xD8`, descendant `foot_right` сохраняет exact slot `0x46`, а два разных
  evaluator одноимённых toes получают общий slot `0x3B` либо `0x3F`. Значит,
  duplicate-политика на binding-слое — all-target; first/last исключены.
- FFPS header теперь разделён без прежней путаницы: `0x04=0x26` — serializer
  version; `0x10` — platform mask (`1=common`, `2=PC`, `8=PS2`); `0x08`
  допускает ноль в fast и Bloom contextual runtime и является 15-битным
  export/session tag candidate с пока неизвестным producer.

## P0 — быстрые runtime-проверки с высокой ценностью

Эти вопросы можно закрывать контролируемыми length-preserving изменениями копий
ресурсов, debug menu и загрузкой известных уровней/персонажей.

| Вопрос | Быстрая проверка | Условие закрытия |
|---|---|---|
| Читает ли runtime `spStaticRenderObject.InvTransform`, пересчитывает его или использует только в отдельных consumers? | serializer trace и последующие reads для `Object_2_lvl`/`Object_3_lvl` в `Alfea02.smo`; лишь затем одиночная mutation | найдено место записи runtime-полей и все render/culling consumers; визуальная совместимость сама по себе вопрос не закрывает |
| Как визуально наследуется отсутствующий parent target? | активный animation tick для `Z_Ankle` и descendant `foot_right` | binding registry уже закрыт; осталось различить итоговый bind pose и world-transform inheritance |
| Что означают collision groups `1/2`? | переключение Group у простого collider и игровые collision/debug проверки | роли групп воспроизводимо различаются либо доказано отсутствие различия в выбранных consumers |
| Что означают `wxFaceData.flags` и `surfaceID`? | изменение одного metadata-поля на выбранной поверхности с движением/звуком/коллизией | найден хотя бы один runtime consumer или подтверждено отсутствие эффекта в проверенных системах |
| Как применяются `ProjectionGroup`, `AlphaSortEnable` и `Priority`? | по одному fixed-size изменению на видимой паре перекрывающихся моделей | определены projection path и порядок сортировки без per-file предположений |
| Как ведут себя наблюдаемые `FinalBlendOp` и state slots? | матрица одиночных изменений на прозрачных, additive, masked и opaque объектах | для каждого реально встреченного значения есть эталонный кадр и подтверждённое render-поведение |
| Как runtime обрабатывает конец AnimTex sequence? | наблюдение и изменение последнего timestamp/общей длительности | различены loop, clamp, restart и frame selection |
| Как runtime связывает GUI anchors, текст и hit-testing? | перенос/rename anchor в menu SMO и проверка текста/клика | зафиксирована цепочка привязки и система координат |

## P1 — быстрые проверки мира, эффектов и defaults

| Область | Открытый вопрос | Практическая граница текущего этапа |
|---|---|---|
| Fog | роль alpha цвета; поведение типов `1/2` и ненулевой density | изменить существующий 20-байтовый payload и сравнить кадры |
| Light | spot type, project-shadow и attenuation | сначала изменить type/углы в существующих полях; optional fields добавлять только после успешных primitive mutation tests |
| ParticleSystem | влияние lifetime/emission, range pairs, loop, world-space и iterative | менять по одному существующему числу/флагу, не перестраивая список полей |
| LensFlare | runtime occlusion | менять существующие radius/speed; ненулевой compound glare требует structural relationship и отложен |
| SkyBox | camera-follow | наблюдать transform относительно камеры; reorder inline models отложен |
| UV controller | формулы FunctionType `0..8` | тестировать только типы, уже представленные в выбранном объекте, и фиксировать движение UV |
| MaterialColorController | формула применения evaluator к material color | менять одну константу/диапазон в существующем controller |
| Navigation | второй байт alternative record и выбор маршрута | безопасно проверить Enabled/portal Open и один reserved byte на копии тестового уровня |
| Occlusion | реальное culling-поведение volumes | переместить существующий volume через node transform и сравнить видимость |
| Billboard | точный смысл axis `1/2` | сначала искать runtime-объект или создавать optional field только на отдельной копии простого node |
| Text | code page, wrap и alignment | equal-length text/glyph tests сначала; добавление отсутствующих fields — после primitive structural test |
| BV defaults | runtime-default для опущенных Position/Rotation и CollisionInfo Transform | сравнить отсутствие поля с явно материализованным default на простом объекте |

## P2 — открытые вопросы формата и loader lifecycle

Эти вопросы важны для будущего writer, но часть из них нельзя честно закрыть
одним визуальным тестом.

- Точный producer 15-битного FFPS export/session tag `0x08`: диапазон и
  отсутствие runtime-зависимости подтверждены, но имя исходной переменной и
  способ генерации в exporter пока неизвестны.
- Порядок создания runtime-объектов, владение inline-объектами и момент
  разрешения ID-only/sized/inline relationships.
- Какие object IDs и service/target bindings обязаны сохраняться при переносе
  visual graph. Два catalog-safe donor/repack эксперимента проходили строгий
  parser, но приводили к падению игры.
- Полный инвентарь stale offsets/inline sizes, оставшихся в `pc-working` после
  старых инструментов. Pristine-корпус остаётся единственным формальным эталоном.
- Точные native engine enum names для 11 material render states, 9 texture
  states и всех `FinalBlendOp`; runtime-тест может доказать поведение, но не имя.
- Runtime-проверка executable-only material sources: их serializer fields и
  layouts (`7`, `13..16`) уже восстановлены, но camera/cubemap/movie branches
  отсутствуют в текущем SMO-корпусе и ещё не проходили authored in-game test.
- Геометрические имена BSP child slots `0/1`; optional Polygon поддержан обоими
  executable, но в корпусе отсутствует.
- Runtime-роль `PartitionNode/PartitionRenderable.DebugColor`.
- Существуют ли реальные `spStaticRenderObject` с несколькими renderables или
  reference-only relationship.
- Точный route-cost/alternative-choice algorithm navigation graph и правила
  сохранения authored non-geometric links при rebuild.
- Точная обработка 16 service/DCC track names, которые не имеют target в
  `bloom_jeans.smo`, и выбор ANM-таблицы для героя/состояния.
- Семантика первых семи ANM-колонок и причина отсутствия `AdvBloom.anm` в
  найденном EXE-блоке.

## Отложено до разработки writer/importer

Следующие задачи намеренно не входят в ближайший runtime-план: они требуют
отдельного длительного реверса, новых образцов или безопасной структурной
пересборки.

- Декодирование PS2 DMA/VIF до всех vertex/index channels и PS2 skin weights.
- Точная семантика двух PS2 mesh count words и алгоритм построения bounds.
- PS2 texture swizzle и аппаратный смысл `auxiliaryValue`/descriptor words.
- Optional attributes редкого vertex layout `0x013E`.
- Поиск настоящего 32-bit index-buffer sample и восстановление storage layout.
- Объяснение дополнительных 12 runtime-байтов layouts `0x097E/0x197E`.
- Воспроизведение exporter-алгоритма деления skin на 16-slot PC palettes.
- Полный coordinated writer для object directory, inline sizes, relationships,
  navigation и multi-pass render graph.
- Создание отсутствующих сложных ветвей: BSP Polygon, compound glare или
  multi-element LensFlare, executable-only texture/material sources и
  произвольные occlusion shapes.
- Полный разбор PS2 SAN playback и соседних SPT/SPL как самостоятельных форматов.
- Поиск SMO с 13 зарегистрированными, но ненаблюдаемыми классами; среди них могут
  быть abstract/runtime/other-format классы, а не отсутствующие типы SMO.

## Ненаблюдаемые serializer-ветви

В `field_definitions` есть 73 подтверждённых executable-ветви, не встреченные в
корпусе. Большинство — опущенные inherited defaults. Приоритетны реальные
пробелы покрытия:

- `spBSPNode.polygon`;
- Position у `spBoxBV`, `spSphereBV`, `spOBBBV` и Rotation у `spOBBBV`;
- `spNode.billboard_axis`;
- `spLightData.project_shadow` и `spLightData.attenuation`;
- `spTextRenderable.wrap_width` и `spTextRenderable.alignment`;
- `spTextureData.SourceNone` и `SourceReference`.

В executable зарегистрированы, но в SMO-корпусе не встречены:
`spAnimation`, `spBoundingVolume`, `spCapsuleBV`, `spCollisionManager`,
`spCollisionMesh`, `spConvexBV`, `spDXShadowMeshSerializer`,
`spDXShadowVolumeManager`, `spEnvironmentMapLayer`,
`spMaterialTextureLayer`, `spPhysicsManager`, `spShadowVolumeManager`,
`spStdLayer`.

Для `spMaterialTextureLayer` и `spStdLayer` отсутствие object-directory sample
остаётся именно пробелом корпуса: их runtime layout, factories и ownership уже
подтверждены по обоим executable.

## Технический долг исследовательской базы

- 2 584 наблюдаемых `spLightData` direct fields всё ещё имеют `is_decoded=0`.
  Их field types `0..8`, payload layouts и семантические определения известны;
  это конфликт выбора между `common pc_ps2` и `platform pc` definitions, а не
  неизвестный формат. Исправление должно закончиться нулевым числом
  неаннотированных содержательных Light-полей и повторным `AUDIT PASS`.
- `docs/research/smo-class-analysis-plan.md` теперь является историей завершённой
  очереди 36 классов, а не источником новых задач.

## Условие закрытия вопроса

Runtime-вопрос закрывается только после фиксации:

1. SHA-256 исходного и изменённого файла;
2. точного field/object locator и побайтового diff;
3. способа запуска уровня, персонажа, меню или эффекта;
4. результата минимум одного повторного запуска после восстановления baseline;
5. платформы и сборки executable;
6. снимка/видео/лога либо явно отрицательного наблюдения;
7. обновления class-документа, evidence базы и этого списка.

Падение игры само по себе не доказывает семантику изменённого поля: сначала
должны быть исключены нарушение размеров, offsets, object graph и случайная
порча соседнего payload.
