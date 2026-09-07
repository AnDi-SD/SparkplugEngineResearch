# Общая база файлов и ресурсов игры

С 1 сентября 2026 года основной исследовательский индекс
`local-data/results/smo-corpus-v2.sqlite` описывает не только SMO, а все файлы
зарегистрированных PC- и PS2-корпусов. Это локальный производный артефакт: он
исключён из Git, не содержит полных копий игровых payload и может быть заново
собран из указанных в `corpora` источников.

Старый подробный слой SMO сохранён без изменения смысла. Общий ресурсный слой
добавлен поверх него в schema v4; поэтому исследования классов, полей и runtime
evidence продолжают работать вместе с инвентарём соседних форматов.

С 5 сентября schema v5 также содержит накопительный native research layer.
Он отделяет известность registration от знания логики и позволяет обновлять
проценты после каждой подтверждённой находки без повторного просмотра обоих
executable. Канонический baseline хранится в
`research/native-research-baseline.json`, а синхронизация выполняется
`research/native_knowledge.py`.

С6 сентября раздельный PC/PS2 учёт ведётся в additive
`native_platform_*` tables через `research/native_platform_knowledge.py`.
Старый mixed ряд не является оценкой каждой платформы. Методика,
неразнесённое историческое знание и UNRATED явно описаны в
[workbench](native-research-workbench.md); текущие snapshots читать из базы,
а не пересчитывать EXE. Resource schema_version остаётся5.

Полный каталог нативных типов намеренно пока не смешан с таблицей SMO `classes`.
PC executable регистрирует 733 типа, из которых лишь часть является классами
сериализованных ресурсов. Будущее расширение хранит отдельно executable hash,
исходный module, native type, inheritance edge, function и ownership evidence.
Текущая воспроизводимая граница описана в
[карте оригинальной архитектуры](../engine/original-architecture.md).

## Снимок базы

| Корпус | Уникальные версии файлов | Физические вхождения | Контейнеры | SMO |
|---|---:|---:|---:|---:|
| `pc-pristine` | 4 295 | 4 295 | 2 | 416/416 |
| `pc-working` | 4 240 | 4 240 | 2 | 416/416 |
| `ps2-pristine` | 5 955 | 12 378 | 79 | 317/317 |
| **Всего** | **14 490** | **20 913** | **83** | **1 149/1 149** |

В PC-корпусах второй контейнер — `@game`: файлы рядом с `Media`, включая
`WinxClub.exe`, launcher, DLL, INI и shaders. В PS2-корпусе это 78 PCK и
`@game` с `SLES_532.19`, `SYSTEM.CNF`, модулями и остальными файлами вне
`DATA/PCK`. Основной executable одновременно является обычной записью `files`
и связан с таблицей `executables.file_id`.

Итоговый форматный аудит: 14 490 назначений для 14 490 файлов, 0 неназначенных
файлов, 0 ошибок анализаторов и 0 оставшихся `unknown`. Статус `unknown` всё равно
остаётся допустимым для будущего corpus: он означает честно проиндексированный,
но ещё не классифицированный формат, а не ошибку сканирования.

## Правила знания

База использует те же границы доказательности, что и исследование SMO:

- `confirmed_*` — структура подтверждена строгим parser, сигнатурой, полным
  corpus-аудитом, executable или runtime;
- `observed_*` / `confirmed_partial` — наблюдение воспроизводится, но полный
  контракт формата или owning field ещё не восстановлен;
- `probable` — например, путь извлечён из бинарной строки SPT/SPL, но поле-владелец
  неизвестно;
- `inventory_only`, `unknown`, `unresolved` — данные сохранены без догадки;
- `ambiguous_*` — найдено несколько кандидатов, и база намеренно не выбирает один.

Каждое утверждение хранит provenance подходящего уровня: статус evidence,
locator, source path и, где применимо, hash. Ошибка строгого стандартного parser
не делает игровой файл «битым»: не вполне стандартные XML-подобные файлы игры,
например, получают `partial` и сохраняют точную диагностическую строку.

## Что хранит ресурсный слой schema v4 внутри текущей schema v5

