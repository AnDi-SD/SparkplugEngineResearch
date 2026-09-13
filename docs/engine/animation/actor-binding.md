# PC actor: discovery, input insertion и запуск

## Discovery и name-slot map

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

Request extent38, **не** playback60:

| Offset | Analytical role |
| ---: | --- |
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

## Продолжение: descendant wrapper, Stop и owned SAN bindings

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
