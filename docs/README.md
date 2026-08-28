# Документация

Этот каталог хранит общую исследовательскую модель Sparkplug. Детали конкретной реализации остаются рядом с кодом инструментов, а здесь фиксируются выводы, которые полезны обоим проектам.

## Навигация

- [Обзор движка и границы исследования](engine/overview.md)
- [Свидетельства из PC/PS2 executable и модов](engine/executable-evidence.md)
- [Физика и коллизии Sparkplug](engine/physics-and-collision.md)
- [Разрешение экрана, камеры и GUI в Winx Club PC](engine/display-resolution-camera-gui.md)
- [Поиск и загрузка ресурсов Winx Club PC](engine/resource-loading.md)
- [Контейнер и объектный граф SMO](formats/smo.md)
- [Подсистема чтения и изменения полей SMO](formats/smo-field-mutation.md)
- [Standalone-текстуры STX и их PC-диалекты](formats/stx.md)
- [Наблюдения по PC и PS2](platforms/pc-vs-ps2.md)
- [Подтверждённые class ID](reference/class-ids.md)
- [Все классы объектов SMO в игровом корпусе](reference/smo-object-types.md)
- [SQLite-индекс игрового корпуса SMO](research/smo-corpus-database.md)
- [Итог завершённого разбора 36 классов SMO](research/smo-class-analysis-plan.md)
- [План быстрой runtime-валидации через debug menu и точка возобновления](research/smo-runtime-validation-plan.md)
- [Фактический инвентарь debug menu и native level matrix PC/PS2](research/winx-debug-runtime-inventory.md)
- [Реестр выполненных runtime-экспериментов SMO](research/smo-runtime-results.md)
- [Актуальные открытые вопросы SMO](../research/open-questions.md)
- [Политика локального корпуса](research/corpus-policy.md)
- [Неизвестные классы Алфеи и привязка анимаций](research/alfea-unknown-resources.md)
- [Полный read-only разбор spMaterialColorController](research/smo-class-sp-material-color-controller.md)
- [Полный read-only разбор spFog](research/smo-class-sp-fog.md)
- [Полный read-only разбор spOBBBV](research/smo-class-sp-obbbv.md)
- [Полный read-only разбор spUVController](research/smo-class-sp-uv-controller.md)
- [Полный read-only разбор spLightData](research/smo-class-sp-light-data.md)
- [Полный read-only разбор spBoxBV](research/smo-class-sp-box-bv.md)
- [Полный read-only разбор spSphereBV](research/smo-class-sp-sphere-bv.md)
- [Полный read-only разбор spNode](research/smo-class-sp-node.md)
- [Полный read-only разбор spRenderNode](research/smo-class-sp-render-node.md)
- [Полный структурный read-only разбор spTextureData](research/smo-class-sp-texture-data.md)
- [Полный структурный read-only разбор spMaterialData](research/smo-class-sp-material-data.md)
- [Полный структурный read-only разбор spMeshData](research/smo-class-sp-mesh-data.md)
- [Полный read-only разбор spModel](research/smo-class-sp-model.md)
- [Полный разбор spStaticRenderObject](research/smo-class-sp-static-render-object.md)
- [Полный read-only разбор spSkin](research/smo-class-sp-skin.md)
- [Полный read-only разбор spCollisionInfo](research/smo-class-sp-collision-info.md)
- [Полный read-only разбор spMeshBV и wxFaceData](research/smo-class-sp-mesh-bv.md)
- [Полный read-only разбор spPartitionRenderable](research/smo-class-sp-partition-renderable.md)
- [Полный read-only разбор spPartitionNode](research/smo-class-sp-partition-node.md)
- [Полный read-only разбор spOctreeNode](research/smo-class-sp-octree-node.md)
- [Полный read-only разбор spPartitionSystem](research/smo-class-sp-partition-system.md)
- [Полный read-only разбор spZone](research/smo-class-sp-zone.md)
- [Полный read-only разбор spZonePortal](research/smo-class-sp-zone-portal.md)
- [Полный read-only разбор spZonePortalNode](research/smo-class-sp-zone-portal-node.md)
- [Полный read-only разбор spBSPNode](research/smo-class-sp-bsp-node.md)
- [Полный read-only разбор spOcclusionVolume](research/smo-class-sp-occlusion-volume.md)
- [Полный read-only разбор spMeshNavigationSet](research/smo-class-sp-mesh-navigation-set.md)
- [Полный read-only разбор spNavigationPortal](research/smo-class-sp-navigation-portal.md)
- [Полный read-only разбор spNavigationGraph](research/smo-class-sp-navigation-graph.md)
- [Полный read-only разбор spSkyBox](research/smo-class-sp-sky-box.md)
- [Полный read-only разбор spParticleSystem](research/smo-class-sp-particle-system.md)
- [Полный read-only разбор spAnimTexController](research/smo-class-sp-anim-tex-controller.md)
- [Полный read-only разбор spLensFlare](research/smo-class-sp-lens-flare.md)
- [Полный read-only разбор spFont](research/smo-class-sp-font.md)
- [Полный read-only разбор spTextRenderable](research/smo-class-sp-text-renderable.md)
- [Полный read-only разбор spTextNode](research/smo-class-sp-text-node.md)
- [Дорожная карта](../ROADMAP.md)
- [План доработки SmoLVLcreator после 0.1.0](../tools/SmoLVLcreator/ROADMAP.md)
- [Дневник](../journal/README.md)
- [Нативная проверка SMO кодом Winx Club](../journal/2026/2026-08-14-native-smo-validator.md)
- [Release candidates SmoViewer 0.4, SmoImporter 0.3 и SMOTextureTool 2.1](../journal/2026/2026-08-14-release-candidates.md)

## Статусы утверждений

Каждое существенное утверждение должно попадать в одну из категорий:

| Статус | Требование |
|---|---|
| Подтверждено | Воспроизводимый разбор/эксперимент и явно указанная область данных |
| Рабочая гипотеза | Есть объяснение наблюдений, но недостаточно независимых проверок |
| Открытый вопрос | Не хватает структуры, чистого примера или runtime evidence |

Числа из corpus scan всегда сопровождаются замечанием о происхождении корпуса. Наблюдение на изменённой папке игры нельзя автоматически считать свойством исходного формата.

## Где хранить подробности

- Общие выводы о движке и связях форматов — здесь.
- Байтовые структуры, которые реализует строгий parser, — здесь и в [`SmoViewer/docs`](../tools/SmoViewer/docs/SMO_FORMAT.md).
- История неподтверждённого texture repack — в [`SMOTextureTool/docs`](../tools/SMOTextureTool/docs/SMO_FORMAT.md); writer отключён после игровых crash.
- Хронология и отрицательные результаты — в `journal/`.
- Вопросы с проверяемым условием завершения — в `research/open-questions.md`, а
  быстрые игровые эксперименты — в `docs/research/smo-runtime-validation-plan.md`.

Документация обновляется вместе с изменением понимания формата. Старые ошибочные гипотезы не маскируются: причина пересмотра записывается в дневнике.
