# 2026-09-06 — PC-реконструкция до 10:00 МСК

## Условия и точка старта

Новый цикл по запросу пользователя: продолжать reverse engineering с прежними
правилами, отчёт к ближайшим 10:00 МСК. Проверенное время старта —
2026-09-05 18:22:54 UTC / 21:22:54 МСК. Deadline — **2026-09-06 07:00 UTC** /
**10:00 МСК**. Завершение проверок/запись отчёта планировать до deadline;
при перебое связи не утверждать точное время остановки без проверки часов.

PC-first, logical dependency order с приоритетом SMO/SAN. Original class names,
доказанные source paths либо явная отметка inferred. Неизвестное сохраняется
в карточках и базе. PS2 отложен, если не нужен текущему доказательству.
Тесты bounded guest/portable; игра, ресурсы и приложения не меняются.
Commit/push/release и новые платные услуги не запускаются. Pro quota инструментам
не видна и не переводится в выдуманный token budget.

Предыдущий цикл: [пять checkpoints](2026-09-05-pc-reconstruction-until-2100.md).
Dirty worktree сохранён; root HEAD `7c2b615`, submodules не переключались.
Последняя сборка: CTest 8/8; actor 93 scenarios / 1857 comparisons,
native empty lifetime 61/61. Canonical DB schema 5:
11 imports / 1511 evidence / 44 coverage snapshots.

| Scope | Начало | Fixed units |
|---|---:|---:|
| Всё EXE | 13,65% | 107,015/784 |
| Engine | 25,94% | 96,740/373 |
| Game | 2,50% | 10,275/411 |
| Direct SMO/SAN | 45,54% | 16,850/37 |

Это class-weighted research coverage, не байты и не готовность tools.

## Первый связный участок

`spAnimationManager`: original global `0x0075F880`, class ID `0x5D214CC1`,
factory `0x00454640`, static frame helper `0x004535A0`. Начать с выполнения
manager → controller/actor tick и intrusive register/unregister, затем
constructor/defaults, name/slot registry, actor input capacity/lifetime.
Внешний engine frame caller и дальнейший resource/render путь — по зависимостям.
База обновляется новыми immutable manifests, без переписывания старых imports.

## Checkpoint 1 — animation manager / controller lifetime

Сохранён immutable
[`native-research-2026-09-06-pc-animation-manager.json`](../../research/native-research-2026-09-06-pc-animation-manager.json),
timestamp evidence snapshot **2026-09-05 19:04:49 UTC /22:04:49 МСК**.
PC manager — original class `5D214CC1`, direct root, exact `0x2C`.
Factory/constructor/destructor protected routes исполнены; frame/name next-ID
initial1, shared case-sensitive names/refcounts, monotonic IDs/no reuse,
empty/long strings, native map ownership, evaluator attach/detach, blank clone.

Original manager→real actor tick исполняется: head→next, enabled gate, engine
delta snapshot один на frame, empty-list counter wrap. Три actor lifetime
проверяют удаление middle/head/tail. Controller copy `423100 -> 419AA0` переносит
только enabled byte10; original actor clone получает fresh40 states/default
speed/apply/advance и независимую регистрацию. Классы перенесены в исходники.
Native callback lifetime и outer engine frame caller не считаются закрытыми.

Original SAN reader + original registry: два simultaneous resources, shared IDs,
индивидуальное освобождение, полное освобождение, reload с новыми IDs.
`bbush.san`: 5 tracks /33 assertions; `bflower.san`: 17 /69. Все tracked native
allocations освобождены. Portable reader остаётся non-owning resolver: отдельно
нужен binding-lifetime API, чтобы BindName не утекал при destruction/reload.

Проверки:

- manager/controller/clone guest **326/326**;
- pristine hash/identity anchors **20/20**;
- portable manager suite **49/49**, **CTest 9/9** после полной header-aware rebuild;
- прежний actor differential suite повторён: **1857/1857**, 93 cases;
- DB native-only in-memory trial `(3, true)`, repeat `(0, false)`, FK `[]`.

База после import: **12 imports /1520 evidence /48 snapshots**,
3 class updates /9 новых evidence. Manager score80[70,88], controller80[70,88],
actor68[55,79]. Неисследованные engine callers/PS2 не получают coverage.

| Scope | После checkpoint 1 | Fixed units |
|---|---:|---:|
| Всё EXE | **13,83%** | 108,395/784 |
| Engine | **26,31%** | 98,120/373 |
| Game | **2,50%** | 10,275/411 |
| Direct SMO/SAN | **45,54%** | 16,850/37 |

Class-weighted score, не byte coverage и не readiness importer.
Runtime registry/controller/actor не добавлены искусственно в direct37.

Новый front: actor input binder `0x005A1C10`, slot-map controller discovery,
two-input priority/counter invariant, nonempty teardown; затем внешний
engine frame/resource/render caller. PS2 deferred. Цикл продолжается до deadline.

## Разведка следующего участка — исходная WIP-запись

`research/probe_pc_actor_binding.py` прошёл **44/44** original checks:
real PC node factory `421E20` даёт default flags `70A00`; actor discovery
`5A33F0` создаёт controller/evaluator, ставит `2000`, игнорирует повтор уже
marked node. Реальные SAN IDs через original actor slot map и binder `5A1C10`
доходят до evaluator inputs. Уничтожение всей nonempty цепочки освобождает все
tracked allocations. Duplicate node names создают два controllers, но map
оставляет последний evaluator: совпадающие имена не являются независимыми slots.

`research/probe_pc_transform_inputs.py`: **14/14**, оригинальный insert/clear
с host safety fence **до** потенциального третьего input. Exclusive replacement
не уменьшает old slot0 counter, но уменьшает old slot1; repeated exclusive
rebind увеличивает `state+48` снова. Этот счётчик нельзя называть точным числом
живых входов без оговорки. Nonexclusive duplicate rebind балансируется.
Incoming input сохраняет **cache физического места назначения**; moved retained
input переносит свой cache. Pointer-only clear не меняет count/priority/caches.
Полный upstream capacity/lifetime invariant пока открыт.

Read-only call candidates проверены дизассемблированием:
`5A205B` — start helper `5A1E30`, `5A216D` — stop, `5A2D71` — tick rebind.
Start helper request extent использует `+00..+34`; source animation `+18`
формирует старшие 8 bits unsigned input priority, младшие 24 берутся из
manager frame. Это пока **static WIP**, не новый начисленный class score.
Outer manager caller найден по **`41CDD1` внутри `41CD50`**, this-owner `+3C`.
По surrounding code это engine frame pipeline; полный запуск/контракт helper
и его собственный caller ещё не проверены. Logs:
`.codex-tmp/cycle-1000-actor-input-binder.log`,
`.codex-tmp/cycle-1000-actor-start-engine-frame.log`.

## Checkpoint 2 — input insertion / actor start

Snapshot **2026-09-05 19:48:15 UTC /22:48:15 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-actor-binding.json),
[подробная карточка](../../docs/research/native-pc-actor-binding.md).
WIP start выше теперь исполнен: **56/56** original start/restart checks,
priority formula, request mutation, события с различными payload и normal flush.
Третий blended candidate остановлен **до binder CALL5A205B**, поэтому unsafe
input не вставлялся. Локальная защита до этого CALL не найдена; upstream остаётся открыт.

