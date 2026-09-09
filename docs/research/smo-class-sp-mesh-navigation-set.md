# `spMeshNavigationSet`: структура данных и общий reader

## Результат

Обновление 9 сентября: общий C++ reader и MeshBV binding проверены по PC,
прежний C# decoder заменён проекцией actual ResourceGraph. Игровая matrix
сохраняет `value & 3`, а порядок/дубликаты neighbours не исправляет. Проверки
связности относятся к анализу корпуса, а не к правилам загрузки. См.
[общие классы навигации](tool-navigation-readers-shared-core-2026-09-09.md).

Ниже сохранён структурный анализ `spMeshNavigationSet` (`0x7297173C`).
Прежний строгий анализатор прочитал 355 экземпляров из трёх корпусов:

| Корпус | SMO | Объектов | Именованных | Размер объекта |
|---|---:|---:|---:|---:|
| `pc-working` | 27 | 119 | 119 | 208..85 055 байт |
| `pc-pristine` | 27 | 119 | 119 | 208..85 055 байт |
| `ps2-pristine` | 26 | 117 | 117 | 208..85 055 байт |

Два лишних PC-объекта находятся в
`Levels/Gardenia/test_world_navmesh.smo`. Производственные PC и PS2 ресурсы
используют один и тот же логический формат.

Главный результат разбора: навигационный graph уже полностью записан в SMO.
`spMeshBV` задаёт треугольники, field 3 задаёт явные рёбра между ними, а две
матрицы заранее выбирают следующий элемент из упорядоченного списка рёбер.
Поэтому восстанавливать маршруты только из геометрии mesh неверно.

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

В production-ресурсах node-секция содержит Position и `IsAnimated=true`.
Только два PC test-world объекта опускают `IsAnimated`, то есть используют его
default `false`. Rotation, Scale, Bone, Static, Child, Billboard и Collision в
этом классе корпусом не записываются, хотя поддерживаются serializer базового
`spNode`.

## Точный layout полей

Ниже section 1 означает `spNavigationSet`, section 0 — собственную секцию.

| Section | Field | Имя | Payload |
|---:|---:|---|---|
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

Таким образом, номера узлов и соседей действительно однобайтовые, но число
соседей в записи — `UInt32`. В корпусе `N=2..59`, степень узла равна 0..4.
Все 23 552 направленных ребра трёх корпусов имеют обратное ребро.

`Enable` присутствует и равен `true` во всех 355 объектах. Число строк
`PortalTransTable` всегда совпадает с числом field 4, а `NodeCount` всегда
совпадает с числом треугольников связанного `spMeshBV`.

## Семантика таблиц маршрутизации

### `TransTable`

Строка — текущий/source node, столбец — destination node. Обычная ячейка не
содержит ID следующего узла. Она содержит индекс в ordered-массив
`LinksTable[current].neighbours`:

```text
selector = TransTable[current, destination]
current  = LinksTable[current].neighbours[selector]
```

Повторение операции приводит к destination. Проверены все 393 496 достижимых
упорядоченных пар трёх корпусов: 131 604 в каждом PC-корпусе и 130 288 на PS2.

Значение `3` имеет двойную роль:

- на диагонали `source == destination` это terminal marker;
- для недостижимой пары это также terminal/unreachable marker;
- у узла со степенью 4 оно может быть обычным индексом четвёртого ребра.

То есть `3` является терминатором только тогда, когда маршрут уже завершён либо
значение выходит за текущую степень узла. В трёх корпусах все 11 125 диагоналей
равны 3; ещё 3 246 недостижимых пар также равны 3. Decoder отдельно проверяет
достижимость, допустимость selector и отсутствие циклов.

### `PortalTransTable`

Строка соответствует occurrence field 4 `Portal`, столбец — начальному узлу.
При движении используется та же строка, но новый current node:

```text
selector = PortalTransTable[portal, current]
if selector >= degree(current): route finished
else current = LinksTable[current].neighbours[selector]
```

В конечном узле selector всегда равен 3. При степени 4 значение 3 остаётся
допустимым четвёртым ребром и маршрут продолжается. Все 14 371 portal/node
маршрутов корпуса завершаются на `3` без циклов; длина проверенных цепочек —
1..42 узла.

## Relationship ownership

Связь с mesh имеет две реальные формы:

| Variant | PC working | PC pristine | PS2 | Всего |
|---|---:|---:|---:|---:|
| inline-owned `spMeshBV` | 117 | 117 | 117 | 351 |
| sized reference на `spMeshBV` | 2 | 2 | 0 | 4 |

Reference-вариант встречается только у `nav_01`/`nav_02` в PC-only
`test_world_navmesh.smo`. Это отдельный storage variant, а не иной формат
графа.

