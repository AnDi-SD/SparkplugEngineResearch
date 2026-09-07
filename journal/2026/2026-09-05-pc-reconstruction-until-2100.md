# 2026-09-05 — продолжение PC-реконструкции до 21:00 МСК

## Условия

Запрос пользователя: продолжать до доступного остатка Pro, но не позже
21:00 МСК 2026-09-05 (`18:00 UTC`), затем дать отчёт. Старт — около 15:08 МСК.
Процент оставшейся подписочной квоты недоступен инструментам, поэтому он не
выдаётся за измеряемый token budget. Цель и крайнее время сохранены в active goal.

Следуем связной PC-first цепочке: `spNode` world-update → SAN key sampling →
`spActor`/controller lifecycle → необходимые соседние зависимости. PS2 deferred.
Не запускаем release/push, не меняем исходные игровые ресурсы, не запускаем
массовые игровые тесты. Результаты сохраняются по завершённым участкам.

## Точка старта

Предыдущий [часовой цикл](2026-09-05-pc-animation-runtime-cycle.md) сохранён
локально; его незакоммиченные изменения сохранены. Root HEAD `7c2b615`.
База schema 5: 6 imports, 1472 evidence, 24 snapshots; показатели:
all 13,30%, engine 25,19%, game 2,50%, direct SMO/SAN 44,84%.

Pristine PC SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Первый участок (история начала цикла)

Проверяются protected bridges вокруг `spNode::0x00421420`, quaternion setter
`0x00420640` и palette matrix builder `0x00461D70`. Читаемые хвосты уже
закреплены; неизвестные preambles не подменяются предполагаемыми инструкциями.
Четыре имеющиеся PC-копии сохраняют те же защищённые entries; отдельной
unprotected сборки среди проверенных копий не найдено. Полный игровой запуск
для этой разведки пока не требуется.

## Checkpoint 1 — node world / около 15:50 МСК

Ограниченная эмуляция оказалась достаточна: unchanged protected bridges
разрешили world flags read, quaternion dirty write и affine stack setup.
Новая карточка: [PC node world](../../docs/research/native-pc-node-world.md).
Полный transform/tree/billboard путь и affine builder проверены; унаследованные
позиция/scale/rotation gates воспроизведены без «улучшения» native поведения.
Rotation-only direct/blend в portable controller теперь тоже ставит dirty bit.

Добавлены `pc_instruction_emulator.py`, node guest probe, семь harness guard
tests, `spNodeTransformMath.h`, world caches/update в `spNode`, отдельный CTest.
Unicorn 2.1.4 установлен только в `.codex-tmp/emulation-python`; существующие
pefile/capstone взяты из local research cache. Ни игра, ни исходные ресурсы
не запускались/не менялись. Guest call: 100 000 instructions / 2 sec; child
Python: 30 sec; Windows APIs не проксируются. Это не in-game validation.

Проверки:

- guest world/bridges: 179/179; portable node-world: 83/83;
- harness guards: 7/7; прежний native animation probe: 48/48;
- прежние read-only inspectors: animation 46/46, track evaluator 15/15;
- чистая сборка `.codex-tmp/Sparkplug-build-pc2100-utf8`: CTest 4/4;
- `git diff --check`: без ошибок whitespace (только LF/CRLF warnings).

Выявлена отдельная проблема старого локального build-dir: `/showIncludes`
prefix был mojibake, и Ninja не отслеживал header dependencies. После изменения
layout `spNode` старые test objects не пересобрались: default-check failure и
segfault. Это воспроизведено без игры. Новый configure под `chcp 65001`
сохраняет правильный русский prefix; чистые 139 targets и все тесты проходят.
`VSLANG=1033` само по себе не помогло: установлен только MSVC locale 1049.
Инструкция исправлена в Sparkplug README, каждому CTest добавлен timeout 20 sec.
Старые временные build dirs не удалялись; никакие пользовательские процессы
не останавливались.

Manifest `native-research-2026-09-05-pc-node-world.json` сначала применён к
in-memory копии только native tables/views: native FK clean, повторный import
идемпотентен, fixed denominators сохранены. Затем применён к canonical DB:
7 imports, 1480 evidence, 28 snapshots. Три class records, восемь новых evidence.

| Оценка | До | Checkpoint 1 |
|---|---:|---:|
| Всё EXE | 13,30% | 13,31% (104,385/784) |
| Движок | 25,19% | 25,23% (94,110/373) |
| Игровая логика | 2,50% | 2,50% (10,275/411) |
| Прямые SMO/SAN | 44,84% | 45,08% (16,680/37) |

Это оценка исследованной class-weighted логики, не доля байтов/готовность tools.
Open: frame caller, collision/scene implementation, dirty bits 2/4 и original
names. Следующий участок — SAN key representations, затем actor/frame.
Работа продолжается до прежней границы 21:00 МСК; release/push не выполнялся.

