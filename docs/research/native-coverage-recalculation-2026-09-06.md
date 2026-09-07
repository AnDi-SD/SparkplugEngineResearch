# Пересчёт изученности PC/PS2 и рабочий контур SMO/SAN

Это исторический срез, не текущие проценты. Исходные цифры6сентября не
переписываются результатами следующего цикла. Текущий контур —
[v2 и причина расширения](native-pc-shader-template-frontier.md), результаты —
[журнал цикла до07:30МСК](../../journal/2026/2026-09-07-pc-smo-san-0730.md).
Актуальный пересчёт: `python research/native_goal_coverage.py report`.

Дата аудита: 6 сентября 2026. Основание: просьба пользователя пересчитать все
проценты и сделать проверяемым условие завершения исследования. Это **аудит
учёта**, не новый запуск анализа EXE. Незавершённые CP19 source edits не зачтены.

## Главные показатели

В скобках **классов с оценкой / всех классов в знаменателе**, а не число
полностью восстановленных классов.

| Область | PC | PS2 |
|---|---:|---:|
| Весь каталог классов EXE | 15,45% (177/733) | 11,37% (137/681) |
| Логика движка | 34,14% (175/329) | 27,75% (135/275) |
| Игровая логика | 0,22% (2/404) | 0,27% (2/406) |
| Исторические 37 SMO/SAN-классов | 59,05% (37/37) | 31,76% (35/37) |
| **Новый рабочий контур SMO/SAN с зависимостями** | **40,72% (175/276)** | **31,16% (135/245)** |

PC numerator: **112,38 эквивалента полностью исследованного класса / 276**.
PS2: **76,35 / 245**. Все классы имеют вес 1; повторов между группами нет.
В PC-контуре 101 класс ещё без отдельной platform assessment, в PS2 — 110.
Они остаются в знаменателе и не получают незаслуженный зачёт.
Узкая старая оценка PC 59,05% не изменилась: изменился учитываемый объём.

Это экспертные оценки **учтённого поведения классов**, не доля инструкций,
строк исходника, времени, методов или готовности приложений. Две цифры после
запятой — результат арифметики, не такая же точность исследования. Наличие
регистрации/имени само по себе не приносит behavioral score. UNRATED означает
отсутствие достаточной раздельной записи, а не доказанное отсутствие старых знаний.
Особенно это важно для игры: старый смешанный ориентир 2,5% нельзя автоматически
разделить между PC и PS2 или заменить двумя копиями 2,5%.

## Почему изменились старые общие цифры

Прежняя independent ledger не полностью учитывала исследования до её создания:
в ней отсутствовали даже отдельные оценки spBaseObject, потоков и буферов.
Перепроверены существующие карточки с раздельными адресами и платформенными
оговорками. Добавлено **182 оценки: 75 PC и 107 PS2** (в том числе уточнения
старых PS2 wire baselines). Существующие PC class scores не повышались.

Общий PC зачёт стал 15,45% вместо 9,28%; engine 34,14% вместо 20,68%.
PS2 стал 11,37% / 27,75% вместо 1,67% / 4,13%.
**Этот прирост — исправление учёта старой работы, не сегодняшние новые открытия.**
Никакие PC runtime tests не превращены в PS2 evidence.

[Неизменяемый manifest оценок](../../research/native-platform-accounting-audit-2026-09-06.json)
содержит отдельные score/bounds/status, hash карточки, платформу, locator и
оставшиеся неизвестные для каждой записи. Новые оценки консервативные:
35–55 — частичный behavioral срез; 60–80 — существенная локальная часть с
открытой интеграцией; 85–90 — глубоко описанный небольшой контракт с остатками.
Это экспертная шкала, не подсчёт одинакового количества функций в разных классах.
Старые более детальные оценки сохранены. Границы не являются статистическими
доверительными интервалами.

## Что входит в новый знаменатель

