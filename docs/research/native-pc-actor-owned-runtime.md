# PC actor: перенос owned runtime и сквозная проверка

Ночной цикл 5/6 сентября 2026 года, checkpoint после
[input/start](native-pc-actor-binding.md) и owned SAN leases. Original названия
классов сохранены. Файлы `spActor.*` — inferred исходные пути; `ForAnalysis`
методы и host ownership helpers не выдаются за найденные symbols.

## Восстановленный участок

В `spActor` теперь реализованы `DiscoverNodeForAnalysis`,
`BindDescendantsForAnalysis`, `StartForAnalysis`, `RebindForAnalysis`,
`StopForAnalysis`, `StopAllForAnalysis` и owned-ветка прежнего Tick.
Используются существующие `spAnimationManager`, `spAnimTrack`,
`spNodeController`, `spTransformTrackEval` и `spNode`; второго evaluator/reader нет.

Actor владеет node controllers; каждый удерживает node и original track evaluator.
Per-controller name lease соответствует одной native registry reference.
Slot-map сохраняет **последний** controller для одинакового имени, вектор хранит
всех. Wrapper обходит descendants в preorder, supplied root не включает.
В destructor по порядку вектора снимается bit2000, освобождается имя, удаляется
controller. Animations остаются **borrowed**, как в native: вызывающий обязан
сохранять ресурс живым, пока actor state на него ссылается. Stop не обнуляет
animation pointer; жизненный цикл ресурса нельзя угадывать по одному running.

Start сохраняет исходные priority/weight/event semantics, включая active restart
без сброса state weight/sample/stopAfterFade. Request fade durations нужны для
расчёта rates и threshold, но **не копируются** в state14/1C: эти слова остаются
неизвестными. Ранее восстановленные state поля теперь явны в PC ABI struct:
fade10, rates18/20, cookie2C, stopAfterFade3C, status40, slotIndex44; unknown14/1C/38
не получили вымышленных ролей.

Binder сохраняет native порядок: old-animation pointer запоминается **перед
обработкой каждого state**, а не единожды для всего массива. Вставка раннего
state может изменить counter позднего ещё до этого чтения. Pointer-only clear,
cache destination inheritance и exclusive counter asymmetry сохранены.

Owned Tick получает актуальный первый physical input evaluator, применяет
controller direct/transition blend, действительно выполняет rebind и Stop
после fade. Прежний явно настроенный manual controller seam оставлен для
изолированных потребителей/сравнений; смешивать его с owned discovery нельзя.
Queued event4/5 имеют payload state, остальные проверенные animation events —
animation, event11 — tag; nonsuppressed Stop выдаёт **immediate3 после flush**.
Actions записываются, native dispatch callbacks не исполняются и не реконструированы.

## Host-only ограничения

- До Start/Rebind/Stop создаётся рабочая копия states и двух physical inputs.
  Тот же reconstructed insert проверяется на копии с переназначенными counter views.
  Capacity error не публикует states/inputs/actions и не меняет request weight.
  Это защитная транзакция, которой original EXE не демонстрирует.
- Copy для preflight — техническая копия host storage, **не native Clone**:
  original actor/evaluator clones по-прежнему имеют fresh runtime payload.
- Bound resources должны иметь leases того же живого manager; переименованный
  node/track не перепривязывается скрыто. Bound node lifetime — shared_ptr вместо
  native intrusive reference. Сам actor должен жить до завершения callback/frame.
- Tree/controllers/prepared samplers ограничены4096, playback по-прежнему40;
  finite/mode/capacity guards и null/foreign-state rejection — host policy.
  Source buffers/borrowed animation pointers не становятся безопасными от
  произвольного внешнего уничтожения только из-за этих проверок.
- Partial tree discovery и последовательный StopAll не являются общим atomic
  rollback. Callback reentry, allocation failures и неизвестные public controls
  не объявляются полностью обработанными.

Нормальная exclusive замена двух входов остаётся разрешена. Защита ограничивает
**перекрывающиеся inputs одного evaluator**, не два actor states вообще.
Original unsafe third nonexclusive input не запускался ни в одном тесте.

## Найденная startup-зависимость node

Первый сквозной comparison разошёлся на initial node orientation. Memory trace
показал: PC constructor `421BA0 -> 4D3740` сначала строит identity, затем
**перезаписывает обе matrices копией global `7600BC..7600DC`**. В PE-only guest
static initialization не выполнялась, поэтому global был нулевым.

Найдено original initializer `6D38E0..6D38EF`: вызов `462250(destination=7600BC,1)`.
Его pointer расположен в initialization table по `73F800`, рядом с zero/identity
3×3/4×4 helpers `6D38B0/C0/D0`. Теперь actor fixture исполняет **этот original
initializer**, не подставляет вручную ожидаемую matrix и не запускает whole CRT.
Имена исходных math типов и полный startup-order contract пока неизвестны.

`probe_pc_node_lifecycle.py`: **13/13**. Проверены raw uninitialized global,
actual initializer, `B4`/vtable, local/world PRS defaults, flags70A00, null parent/
scene link, empty child/collision containers и normal release. Диагностический
global с девятью разными finite values копируется в обе matrices, затем original
initializer восстанавливает identity. Эти диагностические matrices не рендерились.
Portable identity defaults были правильными: расхождение исправлено в test setup.

## Проверки

`SparkplugActorBindingTests`: **81/81**. В том числе настоящий portable
manager→actor→controller→evaluator→node и parent/child world-update; name duplicates,
root exclusion, lease destruction, restart, Stop/StopAll, third-input atomic guard,
разрешённая exclusive замена, unmatched selected state и blank actor clone.
CTest после изменений: **11/11**.

`compare_pc_actor_binding.py` использует настоящий `bbush.san` и два named node:

| Сценарий | Leaf comparisons |
|---|---:|
| normal | 915 |
| blend/restart/StopAll | 1299 |
| oneshot | 731 |
| transition | 726 |
| fade_stop | 558 |
| suppressed Stop | 925 |
| **Всего** | **5154** |

Сравниваются three playback states, priority/counters, оба physical input и все
key caches, local/world position/scale/orientation, node flags и точный порядок
queued/immediate/flush actions. Sampling выполняют реальные prepared SAN keys.
Native manager действительно dispatch-ит actor. В portable observation fixture
manager counter продвигается при временно выключенном actor, затем тот же Tick
вызывается явно для получения actions: direct manager dispatch отдельно проверен
C++ suite. Force world-update(1) вызывается **явно при snapshot**, не объявляется
найденным external engine-frame hook.

Все original object/registry/actor/evaluator/node code paths ограничены guest
memory/instruction/time bounds, CRT floor/fmod, stream/name-owner/allocator и
event dispatch — явные seams. All tracked native allocations released.
Игра/renderer/GUI/PS2 не запускались, assets/apps не менялись.

После изменения fixture повторены original discovery44/start56/tree-Stop19,
а прежние actor1857 и input4368 differential suites остаются отдельными
регрессионными наборами. Это числа проверок, не проценты исследованных байтов.

Следующий связный фронт: remaining actor public controls и upstream overlapping
input invariant, внешний `spEngineCore` frame helper41CD50/manager owner3C,
затем полная resource/render связь. Event dispatcher/reentry и full FFPS/FAT
transaction/writer остаются неизвестными, importer/exporter не объявляются готовыми.
