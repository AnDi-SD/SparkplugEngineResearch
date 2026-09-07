# 2026-09-05 — часовой PC animation runtime-цикл

## Запрос и временное окно

Продолжить исследование на один час после переключения модели. Сохранены
предыдущие правила: логический порядок зависимостей, приоритет PC и классов
SMO/SAN, original class names/module tree, база evidence и четыре оценки.

Начало: 2026-09-05 14:03:47 МСК (`11:03:47 UTC`). Отчёт — около 15:04 МСК.
Цикл завершён: 15:03:47 МСК (`12:03:47 UTC`), ровно через час. Основная
работа и финальные проверки закончены раньше отчётной границы; новых классов
в завершающий интервал не начиналось. Исследовательские процессы завершены.
Новые независимые подсистемы и PS2 в этом цикле не начинались.

## Исходные данные

- root HEAD: `7c2b615a0d14b3ec9d3be3bd70a61599568b58c1`, branch `main`;
- исходный worktree чистый; submodules не менялись;
- pristine PC `local-data/pc-pristine/WinxClub.exe`, SHA-256
  `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`;
- база `local-data/results/smo-corpus-v2.sqlite`, schema 5; 784 types,
  engine 373, game 411, direct SMO/SAN 37;
- исходно: 5 imports (baseline + 4 incremental), 1452 evidence, 20 snapshots.

## Метод и безопасность

Read-only byte/body/vtable inspection, адресный IDA-анализ исследовательской
копии `.i64`, portable CMake/CTest, отдельный MSVC x86 native replay.
Оригинальные игровые файлы, EXE и submodules не менялись. Игра/renderer/D3D
не запускались. Никакого release, tag, commit или push этот цикл не выполняет.

Replay не запускает EXE и не загружает его как module. Python runner сначала
проверяет pristine SHA, затем исполняет отдельный probe с timeout 10 секунд;
только whitelisted открытые bodies попадают в RX memory. Неизвестные calls
заменяются явно названными seams либо аварийным выходом. Это не полноценная
OS sandbox и не in-game integration. Детали —
[PC animation runtime pipeline](../../docs/research/native-pc-animation-runtime.md).

## Что установлено

1. `spTransformTrackEval`: исправлена vtable 7 → 9 slots; раскрыты оба input
   по `0x30`, playback/track/priority и три массива key indices. Sampling time
   берётся из playback `+0x34`, вес из `+0x0C`; внешний time игнорируется.
   Подтверждены validity, weighted PRS, persistent caches, null-track hole,
   blank clone и ограниченность pointer-only clear.
2. `spNodeController`: exact allocation `0x18`, 12 slots, intrusive node и
   direct-owned evaluator, default track/const variants, blank clone. Direct
   и transition local PRS разобраны: position/quaternion смешиваются, scale
   копируется. Dirty bit `1` подтверждён для position/scale; rotation tail защищён.
3. `spActor`: exact allocation `0x54`, playback stride `0x60`, tree/name binder,
   track-to-input соединение и непосредственный tick → controller dispatch.
   Полный scheduler/event state machine не восстановлен и не исполнялся.
4. Quaternion→matrix, matrix→quaternion и slerp проверены native instruction
   replay; small-angle fallback копирует первый quaternion, не nlerp.
5. Native key interval и настоящий linear-position sampler воспроизводят
   необычный two-key endpoint: на последнем time возвращается первый key.
   У three-key track endpoint обычный. Это не объявлено багом игры: upstream
   padding/time invariants не закрыты.
6. `spNode` cached world поля `+0x74/+0x80/+0x8C` подтверждены независимыми
   point consumers. Найден world-update vslot `+0x30 -> 0x00421420`: protected
   preamble и большой читаемый хвост с parent masks, child recursion, collision
   refresh и `flags &= ~7`. Полный контракт входа пока не восстановлен.
7. `spSkin`: закреплён порядок `inverseBind * boneWorld`; native Matrix4 и
   Matrix3 products проверены отдельно. Исправлено старое описание cleanup:
   bone count сбрасывается только на success branch, не на всех error exits.

## Исходники и границы готовности

Добавлены `Sparkplug/Code/Sparkplug/spTransformTrackEval.*` и
`spNodeController.*`, минимальные dependency interfaces
`spEvaluator.*`, `spTransformEval.*`, `spSubController.*`, аналитический
`Analysis/PC/spAnimationMath.h`, ABI layouts/assertions и animation tests.
В `spSkin` добавлен проверяемый palette product helper.