Portable evaluator insert/clear восстановлены: фиксированные два physical slots,
active count, unsigned priorities, destination cache inheritance, асимметрия
counter при exclusive replacement. Counter view привязан к actor state48.
Host capacity/index/counter guards явно отделены от original поведения.
Animation priority-group field18 добавлен с analytical accessor/default0,
name-only clone его не переносит; request38 и offset18 закреплены ABI assertions.
Actor Start/tree/Rebind **ещё не перенесены**, хотя original helpers исполнены.

После header-aware rebuild:

- CTest **10/10**; input C++ **15/15**, animation object **54/54**;
- input differential **4368/4368 /156 cases**;
- прежний actor differential **1857/1857 /93 cases**;
- original discovery/binder **44/44**, insert/clear **14/14**, start **56/56**.

Первый повтор differential ошибочно вызван без обязательного `--portable`:
argparse отказал до тестов. Повтор с явным свежим executable прошёл; результат
ошибочного вызова не учитывается как проверка. Исходные guest probes прошли.

DB native-only trial `(4,true)`, repeat `(0,false)`, FK `[]`; canonical import
без рескана EXE. **13 imports /1529 evidence /52 snapshots**, +4 classes/+9 evidence.

| Scope | Checkpoint 2 | Fixed units |
|---|---:|---:|
| Всё EXE | **13,84%** | 108,525/784 |
| Engine | **26,34%** | 98,250/373 |
| Game | **2,50%** | 10,275/411 |
| Direct SMO/SAN | **45,62%** | 16,880/37 |

Следующий связный шаг: complete tree wrapper/stop, portable owned binding/start
и registry lease lifetime. Затем outer engine frame/helper41CD50 и resource/render.
Исследование продолжается до 10:00 МСК; это промежуточный checkpoint.

## Checkpoint 3 — descendant/Stop и owned SAN name bindings

Snapshot **2026-09-05 20:07:20 UTC /23:07:20 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-animation-owned-bindings.json).
Original wrapper `5A35C0` пропускает supplied root и идёт preorder по descendants,
включая детей неанимированного промежуточного node. Real node objects + synthetic
child-list edges, не native Attach. Stop/StopAll, pointer-only cleanup, remaining
input promotion, flush **перед immediate event3** и повторный inactive Stop
проверены: **19/19**. Destructor order уточнён: clear2000 → unbind → delete
controller, затем playback/vector/map, inherited controller. Animations borrowed.

Переносимый manager теперь выдаёт move-only owned name leases, track освобождает
их при destruction/shrink/rebind/manual-slot replacement. Weak lifetime token
и сохранённое acquired name — явные host safety measures. Native требует живой
manager до release; reverse lifetime/rename не выдаются за original API.
Serializer получил owned-binding wrapper **на том же field core**, не новый
парсер. Partial acquisition очищает references, но не stream position/nextID.

Проверки:

- manager C++ **68/68**, SAN reader **117/117**, CTest **10/10**;
- owned registry native/portable **81+261=342** comparisons на `bbush`/`bflower`;
- прежний reader/PRS four-asset differential **234+780+909+909=2832/2832**;
- оригинальные probes освобождают все tracked allocations; third input не запускался.

Первый compile теста обнаружил неверное тестовое имя `SetPosition`; заменено
на существующий `Seek(essStart,0)`, новая сборка и все проверки выше прошли.
DB trial `(4,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import
без EXE rescans. **14 imports /1535 evidence /56 snapshots**. Четыре карточки,
шесть evidence; score повышен только actor72→74 за новые native helpers.
Source integration сама по себе не повысила уже исследованные manager/track/reader.

| Scope | Checkpoint 3 | Fixed units |
|---|---:|---:|
| Всё EXE | **13,85%** | 108,545/784 |
| Engine | **26,35%** | 98,270/373 |
| Game | **2,50%** | 10,275/411 |
| Direct SMO/SAN | **45,62%** | 16,880/37 |

Далее portable actor tree/Start/Rebind/Stop на уже проверенных блоках,
затем outer frame/resource/render. Полный actor API пока не готов.

## Checkpoint 4 — owned actor runtime и node startup

Snapshot **2026-09-05 20:43:07 UTC /23:43:07 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-actor-owned-runtime.json),
[полная карточка](../../docs/research/native-pc-actor-owned-runtime.md).
Portable actor теперь владеет node controllers/name leases и соединяет
descendant discovery, Start/Rebind/Stop/StopAll с прежним Tick, SAN keys и node.
Original event payload/order, per-state old-pointer capture и input caches/counters
сохранены. Host two-input preflight работает на копии и не публикует отказ;
это не найденный original rollback. Animations borrowed, Stop не снимает pointer.

Первое сравнение обнаружило missing guest static initialization, не ошибку
portable identity default: real node constructor копирует global7600BC в local/
world matrices. Original initializer6D38E0→462250 найден через table73F800 и
теперь исполняется fixture. Отдельный original node lifecycle probe **13/13**.

Проверки:

- CTest **11/11**, owned actor C++ **81/81**;
- шесть real-SAN actor sequences **5154/5154** comparisons;
- прежние actor **1857/1857** и input **4368/4368** differential — повторены;
- после initializer исправления повторены discovery44/start56/tree-Stop19;
- all tracked original allocations released, unsafe third input не исполнялся.

Native manager dispatch-ит actor; portable observation fixture продвигает counter
при disabled actor и явно вызывает тот же Tick для записи actions. Реальный
portable manager dispatch отдельно проверен C++ suite. WorldUpdate(1) явно
вызван при snapshot; внешний engine-frame hook этим не объявляется найденным.
Первый compile нового теста потребовал fully qualified evidence namespace;
после исправления fresh header-aware build и проверки выше прошли.

DB trial `(3,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import
без EXE rescan. **15 imports /1541 evidence /60 snapshots**. Actor74→78 за
сквозную native validation, Node94→95 за startup/ctor; NodeController74 без
накрутки score за одно лишь source integration. Fixed denominators прежние.

| Scope | Checkpoint 4 | Fixed units |
|---|---:|---:|
| Всё EXE | **13,85%** | 108,595/784 |
| Engine | **26,36%** | 98,320/373 |
| Game | **2,50%** | 10,275/411 |
| Direct SMO/SAN | **45,65%** | 16,890/37 |

Продолжается текущий цикл до 10:00 МСК. Следующий фронт — remaining actor controls/
upstream capacity, outer frame41CD50/owner3C и resource/render. Игра, GUI, PS2,
релиз и публикация не запускались, app/assets не изменялись.

## Checkpoint 5 — PC app/core frame и `spTaskTimer`

Snapshot **2026-09-05 21:22:35 UTC /2026-09-06 00:22:35 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-engine-frame-timer.json).
Связный шаг от actor наружу: [PC app/core frame](../../docs/research/native-pc-engine-frame.md),
[`spTaskTimer`](../../docs/research/native-class-sp-task-timer.md).

Найден и исполнен прямой `4C2D7D→41CD50`: PC app всегда запускает общий update,
даже в фоне, затем graphics только для foreground/owner либо Sleep1ms. Return
обоих helpers игнорируется. Bootstrap fields теперь связаны с registered
Animation/Audio/GUI/Cinematic/Network manager classes. Общий update имеет13
внешних этапов плюс nested actor flush110; отдельно проверен graphics first-boundary
two-phase camera-vector walk и begin/end gates.

В engine54/90 доказаны два exact `spTaskTimer`3C, engineB8 — delta28 второго.
Timer factory/one-arg ctor/blank clone/non-owning dtor и четыре операции
исполнены и перенесены. Pause/Reset не очищают delta немедленно; source clock
и child membership независимы. Source/children borrowed, native attachment
helper неизвестен. Host cycle/capacity/divisor guards отделены от original.

