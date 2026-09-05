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

Снимок после PC-first цикла 2026-09-05 хранится четырьмя записями view
`latest_native_coverage`: весь executable — 13,10% (102,705/784), движок —
24,78% (92,430/373), игровая логика — 2,50% (10,275/411), прямые SMO/SAN-типы —
44,54% (16,480/37). Изменения внесены четырьмя incremental manifests для
`spSkin`, `spAnimation`, controller dependency и `spTransformTrackEval`; повторный импорт каждого
manifest возвращает `imported=False` и не создаёт новый snapshot.

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

### Итоговый снимок PC-first цикла 2026-09-05

После импорта четырёх incremental manifests (`spSkin`, `spAnimation`,
controller dependency и `spTransformTrackEval`) `latest_native_coverage`
содержит: весь executable — 13,10% (102,705/784), движок — 24,78%
(92,430/373), игровая логика — 2,50% (10,275/411), прямые SMO/SAN-типы —
44,54% (16,480/37). `spTransformTrackEval` является подтверждённой runtime-
зависимостью SAN, но не сериализованным corpus-классом, поэтому честно меняет
общую и engine-оценки, не расширяя прямой знаменатель 37 типов.
