# `spOBBBVSerializer`

Статус: подтверждены identity, RTTI/lifetime, раздельный PC/PS2 ABI, три поля,
правила suppression и post-read обработка size/rotation.

Обе сборки регистрируют `spOBBBVSerializer` с Class ID `0x68EA2ED1` и direct
base `spSerializer` (`0x42429877`). Аналитический target — corpus-класс
`spOBBBV` (`0x4DA04889`); отдельного target-ID hook в native vtable нет. Exact
PS2 и observed PC размер serializer-а — `0x14`.

| ID | Имя | Запись | Target |
|---:|---|---|---:|
| 0 | `esfOBBBVPosition` | за epsilon `0.001` | source `+0x4C`, mirror `+0x18` |
| 1 | `esfOBBBVSize` | всегда | full size `+0x58` |
| 2 | `esfOBBBVRotation` | если matrix не identity за epsilon | matrix `+0x28`, wire quaternion |

Reader сохраняет full size, вычисляет `halfExtents = size * 0.5` и
`boundingSphereRadius = length(halfExtents)`. Quaternion field 2 преобразуется
в runtime matrix по `+0x28`. Portable reconstruction моделирует доказанные
field order/suppression и derived size, но намеренно не подменяет неизвестную
оригинальную quaternion-to-matrix конвенцию своей реализацией.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer/call `0x006D2DF0/0x006D2E10`, record `0x0075E8E8`;
- getter `0x00439940`, protected factory `0x00439A50`, destructor
  `0x00439950`, clone `0x00439AC0`, deleting destructor `0x00439B10`;
- reader `0x00439BA0`, finalize `0x005A7DB0`, writer `0x00439E70`;
- primary/interface vtables `0x006DF390/0x006DF384`;
- class string `0x006DF4CC`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483390`, record `0x004AA2D0`;
- getter `0x00189950`, reader `0x00189960`, finalize `0x00189BB0`, writer
  `0x00189BC0`;
- deleting destructor `0x0018A040`, clone `0x0018A0B0`, factory
  `0x0018A190`, allocation `0x14`;
- thunks `0x0018A290/0x0018A2A0/0x0018A2B0`;
- primary/interface vtable headers `0x0048F440/0x0048F464`;
- class string `0x0044CB40`.

## Неизвестное

- original header/source paths и method/type names;
- полный `spOBBBV` inheritance/layout и точное назначение mirror-полей;
- исходная matrix/quaternion конвенция, handedness и normalization policy;
- behavior для отрицательных/NaN/Inf size и неединичного quaternion;
- момент пересчёта derived bounds после runtime mutation;
- контролируемый in-game mutation test.

Следующий BV-кандидат — `spConvexBVSerializer`; его object construction и
relationship paths сложнее, поэтому в этот класс они не включены.
