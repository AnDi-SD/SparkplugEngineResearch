# spMeshNavigationSet

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMeshNavigationSet](../../../Sparkplug/Code/Sparkplug/spMeshNavigationSet.h).

## Наследование и serializer-секции

Исполняемые файлы подтверждают цепочку:

```text
spNode (0x695C0F65)
  └─ spNavigationSet (0x74F9013E)
       └─ spMeshNavigationSet (0x7297173C)
```

В каждом объекте идут три секции, каждая со своим пустым field 0 terminator:

1. унаследованная секция `spNode`;
2. унаследованная секция `spNavigationSet`;
3. собственная секция `spMeshNavigationSet`.

## Точный layout полей

Ниже section 1 означает `spNavigationSet`, section 0 — собственную секцию.

| Section | Field | Имя | Payload |
| ---: | ---: | --- | --- |
| 2 | 0 | `Position` | `Single x, y, z` |
| 2 | 8 | `IsAnimated` | optional `UInt8 Boolean` |
| 1 | 0 | `NodeCount` | `UInt32 N` |
| 1 | 1 | `TransTable` | `UInt32 rows=N, columns=N, UInt32[N*N]` |
| 1 | 2 | `PortalTransTable` | `UInt32 rows=P, columns=N, UInt32[P*N]` |
| 1 | 3 | `LinksTable` | явная byte-addressed adjacency table |
| 1 | 4 | `Portal` | repeated relationship на `spNavigationPortal` |
| 1 | 5 | `Enable` | `UInt8 Boolean` |
| 0 | 0 | `Mesh` | один relationship на `spMeshBV` |

`LinksTable` имеет переменную длину:

```text
UInt32 nodeRecordCount                 // равно N
repeat nodeRecordCount:
    UInt8  nodeId                      // плотный порядок 0..N-1
    UInt32 neighbourCount
    UInt8  neighbourNodeId[neighbourCount]
```

`Enable` присутствует и равен `true` во всех 355 объектах. Число строк
`PortalTransTable` всегда совпадает с числом field 4, а `NodeCount` всегда
совпадает с числом треугольников связанного `spMeshBV`.

### `TransTable`

Строка — текущий/source node, столбец — destination node. Обычная ячейка не
содержит ID следующего узла. Она содержит индекс в ordered-массив
`LinksTable[current].neighbours`:

```text
selector = TransTable[current, destination]
current  = LinksTable[current].neighbours[selector]
```

Значение `3` имеет двойную роль:

- на диагонали `source == destination` это terminal marker;
- для недостижимой пары это также terminal/unreachable marker;
- у узла со степенью 4 оно может быть обычным индексом четвёртого ребра.

### `PortalTransTable`

Строка соответствует occurrence field 4 `Portal`, столбец — начальному узлу.
При движении используется та же строка, но новый current node:

```text
selector = PortalTransTable[portal, current]
if selector >= degree(current): route finished
else current = LinksTable[current].neighbours[selector]
```

## Relationship ownership

Связь с mesh имеет две реальные формы:

| Variant | PS2 | Всего |
| --- | ---: | ---: |
| inline-owned `spMeshBV` | 117 | 351 |
| sized reference на `spMeshBV` | 0 | 4 |

Reference-вариант встречается только у `nav_01`/`nav_02` в PC-only
`test_world_navmesh.smo`. Это отдельный storage variant, а не иной формат
графа.

Физический родитель navigation set зависит от его места в цепочке:

| parent `spNavigationGraph` | parent `spNavigationPortal` |
| ---: | ---: |
| 56 | 63 |
| 55 | 62 |

## Явный graph и геометрия mesh

`LinksTable` задаёт авторитетную топологию: возможны вручную заданные переходы без общего геометрического ребра и намеренно отсутствующие направления на общих рёбрах. Автоматическое построение adjacency только по вершинам изменит эти правила.

## Отображение в инструментах

Viewer декодирует все три секции и показывает:

- placement и `IsAnimated`;
- размеры, histogram и preview обеих routing matrices;
- полный ordered adjacency graph, число рёбер, degree range и reciprocity;
- portal relationships, `Enabled` и связанный `spMeshBV`;
- исходные offsets, payload layout и hex рядом с decoded value.

## Почему изменение пока отключено

Отдельное изменение Position или `Enabled` технически просто, но полноценное
редактирование navigation mesh требует атомарно перестроить согласованный набор:

1. треугольники и вершины `spMeshBV`;
2. `NodeCount`;
3. плотные node IDs и ordered `LinksTable`;
4. квадратную `N x N TransTable`;
5. `P x N PortalTransTable`;
6. portal endpoints и обе relationship occurrences.
