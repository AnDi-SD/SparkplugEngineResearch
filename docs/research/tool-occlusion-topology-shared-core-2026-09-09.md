# Общие методы геометрии spOcclusionVolume — 9 сентября 2026

Блок19 текущего цикла. Восстановлены пять отдельных методов actual класса
`spOcclusionVolume`, его подтверждённые начальные поля и Node-only clone.
**Полный Init и загрузка OcclusionVolume в tools остаются открытыми.**
Новый класс не подключён к ResourceGraph, serializer-заглушки нет.

## Исполненная PC-проверка

Pristine PC SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
RTTI43D24430→Node695C0F65, factory470A70, allocation1B8, primary6E8DE0.
Original factory устанавливает borderCountD4=0, planar160=0, walkStamp164=0
и initialized1B4=0; padding сохраняется. Отдельный CheckPlanarity на пустом
объекте возвращаетtrue: это результат leaf-функции, не успешная инициализация.

| PC | Подтверждённое поведение |
|---|---|
| 46D670 | `(own normal × normalized edge) · opposite normal <= 0.001f` |
| 46E820 | Объединяет последовательные коллинеарные рёбра; сохраняет own face первого; удалённые места заполняет последними элементами |
| 46E1A0 | Сопоставляет обратные endpoints, записывает обе opposite faces, снимает border flags, вычитает2 и проверяет convex; отказ сохраняет уже сделанные изменения |
| 46EB10 | Удаляет внутренние рёбра при совпадении трёх компонентов нормали; расстояние плоскости не сравнивает; borderCount не пересчитывает |
| 46E3B0 | При borderCount0 устанавливает planar0; иначе сравнивает все4 компонента каждой плоскости с первой; успех устанавливает1, отказ сохраняет prior planar |

Пятнадцать original-PC результатов и состояний точно совпали с C++:
три знака convex, две обычные/повторные противоположные связи и один отказ,
три варианта объединения, четыре варианта planarity, два удаления внутренних
рёбер. Повторный opposite match оригинал не пропускает: явно подготовленные
три ребра дают unsigned borderCountFFFFFFFF. Дополнительной проверки
manifold в этот метод не внесено; это не проверка допуска такого SMO целиком.

`SetPreparedTopologyForAnalysis` задаёт **явные входы** этих методов. Он не
объявлен игровым initializer: не читает SMO, не сваривает вершины, не создаёт
грани по треугольникам. Host limits4096 и проверки индексов принадлежат этому
анализаторному входу. Арифметика использует общий восстановленный vector math;
проверенные решения совпадают точно, универсальная bit-identical x87 арифметика
на всех пограничных floating-point входах не заявляется.

Original object, face storage, edge objects и containers освобождены настоящими
деструкторами; heap/имена/CRT formatting — явно описанные внешние boundaries.
Console output отключён настоящим конфигурационным байтом73FF60.
Максимальная арена успешных случаев57104 байта. Сохранены100k instructions,
2s/call и30s/child. Старые capped470FE0/470E30 не запускались повторно.

Предварительные попытки диагностических веток остановились на неподдержанном
NULL `%s`, затем отсутствующих CRT string/console dependencies. Задан валидный
fixture name, подключены существующие bounded CRT helpers и штатный console
flag. Игровые geometry/logging decision methods не заменялись callbacks.

## PS2 помогает разложить оставшуюся процедуру

ELF SHA256 `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.
Factory1CE2D0 выделяет1B0; reader1A00B0 вызывает Init1CDA30 и обрабатываетfalse.
Readable PS2 topology1CC0F0 показывает последовательность:
создание граней/рёбер1CBD40 → collinear merge1CB990 → reverse links1CB5A0 →
coplanar removal → planarity → connectivity walk1CAF90; для planar добавляется
обратная сторона. Это **static evidence**, PS2 не исполнялась. R5900 LQ/SQ
разобраны отдельно; неподдержанные MMI/COP1 слова не выдаются за инструкции
другой MIPS ISA.

Чтобы подключить reader, остаётся закрыть весь путь подготовки буферов,
выбор представителей при weld, создание граней, connectivity walk и обратную
сторону planar геометрии. В частности, прежний bounded qsort fixture не
доказывает порядок равных элементов оригинальной MSVCR71.

## Проверка и предел результата

Source OcclusionTopology38/38, FullLoader213/213. Новый тест clone проверяет
уже подтверждённый CP15 Node-only путь: concrete class и position сохраняются,
собственная геометрия не переносится. Source comparison15/15.
Обвязка C ABI и C# не менялась, managed builds и полный корпус не повторялись.
Последовательные build failures и исправления сохранены в evidence; итоговая
проверка не является выпуском. Блок не прибавляет успешно загружаемые уровни.

Evidence: `research/probe_pc_occlusion_topology.py`,
`research/tools-core-occlusion-topology-block-2026-09-09.json`,
`local-data/results/tools-core-cycle-20260909-1900/occlusion-init/`.
