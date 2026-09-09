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

Блок4: общий reader AnimTexController и исходный выбор ключа TextureTrack.
Пять original/source captures,33 runtime selections и повторное использование
inline original graph прошли. C++ Controller488/Material554/FullLoader213;
5 SMO:951/1933/1753/1871/36330 assertions;3 consumer builds чистые.
Старые BloomX tests исправлены по реальным связям и исходным opaque BGRA bytes.
Это перенос чтения и индексатора; looping clock/UI bindings ещё не мигрированы.
Досье: `docs/research/tool-animated-texture-reader-shared-core-2026-09-09.md`.

Блок5: UV/MaterialColor field readers используют общие Trans/Color/Function
serializers.7 свежих original scalar-state captures и4 guards совпали;
UV386/Color118/MaterialColor225/FullLoader213 прошли.5 выборочных SMO:
750/748/802/937/36350 assertions;3 consumer builds0 warnings/errors.
Исправлена C# подстановка scale defaults удалением дублирующего decoder.
Досье: `docs/research/tool-material-functions-shared-core-2026-09-09.md`.