Проверки:

- native app/update **134/134**, включая actual-inline-timer вариант;
- native graphics core **209/209**, backend/cameras — явные seams;
- native timer **34/34**, portable timer **30/30**;
- timer differential **1968/1968**,328 cases, raw float bits;
- hash/call/registration anchors **23/23**, CTest **12/12**;
- native accounting regressions **6/6**, только in-memory tables.

Нормальные guest probes освобождают все tracked allocations. Начальные ошибки
fixture: неучтённый nested actor flush, ctor scout без pointer argument при
original ret4, long-displacement LEA anchor вместо actual short54. Исправлены
только fixture/inspector после чтения original traces. Whole core constructor
отдельно остался на VM-limit (100k,500k, затем bounded continuation2M/20s),
не объявляется разобранным. Common guest limits не менялись. Игра/OS/GPU не запускались.

В учёте обнаружены старые `spEngineCore`/`spPCApp` cards/source, уже включённые
в агрегатный baseline, но без individual progress rows. Добавлен optional
`coverageAccounting=baseline_backfill`: только для missing row + обязательная
причина, **нулевой aggregate delta**. Это предотвращает двойное начисление;
текущий core/app прирост консервативно также не начислен. Будущие increments
смогут использовать сохранённые80/75. Старые manifests не менялись.
TaskTimer0→78 и Manager80→81 дают единственный прирост0,790 units.

Native-only DB trial `(4,true)`, repeat `(0,false)`, FK `[]`; canonical import
без EXE rescan. **16 imports /1549 evidence /64 snapshots**.

| Scope | Checkpoint 5 | Fixed units |
|---|---:|---:|
| Всё EXE | **13,95%** | 109,385/784 |
| Engine | **26,57%** | 99,110/373 |
| Game | **2,50%** | 10,275/411 |
| Direct SMO/SAN | **45,65%** | 16,890/37 |

Продолжается цикл до10:00 МСК. Следующий логический front — portable full frame,
scene manager45A7D0→world-update→palette/render, с необходимыми timer/event
зависимостями. Full SMO/SAN/FAT transaction, writer, callback reentry и unsafe
third-input invariant остаются открыты. Ничего не публиковалось.

## Checkpoint 6 — original сцена, world frame и исправление PC камеры

Snapshot **2026-09-05 22:06:16 UTC /2026-09-06 01:06:16 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-scene-world.json),
[подробная карточка](../../docs/research/native-pc-scene-world.md).

Actual app→timers→SAN actor→SceneManager→owned System Root→world исполнен
без отдельного вызова UpdateWorld из fixture. Native421A60 присоединяет subtree,
переносит между двумя сценами, распространяет scene links/bit100, правильно
освобождает intrusive references. Scene создаёт root и четыре registered
sub-managers; их lifecycle исполнен, алгоритмы ещё не считаются закрытыми.
Manager ctor —45AD50→13CD6C0;45ACE0 оказался node→scene resolver, не ctor.

Original core setup41C300 доказал timer previous10/tail34/count38 и реальный
source/child append54→90. Пустая/непустая ветви и false scene-init проверены:
ошибка не удаляет созданную сцену, не создаёт камеру, не связывает таймеры.
Portable timer append добавлен с явными host graph guards.

Обнаружена и исправлена старая **ошибка нашей PC ABI**, не EXE:
view/projection matrices **D4/114→CC/10C**, basis154→14C. Обе camera factories
реально выделяют238, не только observed extent. SetViewAngle очищает2D:
portable setter исправлен. Original orthographic cache обновляет лишь четыре
коэффициента; fresh-matrix utility теперь не выдаётся за stateful cache update.

Проверки: native scene/world **80/80**, core setup **32/32**, app→world **39/39**,
camera boundary **32/32**, static anchors **29/29**; timer C++ **34/34**, прежний
timer differential повторён **1968/1968**, CTest **12/12**, `git diff --check` clean.
Все tracked allocations normal probes освобождены. Initial bare scene scout
требовал существующий string-owner seam; camera destructor — engine singleton
storage, иначе попадал в известный bounded whole-core ctor. Native код не менялся.
Ошибочный CALL anchor на preceding push исправлен на428574. Никаких OS/GPU/game
вызовов, app/assets/PS2 изменений, релиза, commit или push не было.

