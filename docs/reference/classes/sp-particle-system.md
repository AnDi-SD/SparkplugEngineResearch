# spParticleSystem

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spParticleSystem](../../../Sparkplug/Code/Sparkplug/spParticleSystem.h).

## Layout

`spParticleSystem` (`0x5AFA1A4F`) наследует `spRenderable`. Строго декодированы
1 745 объектов: 619/619/507. Inherited fields — material, fog и совместно
опциональные `UInt32` alpha-sort/priority. Собственная секция полностью покрывает
fields 0..19:

| Field | Значение |
| ---: | --- |
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

| Field | Вид | Payload |
| ---: | --- | --- |
| 12 | point | `Vector3 position` |
| 13 | plane | `Vector3 position, normal; Single sizeX, sizeY` |
| 14 | box | `Vector3 position, size` |
| 15 | sphere | `Vector3 position; Single radius` |
| 16 | disk | `Vector3 position; Single radius` |
| 17 | cylinder | `Vector3 position; Single height, radius` |
| 18 | cone | `Vector3 position; Single height, radius1, radius2` |

Viewer показывает inherited renderable, все ranges/flags, конкретный region и
render node. Изменение оставлено read-only до реализации coordinated ownership и
валидации зависимых material/render-node данных.

Binary layout всех семи regions закрыт; открыты simulation equations и
взаимодействие lifetime/emission/ranges/flags. Они проверяются одиночными
fixed-size изменениями по
`smo-runtime-validation-plan.md`.
