# spPartitionSystem

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPartitionSystem](../../../Sparkplug/Code/Sparkplug/spPartitionSystem.h).

## Итоговая структура

Отсутствующие node-поля используют уже подтверждённые defaults: position
`(0,0,0)`, identity rotation, scale `(1,1,1)`, `IsBone=false` и billboard axis
`0`. Renderable-список поддерживается унаследованным serializer, но во всех 88
системах пуст.

## Наблюдаемые отношения

| Семантика | PS2 | Всего | Encoding / target |
| --- | ---: | ---: | --- |
| owned zone | 30 | 88 | inline `spZone`, физический child системы |
| дополнительные zones | 93 | 279 | sized reference на `spZone` |
| portal nodes | 103 | 309 | inline `spZonePortalNode`, физический child |
| collisions | 1 549 | 4 647 | sized reference на `spCollisionInfo` |
| BSP roots | 18 | 54 | inline `spBSPNode`, физический child |
| octree roots | 12 | 34 | sized reference на `spOctreeNode` |

Field 5 node-секции имеет стабильный порядок: одна owned inline-зона, затем
ссылки на остальные зоны, затем inline portal nodes. После них идут все field 7
collision references. В каждом ресурсе присутствует ровно один именованный
`PartitionSystem`.

Inline BSP payload имеет размеры 210, 311, 412, 513, 614, 715 или 1 624 байта.
Octree reference всегда занимает 8 байт (`objectId`, нулевой inline size).
