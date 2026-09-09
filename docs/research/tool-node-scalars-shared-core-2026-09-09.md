# Общие scalar-поля Node и размещение

Тринадцатый блок цикла. C# Node decoder больше не читает float/boolean/billboard
payload самостоятельно и не нормализует quaternion. Тело scalar cases
`spNodeSerializer::ReadNodeFieldsForAnalysis` вынесено в общий
`ReadScalarFieldForAnalysis`; full loader вызывает тот же helper. Семантика
восстановленного reader не изменена. Optional observation сохраняет authored
quaternion и последние raw flag bytes, не меняя Node.

## Передача в приложения

ABI `spv_node_values` получает один borrowed SMO span и массив descriptor
(field ID, absolute payload offset, size). C# schema выбирает подтверждённую
Node section и отделяет serialized relationships. Это metadata projection,
не повторная реализация scalar reader. Каждое поле применяется к настоящему
свежему `spNode`, после чего выполняется его isolated world update с null camera.
Child/Collision не материализуются этим ABI; готовность графа из этого не следует.

`SpvNodeField`12 bytes, `SpvNodeValues`96 bytes: local P/Q/S, actual orientation,
effective flags/billboard и authored flag bytes. Raw orientation участвует
в побитной native сверке. C# placement получает исходный Q и вызывает уже
общий native local-matrix helper. Матрица `(0,0,.5,.5)` больше не меняется
из-за C# Quaternion.Normalize. Repeated scalar fields разрешены как в reader;
nonzero boolean byte не ограничен значением1.

DTO `SmoNodeData` описывает authored flags и serialized relationships.
`EffectiveFlags` явно отделяет состояние изолированного свежего Node: например,
поле Animated0 не снимает native Animated, а Static1→0 оставляет Static.
Это не итоговое состояние произвольного derived constructor/attached graph.
Существующие structural inspectors проверяют authored fields; runtime graph
остаётся отдельной общей загрузкой. Прежнее описание DTO как effective values
исправлено, скрытого смешения состояний больше нет.

## Удалённое угадывание

`SmoNodeTransformDecoder` удалил запасной PRS byte parser и поиск любого
похожего field0/1/2. Теперь использует общую Node section. `spModel` не является
Node и не получает transform из похожего payload. Position/rotation/scale
offsets для редактора берутся из той же подтверждённой section, последнее
повторение выигрывает. Required section count проверяется по metadata schema.

Старые synthetic fixtures Node без terminator и RenderNode с одной section
исправлены: добавлены настоящие завершающие секции. Это исправление тестовых
входов, не изменение игрового формата. Также восстановлены два UTF-8 разделителя
в тестовом выводе, повреждённые локальным CP1251-чтением; игровые bytes не менялись.

Сборка конечных placement world matrices и часть hierarchy policy ещё остаются
в C# и требуют переноса. Этот блок не объявляет завершённым всё Node/scene ядро.

## Оптимизация и проверка

Уникальный ID index теперь строится один раз на immutable `SmoDocument` и
удерживается через ConditionalWeakTable. Отдельные документы имеют отдельные
индексы; освобождение документа освобождает кэш. Это оптимизация host-каталога,
не переписывание игрового алгоритма. Unknown/non-Node classes отклоняются
до ненужного разбора полей transform adapter.

На PC menu835 Node/RenderNode три измерения до и после: managed allocations
около204.4 МБ →1.69–1.72 МБ за проход, снижение99.17%. Время116–364 мс →4–21 мс;
tiered JIT влияет на короткие результаты, поэтому это не показатель FPS.
Число результатов и checksum одинаковы, Native DLL и вход неизменны.
Harness: `research/node_scalar_benchmark/`; parse/selection вне измеряемого
участка, native heap не входит в managed allocation count.

`validate_tools_node_scalars.py` заново выполняет пять original-PC reader
cases: empty, transforms, false flags, repeated flags, unknown/repeated fields.
P/S/R совпадают побитно, effective flags точно равны. Проверены три descriptor
host rejection. Исходные fixtures проверили полное освобождение своих owners.
C++ NodeSerialization226, четыре Viewer набора9308/2983/1531/1541 и три whole
resource graphs прошли. Входы и hashes — в snapshot. PS2 menu проверен как
input общего inspector; нового PS2 runtime execution этим не заявлено.
