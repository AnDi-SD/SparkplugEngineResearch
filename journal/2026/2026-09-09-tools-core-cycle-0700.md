# 9–10 сентября 2026: ядра tools, цикл до 07:00

- 19:35 МСК: запуск по поручению пользователя; root c99abca, Viewer ddd1c52.
- Первое направление — spatial inspectors на actual ResourceGraph; независимый
  аудит Light выполняется параллельно. Правила и редакторские отсрочки сохранены.
- План и последующие результаты: [цикл](../../docs/research/tools-core-cycle-2026-09-10-0700.md).
- 20:03–20:10: spatial ABI5578/managed15076 checks;14 archived original/source
  states повторно совпали,2 pristine уровня покрыли все8 классов. Light6 fresh
  original/ABI +46 initial managed;5consumer builds. Original FF9ED6FF исправил
  ошибку ожидания старого теста (green3F56D6D8), восстановленный код не менялся.
- Явная несовместимость старых PC/PS2 Corpus reruns сообщена; до новых отдельных
  профилей guard предотвращает перезапись historical evidence.
- Параллельный Sphere/Box реверс дал18 successful actual-PC captures; доказан
  Box rounding defect. Реализация следующего блока ещё выполняется.
- 20:13 МСК: первый блок проверен: Light48 managed, общий FormatTests647;
  актуальные5 consumer builds зелёные. Готовится локальный commit; цикл продолжается.
- 20:37–20:44 МСК: второй блок — actual Sphere/Box и общий OBB scalar reader.
  C++ совпал с 18+11 original cases; пять native suites прошли. Удалены три C#
  scalar/size реализации; 35 ABI rows и 166 managed checks, FormatTests647.
  Viewer GUI/FormatTests и Exporter FormatTests собраны без замечаний.
- Vase/Qc whole graphs загружаются. Blooming_flower останавливается раньше Box
  на прежнем source-less TextureData; причина и offsets зафиксированы отдельно,
  не добавлен искусственный source-wrapper. Пользователь уведомлён.
- Следующий блок — Model/Material selection и authoring reference provenance
  Importer/LVLcreator. Аудит обнаружил host loader limit 64 МиБ при сценариях
  80+ МиБ; сообщено, обходной C# parser не вводится, нужны замеры памяти.
- 20:48 МСК: второй блок сохранён root7ccb2ee / Vieweraf69d81.
- 20:57–21:22 МСК: actual Model/Material selection —3 replacements/50 checks;
  optional reader trace —5 native suites, ABI121348. Exact remap —42 managed
  checks, включая Skin palette и raw field, похожий на ID. Пять consumer builds
  прошли. LVL isolated worker создал7 объектов/2 forests, catalog4273 передан
  один раз, source Alfea02 не изменился. Готовится третий checkpoint.
- Alfea01 cached TextureData1848 не имеет payload-read coverage: его raw branch
  remap приостановлен, пользователь уведомлён. Large-file audit отдельно
  подтвердил исторический1015,77MiB peak и дополнительные16MiB guards; limits
  не менялись. Следующий блок — общая запись скалярных Material параметров.
- 21:27 МСК: третий блок сохранён rootfda6b91 / Viewer3ebdfd0.
- 21:32–21:48 МСК: четвёртый блок — общие Material scalar writer slices и
  перенос двух Importer callers. Native5 suites, ABI4/33, managed44, remap60,
  общий FormatTests647; пять consumer builds прошли. Icy end-to-end пересобран
  в101 объект/13110B через BuildMaterial, original file не изменён.
- CaptureRanges проверяет source SHA один раз на batch. На20 covered extents
  pristine Alfea02:94,866→5,585мс (16,99× для capture operation), peak58,30MiB.
  Полное время импорта таким коэффициентом не оценивается. Unknown DX power
  и неоднозначные single-layer LTS формы остаются явными ограничениями.
- 21:51 МСК: block4 сохранён root8b77ac6 / Viewercaca0d6.
- 21:52–22:02 МСК: block5 переносит Renderable sort/priority в BuildSkin и
  исправляет выбор одинаковых field IDs из неправильной секции. Native5 suites,
  managed44+44, Icy13, FormatTests647,5 consumer builds; ABI6/19 +2 Material
  regressions. Icy output совпал побайтно сblock4.
- Свободно704084KiB RAM; дополнительные .NET/MSBuild принадлежат VS Code
  пользователя, их не останавливали. Managed block5 —1 worker.
- FAT boundary сообщён пользователю. Узкий PS2 manager-region не содержит
  save-кандидата; подготовлено предложение общей HOST записи envelope для
  трёх существующих production writers. Реализация ждёт решения пользователя.
- 22:07 МСК: block5 сохранён rooteff1788 / Viewer8104dba.
- 22:12–22:30 МСК: block6 — actual Skin palette writer и FAT prebinding:
  2 fresh PC cases/40checks, native243/110/298/213/61, ABI6writes/14guards/4locations.
  Один actual graph на все palette writes в Inject; ручная запись и zero-weight
  heuristic удалены. Five consumer builds/managed integration ещё выполняются.
- 22:34 МСК: пользователь сократил текущий цикл до **23:00 МСК 9 сентября**.
  Основной приоритет до остановки — завершить и сохранить Skin palette блок,
  затем небольшой перенос reserved field headers, если хватает времени.
- 22:36 МСК: block6 проверки завершены: пять consumer builds0warnings/errors,
  managed palette35/Renderable44/Icy13/FormatTests647. Icy output побайтно прежний;
  palette regression peak32,39MiB. Whole graph requirement и редкие формы
  сообщены пользователю; готовится локальный checkpoint.
- 22:37 МСК: block6 сохранён root7d12c24 / Viewerd53bb78.
- 22:38–22:46 МСК: block7 — общие reserved headers вместо трёх resize-switch,
  capacity table и пяти ручных header builders. Header118, Collision Alfea02,
  Model50, Icy13 и FormatTests647;5 builds прошли. Все5 integration outputs
  побайтно прежние. Native source/DLL не изменялись;10 original deps проверены.
- 22:50 МСК: block7 сохранён root34fd921 / Viewer21b43a5.
- 22:51–22:56 МСК: Text static audit подтвердил raw byte strings и UInt32 wrap,
  ошибочно трактуемые старым C# parser как UTF-16/Single. Font original factory
  оставляет baseline неинициализированным; одна fresh micro проба прошла,
 4512B освобождены, caps не повышались. Concrete классы ещё не подключены.
- Проверены SHA525 immutable локальных reports из7 блоков и61 относительная
  ссылка в изменённых документах;3,92с, без corpus rerun. Завершается отчёт.
- 22:58:22 МСК: код и исследование остановлены; активных сборок/guest нет.
  Завершающая фиксация отчёта к23:00, цель готовности всех ядер остаётся незавершённой.
