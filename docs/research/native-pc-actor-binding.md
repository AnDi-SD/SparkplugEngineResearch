# PC actor: discovery, input insertion и запуск

Checkpoint ночного цикла 5/6 сентября 2026 года. Продолжение
[manager](native-class-sp-animation-manager.md) и
[playback tick](native-pc-actor-playback.md), не новая альтернативная система.
Original names классов сохранены; имена API/полей ниже **аналитические**.
PC pristine SHA-256 закреплён тем же bounded x86 harness. PS2 не использовался.

## Граница доказательства

Исполнены original node/actor/controller/evaluator factories, discovery,
slot-map lookup, SAN reader/shared-name registry, input binder, start helper и
непустой destructor chain. Heap/stream/shared-name-owner/char_traits/CRT и
event dispatch — явные ограниченные fixtures. Игра, D3D и GUI не запускались.
Guest не имеет host API forwarding, ограничения памяти/инструкций/времени
сохранены. Опасный третий input **не вставлялся**.

Portable перенесён **insert/clear evaluator** и поле приоритета animation.
Полные actor Start/tree/Rebind API на этом checkpoint ещё не перенесены:
native execution и готовый portable source — разные уровни завершённости.

## Discovery и name-slot map

Original PC node factory `0x00421E20` и constructor `0x00421BA0` дают exact
`0xB4`, default flags `0x70A00`. Теперь это прямое PC execution evidence,
а не перенос PS2 defaults.

`0x005A33F0(node)` требует nonnull shared name entry `node+10`, flag `800`
и отсутствие `2000`. Пустое, но существующее имя допустимо. Метод:

1. Создаёт `spNodeController` kind0 (original constructor `5FF400`, extent18).
2. Присоединяет node через `5A15C0` с intrusive retention, выставляет `2000`.
3. Через manager `4545F0` получает слот общего case-sensitive имени.
4. Добавляет controller в actor vector `+3C` через `5A3250`.
5. Через map `5A31D0` записывает slot → evaluator (controller virtual28).

Повтор того же node пропускается из-за `2000`. **Разные node с одним именем**
оба получают controllers, но map хранит один slot: последний evaluator
замещает значение, ранний остаётся без input. Не предполагать unique names
только потому, что у них различные node addresses.

Actor map имеет allocator/sentinel/count по `+30/+34/+38`, node размер18:
left0, parent4, right8, signed-slotC, evaluator10, color14, sentinel15.
Рекурсивный walker `5A34D0` — preorder, child list по node18, child pointer
в list entry8. Полный tree wrapper `5A35C0` пока отдельно не исполнялся.

Непустой actor teardown удаляет controllers/evaluators, снимает node `2000`,
освобождает name bindings и удерживаемые node references. Все tracked native
allocations в нормальных проверенных сценариях освобождены. Это не доказательство
allocation-failure cleanup или произвольного callback reentry.

## Input insert: `0x005FE9C0`

Аргументы: state, track, unsigned priority, exclusive; `ret 10`.
Две физические записи30 в evaluator18/48; число активных — evaluator14.
State48 — uint32 counter. **Это не гарантированно число живых входов.**

Exclusive:

- Если у активного nonnull state priority выше нового, вся операция no-op.
- Иначе slot0 получает новые state/track/priority; его key caches сохраняются.
- Новый state48 увеличивается; count становится1.
- Старый физический slot1, если state nonnull, уменьшает свой state48,
  даже если прежний count равнялся1; очищаются только два его указателя.
- **Старый slot0 не уменьшает counter.** Повторный exclusive rebind той же
  анимации на двух nodes даёт counter2 →4 →6.

Nonexclusive строит новый упорядоченный список из active-prefix:
null state пропускаются; прежние entries того же state исключаются с decrement;
новая запись вставляется перед первым большим unsigned priority, иначе в конец.
Equal-priority retained записи идут раньше новой. Перемещаемый retained input
копируется целиком вместе с cache. Новая/reinserted запись перезаписывает только
state/track/priority и **наследует cache физического места назначения**.
Неактивный хвост не очищается. Incoming counter затем увеличивается один раз.

`0x005FEB70(index)` очищает только state/track. Count, priority, caches и
state48 не меняются; локального bounds check оригинал не показывает.

