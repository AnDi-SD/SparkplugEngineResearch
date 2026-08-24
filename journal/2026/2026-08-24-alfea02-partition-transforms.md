# Alfea02.smo: sector containment не является transform-иерархией

Дата: 2026-08-24

## Наблюдение

В Viewer мелкие объекты `Alfea02.smo` выглядели правдоподобно, но стены и другие
крупные части уровня были разнесены. Пользователь указал на `[4199]`, визуально
похожий на неверно повёрнутую или смещённую комнату.

## Образец

- путь: `Media/Levels/Alfea/Alfea02.smo`;
- SHA-256: `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF`;
- размер: 7 106 313 байт;
- objects: 4 266;
- meshes: 702/702 строго декодированы.

`[4199]` оказался `spModel dormBigroomNOSH-000`, а не mesh. Его `spMeshData`
находится в `[4201]`, содержит 371 вершину и 308 triangles. Local bounds уже
лежат в координатах уровня: `X=-5552.13...-3870.64`,
`Y=-0.05...1150.13`, `Z=-1125.93...501.16`.

## Причина

Каталожное containment сериализует partition примерно как цепочку
`sector01/portal/sector02/portal/sector03/portal/sector04`. Старый resolver
поднимался по `ParentIndex` и принимал совпадающие поля неизвестного sector-
класса за node transforms.

Из-за этого authored world placement `Darch_A01`
`(-4416.13, 0, -1812.98)` после сложения четырёх sector centers становился
`(-22283.88, 603.70, -9635.58)`. Baked room `[4201]` дополнительно получал
translation `sector01`, хотя его вершины уже находились в world coordinates.

Сопоставление всех восьми `sector*` подтвердило, что центры local baked bounds
близки к serialized sector positions. Эти поля относятся к partition/culling,
а не являются placement родителями геометрии.

## Исправление

`ResolveModelWorldMatrix` теперь:

- применяет local transform только для подтверждённых `spNode`, `spRenderNode`
  и `spModel`;
- после `spStaticRenderObject` прекращает подъём, потому что поле хранит готовую
  world-матрицу;
- игнорирует пространственно неподтверждённые sector/portal classes.

После исправления `[4201]` использует identity и сохраняет baked coordinates,
а `[1372] Darch_A01` получает ровно authored static matrix.

## Проверка

Corpus-регрессия закрепляет SHA-256, 4 266 объектов, 702 mesh, тип/имя `[4199]`,
identity для `[4201]` и точное равенство resolved/authored matrices для static
mesh `[1372]`. Прямой прогон `Alfea02.smo` проходит 5 349 assertions.

## Оставшаяся граница

Root-level prototype/gameplay assets без `spStaticRenderObject` всё ещё нужно
структурно отделить от намеренно baked world geometry. Исправление не назначает
им матрицы эвристически.