DB trial `(8,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import
без EXE rescan. **17 imports /1561 evidence /68 snapshots**. New Scene35/
SceneManager62 и Timer78→83/Core80→81/Node95→96 дают1,040 units. Pre-existing
Camera78/CameraData72/DXCamera72 внесены через zero-delta baseline_backfill.
Неизвестные typed scene side effects и отсутствие portable scene source явно
сохранены; четыре scene-owned sub-managers пока не оценены отдельно.

| Scope | Checkpoint 6 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,08%** |110,425/784|
| Engine | **26,85%** |100,150/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **45,68%** |16,900/37|

Цикл продолжается до10:00 МСК. Следующий логический участок — typed scene
registration45A970/45AB30 (RenderNode, Light, Partition, Occlusion), затем
portable Scene/SceneManager и camera/scene culling/render. Full native resource
transaction, event dispatcher и source names не заменяются аналитическими догадками.

## Checkpoint 7 — PC render-node registry, world/cull и portable math

Snapshot **2026-09-05 22:42:00 UTC /2026-09-06 01:42:00 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-render-node-runtime.json),
[подробное доказательство](../../docs/research/native-pc-render-node-runtime.md).

Original425520→13C5390 выделяет exact1D4. Исправлена наша старая трактовка
vtable:14 primary +6 secondary с thisB4, не20 primary. Полный PC ABI покрывает
spheresC8/D8, light-cacheF0, scene128/12C, cull bypass130, matrix caches138/178,
callback vector1C4. Untouched allocator/padding не превращаются в нули.

Native scene render-list — borrowed intrusive head/tail/count18/1C/20, не
vector refs. Actual Attach/reparent переносит три render-узла и nested branch
между двумя scenes без extra refs; middle/head/tail/remove/teardown проверены.
World4250F0 captures flags до base call; clean billboard re-arms следующий кадр.
Bounds-only469820 использует cached matrix X-radius; PRS-dirty затем заменяет
его max(abs scale) radius. Матрицы lazy, dirty снимается до backend success.
Six-plane cull сохраняет касание и пропускает radius<=.001. Direct draw ignores
leaf return; queue456310 останавливается на false. Callback remove использует
swap-last, drain перечитывает list после callback; self-removal может пропустить
соседний callback. Это сохраняется как native hazard, не исправляется догадкой.

Перенесена математика `Analysis/PC/spRenderNodeMath.h`, **не full class runtime**.
Проверки: original registry **55/55**, world/caches/cull/draw **54/54**,
callbacks **15/15**, PC static **21/21**, C++ **11/11**, CTest **13/13**;
differential **1504/1504**,144 cases, abs/rel3e-5 (не raw-bit x87 promise).
Нормальные native allocations освобождены; raw geometry/device/RTTI startup
fixtures отмечены явно. Renderer-less initial scout остановился на dependency
в destructor; исправлена fixture storage. IDA alignment bait проверялся raw
Capstone с реальной entry, не изменением оригинальных bytes. No OS/GPU/game/PS2.

DB native-only trial `(3,true)`, repeat `(0,false)`, FK `[]`; canonical import
без EXE rescan: **18 imports /1569 evidence /72 snapshots**. RenderNode80→87,
Scene35→40, SceneManager62→66 дают0,160 units, direct0,070. `git diff --check`
clean. Внешних публикаций/commit/push, apps/assets изменений не было.

| Scope | Checkpoint 7 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,11%** |110,585/784|
| Engine | **26,89%** |100,310/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **45,86%** |16,970/37|

Цикл продолжается до10:00 МСК. Следом специализированные registrations
45A810/45A8C0 и Light/Partition/Occlusion, затем portable scene/runtime wiring.
Unknown projection ID58DA4026, light-cache/support original names, clone/cache
и geometry getter integration остаются явно открыты.

## Checkpoint 8 — original PC LightManager и перенос selection/cache

Snapshot **2026-09-05 23:19:35 UTC /2026-09-06 02:19:35 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-scene-lights.json),
[подробная карточка](../../docs/research/native-class-sp-light-manager.md).

Оригинальный manager46AB80 выделяет24: borrowed list10/14/18, render-list1C
и owner20. Последний **untouched ctor**, не0; его заполняет Scene. Actual
Attach/reparent переносит Light между сценами и снимает старый cache до unlink.
Original world refresh428C30→46AC60 обновляет nonempty RenderNode cache;
recursive Node420DE0 переключает100, не200 и не membership. Узкий Node helper
перенесён в исходник, автоматическое source Scene wiring ещё открыто.

Eligibility использует lightED/hierarchy100, target121 shadow-exclusion и
пересечение сфер для Point/Spot; Directional/Ambient без distance test.
Helper28 хранит8 ordinary+first ambient. Duplicate ignored, middle remove
swap-last со stale unused tail, reset очищает только count/ambient. Native
partition traversal исполнен на explicit literal payload tree, concrete type
не назван догадкой. Debug manager dispatch исполнен для directional no-op,
не как GPU drawing. Новый portable `spLightManager` содержит borrowed list,
blank clone, eligibility/cache/rebuild/refresh, не всю scene подсистему.

Protected PC LightData factory41A330 доказал exactF0;428EB0→505CB0 подтвердил
пропуск intensity: existing9.5 сохраняется при source3.25, fresh clone1.0.
Это оригинальное поведение, прежний portable copy не исправлялся наоборот.
Документация больше не считает PC allocation/copy неизвестными.

Проверки: **90/90 native**, **22/22 static**, **29/29 C++**,
**1800/1800 differential**,540 cases, **CTest14/14**. Rebuild после formatting
повторён. Initial fixture corrections: explicit root-clone pair seam,
protected capacity8 после первой resolving call, untouched owner20 и cleanup
lazy CloneManager74E060. Original bytes/limits не менялись; все tracked
allocations normal probes freed. Никаких OS/GPU/game/PS2/app/assets writes.

DB native-only trial `(6,true)`, repeat `(0,false)`, FK `[]`; canonical import
без EXE rescan: **19 imports /1579 evidence /76 snapshots**. LightManager72,
LightData80→87, SceneManager66→70, RenderNode87→88:0,840 units/direct0,080;
Node96 неизменен. Старый Light source/card без row backfilled84 с нулевым
delta, чтобы не начислять прошлую работу повторно. No commit/push/release.

| Scope | Checkpoint 8 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,21%** |111,425/784|
| Engine | **27,12%** |101,150/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **46,08%** |17,050/37|

Цикл продолжается до10:00 МСК. Следующие same-scene dependencies:
specialized SkyBox/LensFlare/Projection registrations, SceneInit/Partition/
Occlusion, затем source Scene/world integration. Неизвестные original names,
opaqueDC/setters, projection58DA4026 и concrete partition payload сохраняются
как открытые вопросы, не заменяются придуманными классами.

## Checkpoint 9 — specialized scene managers и camera-follow sky

Snapshot **2026-09-05 23:53:32 UTC /2026-09-06 02:53:32 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-scene-special.json),
[подробная карточка](../../docs/research/native-pc-scene-special-managers.md).

Actual45A810/45A8C0 закрыт: exact SkyBox early return; LensFlare435370B5;
четыре Projection IDs, включая ещё unnamed58DA4026. Original managers при
Attach/reparent сохраняют разные list semantics: sky sentinel duplicate append/
all-match removal, projection sentinel duplicate scan, flare intrusive58/5C.
Exact ABI Sky1D4/SkyManager24/PCProjection24/PCLens38 добавлен; header/TU inferred.

SkyBox **наследует RenderNode, не напрямую Node**; прежняя wire-card исправлена.
Native game blocks5DA6CA/5F9F10 назначают manager14=engine DefaultCamera и
вызывают snapshot48DB40. Actual same-scene reparent/world подтвердил camera
position inheritance +local orientation override после inherited world.
Regular support no-op; separate48DA60→45AD60 проходит все skies,AND results,
игнорирует flush return. Dedicated49E5F0→4CC3C0 снимает **fog24**, не alpha-sort,
потом вызывает base draw424B60. Mutation сохраняется даже у disabled sky.

PC Projection phases2C/30 original true stubs, late34 calls projection38;
helper owner write/ignored return и external enable gate подтверждены.
PC Flare Init capability сравнивает exact8876086A: другое error80004005 всё
равно даёт capability true; Init always enables/returns1. D3D query — explicit
interface seam, не GPU. Query-map nonempty cleanup/composition остаются открыты.

Native проверки **81/81 registration**, **39/39 sky**, **43/43 projection/flare**,
static **25/25**, ABI rebuild и **CTest14/14**. Initial corrections: guessed
ret4 для48DA30 оказался no-arg; empty sky still flush; setter оказался fog;
shared native name-copy потребовал local refcount-aware name-release fixture.
Original bytes/лимиты не менялись; normal tracked allocations освобождены.
Portable classes в этом checkpoint не добавлены: ABI/evidence не выдаются за
полную source implementation. Apps/assets/GPU/game/PS2 не трогались.

DB trial `(8,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import
без EXE rescan: **20 imports /1592 evidence /80 snapshots**. SkyBox25→65,
new SkyManager70/Projection48/PCProjection52/LensManager34/PCLens40,
Scene40→45/SceneManager70→74:2,930 units/direct0,400. Ограниченные игровые
caller blocks не означают закрытого игрового класса, game score не повышен.

| Scope | Checkpoint 9 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,59%** |114,355/784|
| Engine | **27,90%** |104,080/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **47,16%** |17,450/37|

Цикл продолжается до10:00 МСК. Следом portable RenderNode/derived virtual
world/geometry getters и Scene graph integration, обязательные SceneInit→
Partition/Occlusion и projection/flare runtime consumers. Не останавливаться
на recording callbacks вместо анализа ещё присутствующего оригинального кода.

## Checkpoint 10 — Model → RenderNode source/world и реальный clone map

Snapshot **2026-09-06 01:05:00 UTC /04:05:00 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-model-render-world.json),
[подробная карточка](../../docs/research/native-pc-model-render-world.md).

Actual Model factory exact60/default3, getters sphere/minmax без validity-gate,
Mesh sharing, no-op Model mode hook и direct pre/mesh/post protocol исполнены.
Pre false означает успешный skip; mesh false не вызывает post; fog return
игнорируется. Renderable28 — raw DWORD из Debug41D4E0, не renderer float.
PC pre-vector44/byte54 и post-vector34/byte55 исправлены в ABI.

Native append469ED0 сразу пересчитывает bounds без dirty bits. Copy в непустой
RenderNode **добавляет**, не заменяет: каждое повторное вхождение Model получает
независимый clone, Mesh shared. Spheres копируются дважды, matrices/dirty/light
cache не копируются. Реальный static map init/overwrite/root cleanup исполнен
без pair seam:412BE0 always-clone,412C40→4D3810 map-aware. Соседний4D3800 —
другой constructor; public entry разрешает protected constant, raw body до
этого невалиден. Original bytes/лимиты не менялись.

