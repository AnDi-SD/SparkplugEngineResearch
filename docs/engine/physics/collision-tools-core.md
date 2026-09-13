# CollisionInfo и OBB: используемая инструментами часть PC

## Что восстановлено

| Класс | Нужная часть | Подтверждение PC |
| --- | --- | --- |
| `spBoundingVolume` | реальный RTTI-base, физический named prefix, локальная ограничивающая сфера | registration `6D48DD`; ctor `491E50 →416510` |
| `spOBBBV` | параметры OBB, defaults, клонирование, изменение PR CollisionInfo | factory `4879C0 →13C7350`, ctor `4FDF30`, clone `487A20`, update `486930` |
| `spOBBBVSerializer` | полноценный reader/writer известных полей | `439BA0 /439E70` |
| `spCollisionInfo` | primitive/group/PRS/sphere, владельцы, clone, нужные world effects | ctor `465050 →4742A0`, factory `4653A0`, copy `464EF0`, update `4651E0`, destructor `465130` |
| `spCollisionInfoSerializer` | primitive/group/transform, связи и запись | reader `438A80`, index `438A40`, writer `438E20` |
| `spNode` | присоединение/отсоединение/клонирование CollisionInfo; update перед детьми | `421ED0 /421690 /421F80 /421420` |

Идентификаторы: CollisionInfo `47A97C0E`, BoundingVolume `21CC76AF`, OBBBV
`4DA04889`, CollisionInfoSerializer `33380E8C`. PS2 registration/factory
`4802D0 /124B90` дополнительно подтверждают CollisionInfo identity и размер
`0x88`; PC размер `0x8C`. Эти размеры не выдаются за ABI современного C++.