| Таблица | Назначение |
|---|---|
| `resource_formats` | каталог форматов, категория, общий decode/write/evidence status |
| `resource_format_extensions` | наблюдаемые расширения без предположения, что расширение всегда достаточно |
| `resource_format_variants` | common/platform/corpus-варианты и их discriminator |
| `file_format_assignments` | распознанный формат каждого файла, метод, status, parser revision и ошибка |
| `resource_properties` | только проверенные или явно помеченные наблюдаемые свойства |
| `resource_symbols` | имена animation tracks, локализованные строки и наблюдаемые component names |
| `resource_dependencies` | исходная ссылка, нормализованный путь, результат разрешения и target file |
| `container_format_assignments` | паспорт directory/PCK-контейнера |
| `container_properties` | число записей PCK, размер string table, sector size и роль каталога |
| `resource_evidence` | документированное основание форматных и файловых утверждений |
| `native_types` | объединённый PC/PS2 registration catalog; hash намеренно не уникален между platform leaves |
| `native_type_scopes` | принадлежность engine/game и прямое присутствие в SMO/SAN с corpus counts |
| `native_research_progress` | PC/PS2 status, coverage score, диапазон и текущий приоритет класса |
| `native_research_evidence` | атомарные binary/corpus/runtime-факты с locator, hash и confidence |
| `native_coverage_snapshots` | история четырёх процентов: all, engine, game и SMO/SAN |
| `native_research_imports` | hash импортированного baseline/incremental manifest для идемпотентности |

Полные payload в SQLite не копируются. `files` хранит путь, размер, время и
SHA-256; форматные свойства — небольшие типизированные JSON-значения. Полные
байты остаются только в исходном read-only corpus.

## Текущее знание о форматах

### Строго или существенно разобраны

- **SMO / FFPS object graph** — строгий структурный разбор всех 36 наблюдаемых
  class ID; существующий слой объектов, полей, вариантов и runtime evidence.
- **PC SAN** — FFPS `spAnimation`, timing, interpolation, именованные transform
  tracks и привязка к SMO node. PS2 SAN остаётся отдельным частично известным
  вариантом. SAN без именованных tracks сохраняется как валидный FFPS с
  ограничением animation decoder.
- **ANM** — текстовая таблица из восьми колонок с `end`; последняя колонка
  подтверждённо ссылается на SAN. Семантика первых семи колонок открыта.
- **PCK** — string table, index, границы записей и равенство
  `byteOffset = sectorOffset × 0x800` проверяются строго.
- **WXT** — подтверждены два layout: count-prefixed offset table и
  self-delimiting offset table, где первый offset задаёт конец самой таблицы.
- **PC STX** — различаются legacy/tagged, compact E0/E5 и raw20. PS2 indexed,
  palette и swizzle пока не образуют полного универсального decoder.
- **SNC** — текстовые sound-control rows и ссылки на WAV.
- **CCC** — текстовая таблица настроек цвета; назначение отдельных колонок
  остаётся частичным.
- **SFT** — подтверждён FFPS-контейнер font resource; полный отдельный writer не
  заявлен.

### Частично разобраны

- **SPT/SPL** — бинарные gameplay components/placements, наблюдаемые имена и
  строковые ссылки на SMO, SPT, SPL, SAN, SNC и другие ресурсы. Ссылка считается
  `probable`, пока не восстановлено owning field.
- **SPS** — subtitle stream; назначение известно, бинарный контракт разобран
  частично.
- **XML/XML~** — корректный XML разбирается строго; engine-совместимый текст с
  несколькими корнями или нестандартными комментариями остаётся `partial`.
- **WXD, WXS, DAT, MIC, SB2** — назначение или место использования наблюдается,
  но структура пока `inventory_only`.

### Внешние стандарты и текстовые файлы

- WAV/RIFF, MPEG video (`M1V`, `MPG`), MPEG audio (`MP2`), DDS, VAG и ICO
  отмечаются как внешние стандарты;
- 135 PS2 movie stream `PSS`, `ICON.SYS` и `IOPRP300.IMG` имеют отдельные
  inventory-only типы по подтверждённым расширению, имени и месту на диске;
- TXT, Lua, INI, PS2 `SYSTEM.CNF`, BAT, shader assembly (`VSH`/`PSH`) и
  RenderMonkey `RFX` индексируются как текст/XML с честным статусом;
- Windows PE распознаётся по `MZ`/`PE`, а ELF — по magic независимо от
  расширения. Это включает основной PC EXE, DLL/launcher/backup EXE, PS2 ELF и
  ELF-модули;
- служебный `unins*.dat` в `@game` отделён от игровой OneLiner DAT по пути и
  сигнатурному контексту.

## Разрешение зависимостей

Ссылка разрешается по порядку, без неявного выбора:

1. точный нормализованный логический путь;
2. путь относительно каталога исходного файла;
3. единственное совпадение basename во всём corpus;
4. `ambiguous_*`, если кандидатов несколько;
5. `unresolved`, если кандидатов нет.

