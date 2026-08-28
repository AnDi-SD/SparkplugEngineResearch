# `spBSPNode` (`0x7362AB22`)

Полный структурный read-only разбор выполнен по всем уникальным объектам трёх
корпусов с повторным чтением исходных PC SMO и PS2 PCK. Контракт независимо
подтверждён serializers из `WinxClub.exe` и `SLES_532.19`.

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

Это не navigation graph: дерево относится к общей partition/visibility
подсистеме уровня. Названия геометрических сторон для slots 0 и 1 пока не
подтверждены, поэтому Viewer намеренно не называет их front/back или
positive/negative.

## Serializer layout

Объект имеет две секции. Порядок полей унаследованной секции фиксирован:

```text
# section 0: spPartitionNode
field 1  DebugColor          UInt32 ARGB = 0xFFFFFFFF
field 5  PartitionSystem     sized reference, 8 bytes
field 3  Zone                nullable ID-only relationship, 4 bytes; corpus = 0
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

`Plane.Normal` конечен и нормализован. Наблюдаемый диапазон длины —
`0.9999999757..1.0000000297`. Оба executable поддерживают optional `Polygon`,
но поле отсутствует во всех 324 исследованных объектах. Следовательно,
отсутствие polygon является свойством игрового корпуса, а не ограничением
формата.

## Корпус и топология

| Corpus | Объекты | Ресурсы | Размер объекта | Inline BSP edges | Terminal references |
|---|---:|---:|---:|---:|---:|
| `pc-working` | 108 | 18 | 101..1 616 | 90 | 126 |
| `pc-pristine` | 108 | 18 | 101..1 616 | 90 | 126 |
| `ps2-pristine` | 108 | 18 | 101..1 616 | 90 | 126 |
| **Всего** | **324** | **54** | — | **270** | **378** |

В каждом из 18 ресурсов находится ровно одно дерево. На один корпус это 108
split-узлов и 126 terminal leaves. Проверен инвариант полного двоичного леса:
для 18 корней число листьев равно `108 + 18 = 126`. Все вложенные BSP-цели
достижимы ровно один раз, циклов и общих inline-поддеревьев нет.

Размер любого поддерева удовлетворяет точной формуле:

```text
SerializedSize = 101 * BSP split count
```

Минимальный узел с двумя terminal references занимает 101 байт. Самое большое
дерево находится в `Levels/Domino/Domino04.smo`: 16 split-узлов, 17 листьев,
максимальная глубина 6 и размер корня 1 616 байт. В остальных ресурсах 2..7
split-узлов и глубина 1..4.

Формы непосредственных детей на один корпус:

| slot 0 / slot 1 | Узлов |
|---|---:|
| BSP / BSP | 26 |
| Partition leaf / BSP | 15 |
| Partition leaf / Partition leaf | 44 |
| BSP / Partition leaf | 23 |

Все 108 узлов каждого корпуса имеют по одному slot 0 и slot 1. Двадцать сырых
`field_shape` не являются подтипами: они возникают только из разных размеров
inline-поддеревьев. В базе записан один нормализованный variant.

## Разделяющие плоскости

Распределение нормалей одинаково в каждом корпусе:

| Направление | Узлов на corpus |
|---|---:|
| `+X` | 25 |
| `-X` | 20 |
| `+Z` | 29 |
| `-Z` | 18 |
| oblique | 16 |

Нормалей `+Y/-Y` и нулевых нормалей нет. PC и PS2 сохраняют все четыре `float`
плоскости без изменений.

Дополнительная выборка положений объектов из terminal partition nodes дала для
формы `dot(normal, point) - constant` выраженный, но не строгий перекос: slot 0
чаще положителен, slot 1 чаще отрицателен. В обеих ветвях остаются точки другого
знака и точки на плоскости. Это может объясняться пересекающими плоскость
объектами или неполнотой выбранных representative points, поэтому сторона slot
пока остаётся рабочей гипотезой, а не подтверждённым именем поля.

## Свидетельства из executable

PC serializer:

- reader: `0x0044CEB0..0x0044D20F`;
- writer: `0x0044D220..0x0044D602`;
- Plane normal/constant: runtime `+0x84/+0x90`;
- Polygon pointer/count: runtime `+0xA4/+0xA8`.

PS2 serializer:

- reader: `0x0019F8D0..0x0019FBA8`;
- writer: `0x0019FBC0..0x0019FEA0`;
- Plane normal/constant: runtime `+0x70/+0x7C`;
- Polygon pointer/count: runtime `+0x90/+0x94`.

Оба writer всегда записывают field 0 Plane и записывают field 1 Polygon только
при ненулевом количестве вершин. В обоих executable присутствуют независимые
строки `spBSPNodeSerializer`, `ebspnsfBSPNodePlane` и
`ebspnsfBSPNodePolygon`.

## PC/PS2

Все 108 PC working/pristine объектов совпадают побайтно. По canonical resource
path и ordinal сопоставлены 108 PC-pristine/PS2 узлов:

- 108/108 совпадают по топологии, encodings, child types и plane values;
- 91/108 совпадают побайтно;
- оставшиеся различия находятся в служебных object IDs отношений, а не в
  геометрии или структуре дерева.

Таким образом, serialized layout общий для PC и PS2; отличаются runtime offsets,
но отдельная платформенная ветка decoder не нужна.

## Viewer и исследовательская база

`SmoBspNodeDecoder` строго проверяет обе секции, два ordered child slots,
relationship encoding/ownership, единичную нормаль и optional polygon. Inspector
показывает ARGB, nullable Zone/PartitionRenderable, оба branch slots с target
metadata, Plane и Polygon при его появлении. Семантический вывод включается
только после успешного полного decode.

Analyzer записал:

- 10 актуальных field definitions: восемь унаследованных и две собственных;
- 2 268 annotations присутствующих полей;
- один общий PC/PS2 variant и 324 assignments;
- четыре evidence-записи по PC executable, PS2 executable, корпусу и сравнению.

## Воспроизведение

```powershell
dotnet tools/SmoViewer/SmoViewer.Inspect/bin/Release/net8.0/SmoViewer.Inspect.dll `
  research-db analyze-class local-data/results/smo-corpus-v2.sqlite `
  spBSPNode --json

python research/analyze_smo_bsp_node.py `
  local-data/results/smo-corpus-v2.sqlite
```

Связанные [`spOcclusionVolume`](smo-class-sp-occlusion-volume.md) и
[`spMeshNavigationSet`](smo-class-sp-mesh-navigation-set.md) также полностью
разобраны. Геометрические имена slots и ненаблюдаемый Polygon остаются в
каноническом списке открытых вопросов.
