# Физика и коллизии Sparkplug

Документ фиксирует подтверждённую часть collision pipeline Winx Club. Он отделяет
сериализованные данные уровня от runtime-объектов и от отладочной визуализации.

## Структура уровня

`spPartitionNode` содержит независимые отношения к нескольким категориям объектов:

- дочерним `spPartitionNode`;
- `spCollisionInfo`;
- `spStaticRenderObject`;
- `spZonePortal`;
- `spPartitionRenderable`.

Полный разбор уточняет эту схему: relationship `Child` отсутствует в 16 204
объектах точного класса `spPartitionNode`, но сериализован 18 048 раз в
унаследованных секциях 2 256 `spOctreeNode`. Он начинается с octant slot 0..7,
за которым идёт inline relationship. Ещё 648 таких полей находятся в 324
`spBSPNode`: slots 0/1 ведут на inline BSP split либо referenced terminal
`spPartitionNode`. Остальные 232 035 отношений ранее разобранных partition/octree
объектов строго разрешаются на ожидаемые
классы; inline-цели совпадают с 74 324 физическими children. Подробности:
[`../research/smo-class-sp-partition-node.md`](../research/smo-class-sp-partition-node.md).
Геометрия дерева и битовая нумерация октантов описаны в
[`../research/smo-class-sp-octree-node.md`](../research/smo-class-sp-octree-node.md).
Двоичные plane-деревья описаны в
[`../research/smo-class-sp-bsp-node.md`](../research/smo-class-sp-bsp-node.md).

Поэтому физический collider не является дочерней частью визуального
`spStaticRenderObject`. Близость в object table или общий partition node также не
доказывают связь между конкретной моделью и collider.

`spPartitionRenderable` является отдельным листом этой структуры: он хранит один
ARGB `DebugColor` и 1..67 inline physical-child `spModel`, но не хранит transform.
Во всех трёх корпусах строго декодированы 7 208 таких объектов и 19 989 model
relationships; ID-only/reference-only форм нет. Подробности:
[`../research/smo-class-sp-partition-renderable.md`](../research/smo-class-sp-partition-renderable.md).

`spCollisionInfo` сериализует:

1. relationship к primitive bounding volume;
2. collision group (`UInt32`);
3. `esfCollisionInfoTransform`, 40 байт:
   `Vector3 position`, `Quaternion XYZW`, `Vector3 scale`.

Во всех трёх корпусах строго декодированы 10 594 объекта. Наблюдаются три формы:
10 108 полных Primitive+Group+Transform, 476 Primitive+Group и 10 Primitive-only.
Group имеет только наблюдаемые значения 1 (4 271 объект) и 2 (6 313 объектов),
ещё в десяти старых объектах поле отсутствует. Отсутствие optional-полей хранится
явно и не подменяется недоказанным runtime-default. Полный отчёт:
[`../research/smo-class-sp-collision-info.md`](../research/smo-class-sp-collision-info.md).

Transform `spCollisionInfo` является transform самой физической формы. Для обычных
node-based объектов он часто совпадает с world transform родительского `spNode`, но
для форм внутри `spPartitionNode` именно это поле задаёт размещение. Использовать
только transform родителя нельзя.

## Подтверждённые primitive-классы

| Class ID | Имя |
|---:|---|
| `0x3F453DE7` | `spMeshBV` |
| `0x4DA04889` | `spOBBBV` |
| `0x7B4C0876` | `spBoxBV` |
| `0x390946D2` | `spSphereBV` |
| `0x312FABC0` | `spCapsuleBV` |
| `0x1BCC5322` | `spConvexBV` |
| `0x21CC76AF` | `spBoundingVolume` |

Пары ID/имя подтверждены class registration как в PC executable, так и в PS2 ELF.
В самом корпусе Primitive target ограничен четырьмя конкретными классами:
10 162 `spMeshBV`, 423 `spOBBBV`, шесть `spBoxBV` и три `spSphereBV`.
`spCapsuleBV`, `spConvexBV` и точный `spBoundingVolume` зарегистрированы и
допустимы по базовому типу serializer, но сериализованных объектов этих классов
в исследуемых SMO нет.
`spMeshBV` полностью разобран как общий PC/PS2 triangle-list формат. Field 0
содержит version 2, `UInt16` indices и `Vector3` positions. Необязательный field
1 — массив `wxFaceData` класса `0x313C4C17`, по одной разреженной записи на
треугольник: `UInt8 surface type`, `UInt16 flags`, `UInt8 surface ID`.
Восстановлены surface type `stone`, `dirt`, `grass`, `water`, `snow`, `swamp`,
`mud`, `deepwater`, `carpet`; семантика битов flags и surface ID пока остаётся
неизвестной. Все 10 513 объектов трёх корпусов строго декодируются. См.
[`../research/smo-class-sp-mesh-bv.md`](../research/smo-class-sp-mesh-bv.md).