Каждый `spNavigationPortal` встречается в графе своего SMO ровно дважды: один
navigation set владеет им inline, второй ссылается sized reference. На каждый
PC-корпус приходится 70 inline и 70 reference occurrences; на PS2 — 69 и 69.
В сумме это 418 relationships к 209 физическим portal-объектам.

Физический родитель navigation set зависит от его места в цепочке:

| Корпус | parent `spNavigationGraph` | parent `spNavigationPortal` |
|---|---:|---:|
| каждый PC | 56 | 63 |
| PS2 | 55 | 62 |

## Явный graph и геометрия mesh

Сравнение каждой записи `LinksTable` с точным совпадением общих рёбер двух
треугольников дало для каждого PC-корпуса:

- 7 850 directed links по общему геометрическому ребру;
- 32 вручную заданные связи без одного точного общего ребра;
- 4 геометрические shared-edge directions намеренно отсутствуют в graph.

На PS2 числа равны 7 756, 32 и 4. Разница первых чисел объясняется двумя
PC-only test-world объектами; authored-исключения совпадают. Следовательно,
`LinksTable` является авторитетной топологией. Автоматическое построение
adjacency по вершинам уничтожит 32 специальных перехода и включит четыре
намеренно запрещённых направления на каждом corpus snapshot.

## PC и PS2

- Все 119 пар `pc-working`/`pc-pristine` совпадают побайтно.
- По canonical path и ordinal образуются 117 PC/PS2-пар.
- Placement, `NodeCount`, обе матрицы, links, число portals, `Enabled` и классы
  targets совпадают у 117 из 117 пар.
- Полностью сериализованные bytes совпадают у 108 из 117 пар.
- Девять оставшихся различий ограничены identity relationship и bytes
  вложенных descendants; навигационная семантика не меняется.
- Два test-world navigation set существуют только на PC.

## Свидетельства executable

В PC PE32:

- base reader: `0x00448800..0x00448E48`;
- base writer: `0x00447F00..0x004487C1`;
- mesh reader: `0x00448FE0..0x00449124`;
- mesh writer: `0x00449130..0x004493AA`;
- runtime offsets: NodeCount `+0xB4`, matrices `+0xB8/+0xBC`, Enabled `+0xE1`,
  Mesh `+0xE4`.

В PS2 ELF:

- mesh reader/index/writer: `0x0019A920..0x0019AC34`;
- base reader: `0x0019E3C0..0x0019EAA8`;
- base writer: `0x0019EBE0..0x0019F37C`;
- runtime offsets: NodeCount `+0xC0`, matrices `+0xC4/+0xC8`, link arrays
  `+0xCC/+0xD0`, portals `+0xD4/+0xD8`, Enabled `+0xE9`, Mesh `+0xF0`.

Независимый MIPS writer особенно важен: он явно подтверждает `UInt8 nodeId`,
`UInt32 neighbourCount` и `UInt8 neighbourId`. Оба executable содержат один
набор tokens `NumNodes`, `TransTable`, `PortalTransTable`, `LinksTable`,
`Portal`, `Enable`, `Mesh` и возвращают одинаковые class IDs.

## Viewer и исследовательская база

Viewer теперь строго декодирует все три секции и показывает:

- placement и `IsAnimated`;
- размеры, histogram и preview обеих routing matrices;
- полный ordered adjacency graph, число рёбер, degree range и reciprocity;
- portal relationships, `Enabled` и связанный `spMeshBV`;
- исходные offsets, payload layout и hex рядом с decoded value.

Анализатор `research-db analyze-class ... spMeshNavigationSet` идемпотентно
создаёт 16 field definitions, два storage variants, 355 assignments, четыре
evidence-записи и аннотирует 3 254 содержательных поля. Независимый read-only
отчёт находится в
[`analyze_smo_mesh_navigation_set.py`](../../research/analyze_smo_mesh_navigation_set.py).

## Почему изменение пока отключено

Отдельное изменение Position или `Enabled` технически просто, но полноценное
редактирование navigation mesh требует атомарно перестроить согласованный набор:

1. треугольники и вершины `spMeshBV`;
2. `NodeCount`;
3. плотные node IDs и ordered `LinksTable`;
4. квадратную `N x N TransTable`;
5. `P x N PortalTransTable`;
6. portal endpoints и обе relationship occurrences.

Кроме того, четыре геометрические adjacency намеренно отключены, а 32 links не
выводятся из точного общего ребра. Поэтому автоматический rebuild обязан уметь
сохранять authored overrides. До появления такого coordinated graph builder
класс остаётся `confirmed_read_only`; Viewer ничего не записывает в SMO.
`spNavigationPortal` и `spNavigationGraph` уже разобраны; ближайшие тесты
ограничены существующими Enabled/Open/alternative values без rebuild routing
tables.
