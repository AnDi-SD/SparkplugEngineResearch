# Граница записи FFPS/FAT: предложение общей host-реализации

**Статус: предложение, реализация ждёт решения пользователя по правилу 2.**
Перенос трёх writers ниже приостановлен; независимые scalar-задачи продолжаются.
Основание — [правила разработки](../engine/software-development-rules.md):
единая игровая реализация должна опираться на восстановленный оригинал;
замена своим алгоритмом требует отдельного согласования.

## Три production-дубля

| Код | Использование |
| --- | --- |
| [SmoMutationTransaction.BuildContainer](../../tools/SmoViewer/SmoViewer.Editing/SmoMutationTransaction.cs) | Editing/Importer: изменение размеров полей, перенос offsets и вложенных размеров |
| [SmoVisualForestInjector.CreateContainer](../../tools/SmoImporter/SmoImporter.Core/SmoVisualForestInjector.cs) | Importer: добавление, удаление и замена inline/shared ветвей |
| [SmoProjectSerializer.Write](../../tools/SmoLVLcreator/SmoLVLcreator.Core/SmoProject.cs) | LVLcreator: сохранение в Stream и промежуточные документы preview/importer |

Все три независимо собирают header, count, записи
`ID/u16 rawNameLength/rawName/class/offset/size` и нулевую file table.
Повторные расчёты размера таблицы в LVLcreator обслуживают тот же writer,
а не являются дополнительными реализациями записи.

## Почему существующий общий API не заменяет их напрямую

В [spResourceFATSerializer.h](../../Sparkplug/Code/Sparkplug/spResourceFATSerializer.h)
`WriteInlineIndexForAnalysis` прямо обозначен **HOST encoder** подтверждённой
reader grammar. Original whole-file/index writer не найден.
[BuildResourceFileForAnalysis](../../Sparkplug/Code/Sparkplug/spSerializerManager.h)
также обозначен host producer вокруг восстановленных index/reference/payload
contracts, а не найденное тело original SaveResources.

Этот producer заново индексирует actual graph и требует поддержанных writers.
Он не сохраняет произвольные исходные ID, неизвестные payload и skipped fields.
FAT name writer использует C-string контракт с завершающим NUL, тогда как
редакторы сохраняют точные raw name bytes. Поэтому прямая подмена меняет
lossless-контракт. Код внутри `Sparkplug/` не становится оригинальной игровой
реализацией только из-за своего расположения.

## Что установил поиск

Контрольные SHA256:

- PC `WinxClub.exe`:
  `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
- PS2 `SLES_532.19`:
  `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

Исторический PC поиск рассмотрел save indexing, header/reference writer,
FAT cursors и manager load-neighborhood. У header `467260` найден только caller
`467468` внутри reference writer; cursor `465F00/465F20` имеет load callers.
Это отражено в [read-reference](native-pc-read-reference.md) и
[save-reference](native-pc-save-reference.md). Подтверждённые отдельные
index/reference методы не доказывают whole-file writer.

Новый статический PS2 проход ограничен **`[181D70,183150)`**, 5088 байт / 1272
выровненных слова. Все пять stores с displacement `+14` разобраны по владельцу:

- `182270`, `18275C`, `183110` записывают в manager.operationMask только `1`;
- `1823AC`, `18289C` меняют stream origin, а не manager;
- constructor-константа `2` записывается по `+18` — serialization policy;
- вызовы на `1821BC`/`1826A8` с размером 28 байт используют PS2 stream slot
  `+38` (`ReadData`), а не `+3C` (`WriteData`).

Save-кандидатов в участке нет, поэтому поиск на callers/другие регионы не
расширялся. Это **не доказательство отсутствия** writer в других участках,
сборках/exporter-е, косвенном или защищённом коде. Guest execution и повторов
прежних capped probes не было. R5900 LQ/SQ декодированы явно; COP2/MMI оставлены
raw words, без подмены инструкциями другого MIPS ISA.

Доказательства: [локальный аудит](../../local-data/results/tools-core-cycle-20260910-0700/fat-writer-audit/audit.md),
[структурированный результат](../../local-data/results/tools-core-cycle-20260910-0700/fat-writer-audit/audit.json),
[raw disassembly](../../local-data/results/tools-core-cycle-20260910-0700/fat-writer-audit/manager-region.disasm.txt),
[manifest](../../local-data/results/tools-core-cycle-20260910-0700/fat-writer-audit/manifest.json).
Это локальные артефакты вне Git; manifest включает raw bytes, capture script и
хеши. SHA256 `audit.json`:
`7F3300EF76691A8BB159BFED7E0544B7CDEBB7EFE2394695339C3197962045EA`.

## Предлагаемая замена и проверка

Согласовать один явно обозначенный **общий HOST encoder envelope** в host/analysis
слое. Он принимает семь исходных header words, готовые ID/class/offset/size и
точные raw name bytes; записывает count/index и существующую нулевую file table,
рассчитывает data offset и размеры файла. Не придумывается original имя класса.

Граница первой замены — три перечисленных writers и их расчёты размера таблицы.
Порядок записей, ID, header tag/version/platform, raw names и opaque data section
сохраняются. Выбор объектов, relocation, размеры вложенных references и игровые
payload остаются за соответствующими операциями и общими восстановленными
writers. External file tables, новые caps и ранее отложенные lossless случаи
в эту задачу не входят.

Encoder должен поддерживать prefix/Stream запись отдельно от data section.
LVLcreator сохраняет последовательную запись, без дополнительной полной native
копии контейнера. Существующий host FAT encoder также следует подключить к
общему core в допустимом направлении зависимостей; четвёртая реализация не нужна.
`BuildResourceFileForAnalysis` остаётся отдельной orchestration actual graph.
Ожидаемый выигрыш — одна поддерживаемая реализация; скорость требует замера.

Проверка после согласования:

1. Побайтное сравнение с зафиксированными outputs нынешних writers: одна
   size-changing Editing операция, Importer insertion/removal и LVLcreator
   save/preview. Отдельные preservation cases для raw names и opaque bytes.
2. Сверка FAT metadata общим reader и затронутых ссылок actual trace;
   skipped payload не считается прочитанным.
3. Адресная загрузка небольших штатных SMO/SAN оригинальным PC reader по
   существующему bounded profile: metadata, runtime class/ownership и результат.
   Это проверка совместимости выхода с игрой, не original writer semantics.
   При protection limit остановить пример без автоматического повышения caps.
4. Замер wall time и managed/native allocations на тех же операциях, отдельно
   streaming и intermediate document. Большой >64 МиБ whole graph требует
   отдельного ранее описанного memory profile.

Новая реализация отсутствует. Пока решение не принято, FAT migration остаётся
на паузе; общий Renderable scalar writer и другие независимые срезы доступны
для дальнейшего переноса.