Portable `spTransformTrackEval` хранит именно array2 + active count;
`InsertInputForAnalysis`/`ClearInputForAnalysis` воспроизводят наблюдаемые
операции. `PlaybackForAnalysis.bindingUseCount` — non-owning view actor counter;
его lifetime обязан превышать жизнь ссылки evaluator. Literal test setter
`SetInputsForAnalysis` не делает native insert и не меняет counters.

Host-only guards: валидный counter view, sampler, index<2, отказ до любых
изменений при новом count>2. Приоритетный native отказ возвращает host true
(операция корректна, но ничего не меняет); исходный метод void. Input передаётся
по значению, чтобы избежать alias с изменяемым физическим слотом.

## Actor binder: `0x005A1C10`

Защищённый entry приводит к tail `5A1C16`, stack extent334. Проходит controllers,
очищая pointer pairs inputs, чьи state48==0. Запоминает старый animation pointer
для использовавшихся states. В порядке state index обрабатывает state, если
counter>0 **или** index равен selected аргументу binder.

Animation tracks1C/count20/stride44 сопоставляются с actor map через track14 и
`5D0440`. Insert получает state50 priority; exclusive iff state10==0.
После вставок сравниваются old/new animation pointers для ненулевых counters:
потеря даёт event5, получение event4. В обоих last payload — **state pointer**,
не animation pointer. Сам binder очередь не flush-ит. У start/tick события с
тем же formal envelope могут иметь другой payload; не унифицировать их догадкой.

Selected state без совпавших nodes может остаться counter0. Exclusive replacement
иногда вызывает event5 у потерявшего slot1 state, а repeated nonexclusive rebind
сохраняет counters. Upstream invariant, предотвращающий third input, открыт.

## Start: `0x005A1E30..0x005A208A`

Один request pointer, `ret4`, возвращает индекс state либо `FFFFFFFF`.
Выбирается первый free state (counter0); active same-animation match заменяет
free candidate. Если совпадений несколько, последний active match выигрывает.
Ветка отсутствия обоих возвращает sentinel до изменения request (пока static
evidence; bounded execution покрывает нормальный start/restart).

Request extent38, **не** playback60:

| Offset | Analytical role |
|---:|---|
| 00/04/08 | animation / mode / reverse byte |
| 0C/10 | mutable weight / fade mode |
| 14/18 | fade-in duration / fallback rate |
| 1C/20 | fade-out duration / fallback rate |
| 24 | transition duration |
| 28/2C | callback / cookie |
| 30/34 | speed / initial time |

Fade0/3 перезаписывает **request** weight=1; fade2/4 —0; fade1/прочие сохраняют.
При restart уже активной анимации прежний **state** weight остаётся, хотя request
мог измениться. Duration>0 задаёт inverse rate; иначе берётся fallback.
Threshold = animation total14 − requested fade-out duration.

State running4C=1, elapsed5C=0, normalized54=initialTime/totalTime,
status40=0 только для fade2, иначе1. Копируются mode/reverse/fade/rates,
callback/cookie, transition duration и speed. Старые sample34,
stopAfterFade3C и binding counter48 не обнуляются.

Роль **animation18** уточнена: это источник группы приоритета.
`state.priority = (animation18 << 24) | (manager.frame & 0x00FFFFFF)`.
UInt32 truncation оставляет младшие8 bits группы; например group3 и frame12345678
дают03345678. Upstream setter и original имя поля ещё неизвестны. В portable
animation добавлен uint32 analytical accessor; default0/name-only clone сохраняются.

Start ставит event2 (payload animation), fade2 также event6 (payload animation),
затем вызывает binder(selected) и flush engine110 через `413B40`.
Fresh matched start: `[2,4]` либо `[2,6,4]`, один flush; restart `[2]` без
повторного gained event. Bind event4 при этом передаёт payload **state**.

Для третьего blended candidate оригинальный Start исполнен только **до CALL
binder по `5A205B`**: он выбирает state2 и подготавливает поля без local2 guard.
Snapshot обоих inputs неизменен. Потенциально небезопасный CALL не исполнен.
Это отсутствие локальной защиты, а не доказательство, что игра реально
допускает такой запрос: нужно исследовать upstream callers.

## Проверки на checkpoint

