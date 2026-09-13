# spTaskTimer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spBaseObject](../../../Sparkplug/Code/SparkBase/spBaseObject.h), [spTaskTimer](../../../Sparkplug/Code/Sparkplug/spTaskTimer.h).

## Identity и lifecycle

| Факт | PC |
| --- | ---: |
| Class ID / base | `1ACD36E2 /415352A1` |

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
| ---: | --- |
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

Host отклоняет missing clock там, где он действительно нужен, divisor0,
null/repeated/cyclic child graph, более4096 visits/pending entries и depth>128.
Нет обещания global rollback при ошибке provider позднего ребёнка, callback
reentry или произвольном внешнем уничтожении borrowed pointers.