В классы перенесены virtual world dispatch, geometry getters, bounds, lazy
world/inverse matrices, controls/cull и explicit light-manager binding.
Host detach/self-copy guards отмечены; automatic Scene/Partition/GPU ещё нет.
PC MeshData58 owners50/54 untouched: teardown fixture явно обнулила cold
owners и исполнила Resource→lazy ResourceManager30/dtor. Это не native Init.

Проверки: **47/47 Model**, **29/29 ownership**, **23/23 static**, **34/34 C++**,
**2272/2272** cache fields/32 finite сценария; прежние1504/144 проходят;
**CTest14/14** после последнего ABI rebuild. Все normal tracked allocations
освобождены. Apps/assets/PS2/GPU/game/publication не трогались.

DB trial `(7,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import:
**21 imports /1602 evidence /84 snapshots**. Model75→83/RenderNode88→92/
MeshData80→82:0,140 units/direct0,140. Node96 unchanged. Прежние Renderable72/
CloneManager82/ResourceManager75 без progress row backfilled с нулевым delta.

| Scope | Checkpoint 10 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,60%** |114,495/784|
| Engine | **27,94%** |104,220/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **47,54%** |17,590/37|

Цикл продолжается до10:00 МСК. Следующий связный узел — nonempty Renderable
callbacks/alpha-queue/material state, renderer queues и derived world; затем
обязательные SceneInit→Partition/Occlusion, не случайный новый лёгкий класс.

## Checkpoint 11 — Renderable callbacks и renderer queues

Snapshot **2026-09-06 01:37:45 UTC /04:37:45 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-renderer-protocol.json),
[подробная карточка](../../docs/research/native-pc-renderer-protocol.md).

Nonempty pre/post groups исполнены: cdecl5 args, -1 stable erase с increment
ordinal,0 stop только group; direct cdecl3 args проверяет AL. Два последовательных
dispatch сохраняют правильный remaining vector. Material6C сохраняет renderer
C1C4 в одном7400FC; mesh failure skips post/restoration, nested pre перезаписывает
save. Alpha enqueue failure тоже игнорируется, Model возвращает успешный skip.

Original alpha2048×24 и general20 queues/frame flush проверены. Priority wrap,
camera231 z² против perspective distance²; exact ParticleSystem отдельно.
Alpha comparator даже equal/NaN даёт+1; qsort fixture **только preordered**,
original CRT tie order не доказан. General original sort456090 выполнен.
Both flush ignore support/model failures, logical count cleared/capacity kept.
Nine typed vectors8 принимают mode0/2/8, но все PC pass slots20..40=5B7A00;
вызываемые семь phases не рисуют. Startup75F8E8 остаётся unknown.

В исходниках spRenderable callback phases/raw28 copy; full material/renderer
source ещё не подключён. Queue math в Analysis/PC, record8/20/24 ABI сохранены.
Native **75+64**, static**28**, C++**23**, differential**768**:192 fields/64 key
cases +576 fields/48 двухшаговых callback cases. **CTest15/15**, diff-check clean.
Initial compile исправлен на существующий model.Clone API; assertions затем
прошли. Full PCRenderer factory scout остановлен protection100k/2s; limits
не повышались, partial guest discarded. Normal probes free all tracked objects.

DB trial `(4,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import:
**22 imports /1609 evidence /88 snapshots**. Renderable72→85/Model83→85 дают
0,150 units/direct0,020; RenderNode92 unchanged. Старый Renderer source/card
backfilled72 с нулевым delta, без повторного учёта. No app/assets/PS2/GPU/game/
commit/push/release.

| Scope | Checkpoint 11 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,62%** |114,645/784|
| Engine | **27,98%** |104,370/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **47,59%** |17,610/37|

Продолжаю до10:00 МСК: Scene45EC70 и SceneInit→Partition/Occlusion, original
manager types/global ownership и derived world source по тому же графу.

## Checkpoint 12 — Partition graph и Static/Partition render supports

Snapshot **2026-09-06 02:16:39 UTC /05:16:39 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-partition-runtime.json),
[карточка](../../docs/research/native-pc-partition-runtime.md).

Actual PartitionNode84/ZoneC8/System1D8 и Static10C/PCPartitionRenderable8C
constructed/cloned/deleted. System физически RenderNode, но native RTTI parent
Node: IsKindOf(RenderNode)=false. Zone root vector borrowed, root children/
payload direct-owned, Zone/portals/static intrusive. Actual reciprocal Node
registration допускает duplicate refs и корректно unregisters с обеих сторон.
Rootv80 non-notifying reset оставляет reverse links; unsafe scout teardown
fault подтвердил precondition, постоянный test явно clears reverse links перед
teardown. Никакие native guards не убирались, лимиты прежние.

Общий support74 имеет разный complete-object offset. Static8C/CC matrices
submit independently; PCPartition uses shared760058. Actual CRT6D38C0 обязателен:
первое предположение о default zeros опровергнуто initialized native ctor,
исправлено на copied identity, cold probe сохранён как negative startup test.
Static/Partition draw ignores matrix failure, но stops first failed Model;
RenderNode делает наоборот. Original general queue append исполнен.

Native **38/38 spatial**, **38/38 render**, **30/30 static**, ABI build,
**CTest15/15**. Здесь новый source срез — exact ABI, не complete portable
spatial classes. Visibility exact name/AC allocation найден, whole ctor capped;
Shadow global named DXShadowVolumeManager/factory3C scout успешен, GPU не вызывался.

DB trial `(8,true)`, repeat `(0,false)`, native-only FK `[]`, canonical import:
**23 imports /1620 evidence /92 snapshots**. Six spatial scores дали2,450 units,
из них direct1,800; Scene45/RenderNode92 без прироста, denominator unchanged.

| Scope | Checkpoint 12 | Fixed units |
|---|---:|---:|
| Всё EXE | **14,94%** |117,095/784|
| Engine | **28,64%** |106,820/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **52,46%** |19,410/37|

Продолжаю до10:00 МСК: bounded Visibility dependency, whole SceneInit/render,
portable spatial classes и static serializer->matrix consumption. No app/assets/
PS2/game/GPU/commit/push/release.

## Checkpoint 13 — SceneInit и Visibility selection

Snapshot **2026-09-06 03:04:21 UTC /06:04:21 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-visibility-runtime.json),
[карточка](../../docs/research/native-pc-visibility-runtime.md).

Original whole SceneInit/fallback ownership и typed new-Partition transfer
исполнены: RenderNode переходит в новый root до non-notifying old reset;
LightManager list link теперь выставляет сам Init, не старая fixture assignment.
Whole Visibility constructor остаётся capped46C0F0; borrowed manager record
явно отделён от factory-success claim. Оба native scratch/pool constructors,
resize32/destructors и original pool shutdown исполнены отдельно.

Original Visibility46D270 и mixed payload/static/dynamic traversal доказали
adjusted support pointers, stamps/root7C/Scene40, Enabled/bypass/debug gates,
override-camera и unsigned wrap. Начальная плоскость через камеру **не near1C4**.
Два Zone roots ordinary path посещает оба; Debug21 повторяет system root.
Это сохранено как особенность исходного EXE, не исправлялось в игре.
Второй кадр требовал явного imported CRT memmove seam (bounded4096, cdecl),
не исправления оригинального engine кода. Source — частичный original-named
`spVisibilityManager` record selection и anonymous plane math, не Scene walk.

