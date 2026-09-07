# PC `spActor`: playback, события и граница менеджера

Финальный checkpoint цикла 2026-09-05, завершён по просьбе пользователя после
перебоя связи. Новые участки после просьбы остановиться не начинались; доведены
текущие проверки и запись результатов. PS2 и игровые приложения не менялись.

## Что восстановлено

Оригинальный `0x005A2380` пройден в bounded guest с явно заданными playback,
tag и controller fixtures. Portable `Code/Sparkplug/spActor.h/.cpp` повторяет
его scheduler slice; `spController` добавлен как abstract dependency с original
RTTI, enabled state и явно отсутствующей automatic global registry.
Пути этих TU inferred: исходные имена классов доказаны, строки путей не найдены.

Tick использует `actor +0x1C` (apply) и `+0x24` (advance while disabled).
Оба false — выход даже без event flush. Иначе frame delta умножается на actor
`+0x20`, затем на playback `+0x30`; обрабатываются только running entries с
положительным binding-use count. Duration берётся из `spAnimation +0x14`.

| Mode | Проверенное поведение, не original enum name |
|---:|---|
| 0 | одноразовое; equality в конце ещё активно, остановка при progress `<0` или `>1` |
| 1 | `fmod(progress,1)`; отрицательное значение не нормализуется в положительный цикл |
| 2 | ping-pong через `fmod(progress,2)`; направление меняется уже при remainder `>=1` |
| 3 | сохранённая progress-позиция, затем stop на текущем tick; это не постоянная пауза |

Reverse применяется к sample time до endpoint clamp. Clamp вне диапазона
для mode 0/3 не учитывает reverse; при progress ровно 0/1 clamp не производится.
Elapsed растёт даже в mode 3. Transition duration очищается при `elapsed > duration`,
не при equality. Controller blend factor — `elapsed / transitionDuration`.

## Порядок событий и fade

Теги проходят в порядке движения: обычный сегмент исключает старую и включает
новую границу. При пересечении цикла идут остаточные tags → callback → event 10
→ tags нового сегмента. Event 11 несёт tag, event 3 — завершение. Ping-pong
может доставлять граничный tag дважды при развороте; это сохранено, не исправлено
как предполагаемый баг. Проверены reverse и несколько пересечений за tick.

Playback `+0x10` — fade mode: 2 увеличивает weight до 1; 4 увеличивает его до 1
или пересечения sample threshold, затем меняется на 3; 3 уменьшает weight после
threshold. Rates находятся по `+0x18/+0x20`, threshold по `+0x58`, status по `+0x40`.
Events 7/8/9 соответствуют наблюдаемым переходам fade. Нулевой weight сам по себе
**не завершает fade**: оригинал проверяет `<0`, не `<=0`.
При stop-after-fade (`+0x3C`) original Stop выполняет rebind и flush немедленно.

После states идут controller lookup и direct/blended application; окончательное
обновление связей завершившихся одноразовых states происходит после применения
контроллеров, затем flush общей очереди. Настоящие event callbacks/queue processing
в probe заменены recorder-ом; reentrant mutation и полный event dispatcher не
объявляются проверенными. Controller callbacks в portable integration test
используют настоящие восстановленные `spNodeController`/evaluator/keys.

## Constructor и registry

Оригинальный factory `0x005A3620` выделяет exact `0x54`; constructor `0x005A3500`
вызывает protected `spController::0x00423010 -> 0x004C4210`.
Controller имеет physical `spSubController` base, enabled byte `+0x10`, next/prev
`+0x14/+0x18`. Он регистрируется через `0x00453450` в manager global `0x0075F880`:
head/tail находятся по `+0x24/+0x28`. Destructor снимает регистрацию через `0x00453480`.

Это original **`spAnimationManager`**, registration `0x006D35B0..0x006D35D5`,
ID `0x5D214CC1`, factory `0x00454640`. Его frame helper **`0x004535A0`** статически
установлен: увеличивает counter `+0x10`, берёт delta из engine `+0xB8`, обходит
head→next и вызывает controller vslot `+0x1C` при enabled `+0x10`.
На первоначальном actor checkpoint frame helper ещё не исполнялся. Следующий
[manager checkpoint](native-class-sp-animation-manager.md) проверил original
manager→actor tick, registry, clone и list lifetime. Внешний engine call site
и полный callback mutation contract остаются открытыми.

Actor по умолчанию создаёт 40 slots (`0x00741654`). В `0x005A1600` промежуточные
constructor defaults затем стираются `memset`; остаются нули и index в `+0x44`.
Это проверено на всех 40 записях. Tail actor `+0x4C/+0x50` остаётся unwritten.
Пустой actor нормально освобождает три allocations и снимает controller registration.
Registry root в этом lifetime probe synthetic, сами list operations — оригинальные.

## Границы и проверки

Portable класс не реализует полный start/stop/configuration API, tree/name binder,
native input priority insertion и queue. Clone добавлен следующим manager
checkpoint: copied controller enabled, все actor-specific fields fresh.
Animation pointers borrowed. Host guards ограничивают non-finite values,
нулевую duration, пересечения и число событий. Они не объявляются native guards.
Неизвестные actor tail, allocator failures и callback-driven lifetime сохранены.

```powershell
python research\probe_pc_actor_lifecycle.py
python research\compare_pc_actor_tick.py --portable .codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugActorTests.exe
```

93 differential scenarios покрывают modes/reverse, endpoints, multi-cycle tags,
fade, transition и gating, включая точный порядок boundary actions. Отдельный
C++ suite содержит 66 assertions и связную проверку actor → prepared track →
evaluator → node controller → world position. Процессные лимиты guest 30 s,
portable child 10 s, каждый native call 100 000 instructions / 2 s.
Ни EXE как приложение, ни D3D, ни игра не запускались.

Итог: **1857/1857 comparisons**, native empty lifecycle **61/61**, C++ **66/66**,
свежая сборка и **CTest 8/8**. Числа относятся к assertions/comparisons, не к
количеству восстановленных функций или полной готовности класса.
