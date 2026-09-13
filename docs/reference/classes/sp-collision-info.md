# spCollisionInfo

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spCollisionInfo](../../../Sparkplug/Code/Sparkplug/spCollisionInfo.h).

| Платформа | SMO | Размер объекта |
| --- | ---: | ---: |
| PC | 194 | 23–15 539 байт |
| PC | 194 | 23–15 539 байт |
| PS2 | 158 | 36–15 539 байт |
|  | **546** |  |

Все объекты безымянные. Анализатор заново прочитал исходные SMO и объекты
внутри PS2 PCK, строго декодировал 10 594/10 594 экземпляра и аннотировал
31 286 нетерминальных полей.

## Serializer и поля

Оба executable независимо содержат `spCollisionInfoSerializer` и три enum-имени:

| Field | Имя executable | Payload | Наблюдаемость |
| ---: | --- | --- | ---: |
| 0 | `esfCollisionInfoPrimitive` | relationship к `spBoundingVolume` | 10 594 |
| 1 | `esfCollisionInfoGroup` | `UInt32` | 10 584 |
| 2 | `esfCollisionInfoTransform` | position + rotation + scale, 40 байт | 10 108 |

PC reader в ветке field 0 передаёт relationship-loader базовый class ID
`0x21CC76AF` (`spBoundingVolume`). Для field 2 он последовательно читает
`Vector3 position`, quaternion rotation и `Vector3 scale`. PS2 MIPS-код повторяет
тот же switch, базовый class ID и порядок значений. Writer обеих платформ пишет
те же три поля в порядке Primitive, Group, Transform.

Объект имеет ровно одну serializer-секцию и один завершающий пустой field 0.
Наследуемых секций у `spCollisionInfo` нет.

## Три подтверждённые формы

| Вариант | Поля | PS2 pristine |
| --- | --- | ---: |
| `collision_info_full` | 0, 1, 2 | 3 306 |
| `collision_info_no_transform` | 0, 1 | 0 |
| `collision_info_primitive_only` | 0 | 2 |

Это варианты присутствия optional-полей, а не разные типы collider. Редкая
PC-форма без transform состоит из 236 inline `spMeshBV` под `spRenderNode` и
двух корневых ID-only ссылок на `spMeshBV`. Primitive-only форма на PC относится
к четырём inline `spOBBBV`; на PS2 — к одному `spBoxBV` и одному `spSphereBV`.

Отсутствие Group или Transform сохранено декодером явно как `null`. Мы пока не
подменяем его предполагаемым runtime-default: конструктор `spCollisionInfo` ещё
не восстановлен с достаточной уверенностью.

## Primitive relationship

Все 10 594 relationship разрешаются в реальный объект bounding volume.

| Target | Всего | PS2 pristine |
| --- | ---: | ---: |
| `spMeshBV` | 10 162 | 3 178 |
| `spOBBBV` | 423 | 127 |
| `spBoxBV` | 6 | 2 |
| `spSphereBV` | 3 | 1 |

`spCapsuleBV`, `spConvexBV` и сам `spBoundingVolume` зарегистрированы в обоих
executable и допустимы по базовому типу serializer, но сериализованных targets
этих трёх точных классов в исследуемых SMO нет.

## Collision group

| Значение | PS2 pristine | Всего |
| ---: | ---: | ---: |
| 1 | 1 405 | 4 271 |
| 2 | 1 901 | 6 313 |
| поле опущено | 2 | 10 |

Поле является обычным `UInt32`; значения 1 и 2 — единственные наблюдаемые, но
декодер намеренно не превращает их в неподтверждённый enum и не запрещает другие
32-битные значения.

## Transform

40-байтовый payload имеет точный layout:

```text
0x00  Vector3    world position
0x0C  Quaternion world rotation (X, Y, Z, W)
0x1C  Vector3    world scale
```

Viewer нормализует quaternion для построения матрицы, но отображает разложенные
position/rotation/scale. При отсутствии field 2 существующий collision overlay
использует transform родительского node; это поведение Viewer, а не доказанный
сериализованный default движка.

## Физическая топология

| Родитель | Всего |
| --- | ---: |
| `spRenderNode` | 5 511 |
| `spPartitionNode` | 4 647 |
| `spNode` | 432 |
| корневой объект | 4 |

У всех 10 590 inline-форм физический child совпадает с target Primitive.
Четыре корневые PC-копии относятся к двум ID-only объектам и child не имеют.
