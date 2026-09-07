# `spTaskTimer`: PC время задач и анимации

Checkpoint 5 ночного цикла 5/6 сентября 2026 года. Original class name/ID,
factory, one-argument constructor, destructor, clone, четыре local virtual
операции и математический контракт исполнены в bounded x86 guest и перенесены.
Original header/TU/API spellings отсутствуют; `Code/Sparkplug/spTaskTimer.*`
помечены inferred. Это **не подкласс `spTimer`**: direct RTTI base — `spBaseObject`.

Контрольный PC EXE: `local-data/pc-pristine/WinxClub.exe`, SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Identity и lifecycle

| Факт | PC |
|---|---:|
| Class ID / base | `1ACD36E2 /415352A1` |
| Registration / initializer | `75F680 /6D34C0` |
| Getter | `4506C0` |
| Factory / executed body | `450880 ->13CE940..13CE9F2` |
| Constructor / executed body | `4506F0 ->484960..4849BE`, `ret4` |
| Destructor / deleting wrapper | `4506B0 ->406F09 ->4102B0`, `450730` |
| Clone | `450910`, base-copy `40ECE0` |
| Exact allocation / vtable | `3C /6E6968` |

Vtable: `450730 5B7A00 450910 40ECE0 4506C0 408350 408370`, затем
`450750 450840 4506D0 4506E0`. Следующий word `6E6994=3A83126F` —
float32 `0.001`, **не ещё один slot**.

Factory inlines default construction с null source. Constructor принимает один
borrowed source-clock pointer, записывает `+2C`, но **не добавляет себя в child
list источника**. Все слова собственного payload обнуляются, кроме default
relative byte19=1; padding1A/1B остаётся нетронутым. Destructor не удаляет и не
отвязывает source/children, а переходит в обычный `spBaseObject` cleanup.
Clone создаёт fresh timer, регистрирует пару, выполняет base no-op copy;
время, флаги, source и дети не копируются.

## Exact PC layout

| Offset | Роль |
|---:|---|
| `00..0F` | `spBaseObject` |
| `10` | предыдущий sibling; append41C300, default0 |
| `14` | следующий child-list элемент; читается после virtual update |
| `18 /19` | active /relative-clock, defaults0/1 |
| `1A..1B` | untouched padding |
| `1C` | current unsigned milliseconds |
| `20` | source timestamp при Start |
| `24` | накопленное время при Pause |
| `28` | float delta seconds |
| `2C` | borrowed source-clock pointer |
| `30` | начало borrowed child list |
| `34 /38` | конец списка /число детей; append41C300, defaults0 |

В `spEngineCore` находятся два exact members по `54` и `90`: destructor
`41C554..41C566` уничтожает **два `3C` объекта через4506B0**. Поэтому известное
время actor по engine`B8` — это `90+28`, delta второго `spTaskTimer`, а не
случайное поле в неопознанном container. Original core constructor пока не закрыт,
но **setup41C300** отдельно исполнен:54 active/relative1,90.source=54,
90 добавляется в конец54.child list. Пустой/непустой append проверены original
кодом; никакого вызова Start или обнуления timestamps setup не делает.

## Четыре операции

Аналитические имена: Update450750, Start450840, Pause4506D0, Reset4506E0.

Start при необходимости вызывает `6BE2F0`, затем `6BE2D0` на global75F65C,
читает unsigned `75F670 /74E050`, ставит active1 и сохраняет результат в20.
Он **не** сбрасывает current/paused/delta и не обновляет детей.

Update:

- paused: пишет только delta0;
- active +source: копирует source1C и source28; не обновляет source и не читает
  hardware clock;
- active без source: получает unsigned `now =75F670 /74E050` после optional
  refresh. При relative1 `current =now-start+paused` modulo2³²; иначе `current=now`.
  `difference =current-oldCurrent` также unsigned modulo2³². Delta получается
  умножением точного unsigned difference на **float32** constant3A83126F с
  последующим float32 store; промежуточный float32 cast difference был бы неверен.
- после собственного шага проходит child list рекурсивно в preorder; перед
  virtual child slot1C копирует active/relative в ребёнка. Paused root тоже
  обновляет детей. Их source clocks независимы от принадлежности списку.

Pause копирует current1C в paused24 и очищает active. Reset обнуляет active и
три integer timestamps1C/20/24. Оба **сохраняют старую delta до следующего Update**.
Global refresh branch сам по себе ещё не является реконструкцией `spTimer`/
`spMasterTimer` или OS clock API.

## Перенос и safety

`spTaskTimer.*` использует существующий root RTTI/clone core. Clock provider —
явная граница rawTicks/divisor после optional refresh. Child list — explicit
borrowed fixture API; source/children должны пережить использование. Добавленный
`AppendClockChildForAnalysis` воспроизводит source replacement/append из
core41C300 с host graph guards. Полного native detach/reparent API этим не
объявляется. Destructor не выдумывает ownership.

Host отклоняет missing clock там, где он действительно нужен, divisor0,
null/repeated/cyclic child graph, более4096 visits/pending entries и depth>128.
Нет обещания global rollback при ошибке provider позднего ребёнка, callback
reentry или произвольном внешнем уничтожении borrowed pointers.

## Проверки

- `probe_pc_task_timer.py`: **34/34**, original factory/ctor/clone/destructor,
  pause/resume/reset, relative/absolute clocks, divisor, wrap, refresh order,
  linked source и literal four-node child graph;
- `SparkplugTaskTimerTests`: **34/34**, включая host guards/blank clone и
  append/source replacement, добавленные в checkpoint6;
- `compare_pc_task_timer.py`: **1968/1968** leaf comparisons, **328 cases**,
  delta сравнивается **побитно**, а не с допуском;
- `inspect_pc_engine_frame.py`: **23/23** static hash/call/registration anchors;
- CTest **12/12** после header-aware rebuild.

Первый ctor scout ошибочно не передал optional pointer и обнаружил `ret4`
через stack check; после чтения actual484960 body добавлен args=(source,).
Это исправление сигнатуры fixture, не таймаут или зависание игры.

Открыты native child detach/reparent и полная lifetime policy в engine,
original source/API names, hardware timer initialization и game override
`wxGameTimer`. PS2 не исследовалась в этом checkpoint.

Связь с приложением и графическим этапом:
[PC engine frame](native-pc-engine-frame.md).

Продолжение: [native scene/world checkpoint6](native-pc-scene-world.md),
original core setup32 checks и app→timer→actor→world39 checks без literal timer
links и без отдельного test-side UpdateWorld.
