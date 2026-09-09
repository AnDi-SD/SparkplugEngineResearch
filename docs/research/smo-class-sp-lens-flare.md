# `spLensFlare`: общий reader и уточнение структуры элементов

Обновление 9 сентября: прежняя трактовка field1 как одиночного optional record
была ошибкой анализа корпуса. Original-PC reader читает **число элементов,
затем массив записей**. Field0 меняет один primary element, а не добавляет
элемент. Восстановлены общий C++ reader и нужное владение actual spQuad;
старый C# parser удалён. [Доказательства и границы](tool-lens-flare-shared-core-2026-09-09.md).

`spLensFlare` (`0x435370B5`) наследует `spRenderable`. Исторический структурный
анализ охватил шесть объектов (2/2/2); это не новый whole-graph regression.
Уточнённая собственная секция:

| Field | Payload |
|---:|---|
| 0 | primary element: material relationship + ARGB + `Single relativeDistance, scale`; повторная запись заменяет значение, NULL material сохраняет его |
| 1 | `UInt32 count`, затем count записей material relationship + ARGB + `Single relativeDistance, scale` |
| 2 | `Single occlusionSphereRadius, occlusionSpeed` |
| 3 | relationship на `spRenderNode` |

В корпусе один общий variant: один inline-material element, белый цвет,
relative distance 0, scale 200, дополнительный массив пуст, radius 100 и speed 5. Все
nested materials и render-node relationships разрешены. Две PC-копии совпадают
побайтно; обе PC/PS2-пары совпадают логически, но не полными bytes из-за
платформенного содержимого inline material.

Field names, compound member getters и нулевой count независимо подтверждены
PC и PS2 executable. Новый Viewer projector требует actual loaded graph;
оба реальных уровня пока требуют OcclusionVolume. Запись остаётся
координированной задачей вместе с material и render-node ownership.

В корпусе нет непустого дополнительного массива. Свежие PC probes проверили
его чтение, ссылки и resize, но runtime composition остаётся непроверенной.
Authoring records и material relationships отложены до общего structural writer:
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

PC runtime checkpoint2026-09-06 закрыл exact-ID scene registration в
`spPCLensFlareManager`: borrowed intrusive flare links58/5C, removal перед
reparent, exact manager38 и отдельный capability probe устройства. Это не
полный glare/query render. [Нативные методы и границы](native-pc-scene-special-managers.md).
