# Общие world matrices для инструментов — 9 сентября 2026

Блок 14 цикла до 07:30. Источник истины — PC WinxClub.exe SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Подключение

`SmoNodeTransformDecoder` получает world matrices конкретных Node/RenderNode
из существующего `SparkplugSceneRuntime`: это настоящие восстановленные
`spNode`, исходный quaternion→rotation и `UpdateWorld`, а затем исходный affine
builder. Удалены C# перемножение local matrices и подстановка обратных skin bind
matrices в качестве позы Node. Минимальная замена private helper во вьювере
также вызывает общий provider; интерфейс не перерабатывался.

Результат лениво кэшируется на immutable SmoDocument через ConditionalWeakTable.
Native scene освобождается после копирования результатов. Каталожные объекты,
которым не нужен Node, не заставляют загружать посторонние Node sections.
Выбор владельца размещения по каталогу и shared renderable instances остаётся
host policy; остальные Node-derived classes этим не объявляются перенесёнными.
StaticRenderObject пока использует прежний отдельный decoder. Это не полная
реконструкция renderer traversal или всей иерархии уровня.

## Свежая проверка оригинала

`research/validate_tools_node_world.py` выполняет PC quaternion setter420640,
world update421420 для родителя/ребёнка и affine builder461D70. Вход — явно
ограниченный Node-layout fixture; original factory/attach/destructor здесь
не исполняются и не заявляются. Bridge использует настоящие C++ Node owners.

Родитель P=(10,20,30), S=(2,3,4), R=identity; ребёнок P=(1,2,3), S=(.5,2,3),
Q=(0,0,.5,.5). Итоговые 64 bytes совпали: строки матрицы
(.5,.5,0,0), (-3,3,0,0), (0,0,12,0), (12,26,42,1).
Обычное C# localMatrix*parentMatrix даёт другой результат. Требование единичного
quaternion удалено из host SceneCreate: исходный Node его не предъявляет.
Восстановленная математика не изменялась.

`research/node_world_probe/` проверяет shared scene против нового placement
cache на пяти файлах. `validate_tools_node_world_graph.py` сопоставляет точные
float bits, включая signed zero, с фактической полной загрузкой C++ graph:
Icy87, PC menu835, bloom_jeans97, tile_bad35 — всего1054 узла. PS2 menu88
проверен только как вход общего scene adapter, без PC whole-graph/PS2 execution.
Baseline отдельно показывает отличие прежней inverse-bind подстановки до
0.0002084 на Icy; это следствие прежнего пересчёта, а не допуск новой проверки.

## Приостановленный обратный расчёт редактора

Существующий редактор вычислял local PRS через inverse/decompose матриц.
При неравномерном масштабе родителя его прежняя local-only проверка пропускала
результат, который игра размещает иначе. Отрицательная проверка сохранена в
`editor-inverse-before-guard.log`. Прямой native расчёт подтверждён оригиналом.

Patch теперь перед выдачей результата повторно загружает изменённый контейнер
и сравнивает actual Sparkplug world с requested world (существующий допуск0.001).
Если результат другой, выдаёт `NODE_WORLD_WRITE_UNSUPPORTED`, bytes не возвращает.
Это проверка в памяти; исходный SMO не изменяется. ValidateEdit остаётся preflight
и не обещает, что финальная world проверка обязательно пройдёт.

Обратный алгоритм не переписан без согласования. Предложение: сначала найти
соответствующий исходный путь установки world/local PRS; если у игры нет нужной
редакторской операции, согласовать один общий editor adapter с обязательной
проверкой исходным forward runtime. Эта ветка ожидает решения пользователя.

## Проверки и пределы

C++ NodeWorld86; Viewer9312 PC menu,2987 PS2 menu,1559 Icy,1535 bloom_jeans,
1545 tile_bad. Alfea02:36308, в том числе132 collision meshes и запись placement.
Устаревшее требование единичных chandelier normals в level test заменено
сверкой каждого normal с исходными байтами: общий mesh reader, как и оригинал,
не нормализует их (доказательства блока7). Это исправление теста, не engine.
WPF Viewer с минимальным подключением компилируется:0 warnings/0 errors.
Полный корпус не запускался, релиз не собирался. Reports находятся в
`local-data/results/tools-core-cycle-20260909-0730/node-world/`; hashes — snapshot.
