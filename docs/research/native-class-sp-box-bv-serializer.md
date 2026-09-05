# `spBoxBVSerializer`

Статус: подтверждены identity, RTTI/lifetime, раздельный PC/PS2 ABI, два поля,
epsilon позиции и post-read вычисление half-extents/bounding sphere.

Обе сборки регистрируют `spBoxBVSerializer` с Class ID `0x48E43495` и direct
base `spSerializer` (`0x42429877`). Аналитический target — corpus-класс
`spBoxBV` (`0x7B4C0876`); отдельного target-ID hook в native vtable нет. Exact
PS2 и observed PC размер serializer-а — `0x14`.

| ID | Имя | Запись | Target |
|---:|---|---|---:|
| 0 | `esfBoxBVPosition` | за epsilon `0.001` | source `+0x28`, read mirror `+0x18` |
| 1 | `esfBoxBVSize` | всегда | full size `+0x34` |

После чтения size native-код вычисляет `halfExtents = size * 0.5` и сохраняет
`length(halfExtents)` как базовый bounding-sphere radius по `+0x24`. Portable
модель воспроизводит это детерминированное вычисление, но не объявляет остальной
target layout известным.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer/call `0x006D2DC0/0x006D2DE0`, record `0x0075E888`;
- getter `0x004392F0`, protected factory `0x00439320`, destructor
  `0x00439300`, clone `0x00439390`, deleting destructor `0x004393E0`;
- reader vtable entry `0x00439470` (body `0x00439480`), writer entry
  `0x004396B0` (body `0x004396C0`), finalize `0x005A7DB0`;
- primary/interface vtables `0x006DF274/0x006DF268`;
- class string `0x006DF36C`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x004832D0`, record `0x004AA1B0`;
- getter `0x00188840`, reader `0x00188850`, finalize `0x00188A40`, writer
  `0x00188A50`;
- deleting destructor `0x00188C60`, clone `0x00188CD0`, factory
  `0x00188DB0`, allocation `0x14`;
- thunks `0x00188EB0/0x00188EC0/0x00188ED0`;
- primary/interface vtable headers `0x0048F350/0x0048F374`;
- class string `0x0044C580`.

## Неизвестное

- original header/source paths и method names;
- полный `spBoxBV` inheritance/layout и точное назначение mirror-полей;
- behavior для отрицательных/NaN/Inf компонентов size;
- момент пересчёта bounding sphere после runtime mutation;
- контролируемый in-game mutation test.

Следующий BV-кандидат — `spOBBBVSerializer`; quaternion и дополнительные derived
данные делают его немного сложнее, поэтому незавершённый анализ не смешивается с
готовыми Box/Sphere классами.