Native **20/20 SceneInit**, **39/39 Visibility**, static **25/25**, portable
**17/17**, differential **1215/1215** (536/268 sphere cases +679/64 selection
cases), **CTest16/16**, diff-check clean. Whole factory/portal/occluder/Octree/
GPU remain open. В SMO карточках убрано смешение «wire layout завершён» с
«весь класс/runtime разобран».

DB native-only trial `(5,true)`, repeat `(0,false)`, FK `[]`; canonical import
**24 imports /1628 evidence /96 snapshots**. New Visibility50, Scene45→58,
System52→58, Node58→63, Zone65→68 дают0,770 units/direct0,140; no backfill,
fixed denominator и неизменённые исторические manifests.

| Scope | Checkpoint 13 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,03%** |117,865/784|
| Engine | **28,84%** |107,590/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **52,84%** |19,550/37|

Цикл продолжается до10:00 МСК. Следующие обязательные зависимости той же
SceneRender цепочки: ShadowVolumeManager, OcclusionVolume и Octree/portal
геометрия; не переход к случайным классам. No apps/assets/game/OS/GPU/PS2/
commit/push/release, ограничения эмуляции не повышались.

## Checkpoint 14 — whole SceneRender и Shadow/DX state boundary

Snapshot **2026-09-06 03:30:18 UTC /06:30:18 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-scene-render-runtime.json),
[карточка](../../docs/research/native-pc-scene-render-runtime.md).

Original45EC70 исполнен целиком на ordinary/partition и perspective/alternate
camera ветках: actual scene graph, visibility, supports/Model, queues,
actual empty Shadow/Lens phases. GPU device/mesh payload/renderer memory
остаются explicit fixtures, Visibility ctor gap прежний. Первая aggregate
batch достигла30sec; пять **независимых свежих** сценариев разделены, native
calls не продолжались после cap,100k/2sec/child30sec сохранены.

Доказаны camera failure gates и неодинаковые matrix/mesh failure policies
RenderNode/Static/queue. Исправлено прежнее ожидание безусловного event1A:
он **только внутри Debug18 tail**; отдельный empty-debug test исполнил его.
Actual Shadow3C ctor/RTTI/Named-only clone/dtor и singleton hazards закрыты;
shader Init static-only и nonempty light-volume rendering ещё open.

DX4B0A90 всегда caches changed value независимо от HRESULT; повтор того же
значения пропускает device retry. Source `spDXRenderer` single-entry helper
не выдумывает full cache extent/startup. Shadow ABI отдельно, не portable class.
Native **67** (21+17+8+19+2), Shadow **14/14**, static **23/23**, source **6/6**,
differential **960/960** /96 двухшаговых cases, **CTest17/17**, diff-check clean.

DB trial `(4,true)`, repeat `(0,false)`, native-only FK `[]`; canonical import
**25 imports /1635 evidence /100 snapshots**. Scene58→68 +new Shadow30/
DXShadow35 дают0,750 units; older DXRenderer source/card migrated35 без нового
aggregate credit и без выдуманного historical individual score. Direct scope
не расширен managers и здесь не изменился.

| Scope | Checkpoint 14 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,13%** |118,615/784|
| Engine | **29,05%** |108,340/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **52,84%** |19,550/37|

Продолжаю до10:00 МСК. Следующий direct SMO участок этой цепочки —
OcclusionVolume mesh/volume-plane processing, затем Octree/portal queries.
Никаких game/assets/apps/OS/GPU/PS2/commit/push/release изменений.

## Checkpoint 15 — OcclusionVolume geometry и Scene rejection

Snapshot **2026-09-06 04:05:26 UTC /07:05:26 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-occlusion-runtime.json),
[карточка](../../docs/research/native-pc-occlusion-runtime.md).

Original class TU `Code/Sparkplug/spOcclusionVolume.cpp` найден отдельно от
serializer. Exact1B8/14 slots, actual ctor/dtor/Node-only clone, три owned CPU
buffer pointers и signed world/sphere transform, reciprocal partition links.
Native standalone UInt16 weld удаляет duplicate vertex и remaps IB; original
comparator raw-byte/shared-context semantics подтверждены, включая signed zero.
CRT insertion sort — явная bounded fixture, не оригинальный MSVCRT алгоритм.

Full Init470FE0 дошёл до protected geometry и остановлен на100k, без повышения
лимита и без продолжения прерванного вызова. Отдельные prepared buffers и
cached silhouette объявлены **входами** теста, не успешным результатом Init.
С ними original plane builder и whole Scene реально скрывают behind support,
сохраняя foreground/side. Enabled не unregister-ит Occlusion. Scene игнорирует
plane-builderfalse и может использовать старый cache после actual clear;
штатный игровой draw между clear/reinit не заявлен.

Исправлена старая документация: наличие вогнутого authored decagon в корпусе
не доказывает успешного runtime Init. Reader действительно вызывает Init и
проверяетfalse; exact admission/failure transaction остаются открытыми.

Native **84/84** (18 lifetime+8 weld+18 Scene+40 planes), static **24/24**,
portable visibility **24 checks**, differential **1483/1483** (804 sphere /
268cases+679 selection /64cases), **CTest17/17**. Source change — fully-inside
math и exact ABI; полноценного portable Occlusion class пока нет.
DB trial `(3,true)`/repeat `(0,false)`/native-only FK `[]`; canonical
**26 imports /1641 evidence /104 snapshots**. Delta0,370 all/engine,
0,330 direct, без backfill и изменения fixed denominator.

| Scope | Checkpoint 15 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,18%** |118,985/784|
| Engine | **29,14%** |108,710/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **53,73%** |19,880/37|

Продолжаю до10:00 МСК по той же spatial цепочке: concrete Octree queries,
далее порталы. Safety limits прежние; apps/assets/game/PS2/publication не трогал.

## Checkpoint 16 — Octree queries и частичные original-named классы

Snapshot **2026-09-06 04:31:04 UTC /07:31:04 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-octree-runtime.json),
[карточка](../../docs/research/native-pc-octree-runtime.md).

Actual factory/ctorC8/eight null slots, uninitialized Pivot/scratch, zero bounds,
Base-only clone и direct-owned child teardown. Scratch90 — четыре ray-кандидата.
Original leaf/point/sphere/plane query contracts и packed traversal order
перенесены в **частичный** `spPartitionNode/spOctreeNode` source; inferred paths
помечены, отрицательный поиск original .cpp/.h в PC executable записан.

Sphere shortcut `(.7,.7,.9,r1)→F0` уже математического eight-orthants overlap;
его сохранили, не объявляя причиной старого LevelCreator бага. Native
dynamic/static/billboard/Zone placement и reciprocal lists проверены отдельно.
Ray tests исполнили forward/simultaneous/away/zero направления, full ray source
и remaining64..7C methods пока open.

Normal whole Scene с Octree остановлен100k внутри45E870. Isolated minimal
copy также capped,13B5E20 остаётся protected; вызовы не продолжались и caps
не повышались. **Отдельная original Debug21 ветка** whole Scene прошла8
листьев/8 support draws в порядке0,1,2,4,3,5,6,7. Это не normal-path success.

Native **43/43** (19+12+8+4), static **24/24**, C++ **28/28**,
differential **3104/3104 fields /320cases**, **CTest18/18**; full179-target
single-thread rebuild прошёл. Host-only guards/Zone-presence input отделены
от native ABI/полной ownership/Scene semantics.
DB trial `(3,true)`/repeat `(0,false)`/FK `[]`; canonical
**27 imports /1647 evidence /108 snapshots**. Delta0,460 all/engine,
0,440 direct, без нового denominator/backfill.