[Версия v1](../../research/native-goal-smo-san-scope-v1.json) фиксирует **308
различных зарегистрированных классов объединения платформ**, из них 276 PC
(271 engine + 5 game boundary) и 245 PS2 (240 engine + 5 game boundary).

Включены все прежние 37 классов, все class dependencies текущей workbench-очереди,
общие базы, serializers, streams, material/texture/animation/scene/backend families,
а также явно названные, ещё непроверенные consumers/producer candidates.
Это консервативный **рабочий контур для исследования**, а не уже доказанный
полный граф вызовов: присутствие класса в семье/платформе ещё не доказывает, что
он вызывается данным SMO. Такое уточнение входит в обязательный inventory gate.

| Группа | PC | PS2 |
|---|---:|---:|
| Базовые объекты, RTTI и владение | 67,11% (9/9) | 74,44% (9/9) |
| Потоки, пакеты и ошибки | 65,42% (10/12) | 70,38% (12/13) |
| Общая загрузка и сохранение | 80,50% (4/4) | 72,50% (4/4) |
| Геометрия, буферы и оптимизация | 59,13% (22/24) | 53,33% (12/15) |
| Материалы, текстуры и слои | 43,68% (24/38) | 32,65% (19/34) |
| SAN, скелет, контроллеры и вычислители | 47,75% (22/32) | 16,09% (10/32) |
| Сцена, модели, свет и камеры | 69,33% (22/24) | 48,75% (19/24) |
| Размещение, видимость, collision и navigation | 26,72% (23/47) | 13,62% (21/47) |
| Рендер, shaders и render targets | 20,90% (12/30) | 39,17% (8/12) |
| Прочие наблюдавшиеся SMO-объекты и их consumers | 17,63% (13/32) | 9,68% (8/31) |
| Граница кадра и жизненный цикл движка | 42,38% (5/8) | 28,75% (4/8) |
| Привязка ресурсов к уровню и шаблонам | 34,55% (7/11) | 37,73% (7/11) |
| Игровая граница загрузки и кадра | 18,00% (2/5) | 22,00% (2/5) |

Проценты групп тоже взвешены по всем классам группы, включая unrated.
Нельзя усреднять эти 13 процентов без учёта размеров групп.

Дополнительно, без выдумывания RTTI-классов, зарегистрированы шесть обязательств:
FAT helper, spDataBlockSerializer, spDXMeshCombiner, контейнеры/строки/allocator,
plane storage/clipping/protected dispatch, conversion/missing mips/library boundary.
Их нельзя спрятать за хорошим средним по классам: они входят в gates ниже.

Не входят просто по соседству: audio, input, network, unrelated game logic,
реализация сторонних Windows/Direct3D/CRT. Доказанная необходимая SMO/SAN
зависимость добавляется новой версией контура; внешний API-контракт остаётся
предметом исследования даже когда реализация сторонней библиотеки вне цели.

## Когда заканчиваем

**Остановка разрешена только при выполнении обоих условий:**

1. В актуальной версии контура все необходимые PC-классы имеют reviewed score
   100 и status closed; все candidates классифицированы по доказательствам.
   Неизвестное исходное имя само по себе не блокирует закрытое поведение, но
   неизвестный необходимый алгоритм/ветвь не может быть назван closed.
2. Пройдены **все 7 обязательных критериев** из
   [контракта завершения](pc-smo-san-completion-contract.md), включая
   незарегистрированные helpers и сквозную интеграцию.

| Критерий | Текущий статус |
|---|---|
| Полнота инвентаризации и замыкание зависимостей | Частично |
| Полная загрузка реальных PC SMO/SAN | Частично |
| Полное нативное сохранение с проверкой оригинальным загрузчиком | Частично |
| Обработка: animation, skin, scene, visibility, materials и прочие consumers | Частично |
| Lifetime, несколько ресурсов, aliases/reload/clone/rebind и отказы | Частично |
| Полная воспроизводимая native/source сверка | Частично |
| Сквозной ресурс → сцена → реальный PC backend | Не пройден |

