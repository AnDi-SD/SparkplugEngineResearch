# spBoxBVSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spBoxBV](../../../Sparkplug/Code/Sparkplug/spBoxBV.h), [spBoxBVSerializer](../../../Sparkplug/Code/Sparkplug/spBoxBVSerializer.h), [spSerializer](../../../Sparkplug/Code/Sparkplug/spSerializer.h).

Статус: подтверждены identity, RTTI/lifetime, раздельный PC/PS2 ABI, два поля,
epsilon позиции и post-read вычисление half-extents/bounding sphere.

| ID | Имя | Запись | Target |
| ---: | --- | --- | ---: |
| 0 | `esfBoxBVPosition` | за epsilon `0.001` | source `+0x28`, read mirror `+0x18` |
| 1 | `esfBoxBVSize` | всегда | full size `+0x34` |

После чтения size native-код вычисляет `halfExtents = size * 0.5` и сохраняет
`length(halfExtents)` как базовый bounding-sphere radius по `+0x24`. Portable
модель воспроизводит это детерминированное вычисление, но не объявляет остальной
target layout известным.

## Неизвестное

- original header/source paths и method names;
- полный `spBoxBV` inheritance/layout и точное назначение mirror-полей;
- behavior для отрицательных/NaN/Inf компонентов size;
- момент пересчёта bounding sphere после runtime mutation;
- контролируемый in-game mutation test.

Следующий BV-кандидат — `spOBBBVSerializer`; quaternion и дополнительные derived
данные делают его немного сложнее, поэтому незавершённый анализ не смешивается с
готовыми Box/Sphere классами.
