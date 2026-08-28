# `spOctreeNode` (`0x21A70829`)

Статус: **завершён полный структурный/read-only разбор PC/PS2 layout**.
Все 2 256 уникальных экземпляров повторно прочитаны из исходных directory/PCK
ресурсов строгим декодером. Имена полей, порядок и runtime offsets независимо
проверены в PC `WinxClub.exe` и PS2 `SLES_532.19`.

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

## Корпуса

| Корпус | Объекты | SMO | Размер объекта | Дочерние `spPartitionNode` | Дочерние `spOctreeNode` |
|---|---:|---:|---:|---:|---:|
| `pc-working` | 731 | 11 | 620..4 850 123 | 5 128 | 720 |
| `pc-pristine` | 731 | 11 | 620..4 850 123 | 5 128 | 720 |
| `ps2-pristine` | 794 | 12 | 620..3 839 287 | 5 570 | 782 |
| **Всего** | **2 256** | **34** | — | **15 826** | **2 222** |

Все объекты безымянны. Корневыми являются 11 octree-узлов в каждой PC-копии и
12 на PS2; их физический родитель — `spZone`. Остальные 720/720/782 узла
вложены в другой `spOctreeNode`. PS2 добавляет 63 узла в `Gardenia03.smo`.

Всего проверены 24 816 relationship-полей. Из них 18 048 — индексированные
inline children; каждый target разрешается однозначно и совпадает с физическим
child текущего объекта. `PartitionSystem` и `Zone` всегда представлены
ненулевыми sized references, `PartitionRenderable` всегда ID-only null.
`DebugColor` у всех 2 256 объектов равен `0xFF8080FF`.

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
|---:|---|---|---|
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

## Свидетельства executable

PC serializer находится в области VA `0x0044C8E0..0x0044CD6D`. Writer сначала
вызывает базовый `spPartitionNodeSerializer`, затем записывает:

| Field | Имя | PC runtime offset |
|---:|---|---:|
| 0 | `eonsfOctreeNodePivot` | `+0x84` |
| 1 | `eonsfOctreeNodeMins` | `+0xB0` |
| 2 | `eonsfOctreeNodeMaxs` | `+0xBC` |

Независимая PS2 реализация в VA `0x001A0800..0x001A0BEC` подтверждает тот же
порядок с offsets `+0x70`, `+0x9C`, `+0xA8`. Разница offsets — различие runtime
layout, а не формата SMO.

## PC/PS2 и реализация

Все 731 same-path/ordinal пары двух PC-копий побайтно совпадают. Все 731 общих
PC/PS2-объекта семантически равны: совпадают цвет, slots и типы восьми children,
`Pivot`, `Mins` и `Maxs`.

В Viewer добавлены `SmoOctreeNodeDecoder`, наследованные определения
`spPartitionNode` и отображение `Pivot/Mins/Maxs`. Field 2 показывается как
`octant slot=N` плюс разрешённый inline target. Строгий декодер проверяет
порядок секций, slots 0..7, ownership и bounds.

В schema v2 записаны:

- 11 field definitions: восемь унаследованных и три собственных;
- 33 840 декодированных direct fields;
- один общий PC/PS2 variant и 2 256 assignments;
- четыре evidence rows: PC executable, PS2 executable, полный corpus и
  cross-corpus comparison.

Воспроизведение:

```powershell
python research/analyze_smo_octree_node.py `
  local-data/results/smo-corpus-v2.sqlite

SmoViewer.Inspect research-db analyze-class <db> spOctreeNode
```

Python-аудит read-only; C# analyzer повторно читает исходные ресурсы и
идемпотентно обновляет базу. Связанный `spPartitionSystem` и его zone/portal
graph также полностью разобраны.
