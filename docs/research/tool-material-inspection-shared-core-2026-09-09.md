# Единое чтение материала для его потребителей

Шестой блок цикла до19:00 МСК9 сентября. Удалены отдельные C# parsers diffuse
и render states; они и полный MaterialData decoder используют одну инспекцию
исходного spMaterialSerializer. Правила выбора материала ещё не изменены.

## Реализация и исправления приложения

SmoMaterialInspection хранит скалярный снимок и pass/layer metadata на entry
неизменяемого SmoDocument. ConditionalWeakTable не удерживает закрытый документ;
короткий lock защищает первоначальное чтение и доступ к словарю. Native handle
освобождается сразу после снимка. Изменённый SMO разбирается в новый документ,
поэтому совпадение индексов объектов не смешивает кеши. Это host cache данных,
не альтернативный игровой material или resource graph.

Добавлен ABI getter фактических pass blend/layer count. Он возвращает и pass
без слоёв, который старый полный C# DTO не представлял. Ограничения полного
DTO и authored color/power не навязываются скалярному render-state view.
Diffuse view по-прежнему сообщает только authored color.

Старый state parser останавливался после первой пары fields; теперь действуют
все назначения исходного reader. Старый diffuse parser отвергал любой чёрный
RGB как отсутствующий цвет; authored black теперь возвращается успешно.
Восстановленные классы и алгоритмы не изменены. Preview classification и
legacy выбор material по физическим соседям остаются отдельной дальнейшей работой.

## Проверки и замер

Переиспользованы9 запечатанных original-PC captures блока2: полные source
writer captures и ABI state/reference comparisons прошли. Дополнительно3
source/accessor случая пустых passes с blend0/2/6 и прежние3 guards.
Новые original executions не требовались. C++ MaterialSerialization554 и
FullLoader213 прошли. Viewer/Importer/LVLcreator CoreTests builds чистые.

Пять SMO: Alfea0137182, Alfea0236353, PC-menu9341, PS2-tagged-menu3069,
Icy1588 assertions. C# проверяет последнее state assignment, authored black
и независимость документов. Первый test run выявил старый synthetic material
без section terminator: самостоятельный parser не доходил до него, а общий
reader отклонил незавершённый payload. Исправлен fixture, не reader.

Узкий benchmark:10 первых materials pristine Alfea02,100 повторов трёх
потребителей, один прогрев и5 измерений. Медиана5,8782→1,4136ms, около4,16×;
управляемые allocations1072080→888080B, минус17,16%. Успешных чтений18000
в каждом запуске. Это повторное чтение metadata, не полная загрузка или FPS.
Хеши исходного SMO, сравниваемых сборок и benchmark source сохранены.

Полного corpus scan, PS2 исполнения, GPU интеграции и выпуска не было.
Evidence: `research/tools-core-material-inspection-block-2026-09-09.json`.