- `probe_pc_actor_binding.py`: **44/44** original discovery/binder/lifetime.
- `probe_pc_transform_inputs.py`: **14/14** original insert/clear.
- `probe_pc_actor_start.py`: **56/56** original start/restart/priority/safe boundary.
- `SparkplugTransformInputTests`: **15/15** host checks, включая atomic safety guard.
- `compare_pc_transform_inputs.py`: **4368/4368**, **156** deterministic cases,
  проверены оба physical slots/caches/counters, equal/high unsigned priorities,
  null holes и duplicate state. Unsafe third case исключён до guest execution.
- CTest: **10/10** после добавления input suite; отдельные последующие изменения
  проверяются в журнале цикла.

Следующий шаг: portable actor owned binding/start с ясной resource lifetime,
проверка полного wrapper/stop, затем outer engine frame. Read-only call scan уже
связал manager frame с `41CDD1` внутри `41CD50`, owner+3C; full engine helper и
его caller ещё не исполнены. Не считать эту статическую связь готовым renderer.

## Продолжение: descendant wrapper, Stop и owned SAN bindings

`probe_pc_actor_tree_stop.py` — **19/19**. Original wrapper `5A35C0`
пропускает сам переданный root, обходит только его descendants в preorder.
Неименованный/nonanimated промежуточный node не блокирует обход его детей.
Повторный wrapper не дублирует nodes с `2000`. Six-node fixture использует
original objects, но child edges — явно synthetic списки, восстановленные в
empty перед teardown: native Attach/scene notifications здесь не исполнялись.
Проверены четыре найденных descendants и их привязка к настоящим SAN tracks.

`5A20A0(animation,suppressEvent)` ищет **первый совпавший animation pointer**,
не обязательно active. Missing pointer — no-op. Проходит actor slot map и
очищает оба physical inputs, указывающих на найденный state; затем running=0,
counter=0, binder(-1), status=3, flush engine110. Если suppress=false, **после
flush** идёт immediate event3 через `40F9A0`, args(state,animation). Это не queued
`40FA10`; dispatch handlers пока заменены записью. Повторный Stop той же inactive
animation всё ещё flush-ит и отправляет immediate event. Остальные blended
states rebind-ятся, их counter сохраняется, retained input переходит в slot0.
`5A21C0` (analytical StopAll) вызывает nonsuppressed Stop только для states с
counter>0; повтор на полностью stopped actor — no-op.

Portable `spAnimationManager::NameBindingLeaseForAnalysis` теперь владеет ровно
одним registry reference. Это **host RAII helper**, не найденный original class.
Move передаёт reference, destruction/reset unbind-ит приобретённое имя.
Weak manager token защищает от reversed lifetime и замены singleton; обычный
native порядок требует живого manager до release. Explicit manual Unbind
не должен снимать чужие owned references. Потокобезопасность не заявляется.

`spAnimTrack::BindNameForAnalysis` приобретает новую lease до освобождения
старой; `IsBoundToForAnalysis` проверяет identity manager и неизменённое имя.
Изменение имени не выполняет скрытый rebind. ReleaseKeys не освобождает slot;
удаление/shrink track освобождает lease, manual SetBindingSlot её заменяет,
name-only clone не приобретает binding. Хранение первоначального acquired name
при rename — дополнительная host safety policy, не доказанная native rename API.

`spAnimationSerializer::ReadFieldsWithBindingsForAnalysis` вызывает **тот же
проверенный field reader**, затем приобретает owned track bindings. Error
освобождает частичные leases и не выдаёт partial animation/observations.
Позиция stream и уже выданные monotonic IDs не откатываются. Старый non-owning
resolver API сохранён для явных lookup consumers, его контракт не изменился.

Проверки продолжения: manager C++ **68/68**, reader **117/117**, CTest **10/10**.
`compare_pc_san_registry.py`: **81/81** (`bbush.san`) + **261/261** (`bflower.san`)
сравнений six-stage snapshot original/portable: два simultaneous resources,
individual/last release, reload, final release; slots/name references/nextID.
Старый reader/PRS comparison повторён на всех четырёх SAN: **2832/2832**.
Полный portable actor Start/tree/Rebind — следующий незавершённый шаг.

Этот следующий шаг выполнен отдельным
[owned-runtime checkpoint](native-pc-actor-owned-runtime.md): исходники actor API,
5154 differential comparisons и найденная зависимость node от matrix startup.
Выше сохранены границы и результаты именно раннего input/start checkpoint.
