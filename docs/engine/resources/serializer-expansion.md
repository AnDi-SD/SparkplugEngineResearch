# Дополнение независимого учёта сериализаторов PC/PS2

## Какие группы добавлены

Все имена ниже имеют префикс `sp` и суффикс `Serializer`.

| Группа | Новые PC строки |
| --- | --- |
| Разбиение пространства и зоны | BSPNode,OctreeNode,PartitionNode,PartitionRenderable,PCPartitionRenderable,PartitionSystem,Zone,ZonePortal,ZonePortalNode |
| Проекции | Projection,BoxProjection,PyramidProjection |
| Геометрия/коллизии | ConvexBV,CollisionInfo |
| Сцены/эффекты | Cinematic,LensFlare |
| PC графические ресурсы | DXCubeTexture,DXShaderEffect,DXShadowMesh,DXShadowVolume |

## PC: двадцать полных concrete lifetimes

Все20 factory/getter/clone/delete-clone/delete-original прошли:
100 class operations. Каждый новый PC объект имеет allocation20 байт,
primary vptr0 и secondary vptr10. Copy slot всех20 — существующий
no-payload40ECE0. Clone создаёт **отдельный concrete serializer**, регистрирует
пару и использует этот copy. Это не клонирование целевого графа объектов.
Остаточные allocations общего окружения записаны отдельно; полного shutdown
всех singleton’ов результат не утверждает.
