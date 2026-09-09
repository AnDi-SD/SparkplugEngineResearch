# Цикл переноса ядер tools до 07:00 МСК 10 сентября 2026

Начат 9 сентября около 19:35 МСК по поручению пользователя. Первоначальный срок —
10 сентября 07:00 МСК; около22:34 пользователь сократил его до **9 сентября
23:00 МСК**. Имена ранее созданных dossiers/артефактов сохраняются для evidence.
Исходная точка: root `c99abca`, Viewer `ddd1c52`,
TextureTool `1ddb50f`; рабочее дерево чистое. Это цикл разработки, не выпуск.

Приоритет: рабочие ядра всех приложений `tools/`, использующие общие
восстановленные классы. Интерфейсы меняются только для необходимого подключения.
Действуют [правила разработки](../engine/software-development-rules.md) и
[манифест](research-manifesto.md). Ранее отложенные редакторские случаи сохраняются.

## Очередь и методика

1. Заменить остаточные пространственные инспекторы C# проекцией actual classes
   из общего ResourceGraph; не переносить их старые ограничения порядка полей.
2. Подключить Light и простые bounding volumes, затем texture/text inspection
   в минимальном объёме, нужном потребителям.
3. Продолжать аудит записи графа и runtime ядер; необходимые пробелы изучать
   адресно по PC и полезному PS2-аналогу.

Независимые чтения/аудиты делегируются параллельно; изменения общих мостов
и сборки координируются. Общий бюджет процессов около 1 ГиБ. Проверки — на
представителях новых ветвей и соответствующем полном сценарии, без повторного
прохода тысяч неизменённых файлов. Локальные артефакты нового цикла:
`local-data/results/tools-core-cycle-20260910-0700/` (вне Git).

Предыдущая оценка около 60% (диапазон 55–65%) была инженерной оценкой, а не
измерением всех ядер. Счётчик 39 проверенных этапов не является процентом.
На старте 35 C# decoder-модулей: 18 с основным чтением в общем коде,
4 смешанных, 13 с самостоятельным чтением. Это отдельный показатель readers,
не готовность всех runtime и authoring операций.

## Выполненные этапы

Цикл выполняется. **Блок1:** восемь spatial inspectors и Light переведены на
actual ResourceGraph/shared reader. Native suites Spatial97/Light205/FullLoader213;
spatial ABI5578 и managed15 076 checks на560 spatial объектах; Light:
шесть fresh original/ABI matches, отдельное сравнение цвета и48 managed checks.
Пять зависимых проектов собраны, общий FormatTests на одном реальном файле:
647 assertions. Подробности: [spatial](tool-spatial-inspection-shared-core-2026-09-10.md)
и [Light](tool-light-inspection-shared-core-2026-09-10.md).

Срез именно35 decoder-модулей после блока1:27 с основным чтением в общем коде
(77,1%),3 смешанных и5 с самостоятельным чтением. Это не процент всех ядер:
runtime/authoring и блокеры из прежнего среза остаются. Блок1 закрывает один
связный этап из новой серии; девять модулей не считаются девятью готовыми ядрами.

**Блок 2:** Sphere/Box/OBB подключены к общему коду и инспектору. Восстановлены
нужные части actual Sphere/Box; текущий C++ совпал с 18 original-PC cases.
OBB — ещё 11 fresh original/source cases. Доказанная ошибка округления общего
Box/OBB size helper исправлена по PC: `(0.1f,0.1f,1.1f)` даёт радиус `3F0DF579`.
Пять native suites прошли; 35 scalar rows, 196 ABI/source и 166 managed checks;
общий FormatTests — 647. Viewer FormatTests, Viewer GUI и Exporter FormatTests
собраны без ошибок/предупреждений. Whole graph vase/Qc — 77/50 объектов;
blooming_flower остаётся отдельным source-less texture случаем. Подробности:
[Sphere/Box](tool-simple-bv-shared-core-2026-09-10.md) и
[OBB](tool-obb-inspection-shared-core-2026-09-10.md).

Срез 35 decoder-модулей после блока 2: **30** с основным чтением в общем коде
(85,7%), **3** смешанных, **2** самостоятельных. Это процент модулей чтения,
не готовности всех ядер. Следующий крупный участок — использование настоящих
Model/Material связей и общей записи графа в Importer/LVLcreator; основные
игровые writers уже доступны, повторный широкий реверс не требуется.

**Блок 3:** Model/Material selection использует actual links; три полные замены
прошли 50 checks. Удалена эвристика remap восьмибайтовых полей: общий reader
фиксирует точные reference sites и coverage, C# только перемещает подтверждённые
ID words. 42 managed remap checks; worker LVLcreator создал две ветви / 7 объектов
и передал общий catalog один раз. ABI: 121348 checks на трёх файлах; с trace и без
него совпали 4538 объектов и 537 узлов. Пять native suites и пять consumer builds
прошли; общий FormatTests647. [Подробности](tool-authoring-reference-trace-2026-09-10.md).
Это две функции authoring, а не завершение всей записи графов. Счётчик35 readers
остаётся30/3/2; следующий срез — оставшиеся ручные Material builders.

