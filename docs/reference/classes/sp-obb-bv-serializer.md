# spOBBBVSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spOBBBV](../../../Sparkplug/Code/Sparkplug/spOBBBV.h), [spOBBBVSerializer](../../../Sparkplug/Code/Sparkplug/spOBBBVSerializer.h), [spSerializer](../../../Sparkplug/Code/Sparkplug/spSerializer.h).

Статус: подтверждены identity, RTTI/lifetime, раздельный PC/PS2 ABI, три поля,
правила suppression и post-read обработка size/rotation.

| ID | Имя | Запись | Target |
| ---: | --- | --- | ---: |
| 0 | `esfOBBBVPosition` | за epsilon `0.001` | source `+0x4C`, mirror `+0x18` |
| 1 | `esfOBBBVSize` | всегда | full size `+0x58` |
| 2 | `esfOBBBVRotation` | если matrix не identity за epsilon | matrix `+0x28`, wire quaternion |

Reader сохраняет full size, вычисляет `halfExtents = size * 0.5` и
`boundingSphereRadius = length(halfExtents)`. Quaternion field 2 преобразуется
в runtime matrix по `+0x28`. Portable reconstruction моделирует доказанные
field order/suppression и derived size, но намеренно не подменяет неизвестную
оригинальную quaternion-to-matrix конвенцию своей реализацией.

## Неизвестное

- original header/source paths и method/type names;
- полный `spOBBBV` inheritance/layout и точное назначение mirror-полей;
- исходная matrix/quaternion конвенция, handedness и normalization policy;
- behavior для отрицательных/NaN/Inf size и неединичного quaternion;
- момент пересчёта derived bounds после runtime mutation;
- контролируемый in-game mutation test.

Следующий BV-кандидат — `spConvexBVSerializer`; его object construction и
relationship paths сложнее, поэтому в этот класс они не включены.