| Scope | Checkpoint 16 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,24%** |119,445/784|
| Engine | **29,27%** |109,170/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **54,92%** |20,320/37|

Следующий участок до10:00 МСК: ZonePortal/ZonePortalNode→polygon clipping,
с сохранением всех текущих protected-code gaps. No game/assets/apps/PS2/
OS/GPU/commit/push/release; только исследование/source/tests/docs/база.

## Checkpoint 17 — Portal geometry/source и original whole Scene traversal

Snapshot **2026-09-06 05:08:46 UTC /08:08:46 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-zone-portal-runtime.json),
[карточка](../../docs/research/native-pc-zone-portal-runtime.md).

Actual Portal38/NodeC4: owned polygon/borrowed destination и borrowed Node
vector, duplicates/null raw append, inherited-only clones. Node world и Enabled
не преобразуют portal geometry/Open. Native Init копирует вершины и строит
plane по первым трём, без convexity/planarity rewrite; collinear gives zero.
Добавлены partial `spZonePortal/spZonePortalNode` source, exact ABI и пять
original getter names из diagnostics. Paths inferred, host guards отмечены.

Whole45EC70 прошёл через actual portal clipping: front/closed/backface,
fully/partially clipped aperture и synthetic same-facing A→B→A cycle. Root
повторно посещается, portal stamp обрывает цикл, support не дублируется.
Camera-origin near/far сохраняются, portal plane не становится new near.

Первый partial-edge scout остановился на imported CRT __dllonexit; fresh
probe использует checked record-only callback registration, затем исполняет
original6D7F90 cleanup до pool6D7FA0. Это не подмена geometry helper и не
resume capped call. Camera-near-plane45E870 ветка остаётся открытой после
предыдущего независимого cap, full Visibility ctor также не объявлен готовым.

Native **63/63** (16+25+9+8+5), static **32/32**, source **19/19**,
plane differential **1024/1024 /256cases** (all bit-exact on these inputs,
tolerance2e-6, не universal x87 claim), **CTest19/19**, full183-step rebuild
и subsequent22-step update прошли. Исправлено старое «полностью разобраны»
в PortalNode wire-карточке: wire coverage не равна runtime completeness.

DB trial `(3,true)`/repeat `(0,false)`/native-only FK `[]`; canonical
**28 imports /1653 evidence /112 snapshots**. Delta1,080 all/engine units,
1,010 direct, без backfill или изменения fixed denominator.

| Scope | Checkpoint 17 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,37%** |120,525/784|
| Engine | **29,56%** |110,250/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **57,65%** |21,330/37|

Далее до10:00 МСК: polygon clipping491AA0/scratch lifetime. Лимиты прежние,
apps/assets/game/PS2/OS/GPU/publication не затрагивались.

## Checkpoint 18 — Polygon clipping и scratch lifetime

Snapshot **2026-09-06 05:26:21 UTC /08:26:21 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-polygon-clipping.json),
[карточка](../../docs/research/native-pc-polygon-clipping.md).

491660 — **logical resize**, не capacity-only reserve: shrinktail→pool,
grow/reuse с new serials, count0 с temporarily stale head.491A30 копирует
только vertices/plane10/opaque20, сохраняя destinationID/остальные поля.
491AA0 exact positive/negative/on classification, keepCoplanar и empty flags,
ordered intersections. Postpass **повторяет первую вершину**, не идёт по всем;
подтверждено epsilon примером, в source сохранено без «улучшения» оригинала.
Mixed in-place переносит весь global scratch, восстанавливает ID и opaque20;
metadata не равна out-of-place/copy behavior.

Добавлен geometry-only analysis helper, **не invented original class**.
Local arrays128 плюс closing duplicate требуют input<=127; native127 прошёл,
unsafe128 не запускался. Host127 guard явно отделён от поведения EXE.

Native **28/28** (9+11+8), static **22/22**, source **11/11**,
differential **3842/3842 fields /256cases**:512 return/count+3330coordinates,
all numeric bit-exact on these inputs, tolerance3e-6, не universal x87 claim.
**CTest20/20**, observed node1C/record2C ABI compile, actual callback/pool
cleanup и bounded process limits прежние. Старое слово reserve в C13 doc
уточнено; unresolved46C0F0 — другая операция, вопрос не закрыт по аналогии.

DB trial `(1,true)`/repeat `(0,false)`/native-only FK `[]`; canonical
**29 imports /1656 evidence /116 snapshots**. Visibility61→66:
0,050 all/engine units; direct score и denominator без изменения.

| Scope | Checkpoint 18 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,38%** |120,575/784|
| Engine | **29,57%** |110,300/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **57,65%** |21,330/37|

Далее до10:00 МСК — spBSPNode, другая concrete PartitionNode ветка в SMO.
Unknown helper names/metadata, pool errors и protected45E870/full ctor gaps
сохраняются. No apps/assets/game/OS/GPU/PS2/commit/push/release.

## Checkpoint 19 — spBSPNode и camera-leaf Zone

Snapshot **2026-09-06 06:24:15 UTC /09:24:15 МСК**,
[immutable manifest](../../research/native-research-2026-09-06-pc-bsp-runtime.json),
[карточка](../../docs/research/native-pc-bsp-runtime.md).

ExactAC, two owned null slots, untouched plane84/ray94, optional polygonA4/A8
zero; raw plane setter44CDA0 и independent deep polygon480210, Base-only clone
и actual owned cleanup. Добавлен partial original-named BSP source/ABI,
original getters из diagnostics; source/serializer TU paths не выдуманы.

Leaf480710: positive0, negative/zero/NaN1; epsilon PointMask и sphere overlap
отдельны. Ray2-record internal scratch, visible-children optional polygon
и enabled planes/ignored activeCount; no-op plane reduction в отличие от Octree.
Actual reciprocal registrations различают Zone/dynamic/static/billboard.
Whole Scene с BSP и two Zone leaves на трёх camera positions выбирает только
объект нужной комнаты; input decoded graph явный, не whole serializer claim.
Это не закрывает normal recursive plane-vector45E870 или full Visibility ctor.

Native **48/48** (16+15+12+5), static **17/17**, source **17/17**,
differential **2046/2046 fields /256cases** (1020+1026); all383 ray parameters
bit-exact на этой выборке, tolerance2e-6 не universal x87. Full187-step build,
**CTest21/21**. Первый static script неверно ожидал serializer.cpp ASCII path;
после фактического поиска исправлен на class-name-present/path-absent и прошёл.
Wire-карточка больше не оставляет positive/negative только гипотезой и не
приравнивает полный wire-разбор соседей к полному runtime-разбору.

DB native-only trial `(2,true)`/repeat `(0,false)`/FK `[]`; canonical
**30 imports /1662 evidence /120 snapshots**. BSP25→70/Visibility66→67:
0,460 all/engine units,0,450 direct, fixed denominators без изменения.

| Scope | Checkpoint 19 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,44%** |121,035/784|
| Engine | **29,69%** |110,760/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **58,86%** |21,780/37|

До10:00 МСК — оставшиеся bounded spatial consumers и итоговая сверка
документации/базы/границ доказанного. No apps/assets/game/OS/GPU/PS2/publication.

## Checkpoint 20 — nonempty static/occlusion spatial consumers