**Блок 4:** MRS/pass-blend/LTS запись в двух Importer сценариях передана общему
Material writer. Native5 suites, ABI4 positive/33 guards, managed44, Icy
end-to-end101 objects/13110B; пять consumer builds и FormatTests647 прошли.
Пакетная подготовка reference ranges удаляет повторные whole-file SHA: на20
маленьких диапазонах Alfea02 измерено94,866→5,585мс (16,99 раза для этой операции).
Reference remap60 checks сохраняют coverage, порядок и отказ при mutation source.
[Подробности](tool-material-scalar-authoring-2026-09-10.md).
Остальная структурная запись Material/Skin/контейнера ещё не завершена;
35-reader counter по-прежнему30/3/2.

**Блок 5:** Skin authoring использует общие Renderable alpha-sort/priority
writer slices. Исправлена ошибка нашего builder: номера2/3 больше не выбираются
из соседних Model/Skin sections. Native5 suites, managed44+Material44,
Icy13 с побайтным совпадением прежнего результата, FormatTests647;5 builds.
ABI6 Skin cases/19 guards/2 Material regressions. [Подробности](tool-renderable-scalar-authoring-2026-09-10.md).

**Блок6:** Skin palette записывается actual `spSkinSerializer` с actual Node
owners; ручные header/count/ID/matrix writes и zero-weight heuristic удалены.
Две свежие PC пробы подтвердили prebinding исходных ID:40checks. Native5 suites,
ABI6writes/14guards/4locations, managed35+Renderable44, Icy13 и FormatTests647;
пять consumer builds прошли. Icy output побайтно прежний, граф загружается один
раз на все ветви Inject. [Подробности](tool-skin-palette-authoring-2026-09-10.md).
Новый writer требует whole graph load и ровно одного canonical palette field;
rare header/repeated shapes явно отклоняются. Reader counter остаётся30/3/2.

**Блок7:** три resize-switch, отдельная C# capacity table и пять ручных UInt32
header builders переведены на общий header writer. В in-place API запрещено
расширение reservation; обычный BuildHeader сохраняет fallback. Header118,
Collision Alfea02, Model50, Icy13, FormatTests647 и5 consumer builds прошли.
Все5 полных integration outputs побайтно прежние. Native код/DLL не менялись;
10 original dependencies и неизменность writer перепроверены.
[Подробности](tool-reserved-header-authoring-2026-09-09.md).

После7 блоков выполнен [нужный Text аудит](tool-text-inspection-audit-2026-09-09.md):
24 PC regions/5884B подтвердили ошибочную UTF-16/Single интерпретацию в C#.
TextNode использует RenderNode serializer. Одна fresh Font factory/dtor проба
установила неинициализированный baseline; allocation4512B полностью освобождён,
caps сохранены. Actual Font/Text перенос ещё не реализован.

Сжатый [итоговый отчёт к23:00](tools-core-cycle-report-2026-09-09-2300.md) отдельно
фиксирует готовность, ограничения и дальнейшую очередь.

## Отложенные случаи

Сохраняется список из [предыдущего среза](tools-core-migration-status-2026-09-09.md).
Новых исключений из правил при запуске не принято. При переносе spatial
инспекторов приостановлены восемь старых combined PC/PS2 Corpus-профилей:
новый PC runtime не доказывает PS2 equivalence. Guard срабатывает до записи БД,
архивные данные доступны. Подробности и предложение разделить операции —
[spatial dossier](tool-spatial-inspection-shared-core-2026-09-10.md).

`blooming_flower.smo` упирается в прежний source-less TextureData контракт
CP115–116; его успешно проверенный Box scalar не считается whole-file load.
Аудит Importer также выявил несовпадение диапазона применения: общий ResourceGraph
ограничен 64 МиБ, тогда как редактор рассчитан в том числе на 80+ МиБ уровни.
Пока операция использует явный LoadIssue; старый C# обход не добавляется.
Следующий шаг для больших входов — измерить объём actual graph и обосновать
допустимый host limit в пределах бюджета памяти. Read-only аудит нашёл также
16-МиБ ограничения секций/текстур и исторический peak1015,77 МиБ; поднятие лимита
без отдельного supervised profile не выполняется.

Для Alfea01 выявлен конкретный непрочитанный cached TextureData1848 `marble`.
Trace не выдаёт skipped bytes за coverage; remap содержащей их ветви приостановлен.
Нужны подтверждённое чтение именно этой копии либо явно согласованная замена
ветви через общий writer. Прочие покрытые ветви продолжают работать.

Полный Material writer для шаблонов без инициализированного original DX power
остаётся отложенным. Частичные scalar writers это значение не читают и не
подставляют default. Single-layer authoring для legacy/orphan/repeated LTS
требует отдельного определения операции; такие формы явно отклоняются.

Три независимых FAT/envelope writers пока не перенесены: существующий общий
producer является HOST реализацией и меняет lossless-контракт. Узкий PC/PS2
поиск не дал original whole/index writer; отсутствие во всём EXE не утверждается.
Подготовлено [конкретное предложение общего host encoder](tool-container-writer-boundary-2026-09-10.md),
ожидающее решения пользователя по правилу2. До этого независимые ядра продолжаются.
