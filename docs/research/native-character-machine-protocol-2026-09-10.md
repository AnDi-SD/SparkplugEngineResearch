# Общий протокол CharacterStateMachine: переходы, стек, события

После [construction семейства](native-character-machine-family-2026-09-10.md)
и [Bloom lifecycle](native-pc-block-trace-2026-09-10.md) исследованы общие
active операции базовой машины. 35 PC cases, 33 paired PS2 cases:
первые26/24 за4,431 с, дополнительные9 filter cases за1,396 с. Уже успешная
матрица не повторялась ради одного добавленного фильтра.

Входы — явно заданные machine и две state-interface записи. Enter callbacks
исполняют настоящие original true/false leaves; suspend/resume/notify используют
оригинальные базовые методы состояния. Это проверяет протокол машины при
заданных ответах интерфейса, без утверждения о normal game initialization
этих записей или полной логике конкретного атакующего/движущегося состояния.

## Поля и операции

Здесь `data`, `gate`, `mode` — аналитические метки слов по offset,
официальные исходные имена и полный смысл packed data не установлены.

| Поле | PC | PS2 |
|---|---|---|
| current / previous state index | `130/134` | `13C/140` |
| gate | `138` | `144` |
| previous/current/next data | `13C/140/144` | `148/14C/150` |
| state pointer array, 42 entries | `148` | `154` |
| saved state indices, 5 words | `1F0` | `1FC` |
| saved data, 5 words | `204` | `210` |
| stack depth / mode / last byte flag | `218/21C/220` | `224/228/22C` |

| Операция | PC | PS2 | Проверенное поведение |
|---|---|---|---|
| Вход в текущее состояние | `004FABA0` | `002B40B0` | State-v7 получает адрес current data; true очищает gate, false сохраняет его |
| Reset | `004FAC00` | `002B3D80` | Обнуляет индексы/data/depth/mode, gate=1, вызывает state0-v7, затем last byte=1 |
| Выбор следующего | `004FAD70` | `002B3F70` | Сохраняет previous, вызывает self-v19 с next data, переносит data, сообщает0x2720, затем gate/flags и self-v20 |
| Push текущего | `004FAF30` | Inline PS2 в `002B4100` | Записывает state/data в два стека, увеличивает depth, сообщает0x2722, вызывает state-v10 и выбор следующего |
| Pop предыдущего | `004FACE0` | `002B4010` | Уменьшает depth, восстанавливает state/data, gate=0, сообщает0x2723 и вызывает state-v11 |
| Code→mode, virtual16 | `004FAC50` | `002B3D20` | 0→0, 1→5, 3→6; остальные проверенные коды сохраняют прежнее слово |

Сравнивались все bytes PC machine и state records, а не только один
возвращаемый результат. Проверены push при depth0/4 и pop при depth1/5.
Местные PC тела push/pop не содержат проверки границ перед индексированием;
условия их вызова из основного tick ещё не закрыты. Это не доказательство
ошибки игры и не основание добавлять произвольный clamp в восстановленный код.

При обычном выборе gate устанавливается в1 **после** события0x2720. State
flags `1C/1D/1E` устанавливаются в1, затем self-v20 вызывает вход; успешный
вход очищает gate. Pop сам эти три flags не сбрасывает. Reset оставляет
gate=1 после прямого вызова state-v7: это отличается от вызова helper входа.

## События и точные границы

PC исполнил полный per-object notification consumer `0040F9A0` с null
observer pointer. Его PS2 соответствие — `00100520`, проверенное по отдельному
окну. Соседний `001007A0` относится к другому пути через общий subscription
manager; он не использован как соответствие этого consumer.

На PS2 switch/pop исполнились после исключённых SQ prologues до входа
`00100520`. На этой границе code/state/data и все сравниваемые слова машины
совпали с checkpoint оригинального PC consumer. Последующие PS2 callbacks
для этих двух операций подтверждены статически, без объявления их execution.
PS2 push исследован статически в inline теле; два push cases выполнялись
только на PC. Остальные PS2 случаи — целые короткие методы либо явно указанные
prefixes до frame epilogue; SQ/LQ и unknown callees не заменены результатами.

`Notify` PC `004FAE50`, PS2 `002B3E10`:

- code28 вызывает machine-v17; code30 вызывает machine-v15. Пробы остановлены
  на original consumer entries PC `004FADF0/004FB040`, PS2 `002B3F00/002B4100`;
  внутренние update/color-like действия здесь не закрыты.
- code`27F3` выполняет reset. PC делегирует helper, PS2 содержит inline тело;
  итоговые слова и callback state0-v7 совпали.
- Для0/27/29/FFFFFFFF исходный указатель сообщения передаётся current state
  при ненулевом элементе массива; при null вызова нет. Обе ветви сопоставлены.

Reentry и мутации машины из реальных observer callbacks не проверялись.
Порядок повторных чтений после событий сохранён в original windows; snapshot
или запрет мутаций игровому коду не приписывается.

## Дополнительный фильтр

Virtual11 PC `004F5000` / PS2 `002B47F0` возвращает true, когда
`([[machine+0x24]+0x0C] & 0x18) == 0`, где скобки обозначают чтение памяти.
PC читает low byte flags, PS2 — word с тем же mask.
Сравнены hex00/07/08/10/18/20/100/80000000/FFFFFFFF. Более старшие bits результата
не меняют. В обоих телах нет null guard для machine+24; реальная привязка
остаётся обязательной предпосылкой использования этого метода.

## Учёт и открытый объём

База повышается PC20→45 и PS215→35. Тяжёлый tick, выбор конкретных состояний
потомками, полное normal binding, живые observers и изменённый source copy
ещё открыты; остальные35 классов семейства этим блоком не повышаются.
Полностью закрытых классов не добавлено.

Evidence: `local-data/results/native-cycle-20260910-1900/character-state-machine/`:
`protocol-run1.json`, `protocol-filter-run2.json`, точные две версии probe,
`base-and-bloom-windows`, `pc-base-ctor-window`, `transition-windows`,
`object-notify-window`, `v11-windows`. Raw PS2/PC SHA и границы исполнения
сохранены раздельно.
