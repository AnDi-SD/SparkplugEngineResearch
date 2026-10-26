# wxGameTimer

Общие исходники: [wxGameTimer](../../../Winx/Code/wxGameTimer.h).
Класс `78665312` физически и по регистрации наследует
[spTaskTimer](sp-task-timer.md). Native размер PC/PS2 — `88` hex байт;
portable C++ object не воспроизводит multiple-interface ABI.

| Native состояние | Смещение |
| --- | ---: |
| `spTaskTimer` | `00–3B` |
| singleton/support vptr | `3C` |
| собственный байт игровой паузы | `40` |
| owned [spMessageQueue](sp-message-queue.md) | `44–87` |
| borrowed clock этой очереди, равный this | `84` |

PC constructor создаёт настоящий TaskTimer prefix и собственную очередь,
публикует singleton и ставит собственный pause byte в ноль. Отдельный
process-global clock по `75F65C` — реальный [spTimer](sp-timer.md): его
constructor вызывается из global initialization caller, пишет table `7291B8`,
а getter возвращает registration `761578`. Это другой объект и другая роль,
чем borrowed TaskTimer clock pointer внутри Queue.

GameTimer constructor устанавливает у этого global spTimer limited byte `1`
и limit word `100`, сохраняя active/accumulated/startedAt. Clone повторяет
конструкцию и повторно задаёт лимит. Поэтому portable constructor получает
реальный shared spTimer по ссылке и изменяет его состояние напрямую;
заглушки с произвольными global words здесь нет. Factory требует явного typed
binding этого внешнего объекта. Вызовы ОС и регистрация atexit остаются снаружи.

Start сначала очищает собственный pause byte, затем исполняет shared
TaskTimer Start. Pause сначала ставит byte `1`, затем shared Pause сохраняет
current в paused и выключает active. Getter читает сырой байт. Inherited Reset
не меняет собственный pause byte и не очищает Queue; Start/Pause также сохраняют
ожидающие сообщения. Update и его прежние численные границы наследуются.

Clone создаёт fresh TaskTimer/пустую Queue со ссылкой на новый this; runtime time,
собственный флаг и сообщения не копируются. Copy использует literal root no-op.
Destructor сначала очищает owned Queue, затем безусловно обнуляет singleton
и уничтожает base. Удаление более раннего объекта обнуляет singleton даже при
живом более новом instance. Global spTimer limit после удаления не восстанавливается.

PC own methods/lifetime и typed clock configuration квалифицированы отдельно
от shared Queue. Её portable allocation/storage — явная техническая замена;
native pool ABI сохранён отдельно. Статические
[PC](../../../Winx/Analysis/PC/wxGameTimerAbi.h) и
[PS2](../../../Winx/Analysis/PS2/wxGameTimerAbi.h) layouts не закрывают OS clock,
полный PS2 constructor/lifetime/EE arithmetic, inherited child ownership,
native allocation failure или весь игровой цикл.