Регистр учитывается при хранении исходного пути, но индекс поиска объединяет
case-варианты в набор кандидатов. Поэтому существование, например,
`Bloom_crystal.smo` в разных вариантах регистра не приводит к случайному выбору.

Текущий результат: 13 494 ссылки, из них 11 796 разрешены однозначно, 76
неоднозначны и 1 622 не имеют файлового target в том же corpus. Из последних
1 525 — ссылки PS2 SNC на имена WAV: сами PC WAV на PS2 отсутствуют, а звук
представлен VAG/SB2. База сохраняет эту платформенную границу как `unresolved`,
пока не будет доказана точная SNC → VAG/SB2 mapping.

Текущий граф включает как минимум ANM → SAN, SNC → WAV и наблюдаемые связи
SPT/SPL. Более длинные runtime-цепочки (`EXE → ANM → SAN → track → SMO node`)
остаются задачей общего Asset Resolver: сама база уже хранит необходимые
файловые и символьные узлы, но не выдаёт гипотезу за завершённую связь.

## Команды

Команды запускаются из `tools/SmoViewer`:

```powershell
dotnet run --project SmoViewer.Inspect -- research-db sources <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db formats <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db format <database.sqlite> unknown
dotnet run --project SmoViewer.Inspect -- research-db resource-audit <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db resource-errors <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db refresh-unknown <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db integrity <database.sqlite>

python research/native_knowledge.py sync --database <database.sqlite>
python research/native_knowledge.py import-manifest --database <database.sqlite> --manifest <incremental.json>
python research/native_knowledge.py report --database <database.sqlite>
python research/native_knowledge.py report --database <database.sqlite> --json
```

`sync` — только явная bootstrap/refresh-команда: она получает полный registration graph штатным read-only scanner-ом,
затем связывает его с `objects/classes`, добавляет `spAnimation` из SAN и
идемпотентно импортирует manifest. Один class hash может принадлежать разным
platform leaves — например, `spDXAudioManager` и `spPS2AudioManager` используют
`0x116F7E16`; поэтому первичным ключом служит отдельный `native_type_id`, а
SMO-связь проверяется одновременно по имени и hash.

Обычный рабочий цикл после первоначального `sync` использует `import-manifest`:
он вносит только новые атомарные факты и новое состояние классов, пересчитывает
четыре snapshot-показателя из сохранённого состояния и **не открывает PC/PS2
исполняемые файлы**. `report` также читает только SQLite. Поэтому однажды
подтверждённый результат больше не требует повторного поиска в executable.

Текущая политика PC-first хранит `ps2_status=deferred`, пока PS2 не требуется
для снятия PC-неоднозначности, общего ABI либо заблокированного кода. Deferred
не означает `not_started` и не уменьшает уже накопленное PC evidence.

Текущий снимок читается из `latest_native_coverage`; значения и последний
incremental manifest приведены в итоговом разделе ниже. Предыдущие снимки
сохраняются исторически, а повторный импорт manifest возвращает
`imported=False` и не создаёт новый snapshot.

Обновление использует уже существующие `update-directory` и `update-pck`.
Scanner revision заставляет один раз перечитать файлы после изменения parser;
неизменённый подробный индекс SMO objects/direct fields переиспользуется.
Повторный проход с той же ревизией пропускает неизменённые ресурсы и PCK.

`refresh-unknown` повторно классифицирует только прежние `unknown`, для которых
новое правило не требует чтения payload; это позволяет добавлять подтверждённые
path/extension-типы без полного повторного сканирования больших PCK/Movies.

К любой команде чтения можно добавить `--json`. `resource-errors` показывает
точный corpus, путь, формат и diagnostic; `resource-audit` отдельно считает
неназначенные файлы, ошибки, разрешённые, неоднозначные и отсутствующие ссылки.

## Открытые границы

Детерминированный порядок работ, рейтинг сложности и общий критерий
«ресурс изучен» зафиксированы в
[очереди анализа всех ресурсов](game-resource-analysis-priority.md).

- восстановить полные WXD/WXS/DAT и SPS contracts;
- завершить PS2 SAN playback и PS2 STX deswizzle;
- заменить наблюдение строк SPT/SPL строгими field decoders;
- классифицировать новые `unknown` расширения и сигнатуры при добавлении corpus
  по отдельному evidence, не по догадке;
- связать file dependency graph с объектами SMO, animation tracks и runtime
  registrations executable;
- поверх read-only индекса реализовать общий Asset Resolver для Viewer,
  Importer и Level Creator.

### Снимок после часового PC animation runtime-цикла 2026-09-05

