# 2026-09-06 — ускорение SMO/SAN research, PC/PS2 отчёт до12:00

Старт **07:24:40 UTC /10:24:40 МСК**, deadline **09:00 UTC /12:00 МСК**.
Пользователь согласовал автоматизацию паспортов/повторяющейся работы,
dependency-first приоритет общих блокеров, representative corpus tests,
слои регрессии и независимый backlog неизвестных исходных имён. Новый запрос:
отчёт отдельно PC и PS2. Это новый цикл, не продолжение уже закрытого до10:00.

План текущего цикла:

1. Независимый platform ledger и отчёт по all/engine/game/direct SMO/SAN.
   Старые combined проценты не делить арифметически; неоценённое явно показывать.
2. Единый read-only паспорт класса/общих зависимостей/неизвестного и выбранных
   тестов, повторяемый runner с ограничениями; source names не выдумывать.
3. Испытание на общем blocking helper45E870 и связанных Visibility inputs:
   свежие статические/caller evidence, не resume capped native calls и не
   увеличение лимитов; если helper остаётся закрыт, зафиксировать точную границу.
4. Сохранить результаты в source/evidence/DB/docs, проверить affected tests и
   общий checkpoint, дать раздельный PC/PS2 отчёт к12:00.

Состояние старой canonical базы:31 imports/1667 evidence/124 snapshots,
legacy combined all15,44579082%, engine29,71045576%, game2,5%, direct59,02702703%.
Каталог784 (PC733,PS2681), fixed direct37. Эти historical combined оценки
сохраняются отдельно. Новый platform ledger не засчитывает PC evidence на PS2;
миграция старых знаний не является новым reverse progress.

Ограничения прежние: PC-first/PS2 second tier, original/inferred paths,
dirty worktree сохранять; только bounded guest/read-only/portable tests,
100k instructions/2sec per call и30sec child, no game/apps/assets/commit/push/
release/платных услуг. Token budget отсутствует. Предыдущие тесты завершены;
HEAD7c2b615 и submodule revisions не переключаются.

## Checkpoint до11:30 МСК /08:30 UTC

Новая оснастка:

- [workbench и методика PC/PS2](../../docs/research/native-research-workbench.md):
  read-only паспорта из native DB, явная logical dependency очередь4 work items,
  behavior/name/path/ABI gaps, finite test profiles/fresh children/deadline gate;
- независимый `native_platform_*` ledger:3 immutable imports,109 latest
  assessments,24 snapshots; schema остаётся5, legacy31/1667/124 не переписаны;
-54 bootstrap и55 wire/history records — **миграция прежнего знания**, не
  reverse progress. PC71/PS2 38 assessed; игровая логика пока не разнесена;
- [selector](../../research/native_specimens.py) выбирает по готовым class variants;
  SQLite2s/2M VDBE per query, без new indexes/full rescan. File-first join исправил
  type-first caps без повышения ограничений.11 PC SMO/16 variant witnesses/
  159 fingerprint/identity/field checks;5 вариантов не выбраны, не засчитаны.

Новый PC reverse:

- [plane-storage source/evidence](../../docs/research/native-pc-visibility-plane-storage.md):
  resize/reuse/clear, floor1.5 capacity growth,17-of20-byte value copy,
  raw all-enabled/count semantics, release, отдельный45E530 copy-constructor;
- native outer-stack46C350 append/relocation с independent nested ownership,
  spare capacity и seventh aliased append прошёл без входа в45E870;
- исходный allocator header `z:\sparkplug\code\sparkbase\spSTL_allocator.h`
  подтверждён; source name плоскостей не выдуман;
-190 directed native checks5 fresh children,28 static anchors,
 17569 differential fields64 cases/576 operations. Portable source17 unit checks;
  full **CTest22/22 PASS**, около24s;
- existing neighbour profiles:static71,visibility39+Octree19+8,
  memberships94; SAN4 files234+780+909+909=2832 original-reader/portable PRS checks;
- прежние capped45E870/full ctor46C0F0 не повторялись/не продолжались и не
  заменялись фальшивым успехом. Complete near-plane portal/full Scene pipeline
  остаются open. Игра/GPU/OS не запускались.

Новый class-score credit только PC: Visibility67→72 и ZonePortal73→74,
то есть0,06 class-equivalent units; directSMO/SAN59,027027→59,054054%.
Это не2 завершённых класса. PS2 новый reverse credit0;35 отдельно подтверждённых
старых serializer/wire baselines25 дают23,648649% recorded direct credit,
2 direct classes unrated. PC total/engine6,20%/13,81% и PS2 total/engine1,67%/4,13%
— **неполный внесённый исторический зачёт**, не полная оценка изученности EXE.
Game UNRATED не означает0% изученности; старый mixed game2,5% сохранён отдельно.

До12:00 продолжаются контрольный аудит, boundary regressions и оформление.

## Контрольная проверка после11:30 МСК

- Добавлен воспроизводимый профиль `spatial-differential`:9 последовательных
  children, суммарно89,634s. Octree3104 + BSP2046 + polygon clip3842 +
  Visibility1483 = **10475/10475** comparisons. Это повторная проверка
  прежних контрактов, без дополнительного class-score credit.
