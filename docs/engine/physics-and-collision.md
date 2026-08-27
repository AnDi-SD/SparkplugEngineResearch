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

Поэтому физический collider не является дочерней частью визуального
`spStaticRenderObject`. Близость в object table или общий partition node также не
доказывают связь между конкретной моделью и collider.

`spCollisionInfo` сериализует:

1. relationship к primitive bounding volume;
2. collision group (`UInt32`);
3. `esfCollisionInfoTransform`, 40 байт:
   `Vector3 position`, `Quaternion XYZW`, `Vector3 scale`.

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
| `0x312FABC0` | `spCapsuleBV` |
| `0x1BCC5322` | `spConvexBV` |
| `0x21CC76AF` | `spBoundingVolume` |

Пары ID/имя подтверждены class registration как в PC executable, так и в PS2 ELF.
`spMeshBV` содержит индексированные треугольники и вершины. В `Alfea02.smo` все 132
сериализованные collision primitive являются `spMeshBV`; box primitive в этом
уровне отсутствуют.

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