**Полностью закрыто 0/7 крупных критериев; 6/7 имеют частичные результаты.**
Это не «сделано 0% работы»: каждый критерий охватывает всю цепочку, а не один
успешный тест. Не выводим ложные 6/7 = 85,7% готовности из шести partial статусов.
Последняя завершённая CP18 сборка прошла 32/32 CTest suites; она не доказывает
закрытия всех ветвей и не включает незавершённые CP19 правки.
PS2 считается отдельно для информации; PS2 completion gate не является условием
остановки текущего **PC** этапа.

Состав нельзя молча менять для роста процента. Новая зависимость или доказанное
исключение требует новой версии scope и записи delta/основания в журнал.
Отчёт обязан показывать, что именно изменилось: scope, migration или new research.
40,72% не означает, что осталось 59,28% прежнего количества часов.

## База и воспроизведение

Новый основной отчёт, вычисляемый **заново из актуальных class assessments**:

```powershell
python research/native_goal_coverage.py report
python research/native_goal_coverage.py report --json --details
python research/native_goal_coverage.py record
```

Команда report открывает SQLite только для чтения. Никакие исполняемые файлы,
игровые ресурсы или GPU при расчёте не открываются. record сохраняет проверяемый
snapshot; повтор с теми же входными данными идемпотентен.

Добавлены companion tables native_goal_scopes, native_goal_members,
native_goal_snapshots. Старые mixed и independent snapshots сохранены.
Manifest scope immutable по hash, original corpus schema version не меняется.
Snapshot fingerprint учитывает оценки, текущий каталог, direct scopes и scope
manifest: смена знаменателя не теряется даже без нового class score.
Отдельный assessmentLedgerSha256 позволяет отличить её от изменения оценок.

Unit tests проверяют независимость платформ, fresh recalculation, unrated в
знаменателе, отсутствие двойного зачёта, сохранение всех direct classes,
immutable imports, оба условия завершения, обязательные helpers и snapshots
при изменении не только score, но и каталога.

## Матрица повторной оценки карточек

Числа ниже — **внесённые этим аудитом** platform scores. «-» означает, что этот
аудит не изменял соответствующую запись, а не нулевое знание/отсутствие класса.
Точные сведения и оговорки приведены в manifest и карточках.