- В portable plane-storage suite добавлены проверки явно host-only guards,
  сохранности состояния при отказе и by-value fill при alias/reallocation:
  теперь **26/26**. Ошибочный обрезанный batch input отклоняется явно,
  **2/2** отрицательных проверок; неисследованное native error behavior
  этим не закрывается. `allocatorWord` дополнительно подписан как рабочее имя.
- Финальный после этих изменений CTest: **22/22 PASS**,4,11s на текущем
  локальном запуске. Python unittest: **46/46 PASS**,8,34s; в том числе
  неизвестная версия manifest/пустой ID не изменяют in-memory ledger.
- Оба аудита базы прошли:3 immutable platform imports/109 latest assessments,
  прежние31 imports/1667 evidence/124 snapshots сохранены; native-only FK clean.
  Сверены279 ссылок нового набора документов и500 ссылок прежнего цикла.
- Проба чтения паспортов: первые5 в уже работающем Python процессе за7,229ms;
 20 последующих — median0,438ms/max0,628ms на паспорт, без DB writes.
  Это **локальное cache/in-process измерение**, без стоимости старта Python,
  и не доказательство ускорения всего исследования в заданное число раз.

### Отдельно PC

Новое поведение исследовано у plane storage и его вызывающих участков
Visibility/ZonePortal. Подготовлен частичный C++ helper с тестами, но целиком
новый класс в этом цикле не объявляется завершённым. Изменение оценок:
Visibility67→72, ZonePortal73→74; direct SMO/SAN59,027027→59,054054%.
Проверены11 SMO структурно и4 SAN через native reader/portable PRS, не в игре.

### Отдельно PS2

Новых запусков PS2-кода, reconstructed source или нового reverse credit нет.
Проверены и перенесены ранее записанные независимые PS2 evidence;
PC runtime не зачислен PS2. Direct SMO/SAN:35 оценённых классов из37,
recorded credit23,648649%; MaterialColorController и spAnimation остаются unrated.

### Состояние отдельного учёта на checkpoint

| Область | PC: внесённый зачёт; оценено | PS2: внесённый зачёт; оценено |
|---|---|---|
| Весь каталог EXE |6,197817%;71/733|1,666667%;38/681|
| Логика движка |13,808511%;71/329|4,127273%;38/275|
| Логика игры |UNRATED;0/404|UNRATED;0/406|
| Direct project SMO/SAN |59,054054%;37/37|23,648649%;35/37|

**Первые две строки не являются полной исторической оценкой изученности:**
перенос старых сведений по платформам ещё не завершён. UNRATED означает
отсутствие отдельной оценки, а не отсутствие исследования. Старые mixed
проценты сохранены, арифметически между платформами не делились.
Методика и denominators описаны в [workbench](../../docs/research/native-research-workbench.md).

### Вывод о тестовом ускорении

Работает автоматизация получения уже известных сведений, выбора вариантов
и ограниченных проверок; исследовательский порядок фиксируется явно.
Главное ограничение осталось техническим: protected45E870 и46C0F0.
Ни новое имя класса, ни подстановка соседнего copy-constructor эту неизвестность
не устраняют. Следующий связанный этап — новые доступные caller/data evidence
этой границы и конкретные Collision/FFPS-FAT consumers по очереди, без повторного
разгона capped calls. Полный mesh→Scene→GPU путь пока не подтверждён.

Дополнительная проверка безопасности selector/verify после checkpoint:
file/aggregate budgets теперь проверяются до чтения, само чтение bounded даже
при росте файла после stat; кэш исходных байтов32MiB, один sentinel byte сверх
предела допускается только для обнаружения превышения. Verify field cap32
согласован с selector. Добавлены5 отрицательных проверок; итог Python
**51/51 PASS**,9,414s. Реальный сохранённый набор снова **159/159 PASS**,
5 невыбранных вариантов по-прежнему не засчитаны. Игровые файлы не изменялись.

Перед окончанием цикла повторён новый deadline-aware профиль
`plane-storage-differential`: **8/8 children,17569/17569 fields**, без caps
и без новых score increments за повтор. Workbench9/9 перепроверен после
добавления профиля и явной метки shared corpus counts; platform score не
подменяется общим количеством asset instances.

Последний аудит:280 ссылок нового набора/503 ссылки прежних34 документов,
оба ledger replay/FK PASS. `git diff --check` чистый, приложение `tools/`
без изменений, HEAD7c2b615 и оба submodule revisions сохранены. При проверке
около11:58 МСК (08:58 UTC) активных research processes0.
Игра, GPU, PS2 executable, commit/push/release в этом цикле не запускались.

## Завершение

Цикл остановлен к12:00 МСК; время финальной отметки09:00:13 UTC /12:00:13 МСК.
Новые исследования после deadline не начинаются. Последний общий Python
прогон51/51 PASS (9,797s), whitespace check PASS, активных research processes0.
Раздельные PC/PS2 результаты и незакрытые границы приведены выше; дальнейшая
работа требует следующего согласованного цикла.
