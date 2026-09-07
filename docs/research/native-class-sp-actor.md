# `spActor`: PC scheduler — частичный разбор

Статус: `substantial` playback slice, PC-only 2026-09-05. Добавлен portable
`spActor` с original modes/tags/fade/controller order и явными registry/event seams.
Полный публичный API и event reentrancy не восстановлены. Актуальная
[карточка playback](native-pc-actor-playback.md) уточняет прежний scout ниже.

2026-09-07 [CP66](native-pc-skin-san-render.md): настоящий SAN reader,
manager/actor tick и прочитанная Skin bone связаны до palette/constants/draw
на сохранённом графе. Четыре побитовых capture; full frame ещё открыт.

[CP67](native-pc-actor-controls.md) восстановил configurable capacity,
first-match/query-by-counter и deferred fade-stop. Game init задаёт2, но
другой caller расширяет actor до19; общий two-state invariant не утверждается.

Class ID `0x19D676E6`; registration `0x00766480`; factory `0x005A3620`
выделяет `0x54`, constructor `0x005A3500`. Vtable `0x00703F80` содержит девять
slots; `+0x1C -> 0x005A2380` — playback tick, `+0x20 -> 0x005A35C0` — tree bind.
Registration getter `0x005A33E0`; clone `0x005A3680` использует защищённый
controller copy `0x00423100 -> 0x00419AA0`: в
[следующем checkpoint](native-class-sp-animation-manager.md) исполнено и
перенесено copied enabled byte при fresh actor-specific runtime state.

## Наблюдаемые поля

Actor `+0x1C` разрешает применение transforms; `+0x24` разрешает продвижение
времени при выключенном `+0x1C`; `+0x20` — общий time multiplier. Массив
playback entries начинается по `+0x28`, capacity по `+0x2C`; stride `0x60`.
Controllers vector имеет begin/end/capacity по `+0x40/+0x44/+0x48`.
Allocation `0x54` не означает, что названы все поля; хвост `+0x4C..+0x53` открыт.

| Playback offset | Роль, подтверждённая accesses |
|---:|---|
| `+0x00` | animation pointer |
| `+0x04` | mode `0..3`, original enum names неизвестны |
| `+0x08` | reverse flag |
| `+0x0C` | blend weight |
| `+0x24` | transition duration |
| `+0x28` | callback relationship |
| `+0x30` | per-entry time multiplier |
| `+0x34` | sample time |
| `+0x48` | evaluator binding-use counter; полный invariant открыт |
| `+0x4C` | running flag |
| `+0x50` | unsigned priority |
| `+0x54` | normalized progress |
| `+0x58` | fade threshold |
| `+0x5C` | elapsed time |

Имена полей аналитические, не original symbols. Exact observed structs и
size/offset assertions хранятся в `Analysis/PC/SparkplugAbi.h`.

## Связь с анимацией и узлом

Node discovery `0x005A33F0` принимает именованный node с flag `0x800` и без
`0x2000`, создаёт controller kind `0`, присоединяет node, выставляет `0x2000`,
вызывает name binding `0x004545F0` и индексирует controller/evaluator по slot.
Рекурсивный walker `0x005A34D0` вызывается из tree binder `0x005A35C0`.

В защищённом bind-entry `0x005A1C10` доступен настоящий tail с
`0x005A1C16`: tracks animation (stride `0x44`) соединяются с playback entries
и передаются в `spTransformTrackEval::0x005FE9C0` по `0x005A1D26`.

Tick `0x005A2380..0x005A2DF1` сначала обновляет playback time/weight, затем
проходит controllers. Через controller `+0x28` получает evaluator, затем его
первый playback pointer по `+0x18`. Если duration `+0x24 > 0`, вызывает
controller `+0x20` с `(sampleTime, elapsed/duration)` по `0x005A2D4D`;
иначе controller `+0x1C` с sampleTime по `0x005A2D5F`.

Таким образом доказан непосредственный scheduler
[`spTransformTrackEval`](native-class-sp-transform-track-eval.md) и caller
[`spNodeController`](native-class-sp-node-controller.md). Внешний frame caller
самого actor tick установлен как `spAnimationManager::0x004535A0` и исполнен
следующим checkpoint. Mode/reverse/fade уже проверены playback checkpoint;
event dispatcher/reentrancy и внешний engine caller остаются неизвестными.

Хеш tick, vtable и call sites закреплены read-only inspector. На этапе scout
tick ещё не исполнялся; последующий checkpoint добавил 93 differential scenarios
оригинального tick и portable source, а также original empty constructor/destructor.
Manager caller `0x004535A0`, controller copy и actor clone теперь исполняются
в `research/probe_pc_animation_manager.py`; класс всё ещё не объявляется полным.

Следующий [input/start checkpoint](native-pc-actor-binding.md) исполнил discovery,
native slot-map/binder, непустое освобождение, Start/restart и priority formation.
44 binding +56 start assertions; portable input insert/clear сверены на156 cases.
Actor Start/tree/Rebind в portable классе на этом checkpoint ещё не перенесены.
Counter48 не является точным live-input refcount: repeated exclusive insertion
увеличивает его без decrement old slot0. Start не имеет local2 guard перед
binder; третий input намеренно не запускался. Event reentry/upstream capacity
и внешний engine frame остаются открытыми.

Последующий [owned-runtime checkpoint](native-pc-actor-owned-runtime.md) перенёс
Discover/descendant wrapper/Start/Rebind/Stop/StopAll и соединил их с Tick.
C++81/81 и5154 comparisons шести real-SAN sequences прошли. Native node startup
dependency выполнена оригинальным initializer, unknown state14/1C/38 сохранены.
Remaining public controls, upstream capacity, event dispatch/reentry и outer
engine/render frame остаются открытыми; это не завершённый весь `spActor`.
