# `spNavigationPortal`: структура данных и общий reader

## Результат

Обновление 9 сентября: общий C++ reader подтверждён original-PC executions,
прежний C# decoder удалён. Reader сохраняет raw node/path bytes без проверки
их согласованности с routing graph. Указатели graph/endpoints конструктором
не инициализируются; omission учитывается явно. См.
[общие классы навигации](tool-navigation-readers-shared-core-2026-09-09.md).

Ниже сохранён структурный анализ `spNavigationPortal` (`0x385662AA`).
Прежний строгий анализатор проверил 209 объектов: 70 в `pc-working`, 70 в
`pc-pristine` и 69 в `ps2-pristine`. Один дополнительный портал принадлежит
PC-only `Levels/Gardenia/test_world_navmesh.smo`.

Класс наследует `spNode` и после node-терминатора содержит собственную секцию:

| Field | Семантика | Payload |
|---:|---|---|
| 0 | `navigation_portal.graph` | relationship на `spNavigationGraph` |
| 1 | `navigation_portal.endpoint_set` | ровно две последовательные relationship на `spMeshNavigationSet` |
| 2 | `navigation_portal.node_pair` | повторяемая пара `UInt8 firstNode, secondNode` |
| 3 | `navigation_portal.path` | повторяемая тройка `UInt8 sourceSet, destinationSet, alternativeIndex` |

Field 2 хранит пары треугольников/узлов на двух сторонах портала. Все значения
проверены против `NodeCount` соответствующего navigation set. Field 3 не является
командой движения: это обратный индекс участия портала в альтернативных маршрутах
`spNavigationGraph`. Все 8 044 записи точно согласованы с графом.

## Relationship-варианты и платформы

Найдены три варианта хранения двух endpoint relationships: меняется только
inline/reference ownership, логическая структура едина. Все 70 пар PC-корпусов
совпадают побайтно; все 69 PC/PS2-пар совпадают семантически, 65 — полностью
сериализованными bytes.

PC writer/reader и независимый PS2 serializer называют те же поля, включая
`m_uSrcSet`, `m_uDstSet` и `m_uPathIndex`. Viewer показывает node state, graph,
обе стороны, пары узлов и path membership. Изменение пока read-only: оно требует
атомарно перестроить оба navigation set и таблицы `spNavigationGraph`.

Воспроизводимый отчёт: [`analyze_smo_navigation_portal.py`](../../research/analyze_smo_navigation_portal.py).
