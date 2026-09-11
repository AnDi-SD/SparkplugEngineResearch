# Text geometry и материалы FontManager, 11 сентября 2026

Блок14 цикла до19:00. Общие классы генерируют настоящие PC text vertices/UV/
цвета и triangle indices. Одновременно исправлены две старые неточности
FontManager: неверный тип owning-полей и фиктивная успешная инициализация.
GPU consumer будет подключён следующим блоком; полного Text renderer пока нет.

## Оригинальное поведение и исправление реконструкции

PC41E8B0 создаёт DXMaterial4A9460, pass45F610 с Blend2, один StdLayer460DB0;
texture states1/2 получают3. Material state8=2, state3=0, state9=1,
flags6C/6D=1. Две ссылки manager34/38 указывают на один material, refcount2.
Material имеет188 bytes; specular power+B8 остаётся неинициализированным.
Это **не Font**. Actual factory/initializer/destructor выполнены в свежем probe.
Теперь типы API и обе ABI-таблицы называют эти поля primary/fallback Material.

PS2 static cross-check:168620 вызывает PS2Material producer1F2A90, создаёт
pass170320 и layer170BE0, задаёт те же conceptual states/flags, сохраняет
материал в34/30. Vtables4916E0/491704 совпадают с подтверждённым PS2Material.
Это дополнительная статическая проверка структуры, не PS2 geometry execution.
PS2 material color/default reconstruction остаётся отдельной границей.

PC destructor41FB90 сначала освобождает записи registry, затем fallback38 и
primary34, затем storage. Общий код сохраняет этот порядок release edges.
PC4C3830 после material initialization ещё вызывает4C36A0 и заполняет borrowed
Font28/2C. Эта стадия не выполнялась прежним кодом, несмотря на возвращаемые
true/readiness=true. Теперь `InitializePCMaterialForAnalysis` явно закрывает
только подтверждённую material-часть. Полный PC initialize оставляет этот prefix,
возвращает false и не объявляет системный Font готовым. Неспециализированный
initialize также не выдумывает успешный platform producer. Инструменты с
загруженным явным Font не зависят от системного шрифта.

## Генератор

`spFontManager::BuildPCText3DGeometryForAnalysis` восстанавливает CPU body
41F3C0. NULL/empty возвращают пустой результат до проверки Font. Для остальных
нужны выбранный Font и назначенный baseline. Char20 — только advance; newline
сдвигает строку поZ на height; прочие controls ниже20 не рисуются. Glyphs20..FF
имеют24-byte vertex (XYZ,ARGB,UV), четыре vertices и indices012/132.

Z сначала усечён к Int32; topZ=Z−baseline+height, bottom=top−height. Integer
переполнения сохраняются через UInt32 storage/signed interpretation. X и UV
сохраняют наблюдённые x87 float spills. Wrap сравнивается **signed** и откатывает
выход к началу слова; это иной consumer, чем unsigned character measurement.
Whitespace snapshots и strip winding не выведены из предположения о layout.

`spTextRenderable::BuildPCGeometryForAnalysis` использует выбранные Font/color,
cached measuredWidth и alignment:1→−width/2,2→−width,прочие→0. В частности,
unknown alignment в drawing отличается от bounds; исходная странность сохранена.
Text alpha gate437CB0 зависит от собственного AlphaSort независимо от material/
blend gate Model. Общий virtual predicate теперь отражает это различие.

Генератор ограничен65535 input bytes,4096 glyphs,8×(length+1) loop iterations
и finite Int32 Z. Выход публикуется атомарно. Это явные host bounds.
Original `ABAB`,wrap5 не завершился за100000 instructions: loop возвращается
к началу слишком длинного слова. Процесс остановлен, guest не возобновлялся.
Общий код возвращает `TEXT_GEOMETRY_ITERATION_LIMIT`, а не иной перенос строки.

## Проверки

Raw: `local-data/results/tools-core-cycle-20260911-1900/text-geometry/`.
Манифест: `research/tools-core-text-geometry-2026-09-11.json`.

- Final original-run3:58 checks,11 geometry cases,1,242s,arena10096 bytes.
  Actual Font/manager/material factories, reader, initialization, glyph lookup,
  geometry и teardown; каждый tracked object освобождён.
- Renderer buffer acquisition/setup/draw — три объявленных platform callbacks.
  Text calls получают явный renderer44=1 (alpha dispatch phase), без утверждения
  о выполнении полной visibility/queue/frame. Direct FontManager cases обходятся
  без этой предпосылки. Никаких D3D/OS/GPU вызовов оригинала на хосте.
- FTG1 golden fixture7004 bytes,
  SHA256`A8E211A68086892D7AB8EBECF356A7871863AA34795A5C5A47142802C0C829B2`.
  2484 output bytes (vertices/indices) совпадают побайтно; material words
  проверены отдельно с unknown-power presence, без принятияCC за default.
- Native324 checks; прежние TextRuntime672 и FontSerialization прошли.
  Final TextGeometry + affected EngineCore suites2/2 прошли. Большой EngineCore
  запускался из-за изменившихся typed APIs/ABI labels, без полного SMO corpus.
- Menu graph372 и Icy graph131 C ABI checks прошли на final DLL.

Initial probe-run2 ожидал draw до alpha-dispatch phase; original корректно
поставил Text в очередь, поэтому тест не нашёл draw event. Уточнён fixture,
игровой gate не подменён. Два C4244 warnings в новом triangle literal устранены
явным UInt16 array; final native build без этих предупреждений.

## Открыто

GPU Text, material consumer с неинициализированным и неиспользуемым power,
полный системный Font producer, callbacks/full frame и PS2 geometry execution.
Default Font material power нельзя объявлять0: renderer должен переносить
unknown явно и не использовать его в нелитой Text ветке. EXE score не менялся;
этот блок измеряет готовность нужного общего кода и проверку по оригиналу.