После четырёх предыдущих incremental manifests добавлен
[`native-research-2026-09-05-pc-animation-runtime.json`](../../research/native-research-2026-09-05-pc-animation-runtime.json).
Он обновляет семь class progress records и сохраняет 20 атомарных evidence
records. `latest_native_coverage`: весь executable — 13,30% (104,235/784),
движок — 25,19% (93,960/373), игровая логика — 2,50% (10,275/411), прямые
SMO/SAN-типы — 44,84% (16,590/37).

`spActor`, `spNodeController`, `spSubController`, `spTransformTrackEval` —
runtime-зависимости, а не новые сериализованные corpus-типы: они не расширяют
прямой знаменатель 37. В этой группе растут только `spAnimation`, `spNode`,
`spSkin`. Это class-weighted оценка исследованной логики, не доля разобранных
байтов EXE и не процент готовности native importer. PS2 deferred, игровой код
в этом цикле не анализировался. Метод и проверки — в
[журнале](../../journal/2026/2026-09-05-pc-animation-runtime-cycle.md).

### Следующий checkpoint: PC node world, 2026-09-05

Добавлен immutable
[`native-research-2026-09-05-pc-node-world.json`](../../research/native-research-2026-09-05-pc-node-world.json):
три class records, восемь evidence; всего 7 imports / 1480 evidence / 28 snapshots.
Текущие оценки: всё EXE **13,31%** (104,385/784), движок **25,23%**
(94,110/373), игра **2,50%** (10,275/411), прямые SMO/SAN **45,08%** (16,680/37).
Protected node/quaternion/affine entries и transform/tree/billboard slice
закреплены bounded guest emulation; это не in-game integration.
[Текущий журнал](../../journal/2026/2026-09-05-pc-reconstruction-until-2100.md).

### Checkpoint PC SAN keys, 2026-09-05

Добавлен immutable
[`native-research-2026-09-05-pc-animation-keys.json`](../../research/native-research-2026-09-05-pc-animation-keys.json):
3 class records / 8 evidence; всего 8 imports / 1488 evidence / 32 snapshots.
Оценки: всё EXE **13,36%** (104,755/784), движок **25,33%** (94,480/373),
игра **2,50%** (10,275/411), прямые SMO/SAN **45,41%** (16,800/37).
Representations, preparation и reader-to-sampler закреплены
[bounded guest/portable checks](native-pc-animation-keys.md); полный native loader,
ownership и frame integration пока не считаются готовыми.

### Checkpoint PC animation object lifecycle, 2026-09-05

Immutable [manifest](../../research/native-research-2026-09-05-pc-animation-lifecycle.json):
4 class records / 12 evidence; база 9 imports / 1500 evidence / 36 snapshots.
Оценки: всё EXE **13,60%** (106,615/784), движок **25,83%** (96,340/373),
игра **2,50%** (10,275/411), прямые SMO/SAN **45,54%** (16,850/37).
`spTrack`/`spAnimTrack` — runtime dependencies, знаменатель прямых assets остаётся 37.
Уточнение physical `spNamedObject` base и name-only clone внесено новым evidence,
без изменения уже импортированных manifests. [Карточка](native-pc-animation-lifecycle.md).
## PC SAN reader checkpoint — 2026-09-05 18:39 МСК

Импортирован immutable `native-research-2026-09-05-pc-san-reader.json`:
1 class update / 6 evidence. Перед записью проверены native-only in-memory
trial, повторный import `(0, false)` и FK. База: 10 imports / 1506 evidence /
40 snapshots. [Reader evidence](native-pc-san-reader.md).

Оценки из `latest_native_coverage`: всё **13,61%** (106,685/784), движок
**25,85%** (96,410/373), игра **2,50%** (10,275/411), прямые SMO/SAN
**45,54%** (16,850/37). `spAnimationSerializer` — runtime dependency, не один
из 37 непосредственно сериализованных типов, поэтому последняя оценка не растёт.
Это class-weighted research coverage, не процент готовности импортера.

## Финальный actor checkpoint — 2026-09-05

После native-only trial/idempotence/FK импортирован
`native-research-2026-09-05-pc-actor-playback.json`: 1 class update / 5 evidence.
Итого **11 imports / 1511 evidence / 44 snapshots**. Актуальные оценки:
всё **13,65%** (107,015/784), engine **25,94%** (96,740/373), game **2,50%**
(10,275/411), direct SMO/SAN **45,54%** (16,850/37). Fixed denominators сохранены.
Actor/serializer — runtime dependencies; их прогресс не приписан direct wire types.
[Evidence](native-pc-actor-playback.md),
[итоговый журнал](../../journal/2026/2026-09-05-pc-reconstruction-until-2100.md).