Snapshot **2026-09-06 06:34:29 UTC /09:34:29 МСК**,
[manifest](../../research/native-research-2026-09-06-pc-spatial-consumers.json),
[карточка](../../docs/research/native-pc-spatial-consumers.md).

Уточнено incomplete C19 описание: BSP Static480AD0 при Zone/touching остаётся
в текущем узле; только no-Zone использует mask48. Octree449A90 всегда mask48,
включая original F0 shortcut. Actual refs per membership/duplicates, Scene88
publication и full recursive destruction подтверждены. Occlusion480630/
449D30 — borrowed reciprocal lists, static400 без RenderNode billboard
exception, notifying drain; явный worldSphere не выдаётся за full Init.

Whole original Scene по **Debug21 unclipped branch** рисует duplicated Static
один раз/frame: BSP2 refs вroot, Octree4 refs вtwo leaves; два frames и actual
cleanup. Static-only setup сначала не создал DebugManager; fresh probe вызвал
original factory41E380/published global, без маппингаNULL или ослабления guard.
Normal45E870/full Visibility ctor/Occlusion Init gaps остаются открытыми.

Native **94/94** (21+20+6 для каждого дерева), static **15/15**. Portable source
не изменялся; prior full **CTest21/21** остаётся проверкой тех же исходников.
Не добавлен invented original class или новый denominator.

DB trial `(4,true)`/repeat `(0,false)`/native-only FK `[]`; canonical
**31 imports /1667 evidence /124 snapshots**. BSP70→72/Octree65→67/
Occlusion58→59/Static70→71, delta0,060 all/engine/direct units.

| Scope | Checkpoint 20 | Fixed units |
|---|---:|---:|
| Всё EXE | **15,45%** |121,095/784|
| Engine | **29,71%** |110,820/373|
| Game | **2,50%** |10,275/411|
| Direct SMO/SAN | **59,03%** |21,840/37|

До10:00 МСК — итоговая сверка source/tests/immutable manifests/canonical DB
и журнала; no apps/assets/game/OS/GPU/PS2/commit/push/release.

## Итоговая сверка перед deadline

Срез **2026-09-06 06:49 UTC /09:49 МСК**. Новые классы на этом этапе не
начинаются: сверяются результаты связного animation→world→render→spatial цикла.

Добавлен read-only [audit script](../../research/audit_native_cycle_20260906.py).
Проверены SHA всех31 immutable imported manifests, наличие всех20 checkpoints
цикла, согласованность latest rows **48 затронутых классов**,86 evidence files,
native-only FK в in-memory копии и локальные ссылки research/index/journal.
Каталог784/engine373/game411/PC733/PS2681 и direct37 сохранён.
48 записей — **не 48 полностью восстановленных классов**.

Независимый score ledger (без вызова функции расчёта из native_knowledge)
повторно вывел delta **14,080 all/engine units**, **4,990 direct units**,
game0. Все **11 metadata backfills** дали нулевой дополнительный credit.
За цикл добавлены20 imports,156 evidence records и80 snapshots; итоговые
31/1667/124 согласованы с canonical DB, EXE ради процентов не сканировался.

Повторный полный **CTest21/21** на последнем собранном source прошёл.
Выборочный original/source regression chain — **15 931 сравнение полей**:

| Проверка | Повторный результат |
|---|---:|
| Actor tick,93 cases |1857/1857|
| Owned SAN→actor→input→node, normal sequence |915/915|
| bflower SAN reader/PRS |780/780|
| Task timers,328 cases |1968/1968|
| Model→RenderNode world,32 sequences |2272/2272|
| Renderable callbacks/alpha keys |768/768|
| Visibility sphere/selection |1483/1483|
| Polygon clipping,256 cases |3842/3842|
| BSP queries,256 cases |2046/2046|

Это повторная выборка, не сумма уникальных тестов всего цикла; floating-point
tolerances остаются такими, как описаны в соответствующих scripts/cards.
Также повторно прошли original native manager326, app/timer/SAN/world39,
graphics-frame209, whole SceneRender67, Portal63 и spatial consumers94:
**798 проверок**, с прежними явно описанными внешними boundaries.
Safety harness8/8 и DB accounting6/6 tests зелёные. Прежние100k/2sec/30sec caps
не повышались, unsafe third actor input/128-vertex clip не запускались.

`git diff --check` чист. HEAD **7c2b615a0d14b3ec9d3be3bd70a61599568b58c1**,
SmoViewer **ad1dd997c7002d85a7486d519b59e45faa3df540**, SMOTextureTool
**48e4ff52f0e6a4331bed35009bab6f573dd20cec** — прежние. Dirty worktree не
сбрасывался; приложений, игровых файлов, release/commit/push не было.

## Сводка результата цикла

| Scope | Старт | Сейчас | Прирост, процентных пунктов |
|---|---:|---:|---:|
| Всё, fixed784 |13,65%|**15,45%**|+1,80|
| Engine, fixed373 |25,94%|**29,71%**|+3,77|
| Game, fixed411 |2,50%|**2,50%**|0|
| Direct SMO/SAN, fixed37 |45,54%|**59,03%**|+13,49|

Это class-weighted исследовательская оценка фиксированного каталога,
**не процент bytes/instructions PC EXE и не готовность tools**. Каталог
включает PC/PS2 identities, но этот цикл новых PS2 evidence не добавлял.

Основной результат: сцеплены actual app/timers/animation/owned actor→world,
scene/camera/light/model→renderer queues и scene visibility→partition/
octree/BSP/portals/occlusion consumers. Original-named partial sources и exact
ABI отделены от отдельно исполненных native paths; anonymous helpers не
получили придуманных original class names. Приступать к исправлению Viewer
по этим данным нужно отдельным scoped implementation task, не объявлять его
исправленным данным исследовательским циклом.

Оставшиеся ключевые пробелы сохранены в
[каноническом списке](../../docs/research/native-open-questions.md): protected
Visibility ctor/plane-vector45E870/Occlusion Init, полная loader transaction,
actor input capacity/upstream lifetime, nonempty Collision и GPU/shadow/
shader consumers. Полный portable Scene/render pipeline ещё не восстановлен.
Следующий этап — продолжать spatial/resource-to-render зависимости и эти
встреченные пробелы, а не выбирать несвязанные классы ради лёгкости.

При финальной cross-document сверке обновлены также `research/open-questions.md`,
`docs/engine/runtime-resource-pipeline.md` и `smo-runtime-validation-plan.md`:
BSP sides больше не неизвестны, средняя scene/render часть больше не помечена
целиком неисследованной, старый pre-release scope явно исторический. In-game
маршруты остаются отдельными невыполненными проверками; current PC-first/
direct-SMO-SAN priority и PS2 second tier не потеряны. Повторный read-only audit
охватывает33 research/index/journal документа; все ссылки существуют.
Дополнительный повтор статических anchors C15–20:24+24+32+22+17+15 = **134/134**.

Контроль **06:57:42 UTC /09:57:42 МСК**: финальная single-thread сборка
подтвердила `ninja: no work to do` — binaries соответствуют последним source.
Процессов python/idat/cmake/ninja/ctest не осталось; все запущенные test handles
завершились. Новые native исследования остановлены перед deadline, остаются
запись фактического времени завершения и передача отчёта пользователю.

Цикл закрыт по проверенным часам **2026-09-06 07:00:07 UTC /10:00:07 МСК**.
Исследования и тесты уже остановлены; итог20 checkpoints,48 class records,
31 imports/1667 evidence/124 snapshots и четыре процента выше переданы в отчёт.
Следующий native участок в этом цикле не начинался.