| Класс | PC | PS2 | Основание |
|---|---:|---:|---|
| `spBaseObject` | 85 | 90 | [карточка](native-class-sp-base-object.md) |
| `spNamedObject` | 80 | 85 | [карточка](native-class-sp-base-object.md) |
| `spPropertySystem` | 35 | 65 | [карточка](native-class-sp-base-object.md) |
| `spCloneManager` | - | 65 | [карточка](native-class-sp-base-object.md) |
| `spRTTIManager` | - | 65 | [карточка](native-class-sp-base-object.md) |
| `spMemoryStream` | 85 | 85 | [карточка](native-class-sp-memory-stream.md) |
| `spFileStream` | 90 | 90 | [карточка](native-class-sp-file-stream.md) |
| `spPCFileStream` | 85 | - | [карточка](native-class-sp-pc-file-stream.md) |
| `spPCKManager` | 75 | 80 | [карточка](native-class-sp-pck-manager.md) |
| `spAsyncFileStreamManager` | 80 | 80 | [карточка](native-class-sp-async-file-stream-manager.md) |
| `spPCAsyncFileStreamManager` | 80 | - | [карточка](native-class-sp-pc-async-file-stream-manager.md) |
| `spPS2AsyncFileStreamManager` | - | 65 | [карточка](native-class-sp-ps2-async-file-stream-manager.md) |
| `spPS2FileStream` | - | 65 | [карточка](native-class-sp-ps2-file-stream.md) |
| `spPS2Helper` | - | 65 | [карточка](native-class-sp-ps2-helper.md) |
| `spPS2IOPModuleManager` | - | 75 | [карточка](native-class-sp-ps2-iop-module-manager.md) |
| `spApp` | 65 | 70 | [карточка](native-class-sp-app.md) |
| `spPS2App` | - | 70 | [карточка](native-class-sp-ps2-app.md) |
| `wxPCApp` | 45 | - | [карточка](native-class-wx-pc-app.md) |
| `wxPS2App` | - | 55 | [карточка](native-class-wx-ps2-app.md) |
| `wxEngineCore` | 45 | 55 | [карточка](native-class-wx-engine-core.md) |
| `spError` | 65 | 75 | [карточка](native-class-sp-error.md) |
| `spErrorManager` | 65 | 65 | [карточка](native-class-sp-error-manager.md) |
| `spPCErrorManager` | 75 | - | [карточка](native-class-sp-pc-error-manager.md) |
| `spPS2ErrorManager` | - | 80 | [карточка](native-class-sp-ps2-error-manager.md) |
| `spSubscriptionManager` | 65 | 75 | [карточка](native-class-sp-subscription-manager.md) |
| `spFontManager` | 65 | 70 | [карточка](native-class-sp-font-manager.md) |
| `spPCFontManager` | 60 | - | [карточка](native-class-sp-pc-font-manager.md) |
| `spPS2FontManager` | - | 80 | [карточка](native-class-sp-ps2-font-manager.md) |
| `spInputManager` | 35 | 50 | [карточка](native-class-sp-input-manager.md) |
| `spDXInputManager` | 50 | - | [карточка](native-class-sp-dx-input-manager.md) |
| `spPS2InputManager` | - | 55 | [карточка](native-class-sp-ps2-input-manager.md) |
| `spDebugManager` | 35 | 35 | [карточка](native-class-sp-debug-manager.md) |
| `spEntityManager` | 65 | 70 | [карточка](native-class-sp-entity-manager.md) |
| `spGameLevel` | 55 | 60 | [карточка](native-class-sp-game-level.md) |
| `spGameLevelSerializer` | 45 | 50 | [карточка](native-class-sp-game-level-serializer.md) |
| `spTemplateObject` | 55 | 60 | [карточка](native-class-sp-template-object.md) |
| `spTemplateInstance` | 50 | 55 | [карточка](native-class-sp-template-instance.md) |
| `spTemplateManager` | 65 | 70 | [карточка](native-class-sp-template-manager.md) |
| `spTemplateSerializer` | 45 | 50 | [карточка](native-class-sp-template-serializer.md) |
| `spIndexBuffer` | 80 | 80 | [карточка](native-class-sp-index-buffer.md) |
| `spVertexBuffer` | 75 | 80 | [карточка](native-class-sp-vertex-buffer.md) |
| `spMesh` | 60 | 65 | [карточка](native-class-sp-mesh.md) |
| `spRenderMesh` | 65 | 80 | [карточка](native-class-sp-render-mesh.md) |
| `spPlatformSpecificMeshData` | 75 | 85 | [карточка](native-class-sp-platform-specific-mesh-data.md) |
| `spDXMeshData` | 55 | 65 | [карточка](native-class-sp-dx-mesh-data.md) |
| `spPS2MeshData` | 45 | 60 | [карточка](native-class-sp-ps2-mesh-data.md) |
| `spPS2Mesh` | - | 60 | [карточка](native-class-sp-ps2-mesh.md) |
| `spDXVertexBuffer` | 70 | - | [карточка](native-class-sp-dx-buffers.md) |
| `spDXIndexBuffer` | 70 | - | [карточка](native-class-sp-dx-buffers.md) |
| `spDXSharedMeshData` | 65 | - | [карточка](native-class-sp-dx-shared-mesh-data.md) |
| `spDXSharedMeshDataSerializer` | 65 | - | [карточка](native-class-sp-dx-shared-mesh-data.md) |
| `spDXMeshSerializer` | 65 | - | [карточка](native-class-sp-dx-mesh-serializer.md) |
| `spDXCombinedVB` | 45 | - | [карточка](native-class-sp-dx-combined-vb.md) |
| `spSceneGraphOptimizer` | 55 | - | [карточка](native-class-sp-scene-graph-optimizer.md) |
| `spDXSceneGraphOptimizer` | 35 | - | [карточка](native-class-sp-scene-graph-optimizer.md) |
| `spSerializerHook` | 75 | 80 | [карточка](native-class-sp-serializer-hook.md) |
| `spPS2SerializerHook` | - | 85 | [карточка](native-class-sp-serializer-hook.md) |
| `spPS2Material` | - | 70 | [карточка](native-class-sp-ps2-material.md) |
| `spPS2MaterialDataSerializer` | 60 | 65 | [карточка](native-class-sp-ps2-material-data-serializer.md) |
| `spPS2MeshDataSerializer` | 50 | 55 | [карточка](native-class-sp-ps2-mesh-data-serializer.md) |
| `spPS2TextureDataSerializer` | 50 | 55 | [карточка](native-class-sp-ps2-texture-data-serializer.md) |
| `spRenderTarget` | 60 | 65 | [карточка](native-class-sp-render-target.md) |
| `spCubeRenderTarget` | 60 | 65 | [карточка](native-class-sp-render-target.md) |
| `spDXRenderTarget` | 55 | - | [карточка](native-class-sp-render-target.md) |
| `spPCRenderTarget` | 60 | - | [карточка](native-class-sp-render-target.md) |
| `spDXCubeRenderTarget` | 60 | - | [карточка](native-class-sp-render-target.md) |
| `spPS2RenderTarget` | - | 65 | [карточка](native-class-sp-render-target.md) |
| `spPS2CubeRenderTarget` | - | 70 | [карточка](native-class-sp-render-target.md) |
| `spRenderTargetManager` | 60 | 60 | [карточка](native-class-sp-render-target.md) |
| `spPCRenderTargetManager` | 60 | - | [карточка](native-class-sp-render-target.md) |
| `spPS2RenderTargetManager` | - | 60 | [карточка](native-class-sp-render-target.md) |
| `spMaterialRenderTargetTexture` | 50 | 55 | [карточка](native-class-sp-material-render-target-texture.md) |
| `spMaterialCameraViewTexture` | 50 | 55 | [карточка](native-class-sp-material-render-target-texture.md) |
| `spMaterialCubeMapTexture` | 50 | 55 | [карточка](native-class-sp-material-render-target-texture.md) |
| `spMaterialPassLayer` | - | 55 | [карточка](native-class-sp-material-layers.md) |
| `spMaterialTextureLayer` | - | 50 | [карточка](native-class-sp-material-layers.md) |
| `spStdLayer` | - | 50 | [карточка](native-class-sp-material-layers.md) |
| `spAnimTexControllerSerializer` | 60 | 65 | [карточка](native-class-sp-anim-tex-controller-serializer.md) |
| `spUVControllerSerializer` | 55 | 60 | [карточка](native-class-sp-uv-controller-serializer.md) |
| `spMatColorControllerSerializer` | 55 | 60 | [карточка](native-class-sp-mat-color-controller-serializer.md) |
| `spLightControllerSerializer` | 60 | 65 | [карточка](native-class-sp-light-controller-serializer.md) |
| `spTransFunctionEvalSerializer` | 55 | 60 | [карточка](native-class-sp-trans-function-eval-serializer.md) |
| `spFunctionEvalSerializer` | 60 | 65 | [карточка](native-class-sp-function-eval-serializer.md) |
| `spColorFuncEvalSerializer` | 60 | 65 | [карточка](native-class-sp-color-func-eval-serializer.md) |
| `spCameraSerializer` | 55 | 65 | [карточка](native-class-sp-camera-serializer.md) |
| `spCameraDataSerializer` | 55 | 65 | [карточка](native-class-sp-camera-data-serializer.md) |
| `spLightSerializer` | 60 | 65 | [карточка](native-class-sp-light-serializer.md) |
| `spLightDataSerializer` | 60 | 65 | [карточка](native-class-sp-light-data-serializer.md) |
| `spSphereBVSerializer` | 60 | 65 | [карточка](native-class-sp-sphere-bv-serializer.md) |
| `spBoxBVSerializer` | 60 | 65 | [карточка](native-class-sp-box-bv-serializer.md) |
| `spOBBBVSerializer` | 55 | 60 | [карточка](native-class-sp-obb-bv-serializer.md) |
| `spSkinSerializer` | 80 | - | [карточка](native-class-sp-skin.md) |
| `spSubController` | 35 | - | [карточка](native-class-sp-controller.md) |
| `spTrack` | 90 | - | [карточка](native-class-sp-animation.md) |
| `spEngineCore` | - | 55 | [карточка](native-class-sp-engine-core.md) |
| `spResourceManager` | - | 55 | [карточка](native-class-sp-resource-manager.md) |
| `spSerializer` | - | 65 | [карточка](native-class-sp-serializer.md) |
| `spSerializerManager` | - | 60 | [карточка](native-class-sp-serializer-manager.md) |
| `spTexture` | - | 65 | [карточка](native-class-sp-texture.md) |
| `spTextureBuffer` | - | 70 | [карточка](native-class-sp-texture-buffer.md) |
| `spTextureData` | - | 60 | [карточка](native-class-sp-texture-data.md) |
| `spTextureDataSerializer` | - | 60 | [карточка](native-class-sp-texture-data-serializer.md) |
| `spDXTextureDataSerializer` | - | 45 | [карточка](native-class-sp-dx-texture-data-serializer.md) |
| `spMeshData` | - | 60 | [карточка](native-class-sp-mesh-data.md) |
| `spMeshDataSerializer` | - | 60 | [карточка](native-class-sp-mesh-data-serializer.md) |
| `spDXMeshDataSerializer` | - | 50 | [карточка](native-class-sp-dx-mesh-data-serializer.md) |
| `spMaterial` | - | 60 | [карточка](native-class-sp-material-runtime.md) |
| `spMaterialData` | - | 65 | [карточка](native-class-sp-material-runtime.md) |
| `spMaterialSerializer` | - | 60 | [карточка](native-class-sp-material-serializer.md) |
| `spMaterialDataSerializer` | - | 60 | [карточка](native-class-sp-material-data-serializer.md) |
| `spDXMaterialDataSerializer` | - | 55 | [карточка](native-class-sp-dx-material-data-serializer.md) |
| `spCamera` | - | 60 | [карточка](native-class-sp-camera.md) |
| `spCameraData` | - | 60 | [карточка](native-class-sp-camera.md) |
| `spPS2Camera` | - | 60 | [карточка](native-class-sp-camera.md) |
| `spFog` | - | 75 | [карточка](native-class-sp-fog.md) |
| `spFogSerializer` | - | 70 | [карточка](native-class-sp-fog-serializer.md) |
| `spLight` | - | 60 | [карточка](native-class-sp-light.md) |
| `spLightData` | - | 65 | [карточка](native-class-sp-light-data.md) |
| `spNode` | - | 60 | [карточка](native-class-sp-node.md) |
| `spNodeSerializer` | - | 60 | [карточка](native-class-sp-node-serializer.md) |
| `spRenderNode` | - | 55 | [карточка](native-class-sp-render-node.md) |
| `spRenderNodeSerializer` | - | 55 | [карточка](native-class-sp-render-node-serializer.md) |
| `spRenderable` | - | 60 | [карточка](native-class-sp-renderable.md) |
| `spRenderableSerializer` | - | 55 | [карточка](native-class-sp-renderable-serializer.md) |
| `spModel` | - | 60 | [карточка](native-class-sp-model.md) |
| `spModelSerializer` | - | 55 | [карточка](native-class-sp-model-serializer.md) |
| `spRenderer` | - | 45 | [карточка](native-class-sp-renderer.md) |
| `spPCRenderer` | 35 | - | [карточка](native-class-sp-renderer.md) |
| `spPS2Renderer` | - | 40 | [карточка](native-class-sp-renderer.md) |