## PC animation-manager checkpoint — ночь 2026-09-05/06

Immutable [manifest](../../research/native-research-2026-09-06-pc-animation-manager.json):
3 class updates (`spAnimationManager`, `spController`, `spActor`) /9 evidence,
native-only trial/repeat/FK проверены до canonical import.
База: **12 imports /1520 evidence /48 snapshots**.
Оценки: всё **13,83%** (108,395/784), engine **26,31%** (98,120/373), game
**2,50%** (10,275/411), direct SMO/SAN **45,54%** (16,850/37).
Runtime-зависимости не расширяют direct37; метод и знаменатели не менялись.
[Доказательства](native-class-sp-animation-manager.md),
[журнал продолжающегося цикла](../../journal/2026/2026-09-06-pc-reconstruction-until-1000.md).

## PC actor input/start checkpoint — 2026-09-05 19:48 UTC

Immutable [manifest](../../research/native-research-2026-09-06-pc-actor-binding.json):
4 class updates /9 evidence; native-only in-memory trial4, repeat0, FK clean.
База: **13 imports /1529 evidence /52 snapshots**. Evaluator86, actor72,
animation91, node94: оценка относится к изученности, не полноте portable actor API.
Всё **13,84%** (108,525/784), engine **26,34%** (98,250/373), game **2,50%**
(10,275/411), direct SMO/SAN **45,62%** (16,880/37). Direct37 не расширялся.
[Доказательства и safety boundary](native-pc-actor-binding.md): original
discovery/binder/start,156 differential input cases, portable insert/clear;
третья вставка не исполнялась. Полный actor Start/tree/Rebind source ещё открыт.

## PC owned animation bindings — 2026-09-05 20:07 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-animation-owned-bindings.json):
4 class records /6 evidence, **14 imports /1535 evidence /56 snapshots**.
Новые original descendant/Stop helpers повышают actor72→74; перенос уже
исследованного registry lifetime в source не повышает остальные scores.
Всё **13,85%** (108,545/784), engine **26,35%** (98,270/373), game **2,50%**,
direct SMO/SAN **45,62%**. 342 differential owned-name lifetime comparisons,
2832 прежних reader/PRS comparisons и CTest10/10. Это не процент готовности tools.

## PC owned actor runtime — 2026-09-05 20:43 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-actor-owned-runtime.json):
3 class records /6 evidence, **15 imports /1541 evidence /60 snapshots**.
Native-only trial3, repeat0, FK clean. Actor78, Node95, NodeController74 unchanged.
Всё **13,85%** (108,595/784), engine **26,36%** (98,320/373), game **2,50%**,
direct SMO/SAN **45,65%** (16,890/37). Denominators прежние.
[Подробная граница](native-pc-actor-owned-runtime.md): owned actor source,
5154 сквозных comparisons, CTest11/11 и original node/global-init13 checks.
External frame, native event dispatcher и full resource loader остаются открытыми.

## PC frame/task timer — 2026-09-05 21:22 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-engine-frame-timer.json):
4 class records /8 evidence, **16 imports /1549 evidence /64 snapshots**.
Native-only trial4, repeat0, FK clean; CTest12/12, original update134/graphics209,
timer34 native/30 portable/1968 bit-exact comparisons.
Всё **13,95%** (109,385/784), engine **26,57%** (99,110/373), game **2,50%**,
direct SMO/SAN **45,65%**. Direct37 и исходный метод оценок не расширялись.

Уточнение учёта: optional class field `coverageAccounting: "baseline_backfill"`
допустим только при отсутствии progress row и непустом `baselineBackfillReason`.
Он сохраняет текущий class score/evidence, **не добавляя score в старый aggregate
baseline повторно**. Имеющийся ряд так обновить нельзя, обычный default остаётся
`incremental`. Старые manifests не меняются; идемпотентный repeat работает прежде
валидации нового импорта. `test_native_knowledge_accounting.py` —6 in-memory tests.

В этом checkpoint старые engine-core80/PC-app75 cards внесены именно как backfill:
индивидуальный prior score не был сохранён, поэтому ни двойное начисление, ни
догадочный current-cycle delta не применены. Прирост0,790 units относится только
к новому TaskTimer78 и manager80→81. Следующие исследования используют эти
persisted individual rows. [Границы текущего доказательства](native-pc-engine-frame.md).

