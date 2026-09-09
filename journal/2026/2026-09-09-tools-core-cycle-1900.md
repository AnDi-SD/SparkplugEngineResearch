# Ядра tools — цикл 9 сентября до 19:00 МСК

08:12 МСК: пользователь поручил продолжать перенос общей логики, следующий
отчёт в19:00 МСК. Редакторские исключения отложены до LVLcreator; альтернативные
алгоритмы не согласованы. PC остаётся эталоном, PS2 используется по конкретной
пользе; UI вторичен. Исходный checkpoint1495999, Viewer4a407fd, Texture1ddb50f.

Проверены правила и предыдущий результат. Первый следующий шаг — сохранённый
StaticRenderObject scout и перенос нужного reader без C#-дублирования.
Уточнено: packed combiner120 — cached member, standalone/physical bytes84;
читать120 из файла без исследования реального потребителя не следует.

Текущий отчёт: [цикл](../../docs/research/tools-core-cycle-2026-09-09-1900.md).

Блок1: восстановлены StaticRenderObject и его reader/writer/indexer.44FE30 —
индексация references, не header factory. Пять fresh original scalar sections
совпали побайтно с source/CABI; общий RenderNode support выделен без смены
семантики. Static52, RenderNode34, FullLoader213, host graph registration прошли.
Старые ограничения C# матриц удалены; legacy inverse authoring перемещён в
Editing без изменения и оставлен на LVLcreator review. Viewer/Importer/LVLcreator
CoreTests builds прошли0 warnings/errors. Детали/выборка/ограничения:
`docs/research/tool-static-render-object-shared-core-2026-09-09.md`.

Блок2: общий MaterialData reader подключён к C# вместо самостоятельного
декодера; инспекция ID не создаёт substitute referents. Original9 selected
cases и3 host guards прошли,6 полных source reader/writer captures совпали.
MaterialSerialization554/Controller393/Color225/FullLoader213 прошли;
5 SMO:37097/36322/9326/3001/1573 assertions. Сборки Viewer/Importer/LVLcreator
CoreTests без предупреждений/ошибок. Проверка текущего DTO одним слоем на проход
остаётся явным host-пределом, native поддерживает8×8. Следующая задача Model/Skin.
Досье: `docs/research/tool-material-reader-shared-core-2026-09-09.md`.

Блок3: Model и Skin C# readers заменены общими исходными секциями; metadata
палитра не создаёт substitute Node. Original11/source writer/ABI cases совпали,
включая alpha256→true, repeat/clear palette и raw NaN/Inf matrix bits.4 guards;
C++ Skin119/Material554/FullLoader213. Старые Material9 original captures
повторно использованы для проверки нового общего reference helper без повторной
эмуляции.5 SMO:37101/36326/9330/3005/1577 assertions;3 consumer builds чистые.
Досье: `docs/research/tool-model-skin-reader-shared-core-2026-09-09.md`.
