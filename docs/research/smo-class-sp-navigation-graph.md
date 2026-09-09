# `spNavigationGraph`: структура данных и общий reader

## Результат

Обновление 9 сентября: инструменты используют восстановленный C++ reader.
Свежие PC executions показали, что ограничения старого строгого decoder
на связность, полный порядок строк и reserved=0 не являются условиями reader.
См. [общие классы навигации](tool-navigation-readers-shared-core-2026-09-09.md).
Routing runtime и writers ещё не перенесены.

Ниже сохранён результат структурного анализа `spNavigationGraph` (`0x188A161F`):
27 + 27 + 26 объектов в трёх корпусах, всего 80. Наблюдавшиеся секции:

```text
spNode
  -> секция spRenderNode (пустая в этих образцах)
  -> spNavigationGraph
```

Собственная секция имеет точный layout:

| Field | Семантика | Payload |
|---:|---|---|
| 0 | navigation sets | повторяемая relationship на `spMeshNavigationSet` |
| 1 | portals | повторяемая relationship на `spNavigationPortal` |
| 2 | table size | `UInt32`, в корпусе равно числу sets |
| 3 | path row | повторяемая запись ниже |

```text
UInt32 sourceSet
UInt32 destinationSet
UInt8  nextPortal
UInt8  alternativeCount
repeat alternativeCount:
    UInt8 firstPortal
    UInt8 reserved       // всегда 0 в исследованном корпусе
```

В исследованном корпусе записи идут row-major и образуют полную квадратную таблицу. `255` у
`nextPortal` означает self/unreachable. Проверены все 2 507 строк: 1 282
достижимых направления, 870 недостижимых и 355 диагоналей. Все 2 344
альтернативы восстановлены по endpoint topology; их первый portal и весь маршрут
точно совпадают с 8 044 обратными membership-записями порталов. Второй байт
альтернативы оставлен с честным именем `reserved`: корпус доказывает его нулевое
значение, но не runtime-семантику.

Все PC-копии совпадают побайтно. Все 26 PC/PS2-пар совпадают семантически, 23 —
побайтно. Отдельный PC test-world graph отличается `Animated=false`. Viewer
декодирует всю таблицу и проверяет маршруты без эвристического сканирования.
Редактирование требует согласованного rebuild graph, sets и portals.

Воспроизводимый отчёт: [`analyze_smo_navigation_graph.py`](../../research/analyze_smo_navigation_graph.py).