## PC scene/world — 2026-09-05 22:06 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-scene-world.json):
8 class records /12 evidence, **17 imports /1561 evidence /68 snapshots**.
Native-only trial8, repeat0, FK clean. New Scene35/SceneManager62, Timer78→83,
Core80→81, Node95→96 дают1,040 units. Старые Camera78/CameraData72/DXCamera72
внесены как zero-delta baseline backfill; четыре scene-owned managers пока не
оценены отдельно. Нет повторного начисления за прежние camera source/cards.

Всё **14,08%** (110,425/784), engine **26,85%** (100,150/373), game **2,50%**
(10,275/411), direct SMO/SAN **45,68%** (16,900/37). Fixed sets сохранены.
[Доказательства](native-pc-scene-world.md): original scene/world80, core setup32,
app→world39, camera32 и static29 checks; CTest12/12 и timer1968 comparisons.
Scene/SceneManager пока ABI/probes, не готовые portable classes; render/resource
границы остаются открыты. Проценты — class-weighted исследование, не готовность tools.

## PC render-node runtime — 2026-09-05 22:42 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-render-node-runtime.json):
3 class records /8 evidence, **18 imports /1569 evidence /72 snapshots**.
Native-only trial3, repeat0, FK clean. RenderNode80→87, Scene35→40,
SceneManager62→66: только0,160 новых units, из них0,070 direct SMO/SAN.
Оценка не начисляется за переописание прежнего PS2 материала.

Всё **14,11%** (110,585/784), engine **26,89%** (100,310/373), game **2,50%**
(10,275/411), direct SMO/SAN **45,86%** (16,970/37). Fixed sets/метод прежние.
[Доказательства](native-pc-render-node-runtime.md): original registry55,
world/cull/draw54, callback15, static21; portable math1504/144 cases, CTest13/13.
Полные portable Scene/Manager/RenderNode runtime и specialized registrations
остаются открыты; GPU/игра не запускались, raw EXE не пересканирован при импорте.

## PC scene lights — 2026-09-05 23:19 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-scene-lights.json):
6 class records /10 evidence, **19 imports /1579 evidence /76 snapshots**.
Native-only trial6, repeat0, FK clean. New LightManager72, LightData80→87,
SceneManager66→70, RenderNode87→88:0,840 units/direct0,080. Node96 без прироста.
Прежний Light source/card без individual row внесён как zero-delta
baseline_backfill84, без повторного начисления за старую работу.

Всё **14,21%** (111,425/784), engine **27,12%** (101,150/373), game **2,50%**
(10,275/411), direct SMO/SAN **46,08%** (17,050/37). Это class-weighted
исследование, не byte coverage и не готовность tools; fixed sets сохранены.
[Проверки и границы](native-class-sp-light-manager.md): native90/static22,
C++29/differential1800 (540 cases), CTest14/14. Scene/partition/backend wiring
ещё не объявлен законченным. No GPU/game/PS2 или app/assets/publication.

## PC specialized scene managers — 2026-09-05 23:53 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-scene-special.json):
8 class records /13 evidence, **20 imports /1592 evidence /80 snapshots**.
Native-only trial8, repeat0, FK clean; canonical import без EXE rescan.
SkyBox25→65, new SkyManager70/Projection48/PCProjection52/LensManager34/PCLens40,
Scene40→45/SceneManager70→74:2,930 units, direct0,400. Small game caller blocks
не засчитываются как разобранный игровой класс; fixed scopes не расширяются.

Всё **14,59%** (114,355/784), engine **27,90%** (104,080/373), game **2,50%**
(10,275/411), direct SMO/SAN **47,16%** (17,450/37). Это прежняя class-weighted
оценка, не готовность импортера и не процент инструкций EXE.
[Доказательства](native-pc-scene-special-managers.md): native81/39/43,
static25, ABI rebuild и CTest14/14. Source manager classes/Scene runtime пока
не готовы; actual scene-specific списки/sky-pass/camera owner не неизвестны целиком.

## PC Model → RenderNode source/world — 2026-09-06 01:05 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-model-render-world.json):
7 class records /10 evidence, **21 imports /1602 evidence /84 snapshots**.
Native-only trial7, repeat0, FK clean; canonical import без EXE rescan.
Model75→83/RenderNode88→92/MeshData80→82:0,140 units/direct0,140. Node96 без
прироста; прежние Renderable72/CloneManager82/ResourceManager75 backfilled
с нулевым delta, чтобы не засчитывать старые source/cards второй раз.

Всё **14,60%** (114,495/784), engine **27,94%** (104,220/373), game **2,50%**
(10,275/411), direct SMO/SAN **47,54%** (17,590/37). Fixed scopes/метод прежние.
[Доказательства](native-pc-model-render-world.md): original47/29/static23,
source34,2272 comparisons/32 сценария, прежние1504/144 и CTest14/14.
Automatic Scene/Partition/renderer ещё открыты; no GPU/game/PS2/publication.