## Checkpoint 2 — SAN keys / около 17:15 МСК

Закрыт связный участок payload → shared descriptors → cubic preparation →
PRS sampling. [Подробная карточка](../../docs/research/native-pc-animation-keys.md)
содержит точные адреса, четыре representations, descriptor/value strides,
семь pool hints, формулы и границы доказательств. Protected reader и quaternion
preparation разрешены исполнением оригинальных инструкций в bounded guest.
CRT `acos`, stream byte-source и descriptor allocator — явные fixtures;
полный loader/OS/game startup не запускались.

Добавлен analytical `spAnimationKeySampling.h`, а не придуманный native class.
Подготовленные keys подключены к evaluator/controller/world; native two-key
endpoint сохранён. Host finite/bounds guards явно отделены от оригинала.
Промежуточные quaternion products используют wider intermediates в соответствии
с x87; тестовые quaternion inputs закреплены в float32, без введения недоказанного
native clamp или normalization.

На четырёх pristine SAN проверены shared-array counts. `barrel.san` не имеет
field `64`: старое описание terminator исправлено на optional reserve hint.
`bbush.san` содержит cubic Vector3 scale, который текущий viewer decoder не
поддерживает; `bflower.san` содержит single-key quaternion. Это важно для будущего
native importer/viewer, но сами приложения и ресурсы сейчас не менялись.

Проверки: guest keys/reader/preparation **396/396**, portable/native PRS/cache
comparison **120/120**, portable keys **60/60**, hash/body inspector **12/12**,
harness guards **7/7**, CTest **5/5**. Процессные/инструкционные лимиты сохранены.

Immutable manifest `native-research-2026-09-05-pc-animation-keys.json`:
3 class updates / 8 evidence. Перед canonical import — in-memory native-only
trial, повторный import `(0, false)`, FK clean. База: 8 imports / 1488 evidence /
32 snapshots. Fixed denominators не менялись.

| Оценка | Checkpoint 1 | Checkpoint 2 |
|---|---:|---:|
| Всё EXE | 13,31% | 13,36% (104,755/784) |
| Движок | 25,23% | 25,33% (94,480/373) |
| Игровая логика | 2,50% | 2,50% (10,275/411) |
| Прямые SMO/SAN | 45,08% | 45,41% (16,800/37) |

Следующий front: создание `spAnimation`/embedded tracks, descriptor/tag ownership,
затем actor/frame. Полный класс/loader и in-game integration пока не заявляются.

## Checkpoint 3 — animation objects / около 18:00 МСК

[Карточка](../../docs/research/native-pc-animation-lifecycle.md) и три новых
original classes: `spTrack`, `spAnimTrack`, `spAnimation`. Exact PC sizes
`0x14`, `0x44`, `0x84`; protected factories/constructors, defaults, append/resize,
pool cleanup и stable tag order проверены оригинальными guest instructions.

Исправлена важная прежняя интерпретация: `spAnimation` physically inherits
`spNamedObject`, а `0x00413120` копирует shared name. Engine RTTI по-прежнему
указывает controller chain. Clone не переносит tracks/duration/tags, но имя
сохраняет. Исправлены карточка, ABI, inspector и portable name-copy path.

Новые original hazards: standalone track owner не инициализирован; incoming
ownership flag определяет освобождение старых arrays; release оставляет stale
descriptor pointers; shrink использует capacity вместо constructed count.
Они не объявлены безопасным публичным Reset и не перенесены в host без guards.
Последний возвращённый descriptor переводит блок в spare cache, а не оставляет
64 active free slots. Animation destructor освобождает и этот spare.

Проверки: lifecycle **206/206**, identity/body hashes **20/20**, object C++
**51/51**, свежая dependency-correct сборка и **CTest 6/6**. SEH fixture расширена
явным GDT/FS segment; null остаётся unmapped, guards **8/8**. Windows API/loader
не запускаются. Старая ABI-prefix проверка обновлена до exact `0x84`; результат
CTest на прежних binaries после failed compile не засчитывался — выполнена новая
успешная сборка и повторная проверка.

Manifest `native-research-2026-09-05-pc-animation-lifecycle.json`:
4 class updates / 12 evidence; native-only in-memory trial, idempotence и FK clean,
затем canonical import. База: 9 imports / 1500 evidence / 36 snapshots.

| Оценка | Checkpoint 2 | Checkpoint 3 |
|---|---:|---:|
| Всё EXE | 13,36% | 13,60% (106,615/784) |
| Движок | 25,33% | 25,83% (96,340/373) |
| Игровая логика | 2,50% | 2,50% (10,275/411) |
| Прямые SMO/SAN | 45,41% | 45,54% (16,850/37) |

Следующий связный участок: full PC SAN reader на малом реальном файле,
name-binding/transaction seams, затем actor/frame. Никакие приложения, игровые
ресурсы, releases или remotes не изменены. Работа продолжается до 21:00 МСК.
## Checkpoint 4 — full SAN reader / около 18:40 МСК

