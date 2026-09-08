# Wire layout `spParticleSystem`

Нативное продолжение: [CP120 — параметры, writer и whole pickup scene](native-pc-particle-parameters.md).
Указанная ниже обязательность fields относится к исследованным authored файлам.
Оригинальный writer пропускает default scale и нулевой render node; это отдельно
подтверждено исполнением, как и допуск0,001 при сравнении векторов с default.
В [CP121](native-pc-particle-sampling.md) выполнены все7 native sampler functions;
по writer diagnostics и вычислениям исправлены прежние ошибочные метки порядка
height/radii у cylinder/cone. Это PC runtime evidence, не новый PS2 runtime test.

## Layout

`spParticleSystem` (`0x5AFA1A4F`) наследует `spRenderable`. Строго декодированы
1 745 объектов: 619/619/507. Inherited fields — material, fog и совместно
опциональные `UInt32` alpha-sort/priority. Собственная секция полностью покрывает
fields 0..19:

| Field | Значение |
|---:|---|
| 0 | две `Vector3`: диапазон acceleration |
| 1 | optional `Vector3` emission direction |
| 2, 3, 4 | пары `Single`: velocity, angle, scale; scale обязательна |
| 5 | два ARGB `UInt32` |
| 6 | пара `Single`: emission time/lifetime |
| 7, 8, 9 | однобайтовые loop, world-space, iterative |
| 10 | `Single` emission rate |
| 11 | обязательный `Single` bounding sphere radius |
| 12..18 | ровно один emission-region variant |
| 19 | обязательная relationship на `spRenderNode` |

## Семь подвидов emission region

| Field | Вид | Payload | Объектов во всех корпусах |
|---:|---|---|---:|
| 12 | point | `Vector3 position` | 480 |
| 13 | plane | `Vector3 position, normal; Single sizeX, sizeY` | 117 |
| 14 | box | `Vector3 position, size` | 15 |
| 15 | sphere | `Vector3 position; Single radius` | 646 |
| 16 | disk | `Vector3 position; Single radius` | 110 |
| 17 | cylinder | `Vector3 position; Single height, radius` | 5 |
| 18 | cone | `Vector3 position; Single height, radius1, radius2` | 372 |

Порядок и ширина членов подтверждены не только корпусом, но и именами/getter
assertions в PC и PS2 executable. Все PC working/pristine объекты совпадают
побайтно. Для 507 PC/PS2-пар совпадают 455 полных field-semantic signatures и
304 сериализованных объекта; остальные различия относятся к платформенным
вложенным ресурсам и authored данным.

Viewer показывает inherited renderable, все ranges/flags, конкретный region и
render node. Изменение оставлено read-only до реализации coordinated ownership и
валидации зависимых material/render-node данных.

Binary layout всех семи regions закрыт; открыты simulation equations и
взаимодействие lifetime/emission/ranges/flags. Они проверяются одиночными
fixed-size изменениями по
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

Воспроизводимый отчёт: [`analyze_smo_particle_system.py`](../../research/analyze_smo_particle_system.py).
