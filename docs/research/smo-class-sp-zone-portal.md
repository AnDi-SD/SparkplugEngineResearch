# `spZonePortal` (`0x6523AC37`)

Статус: **полный структурный/read-only decode PC и PS2**. Запись и мутация
поля пока намеренно не разрешены.

## Итог

`spZonePortal` — направленное ребро между двумя `spZone`. Источник не хранится
в payload самого портала: это field 3 `Zone` физического родителя
`spPartitionNode`. Назначение задаёт field 0 `DestinationZone`.

У класса одна serializer-секция:

```text
spZonePortal section
field 0: DestinationZone  # одно ненулевое relationship -> spZone
field 1: Polygon          # UInt32 count + Vector3[count]
field 2: Open             # Boolean
field 0, empty: terminator
```

Порядок строгий, все три поля обязательны и встречаются ровно по одному разу.
Во всём корпусе polygon всегда является четырёхугольником, а `Open` всегда
равен `true`. Decoder при этом проверяет общий доказанный формат polygon и
принимает не менее трёх конечных вершин, не зашивая наблюдаемое число четыре в
само описание бинарного типа.

## Корпус

| Корпус | Объектов | SMO | Inline destination | Reference destination | Polygon | `Open=true` |
|---|---:|---:|---:|---:|---:|---:|
| PC working | 206 | 18 | 93 | 113 | 206 × 4 вершины | 206 |
| PC pristine | 206 | 18 | 93 | 113 | 206 × 4 вершины | 206 |
| PS2 pristine | 206 | 18 | 93 | 113 | 206 × 4 вершины | 206 |
| **Всего** | **618** | **54** | **279** | **339** | **2 472 вершины** | **618** |

Каждый портал физически принадлежит `spPartitionNode`. Две формы
`DestinationZone` не являются разными подтипами:

- sized reference указывает на уже сериализованную `spZone`;
- inline relationship впервые сериализует destination zone, которая тогда
  является физическим child портала.

То есть форма relationship отражает владение и порядок сериализации, а не
игровую разновидность портала.

## Пары и направление polygon

В каждом корпусе найдено 103 объекта `spZonePortalNode`. Каждый из них
ссылается ровно на два `spZonePortal`. Эти порталы образуют обратные рёбра:
source первого равен destination второго и наоборот.

Для всех **309** пар трёх корпусов последовательность четырёх вершин второго
polygon является точным разворотом последовательности первого. Следовательно,
winding polygon кодирует направление портала, а парный `FrontToBack` /
`BackToFront` использует ту же геометрию с обратным порядком вершин.

## Подтверждение executable

PC `WinxClub.exe`:

- index/read/write-код: `0x0044DDA0..0x0044E4D8`;
- reader: `0x0044DDE0..0x0044E0D2`;
- writer: `0x0044E0E0..0x0044E4D8`;
- runtime layout: destination pointer `+0x14`, polygon count `+0x18`, polygon
  vertex pointer `+0x1C`, open byte `+0x20`.

PS2 `SLES_532.19`:

- reader: `0x001A3310..0x001A3604`;
- index: `0x001A3610..0x001A364C`;
- writer: `0x001A3650..0x001A3920`;
- те же runtime offsets `+0x14/+0x18/+0x1C/+0x20`;
- destination создаётся через class hash `0x61254AB3` (`spZone`), null
  destination явно отвергается диагностической строкой.

Оба writer последовательно записывают fields `0`, `1`, `2` и terminator.

## Сопоставление платформ

- все 206 PC working/pristine объектов совпадают побайтно;
- все 206 одноимённых PC/PS2 объектов совпадают семантически после
  нормализации object IDs и ownership relationship;
- на обеих платформах одинаковы cardinality, порядок полей, polygon и значение
  `Open`.

Разница фактических байтов inline relationship обусловлена вложенным содержимым
zone и platform-specific object IDs, а не layout `spZonePortal`.

## Реализация и воспроизведение

Viewer получил строгий `SmoZonePortalDecoder`; Inspector показывает
`DestinationZone`, polygon и `Open`. Analyzer повторно читает исходные
directory/PCK-ресурсы, проверяет ownership, targets, парность и winding, затем
идемпотентно записывает три определения полей, один variant, 618 назначений,
1 854 semantic field annotations и четыре evidence-записи.

```powershell
dotnet tools/SmoViewer/SmoViewer.Inspect/bin/Debug/net8.0/SmoViewer.Inspect.dll `
  research-db analyze-class local-data/results/smo-corpus-v2.sqlite `
  spZonePortal --json

python research/analyze_smo_zone_portal.py `
  local-data/results/smo-corpus-v2.sqlite
```

Разбор продолжен в
[`spZonePortalNode`](smo-class-sp-zone-portal-node.md): подтверждены две секции,
унаследованный node-transform и фиксированный порядок двух portal-ссылок.