[Карточка](../../docs/research/native-pc-san-reader.md): exact serializer `0x4C`,
secondary-interface this, 14 scratch counters, reserve `hint+1`, полный field loop
и real tag names. Оригинальный reader прошёл четыре pristine SAN: **1034 checks**.
Затем duration/PRS sampler плюс объектные данные сравнены с переносимым кодом:
**2832 comparisons**. Исходные игровые файлы не менялись, игра не запускалась.

`spAnimationSerializer` добавлен под original TU на общем `spStream` /
`spDataBlockSerializer` core. Строгие host limits/terminator/ownership явно
отделены от original behavior; full FFPS/FAT loader, writer и registry не обещаны.
Ошибка первого C++ fixture (неоткрытый write stream) исправлена в тесте, а не
ослаблением core. Свежая сборка: **CTest 7/7**, reader **84/84**.

Native malformed/reuse probe **137/137**: reader умеет возвращать success на
пустом/оборванном header; payload failure возвращает false без отката target.
Служебные counters сбрасываются при повторном использовании serializer с новым
объектом. Безопасная повторная загрузка заполненного target не доказана.
Body/identity anchors **8/8**. Bounds и процессные лимиты сохранены.

Manifest `native-research-2026-09-05-pc-san-reader.json`: 1 class update /
6 evidence; native-only trial, idempotence, FK, затем canonical import.
База: 10 imports / 1506 evidence / 40 snapshots.

| Оценка | Checkpoint 3 | Checkpoint 4 |
|---|---:|---:|
| Всё EXE | 13,60% | 13,61% (106,685/784) |
| Движок | 25,83% | 25,85% (96,410/373) |
| Игровая логика | 2,50% | 2,50% (10,275/411) |
| Прямые SMO/SAN | 45,54% | 45,54% (16,850/37) |

Serializer — dependency, не один из 37 direct wire types: знаменатели не
менялись и дополнительный процент прямым SMO/SAN не приписывался.
Следующий front: `spActor` playback/frame и name registry. Цикл продолжается;
релизы, push, игровые ресурсы и приложения не затронуты.

## Checkpoint 5 и завершение

После перебоя связи пользователь попросил закончить последние шаги и дать отчёт.
Текущее actor-сравнение доведено до результата, новые ветки не начинались.
Финальная проверка времени при возобновлении показала уже `18:03 UTC` / 21:03 МСК;
точная остановка в 21:00 не заявляется. После этого выполнялись только завершение
текущей проверки, запись evidence/документации и подготовка отчёта.

[Actor playback](../../docs/research/native-pc-actor-playback.md): добавлен original
`spActor` scheduler slice и abstract `spController` dependency. Modes/reverse,
точные endpoint inequalities, tags/callback order, fade/transition и gating
проверены на **93 сценариях / 1857 comparisons** без расхождений.
Последовательно исправлены три неверно перенесённых equality cases: ping-pong
direction при remainder 1, reverse sample при progress 0, fade при weight 0.
Финальный прогон повторён полностью после исправлений.

Original empty actor lifecycle — **61/61**: exact `0x54`, 40 zeroed playback
slots с index `+0x44`, intrusive controller register/unregister, три allocations
освобождены. Native registry root synthetic; list operations original.
Manager caller `0x004535A0` установлен статически как `spAnimationManager`,
но не исполнялся; manager coverage отдельно не повышался.

Fresh CTest **8/8**, actor C++ **66/66**, включая связанную цепь actor → keys →
evaluator → controller → world position. Повторены runtime anchors **46/46**
и harness guards **8/8**. Игрового запуска нет. Full actor setup/tree/clone,
registry, queue reentrancy и nonempty lifecycle — явные границы, не готовые API.

Manifest `native-research-2026-09-05-pc-actor-playback.json`:
1 class update / 5 evidence. Native-only trial, idempotence и FK clean, затем
canonical import. Итог базы: **11 imports / 1511 evidence / 44 snapshots**.
За этот цикл добавлены пять manifests и 39 evidence, предыдущее не перезаписывалось.

| Оценка | Начало цикла | Итог |
|---|---:|---:|
| Всё EXE | 13,30% | **13,65%** (107,015/784) |
| Движок | 25,19% | **25,94%** (96,740/373) |
| Игровая логика | 2,50% | **2,50%** (10,275/411) |
| Прямые SMO/SAN | 44,84% | **45,54%** (16,850/37) |

Это прежние fixed-denominator class-weighted оценки, не доля байтов и не
готовность tools. Остаток Pro не измерялся: утверждение об исчерпании квоты
не делается. Результаты сохранены локально, commit/push/release не выполнялись.

Следующее логическое продолжение: исполнить уже найденный manager/frame caller,
восстановить original name registry, доказать input capacity/ownership invariants,
затем соединять с full resource loader и skin/render submission. Сейчас цикл закрыт.
