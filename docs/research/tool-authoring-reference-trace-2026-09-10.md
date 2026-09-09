# Model/Material authoring и точные позиции ссылок

Третий блок цикла до 07:00 МСК 10 сентября. Приоритет — действующие команды
Importer/LVLcreator. Новый игровой алгоритм не изобретён: приложения получают
связи от actual ResourceGraph, а позиции ссылок — от общего восстановленного
reader. Основы: [PC reference reader](native-pc-read-reference.md),
[whole loader](native-pc-full-loader.md) и [save references](native-pc-save-reference.md).
Новые PC/PS2 original execution cases в этом блоке не заявляются.

## Выбор объектов для замены

`SmoLevelModelGraphReplacer` ранее выбирал Material как непосредственного
физического ребёнка Model. Это неверно для reference-only материала и для
позднего переопределения поля. Сейчас `SmoLoadedResources.Models` задаёт настоящие
MeshId/MaterialId. Физическая ветка остаётся границей перестановки байтов:
один component соответствует одному inline Mesh, а дополнительные Model —
потребители этого ресурса со своими материалами.

Перед заменой проверяется, что выбранный физический Mesh действительно является
активным Mesh его Model. Материал другого потребителя не меняется, если поздняя
ссылка переключила его на другой Mesh. Старые сериализованные ссылки всё ещё
требуют перемещения/переназначения байтов; они не выдаются за активное состояние.

`ModelGraphLinkRegression`: три полные замены, 50 checks — обычное inline
размещение; общий Mesh с разными вынесенными материалами и неактивными inline
материалами; поздняя ссылка на другой Mesh вне заменяемой ветки. Проверяется
неизменность исходного документа, чужой ветки и неактивных материалов.
Первая версия последнего fixture использовала NULL Mesh, который существующий
whole-reader host guard не поддерживает. Исправлен именно тест: он использует
другой настоящий Mesh. Shared reader ради ожидания теста не менялся.

## Наблюдение общего читателя

Прежний `SmoAdditiveForestPlanner.RemapObjectIds` считал любое поле из восьми
байтов с нулевым вторым словом ссылкой, если первое слово попадало в карту ID.
При этом ссылки внутри массива Skin palette он не обходил. Теперь discovery
происходит исключительно в `spSerializer::ReadReferenceForAnalysis` — после
настоящего чтения prefix, без повторного наблюдения предварительных guards.

Необязательный host trace фиксирует ID, inline size, абсолютные позиции слов,
активного consumer и результат resolution. Источник явно задан как тот же stream,
которому whole loader установил data origin. Неизвестный или отдельный stream
инвалидирует trace; результат игрового reader от наблюдения не меняется.

Payload coverage снимается в трёх действительных путях: outer materialization,
recursive reference и prepared Mesh. Диапазон включает восьмибайтовый object
header. Пропуск existing/cache payload не считается чтением его содержимого.
`complete` всего trace означает успешную загрузку, а не покрытие каждого диапазона.
Всего допускается 262144 строки; превышение/ошибка выделения выключает только
наблюдение. Original dispatch, владение и порядок чтения сохранены.

`spv_graph_load_with_trace` использует тот же ResourceGraph. Обычная загрузка
без trace остаётся доступной для потребителей, которым карта не нужна.
ABI передаёт массивы простых DTO размером 28/24 байта. C# не повторяет resolver.
Ошибка карты отделена от `LoadIssue`: она запрещает remap, сохраняя доступные
результаты обычного графа.

## Переназначение и транспорт между процессами

`CaptureRange` требует тот же документ и hash исходных байтов, полное чтение
каждой копируемой FAT записи по её точному extent. Сохраняются observed ID sites,
inline size/consumer и catalog metadata с hash raw name. Сам range имеет SHA-256.
Importer проверяет полное соответствие изменяемых Entries этому снимку и actual
inline reference у каждой записи; у корня — также действительного consumer.
Одного физического вложения для этого недостаточно.

Переназначаются только четыре байта на наблюдённых ID sites. Неизвестные поля,
порядок, заголовки и матрицы остаются побитово прежними. Нужна полная nonzero
однозначная карта без коллизий с оставленными ресурсами. Consumer IDs и sealed
catalog обновляются вместе с ID; повторный remap использует новое состояние.
Manual plan без provenance может вставляться без изменений, но не переназначаться.

