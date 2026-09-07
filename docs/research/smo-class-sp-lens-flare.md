# Полный разбор `spLensFlare`

`spLensFlare` (`0x435370B5`) наследует `spRenderable`; строго декодированы все
шесть объектов (2/2/2). Собственная секция:

| Field | Payload |
|---:|---|
| 0 | repeated: material relationship + ARGB + `Single relativeDistance, scale` |
| 1 | glare: zero `UInt32` sentinel либо такая же element-запись |
| 2 | `Single occlusionSphereRadius, occlusionSpeed` |
| 3 | relationship на `spRenderNode` |

В корпусе один общий variant: один inline-material element, белый цвет,
relative distance 0, scale 200, glare отсутствует, radius 100 и speed 5. Все
nested materials и render-node relationships разрешены. Две PC-копии совпадают
побайтно; обе PC/PS2-пары совпадают логически, но не полными bytes из-за
платформенного содержимого inline material.

Field names, compound member getters и zero-glare форма независимо подтверждены
PC и PS2 executable. Viewer теперь выводит все значения; запись остаётся
координированной задачей вместе с material и render-node ownership.

В корпусе нет ненулевого glare и multi-element arrangement, поэтому runtime
composition остаётся непроверенной. Ближайший этап меняет только существующие
occlusion radius/speed; замена нулевого sentinel на compound glare требует новой
material relationship и отложена до structural writer:
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

PC runtime checkpoint2026-09-06 закрыл exact-ID scene registration в
`spPCLensFlareManager`: borrowed intrusive flare links58/5C, removal перед
reparent, exact manager38 и отдельный capability probe устройства. Это не
полный glare/query render. [Нативные методы и границы](native-pc-scene-special-managers.md).