Пути новых файлов inferred, class names/IDs exact, методы `ForAnalysis` не
выдаются за recovered original spelling. Самостоятельного portable `spActor`,
полного SAN decoder или world updater пока нет. Sampling — явный seam;
safe host ownership/null guards и two-input limit отделены от native behavior.
Это два содержательных class slices плюс зависимости, не «пять закрытых классов».

## Проверки

| Проверка | Результат |
|---|---|
| CMake build, MSVC x64, последовательная сборка | PASS |
| CTest | 3/3 suites PASS |
| Новый `SparkplugAnimationTests` | 32 assertions PASS |
| `inspect_transform_track_eval.py` | 15/15 PASS |
| `inspect_animation_runtime.py` | 46/46 PASS |
| `probe_pc_animation.py`, отдельный x86 process | 48/48 PASS |
| `test_pc_animation_probe.py`, mocked safety gates | 5/5 PASS |

В replay checks входят агрегированные серии: 128 quaternion→matrix, 128
matrix→quaternion, 640 interpolation, по 64 Matrix4/Matrix3 products;
дополнительно проверены реальные связки controller/evaluator с native math.
Регрессии закрепляют уже найденные ветви: zero-old-weight rotation сохраняет
первое значение, а transition position допускает factor выше `1` без clamp.
Это серии численных проверок, не набор разных игровых моделей и не FPS/render test.

## База и оценка

Новый immutable после импорта manifest:
[`native-research-2026-09-05-pc-animation-runtime.json`](../../research/native-research-2026-09-05-pc-animation-runtime.json).
Семь progress updates, 20 атомарных evidence; executable/corpus rescan не нужен.
Сначала проверены пути, диапазоны, delta и idempotence на in-memory копии только
native-таблиц. Полный многогигабайтный corpus integrity/FK scan не запускался.

Фактический импорт завершился `progress=7 imported=True executable_scan=False`;
повторный — `progress=0 imported=False`. После него: 6 imports, 1472 evidence,
24 snapshots. Хеши всех шести импортированных manifests сохранны; native-only
логические foreign keys проверены без обхода corpus. Все 277 проверенных
локальных Markdown-ссылок существуют; `git diff --check` проходит.

| Класс | Было → стало | Причина ограничения |
|---|---:|---|
| `spTransformTrackEval` | 38 → 70 | decoder и insertion lifecycle ещё seams |
| `spNodeController` | 0 → 68 | const evaluator, protected setter, frame integration |
| `spActor` | 0 → 32 | только связный scheduler/binder scout |
| `spSubController` | 25 → 35 | уточнён abstract slot, не весь controller ABI |
| `spAnimation` | 68 → 72 | редкие encodings/tags/constructor остаются открыты |
| `spNode` | 85 → 87 | world tail найден, protected preamble/callbacks ещё открыты |
| `spSkin` | 75 → 80 | математика закрыта, полный runtime/stream lifecycle — нет |

| Область | До | После | Диапазон после | Units / denominator |
|---|---:|---:|---:|---:|
| Весь executable | 13,10% | 13,30% | 11,29–15,26% | 104,235 / 784 |
| Логика Sparkplug | 24,78% | 25,19% | 22,19–28,19% | 93,960 / 373 |
| Логика Winx | 2,50% | 2,50% | 1,50–3,50% | 10,275 / 411 |
| Прямые SMO/SAN-классы | 44,54% | 44,84% | 39,86–49,86% | 16,590 / 37 |

Это прежняя class-weighted оценка исследованной логики, не процент байтов EXE,
не доля готового импортёра и не число полностью закрытых классов. Runtime
dependencies не добавлялись к прямому знаменателю 37; PS2 оставлен deferred.

## Отрицательные результаты и следующий проход

`0x00420530/0x004212F0` — debug dump, `0x00420E60` — bounds aggregation, не
world-update. Protected thunks `0x00420B20/0x00420BD0/0x00420D20` не раскрыты;
случайные инструкции после jump не приняты за настоящий runtime flow.
Неполные decompiler stack frames сверялись с disassembly, а не использовались
как достоверные C++ signatures.

Следующий связный порядок: protected `spNode::0x00421420` preamble и frame
caller → collision/billboard/dirty invariants → все SAN key representations и
time padding → внешний actor tick и input ownership/capacity → ограниченная
игровая проверка всей цепочки. Неподтверждённые части не перенесены в инструменты
импорта/экспорта «по аналогии».