В `Alfea02.smo` все 132 сериализованные collision primitive являются
`spMeshBV`; box primitive в этом уровне отсутствуют.

`spOBBBV` полностью разобран отдельно: собственный field 1 содержит полный размер
box, из которого runtime получает half-extents умножением на `0.5`. Необязательные
field 0/2 задают локальные position/quaternion rotation относительно transform
родительского `spCollisionInfo`; в доступном корпусе они всегда опущены. См.
[`../research/smo-class-sp-obbbv.md`](../research/smo-class-sp-obbbv.md).

`spBoxBV` и `spSphereBV` также полностью декодируются в read-only режиме. Box
хранит full size, sphere — один `Single radius`; оба имеют optional local
position. См. [`spBoxBV`](../research/smo-class-sp-box-bv.md) и
[`spSphereBV`](../research/smo-class-sp-sphere-bv.md).

## Runtime collision manager

PS2 ELF сохраняет доступный для анализа код `spCollisionManager`. Узкая фаза
принимает два `spCollisionInfo`, проверяет enable/group state и выбирает функцию
теста по двум primitive type через таблицу 7×7. Mesh является отдельным случаем;
остальные пары проходят через таблицу функций.

Отдельная функция рекурсивно обходит collision-списки `spPartitionNode` и рисует
`spMeshBV`, `spOBBBV` и `spBoxBV`. Это debug draw, а не collision query; смешивать
его с narrow phase нельзя.

## Проверка стола в Alfea02

При сохранении редактор переместил три визуальных placement:

- `[2722] funkytableNOSH`;
- `[2726] mags`;
- `[2760] funkytable`.

Их общий исходный visual bounds:

```text
X -5437.95 .. -5317.09
Y   608.51 ..   659.37
Z -3045.69 .. -2926.67
```

Отдельная физическая форма `[1328] spCollisionInfo / [1329] spMeshBV` имеет 16
треугольников и bounds:

```text
X -5438.72 .. -5315.62
Y   606.93 ..   660.64
Z -3051.90 .. -2919.20
```

Её верхние треугольники находятся на `Y=660.64`, то есть форма обхватывает исходный
стол. В изменённом файле байты всех 132 `spMeshBV` остались неизменны, поэтому
визуальный стол переместился, а collider остался на старом месте. Это объясняет и
невозможность запрыгнуть на стол в новой позиции, и невидимое препятствие в старой.

## Следствия для редактора

- collision overlay обязан использовать собственный transform `spCollisionInfo`;
- визуальные placement и collision shape должны оставаться отдельными сущностями;
- автоматическую связь можно только предлагать по совпадению bounds/поверхностей,
  но пользователь должен видеть и подтверждать её;
- команда совместного переноса должна изменять оба объекта одной транзакцией;
- affine-редактирование collision shape сохраняется запеканием подтверждённой
  world-delta в её локальные вершины при неизменном 40-байтовом
  `spCollisionInfoTransform`; это покрывает translation, rotation и scale без
  изменения структуры контейнера.

В `SmoLVLcreator.Core` это реализовано как неразрушающий
`SmoLevelCollisionLink`: исходные сущности не сливаются, а высокоуверенное
совпадение вычисляется по world-space bounds внутри одного `spPartitionNode`.
Обычный режим `MOVE LINKED` расширяет одну transform-сессию на обе стороны связи;
при отключении режима исходный visual или collider остаётся доступен для
независимого перемещения. Инспектор показывает найденную пару и confidence.

## Воспроизводимые инструменты

- `research/inspect_pe_physics.py` — class registration и xref в PC PE;
- `research/inspect_elf_physics.py` — class registration и MIPS disassembly PS2 ELF;
- `SmoViewer.Inspect diff` — сравнение placement, collision bytes и пространственной
  близости render/collision geometry.
