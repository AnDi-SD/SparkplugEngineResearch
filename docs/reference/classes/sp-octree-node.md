# spOctreeNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spOctreeNode](../../../Sparkplug/Code/Sparkplug/spOctreeNode.h).

## Назначение и layout

`spOctreeNode` наследует `spPartitionNode` и добавляет геометрию восьмеричного
разбиения. Объект содержит две serializer-секции:

```text
SBOO spOctreeNode

# унаследованная секция spPartitionNode
field 1: UInt32 DebugColor (ARGB)
field 5: sized reference -> spPartitionSystem
field 3: sized reference -> spZone
repeat 8 times field 2:
    UInt32 octantSlot
    UInt32 childObjectId
    UInt32 inlineSerializedSize
    inline SBOO spPartitionNode | spOctreeNode
field 6: UInt32 0                       # null PartitionRenderable
field 0: empty base-section terminator

# собственная секция spOctreeNode
field 0: Vector3 Pivot
field 1: Vector3 Mins
field 2: Vector3 Maxs
field 0: empty own-section terminator
```

У каждого объекта ровно 15 содержательных direct fields и два terminator.
`CollisionInfo`, `ZonePortal` и `StaticRenderObject` в унаследованной секции
octree-узла отсутствуют: эти списки принадлежат листовым `spPartitionNode`.
Большое число сырых `field_shape` — 2 231 — вызвано только размерами восьми
inline-поддеревьев и не является набором подтипов. После нормализации layout один.

## Pivot, Mins и Maxs

Все три собственных поля обязательны, имеют длину 12 байт и содержат конечные
little-endian `Single`. Во всех объектах выполняется:

```text
Mins.X <= Pivot.X <= Maxs.X
Mins.Y <= Pivot.Y <= Maxs.Y
Mins.Z <= Pivot.Z <= Maxs.Z
```

`Pivot` — не обязательно геометрический центр bounds. В каждой PC-копии он
точно совпадает с midpoint лишь у 177 из 731 объектов, на PS2 — у 179 из 794;
максимальное отклонение одной координаты от midpoint равно примерно 5 447,85.
Следовательно, это самостоятельная точка разбиения, а не производное поле.

Число различных векторов на каждой платформе одинаково, несмотря на 63
дополнительных PS2-узла: 687 `Pivot`, 591 `Mins` и 606 `Maxs`. Дополнительный
уровень повторно использует уже встречавшиеся значения.

## Значение `octantSlot`

Первый `UInt32` field 2 — не часть обычного relationship и не произвольный
порядковый номер. Он кодирует сторону от `Pivot` по трём осям:

| Slot | X | Y | Z |
| ---: | --- | --- | --- |
| 0 | low | low | low |
| 1 | high | low | low |
| 2 | low | high | low |
| 3 | high | high | low |
| 4 | low | low | high |
| 5 | high | low | high |
| 6 | low | high | high |
| 7 | high | high | high |

Иными словами, bits 0/1/2 выбирают high-половину X/Y/Z. Для всех 2 222
вложенных octree-children их `Mins/Maxs` точно совпадают с соответствующей
половиной parent bounds: low означает `parent.Mins..parent.Pivot`, high —
`parent.Pivot..parent.Maxs`. Листовой child имеет класс `spPartitionNode` и
собственных bounds не сериализует.

Это также уточняет прежний разбор `spPartitionNode`: field 2 действительно
отсутствует во всех 16 204 объектах точного класса `spPartitionNode`, но
используется 18 048 раз в унаследованных секциях `spOctreeNode`.

## PC/PS2 и реализация

Все 731 same-path/ordinal пары двух PC-копий побайтно совпадают. Все 731 общих
PC/PS2-объекта семантически равны: совпадают цвет, slots и типы восьми children,
`Pivot`, `Mins` и `Maxs`.

В schema v2 записаны:

Python-аудит read-only; C# analyzer повторно читает исходные ресурсы и
идемпотентно обновляет базу. У связанного `spPartitionSystem` и zone/portal
graph завершён структурный wire-разбор, **не вся runtime-логика**.
