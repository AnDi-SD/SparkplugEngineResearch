# spActor

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spActor](../../../Sparkplug/Code/Sparkplug/spActor.h).

## Наблюдаемые поля

Actor `+0x1C` разрешает применение transforms; `+0x24` разрешает продвижение
времени при выключенном `+0x1C`; `+0x20` — общий time multiplier. Массив
playback entries начинается по `+0x28`, capacity по `+0x2C`; stride `0x60`.
Controllers vector имеет begin/end/capacity по `+0x40/+0x44/+0x48`.
Allocation `0x54` не означает, что названы все поля; хвост `+0x4C..+0x53` открыт.

| Playback offset | Роль, подтверждённая accesses |
| ---: | --- |
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