Один `SmoFileReferenceRemapContext` разделяет immutable ID map и новый catalog
между всеми ranges плана. Иначе предельные 8192 ranges × 8192 IDs могли бы
породить около 256 МиБ лишних массивов на каждое поколение плана. Рабочий процесс
передаёт catalog один раз, последующие ranges ссылаются на его hash. Приёмная
сторона разделяет один проверенный массив и проверяет range SHA/positions/catalog.
Это private host transport наблюдений нашего worker, **не** независимый запуск
reader и не импорт произвольного внешнего evidence. Старый результат worker без
обязательной provenance отклоняется; обходной C# resolver не добавлен.

## Проверки текущего этапа

Пять native suites: ReferenceReadTrace61, ReadReference298, FullLoader213,
MeshReader346, SkinSerialization119. Пять downstream проектов собраны без
ошибок/предупреждений. Общий FormatTests — 647 assertions.

`ForestReferenceRemapRegression` — 42 checks: два nested Skin references
`[7,7,1] → [107,107,1]`, повторное переназначение, actual Skin.Mesh, неизменность
матриц и неизвестного Node field30 `[7,0]`. Проверены worker JSON roundtrip,
однократная передача catalog, неверные maps/hash/offset/type/size/name/owner,
перекрывающиеся ID sites, неправильный inline size word и изменение исходного
ParseOwned массива, а также обычная вставка manual plan. Source/original callbacks новых классов
здесь не реконструировались; native тесты проверяют host observation boundary.

Результаты находятся в `local-data/results/tools-core-cycle-20260910-0700/`:
`model-graph-links/` сохраняет первый независимый результат selection;
`reference-trace/` — текущую native/managed интеграцию. Артефакты локальные;
hashes фиксируются manifest блока. ABI на vase/Qc/Alfea01: 121348 checks,
4538 объектов и 537 Node snapshots побитово одинаковы с trace и без него.
8842 reference и 9645 payload rows занимают 479056 байт DTO; 572 Mesh проходят
через prepared path. Проверены 30 ABI guards. В Alfea01 покрыты 4410/4411 FAT
extent: TextureData ID1848 `marble` по `[2844011,2860459)` взят из cache, его
содержимое не читалось. Authoring range с этими байтами остаётся явным отказом;
успешная загрузка сцены это ограничение не снимает.

Existing isolated worker LVLcreator также прошёл на pristine Alfea02, Mesh1373:
маленький glb triangle без текстур создаёт два отдельных placement. Получены
7 новых объектов в двух forests (538/369 байт), 12 ID sites. Catalog4273 записан
один раз. JSON/attachment hashes, ID/inline size, catalog metadata и root consumer
проверены; исходный SMO не изменился, полный output SMO не создавался.
Это integration run нашего worker, не новый запуск оригинальной игры.

Сохранены и не выдаются за native failures ошибки стенда: первая C# сборка
использовала `Contains(0)` вместо `Contains(0u)`; первый ABI helper содержал
ошибочный hardcoded MeshData class ID, исправленный по исходникам. Ранняя запись
`build-1.json` от managed orchestration перекрыла wrapper metadata native build;
полный `native-1.log` с компиляцией и пятью passed suites сохранён. Следующие
managed результаты находятся в отдельном каталоге, native test log архивируется.

## Границы

Сохраняются прежние repeated-slot, multipass, unusual-header и world-inverse
отсрочки. Trace не превращает raw branch в canonical serialization и не
восстанавливает ещё отсутствующий whole-file original Save. Следующая часть
authoring — подключение доступных Material/Model/Skin writers через общий host
save context; прямые builders ещё не считаются полностью перенесёнными.

Большие входы остаются отдельной задачей. Fresh read-only аудит нашёл файлы
76,88 и 111,48 МиБ: два 64-МиБ entry guards, 16-МиБ limits секций и настоящие
BGRA mip4096²/3000². Исторический полный прогон меньшего файла достиг 1015,77 МиБ;
это не новый native benchmark. Простое поднятие общего лимита не решает вопроса.
Аудит и план отдельного supervised load находятся в `large-container-audit/`.
Ни limits, ни файлы не менялись; полная перепаковка таких уровней отложена.
