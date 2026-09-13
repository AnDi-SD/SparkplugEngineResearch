# spPartitionNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPartitionNode](../../../Sparkplug/Code/Sparkplug/spPartitionNode.h).

[PC Visibility runtime](../../engine/visibility/visibility-runtime.md) дополнительно доказал
root7C как visited frame stamp и отбор payload/static/dynamic support pointers.
Это не означает закрытый Octree/portal/occluder traversal.

## Назначение и общий layout

`spPartitionNode` — связующий узел spatial-partition дерева. Он принадлежит
`spOctreeNode` либо `spZone`, ссылается на общие `spPartitionSystem` и `spZone`
и собирает collision, portal и render-объекты своего участка.

Обе платформы используют один порядок serializer-полей:

```text
SBOO spPartitionNode
field 1: UInt32 DebugColor (ARGB)
field 5: relationship -> spPartitionSystem
field 3: relationship -> spZone
repeat field 2: UInt32 slot + inline relationship -> child spPartitionNode/spOctreeNode
repeat field 0: relationship -> spCollisionInfo
repeat field 4: relationship -> spZonePortal
repeat field 7: relationship -> spStaticRenderObject
field 6: null ID or inline relationship -> spPartitionRenderable
field 0: empty serializer-section terminator
```

Последний пустой field 0 — terminator, а не пустой `CollisionInfo`. Большой
диапазон serialized size (49..7 251 404 байт) создают вложенные inline-поддеревья,
а не разные подвиды узла. Во всех файлах найден один структурный вариант.

## Поля и отношения

| Field | Имя serializer | Cardinality на объект | Наблюдаемая форма |
| ---: | --- | ---: | --- |
| 0 | `CollisionInfo` | 0..69 | sized reference или owned inline `spCollisionInfo` |
| 1 | `DebugColor` | ровно 1 | `UInt32` ARGB |
| 2 | `Child` | 0 у точного класса; 8 у `spOctreeNode` | `UInt32 slot` + owned inline `spPartitionNode`/`spOctreeNode` |
| 3 | `Zone` | ровно 1 | ненулевая sized reference на `spZone` |
| 4 | `ZonePortal` | 0..6 | owned inline `spZonePortal` |
| 5 | `PartitionSystem` | ровно 1 | ненулевая sized reference на `spPartitionSystem` |
| 6 | `PartitionRenderable` | ровно 1 | ID-only null либо owned inline `spPartitionRenderable` |
| 7 | `StaticRenderObject` | 0..595 | sized reference или owned inline `spStaticRenderObject` |

Точные профили:

| CollisionInfo | ZonePortal | StaticRenderObject | PartitionRenderable inline / null |
| ---: | ---: | ---: | ---: |
| 8 140 | 206 | 52 171 | 2 324 / 2 930 |
| 8 140 | 206 | 54 043 | 2 560 / 3 136 |

Для `CollisionInfo` каждая PC/PS2-копия содержит 1 549 inline и 6 591 sized
references. Для `StaticRenderObject` это 20 469/31 702 в каждой PC-копии и
20 913/33 130 на PS2. `ZonePortal` всегда inline; `PartitionSystem` и `Zone`
всегда reference-only.

### Field 2 — индексированный `Child`

Payload отличается от обычного relationship дополнительным первым словом:

```text
UInt32 slot             # 0..7
UInt32 childObjectId
UInt32 inlineSize
inline SBOO spPartitionNode | spOctreeNode
```

Slots идут строго 0..7 и являются битовой маской high-половины X/Y/Z octree.
Все targets — физические inline children. Полный разбор находится в
[`smo-class-sp-octree-node.md`](sp-octree-node.md). Редактирование
отношений по-прежнему не включено.

### Field 1 — `DebugColor`

Значение хранится little-endian и показывается Viewer как `#AARRGGBB` с
отдельными A/R/G/B. На обеих платформах встречается один набор из 19 точных
цветов. Основная восьмицветная шкала:

| ARGB | PS2 pristine |
| --- | ---: |
| `0x8F48488F` | 697 |
| `0x9F50509F` | 678 |
| `0xAF5858AF` | 716 |
| `0xBF6060BF` | 712 |
| `0xCF6868CF` | 680 |
| `0xDF7070DF` | 677 |
| `0xEF7878EF` | 709 |
| `0xFF8080FF` | 725 |
| остальные 11 значений | 99 |

## PC/PS2

Две PC-копии побайтно совпадают для всех 5 254 узлов в 29 ресурсах.

- `DebugColor` совпадает у 5 254 из 5 254;
- вся упорядоченная семантика отношений совпадает у 5 251 из 5 254;
- три различия находятся в `Alfea/Alfea_broken_01.smo` ordinal 2,
  `Cloud01/Cloud01_02.smo` ordinal 2 и `Swamp/BMS_03.smo` ordinal 151.

Это различия состава уровней, не serializer layout: номера, порядок и encoding
полей остаются общими.