## PC renderer callback/queue protocol — 2026-09-06 01:37 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-renderer-protocol.json):
4 class records /7 evidence, **22 imports /1609 evidence /88 snapshots**.
Native-only trial4/repeat0/FK clean; canonical import без EXE rescan.
Renderable72→85/Model83→85:0,150 units/direct0,020; RenderNode92 unchanged.
Прежний renderer source/card backfilled72 с нулевым delta, не новый класс с0.

Всё **14,62%** (114,645/784), engine **27,98%** (104,370/373), game **2,50%**
(10,275/411), direct SMO/SAN **47,59%** (17,610/37). Fixed class-weighted sets.
[Проверки](native-pc-renderer-protocol.md): native75+64/static28,
portable23/768 fields и CTest15/15. CPU callbacks/math и ABI не означают готовую
portable Scene/renderer/GPU; alpha CRT tie order и full ctor остаются open.

## PC partition/static runtime — 2026-09-06 02:16 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-partition-runtime.json):
8 class records /11 evidence, **23 imports /1620 evidence /92 snapshots**.
Native-only trial8/repeat0/FK clean; canonical import без EXE rescan.
PartitionNode25→58/System25→52/Zone25→65/PartitionRenderable25→65/Static30→70,
new PCPartition65:2,450 class units, direct1,800. Scene45/RenderNode92 retained.

Всё **14,94%** (117,095/784), engine **28,64%** (106,820/373), game **2,50%**
(10,275/411), direct SMO/SAN **52,46%** (19,410/37). Fixed class-weighted sets,
не процент инструкций EXE или готовности импортера.
[Проверки](native-pc-partition-runtime.md): native38+38/static30, exact ABI
compiled/CTest15. Полные portable spatial/Scene/Visibility/serializer-to-backend
ещё open. Два вновь названных manager globals не считаются готовыми классами.

## PC SceneInit/Visibility — 2026-09-06 03:04 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-visibility-runtime.json):
5 class records /8 evidence, **24 imports /1628 evidence /96 snapshots**.
Native-only trial5/repeat0/FK clean; canonical import без EXE rescan.
New Visibility50 +Scene45→58/System52→58/Zone65→68/PartitionNode58→63:
0,770 units/direct0,140, без backfill и расширения denominator.

Всё **15,03%** (117,865/784), engine **28,84%** (107,590/373), game **2,50%**
(10,275/411), direct SMO/SAN **52,84%** (19,550/37). Это class-weighted research,
не процент готового исходника/инструкций или импортера.
[Проверки](native-pc-visibility-runtime.md): native20+39/static25,
portable17/1215 differential fields и CTest16/16. Whole Visibility ctor capped,
record-oriented source не объявлен полной Scene/portal/occluder реализацией.

## PC whole SceneRender / Shadow — 2026-09-06 03:30 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-scene-render-runtime.json):
4 class records /7 evidence, **25 imports /1635 evidence /100 snapshots**.
Native-only trial4/repeat0/FK clean. Scene58→68 +new Shadow30/DXShadow35
дают0,750 engine/all units; direct unchanged. Older DXRenderer family source/
card migrated conservatively35, **zero aggregate credit**; historical individual
35 score не утверждается и весь класс не считается новым.

Всё **15,13%** (118,615/784), engine **29,05%** (108,340/373), game **2,50%**
(10,275/411), direct SMO/SAN **52,84%** (19,550/37). Same class-weighted scopes.
[Проверки](native-pc-scene-render-runtime.md): native67+14/static23,
DX state source6/960 fields и CTest17. Nonempty occluders/shadows/shaders,
Visibility full ctor и portable Scene/runtime startup остаются open.

## PC Occlusion geometry/Scene — 2026-09-06 04:05 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-occlusion-runtime.json):
3 class records /6 evidence, **26 imports /1641 evidence /104 snapshots**.
Native-only trial3/repeat0/FK clean; canonical import без EXE rescan.
Occlusion25→58 +Visibility50→52 +Scene68→70 дают0,370 all/engine units,
direct0,330. Никакого нового backfill или расширения denominator.

Всё **15,18%** (118,985/784), engine **29,14%** (108,710/373), game **2,50%**
(10,275/411), direct SMO/SAN **53,73%** (19,880/37). Class-weighted research,
не byte/instruction/source completeness и не готовность импортера.
[Проверки](native-pc-occlusion-runtime.md): native84/static24, portable
visibility24/1483 differential fields и CTest17. Full protected Init capped;
prepared-buffer/cached-silhouette границы отражены в evidence, не скрыты.

