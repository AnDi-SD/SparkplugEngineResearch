# spBSPNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spBSPNode](../../../Sparkplug/Code/Sparkplug/spBSPNode.h).

## Назначение и наследование

`spBSPNode` — внутренний узел двоичного дерева пространственного разбиения. Он
наследует serializer `spPartitionNode`, добавляет разделяющую плоскость и имеет
ровно две ветви. Корень дерева физически принадлежит `spPartitionSystem`, а
вложенные split-узлы сериализуются inline и физически принадлежат родительскому
`spBSPNode`. Конечная ветвь является sized reference на уже существующий
`spPartitionNode`.

```text
spPartitionSystem.PartitionRoot
└─ inline spBSPNode
   ├─ branch slot 0: inline spBSPNode | reference spPartitionNode
   └─ branch slot 1: inline spBSPNode | reference spPartitionNode
```

## Serializer layout

Объект имеет две секции. Порядок полей унаследованной секции фиксирован:

```text
# section 0: spPartitionNode
field 1  DebugColor          UInt32 ARGB = 0xFFFFFFFF
field 5  PartitionSystem     sized reference, 8 bytes
field 3  Zone                nullable ID-only relationship, 4 bytes; известное значение = 0
field 2  Child slot 0        UInt32 slot + relationship
field 2  Child slot 1        UInt32 slot + relationship
field 6  PartitionRenderable null ID-only relationship, 4 bytes
field 0  terminator

# section 1: spBSPNode
field 0  Plane               Vector3 normal + Single constant, 16 bytes
field 1  Polygon             UInt32 count + Vector3[count], optional
field 0  terminator
```

Для terminal child payload field 2 занимает 12 байт: четырёхбайтовый slot,
object ID и нулевой inline size. Для вложенного BSP payload равен
`12 + child.SerializedSize`. Ни `CollisionInfo`, ни `ZonePortal`, ни
`StaticRenderObject` в BSP-секции базового класса не встречаются.

## топология

| Объекты | Ресурсы | Размер объекта | Inline BSP edges | Terminal references |
| ---: | ---: | ---: | ---: | ---: |
| 108 | 18 | 101..1 616 | 90 | 126 |
| **324** | **54** | — | **270** | **378** |

Размер любого поддерева удовлетворяет точной формуле:

```text
SerializedSize = 101 * BSP split count
```

Минимальный узел с двумя terminal references занимает 101 байт. Самое большое
дерево находится в `Levels/Domino/Domino04.smo`: 16 split-узлов, 17 листьев,
максимальная глубина 6 и размер корня 1 616 байт. В остальных ресурсах 2..7
split-узлов и глубина 1..4.

| slot 0 / slot 1 | Узлов |
| --- | ---: |
| BSP / BSP | 26 |
| Partition leaf / BSP | 15 |
| Partition leaf / Partition leaf | 44 |
| BSP / Partition leaf | 23 |

## Разделяющие плоскости

Нормалей `+Y/-Y` и нулевых нормалей нет. PC и PS2 сохраняют все четыре `float`
плоскости без изменений.

## PC/PS2

- 108/108 совпадают по топологии, encodings, child types и plane values;
- 91/108 совпадают побайтно;
- оставшиеся различия находятся в служебных object IDs отношений, а не в
  геометрии или структуре дерева.

Таким образом, serialized layout общий для PC и PS2; отличаются runtime offsets,
но отдельная платформенная ветка decoder не нужна.

## Viewer и исследовательская база

Analyzer записал:
