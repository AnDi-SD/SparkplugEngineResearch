# `spPartitionNode` (`0x67672341`)

Статус: **завершён полный структурный/read-only разбор наблюдаемого PC/PS2
layout**. Все уникальные экземпляры из `pc-working`, `pc-pristine` и
`ps2-pristine` повторно прочитаны из исходных directory/PCK-данных строгим
декодером. Отдельно проверены обе реализации serializer в исполняемых файлах.

[PC Visibility runtime](native-pc-visibility-runtime.md) дополнительно доказал
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

## Корпуса

| Корпус | Объекты | SMO | Физические вхождения | Размер |
|---|---:|---:|---:|---:|
| `pc-working` | 5 254 | 29 | 5 254 | 49..7 251 404 |
| `pc-pristine` | 5 254 | 29 | 5 254 | 49..7 251 404 |
| `ps2-pristine` | 5 696 | 30 | 5 832 | 49..5 585 452 |
| **Всего** | **16 204** | **88** | **16 340** | — |

Все узлы безымянны. В каждой PC-копии физическими родителями являются 5 128
`spOctreeNode` и 126 `spZone`; на PS2 — 5 570 и 126. Дополнительные 442
уникальных PS2-узла находятся в PS2-only `Gardenia03.smo`, а разница между
5 696 уникальными и 5 832 физическими вхождениями вызвана повтором ресурсов в
разных PCK.

## Поля и отношения

| Field | Имя serializer | Cardinality на объект | Наблюдаемая форма |
|---:|---|---:|---|
| 0 | `CollisionInfo` | 0..69 | sized reference или owned inline `spCollisionInfo` |
| 1 | `DebugColor` | ровно 1 | `UInt32` ARGB |
| 2 | `Child` | 0 у точного класса; 8 у `spOctreeNode` | `UInt32 slot` + owned inline `spPartitionNode`/`spOctreeNode` |
| 3 | `Zone` | ровно 1 | ненулевая sized reference на `spZone` |
| 4 | `ZonePortal` | 0..6 | owned inline `spZonePortal` |
| 5 | `PartitionSystem` | ровно 1 | ненулевая sized reference на `spPartitionSystem` |
| 6 | `PartitionRenderable` | ровно 1 | ID-only null либо owned inline `spPartitionRenderable` |
| 7 | `StaticRenderObject` | 0..595 | sized reference или owned inline `spStaticRenderObject` |

Во всех трёх корпусах проверены 232 035 relationship-полей. Все ненулевые ID
разрешаются однозначно и ведут на ожидаемый класс; все 74 324 inline-отношения
совпадают с физическими children текущего узла.

Точные профили:

| Корпус | CollisionInfo | ZonePortal | StaticRenderObject | PartitionRenderable inline / null |
|---|---:|---:|---:|---:|
| каждая PC-копия | 8 140 | 206 | 52 171 | 2 324 / 2 930 |
| PS2 | 8 140 | 206 | 54 043 | 2 560 / 3 136 |

Для `CollisionInfo` каждая PC/PS2-копия содержит 1 549 inline и 6 591 sized
references. Для `StaticRenderObject` это 20 469/31 702 в каждой PC-копии и
20 913/33 130 на PS2. `ZonePortal` всегда inline; `PartitionSystem` и `Zone`
всегда reference-only.

### Field 2 — индексированный `Child`

PC и PS2 executable независимо содержат `epnsfPartitionNodeChild`,
`GetChild(i)`, reader, writer и проверку, запрещающую null child. PS2 writer
однозначно помещает field 2 между `Zone` и `CollisionInfo`. В 16 204 объектах
точного класса `spPartitionNode` поле действительно отсутствует, но это не
executable-only возможность: унаследованная секция каждого `spOctreeNode`
содержит ровно восемь таких полей, всего 18 048 записей.

Payload отличается от обычного relationship дополнительным первым словом:

```text
UInt32 slot             # 0..7
UInt32 childObjectId
UInt32 inlineSize
inline SBOO spPartitionNode | spOctreeNode
```

Slots идут строго 0..7 и являются битовой маской high-половины X/Y/Z octree.
Все targets — физические inline children. Полный разбор находится в
[`smo-class-sp-octree-node.md`](smo-class-sp-octree-node.md). Редактирование
отношений по-прежнему не включено.

### Field 1 — `DebugColor`

Значение хранится little-endian и показывается Viewer как `#AARRGGBB` с
отдельными A/R/G/B. На обеих платформах встречается один набор из 19 точных
цветов. Основная восьмицветная шкала:

| ARGB | PC pristine | PS2 pristine |
|---|---:|---:|
| `0x8F48488F` | 643 | 697 |
| `0x9F50509F` | 625 | 678 |
| `0xAF5858AF` | 659 | 716 |
| `0xBF6060BF` | 656 | 712 |
| `0xCF6868CF` | 628 | 680 |
| `0xDF7070DF` | 623 | 677 |
| `0xEF7878EF` | 651 | 709 |
| `0xFF8080FF` | 667 | 725 |
| остальные 11 значений | 99 | 99 |

Название поля доказано executable, но влияние конкретного debug-цвета в игре
контролируемой мутацией ещё не проверено. Поэтому значение остаётся read-only.

## PC/PS2

Две PC-копии побайтно совпадают для всех 5 254 узлов в 29 ресурсах. Между PC
pristine и PS2 сопоставлены те же 5 254 узла по canonical path и ordinal:

- `DebugColor` совпадает у 5 254 из 5 254;
- вся упорядоченная семантика отношений совпадает у 5 251 из 5 254;
- три различия находятся в `Alfea/Alfea_broken_01.smo` ordinal 2,
  `Cloud01/Cloud01_02.smo` ordinal 2 и `Swamp/BMS_03.smo` ordinal 151.

Это различия состава уровней, не serializer layout: номера, порядок и encoding
полей остаются общими.

## Свидетельства executable

PC `WinxClub.exe` содержит class registration с hash `0x67672341`, строку
`spPartitionNodeSerializer`, все восемь `epnsfPartitionNode*`, вызовы
`GetPartitionSystem`, `GetZone`, `GetChild(i)`, `GetCollisionInfo(i)`,
`GetZonePortal(i)`, `GetStaticRenderObject(i)` и `GetPartitionRenderable`.
Reader/writer находятся в области VA `0x0044B700..0x0044C6D5`.

PS2 `SLES_532.19` независимо подтверждает тот же enum и порядок. Полный writer:
VA `0x001A16D0..0x001A1DEC`. В runtime-объекте видны `DebugColor +0x40`,
`Zone +0x50`, `PartitionSystem +0x60`, `PartitionRenderable +0x64`, child count
`+0x4C`, portal count `+0x58` и static-render count `+0x2C`.

## Реализация и база

`SmoPartitionNodeDecoder` проверяет обязательные поля, точный writer order,
cardinality, target class, encoding и physical ownership каждого inline SBOO.
Inspector показывает цвет и каждую resolved relationship, включая явный null.

В schema v2 записаны:

- восемь field definitions, включая индексированный `partition_node.child`;
- 248 239 декодированных direct fields;
- один общий PC/PS2 variant и 16 204 assignments;
- четыре evidence rows: PC executable, PS2 executable, полный corpus и
  cross-corpus comparison.

Воспроизведение:

```powershell
python research/analyze_smo_partition_node.py `
  local-data/results/smo-corpus-v2.sqlite

SmoViewer.Inspect research-db analyze-class <db> spPartitionNode
```

Обе команды идемпотентны относительно исходных ресурсов; Python-аудит работает
read-only. Разбор наследника продолжен в
[`spOctreeNode`](smo-class-sp-octree-node.md).

## PC runtime — 2026-09-06

[Отдельный native-разбор](native-pc-partition-runtime.md) подтвердил exact84,
BaseObject (не Node),33 virtual slots, intrusive static/portal/Zone refs против
direct-owned children/payload, borrowed reciprocal RenderNode registration и
Scene propagation. File inline ownership не следует переносить буквально в
runtime containers. Полный structural/read-only статус выше **не** означает
полный runtime: query/visibility/portal/debug и original API ещё открыты.