## PC Octree runtime/source — 2026-09-06 04:31 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-octree-runtime.json):
3 class records /6 evidence, **27 imports /1647 evidence /108 snapshots**.
Native-only trial3/repeat0/FK clean; canonical import без EXE rescan.
Octree25→65 +PartitionNode63→67 +Visibility52→54:0,460 all/engine units,
0,440 direct, без backfill/изменения fixed denominator.

Всё **15,24%** (119,445/784), engine **29,27%** (109,170/373), game **2,50%**
(10,275/411), direct SMO/SAN **54,92%** (20,320/37). Class-weighted research,
не процент инструкций EXE или полной готовности исходников/importer.
[Проверки](native-pc-octree-runtime.md): native43/static24/source28,
3104 differential fields/320cases/CTest18; normal copy45E870 capped,
actual whole Debug21 traversal не выдаётся за обычную clipped ветку.

## PC Portal runtime/source — 2026-09-06 05:08 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-zone-portal-runtime.json):
3 class records /6 evidence, **28 imports /1653 evidence /112 snapshots**.
Native-only trial3/repeat0/FK clean; canonical import без EXE rescan.
Portal25→73 +PortalNode25→78 +Visibility54→61:1,080 all/engine units,
1,010 direct, без backfill/изменения fixed denominator.

Всё **15,37%** (120,525/784), engine **29,56%** (110,250/373), game **2,50%**
(10,275/411), direct SMO/SAN **57,65%** (21,330/37). Это условная class-weighted
оценка исследования, не доля инструкций/байтов или готовность импортера.
[Проверки](native-pc-zone-portal-runtime.md): native63/static32/source19,
1024 plane fields/256cases/CTest19. Clipped whole Scene portal/cycle executed;
near-plane45E870 branch не объявлен завершённым, full ctor gap остаётся.

## PC polygon clipping — 2026-09-06 05:26 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-polygon-clipping.json):
1 class record /3 evidence, **29 imports /1656 evidence /116 snapshots**.
Native-only trial1/repeat0/FK clean; canonical import без EXE rescan.
Visibility61→66 даёт0,050 all/engine units. Anonymous clipping dependency
не становится выдуманным классом и не увеличивает direct37 denominator/score.

Всё **15,38%** (120,575/784), engine **29,57%** (110,300/373), game **2,50%**
(10,275/411), direct SMO/SAN **57,65%** (21,330/37, без изменения).
Class-weighted research, не процент инструкций или готовности приложения.
[Проверки](native-pc-polygon-clipping.md): native28/static22/source11,
3842 differential fields/256cases/CTest20; safe127 test,128 не исполнялся.

## PC BSP runtime/source — 2026-09-06 06:24 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-bsp-runtime.json):
2 class records /6 evidence, **30 imports /1662 evidence /120 snapshots**.
Native-only trial2/repeat0/FK clean; canonical import без EXE rescan.
BSP25→70 и Visibility66→67:0,460 all/engine units,0,450 direct; fixed
denominators784/373/411/37 не менялись.

Всё **15,44%** (121,035/784), engine **29,69%** (110,760/373), game **2,50%**
(10,275/411), direct SMO/SAN **58,86%** (21,780/37). Условная class-weighted
оценка исследования, не доля bytes/instructions и не готовность импортера.
[Проверки](native-pc-bsp-runtime.md): native48/static17/source17,
2046 differential fields/256cases/CTest21. Whole camera-leaf Zone Scene
прошёл, но normal recursive protected45E870/full constructor остаются open.

## PC spatial consumers — 2026-09-06 06:34 UTC

[Immutable manifest](../../research/native-research-2026-09-06-pc-spatial-consumers.json):
4 class records /5 evidence, **31 imports /1667 evidence /124 snapshots**.
Native-only trial4/repeat0/FK clean; canonical import без EXE rescan.
BSP70→72/Octree65→67/Occlusion58→59/Static70→71:0,060 units all/engine/direct.

Всё **15,45%** (121,095/784), engine **29,71%** (110,820/373), game **2,50%**
(10,275/411), direct SMO/SAN **59,03%** (21,840/37). Class-weighted research,
не инструкция/byte/source/tool readiness. [Проверки](native-pc-spatial-consumers.md):
native94/static15; original Debug21 Static dedup не заменяет normal clipping.
Portable source не менялся; CTest21 проверяет предыдущий source checkpoint.
