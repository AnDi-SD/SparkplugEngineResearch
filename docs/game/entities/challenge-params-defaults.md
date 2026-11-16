# wxChallengeParams: defaults блока параметров на PC

Constructor `wxChallengeParams` передаёт `this+0x124` внутреннему helper,
который записывает19DWORD defaults в76байт и возвращается. Это отдельное
завершённое тело: без вызовов, ветвлений, FPU и доступа к чужим объектам.
Связь с19полями подтверждает собственный издатель свойств. Его labels
приведены как имена регистрации; арифметика времени, score, процентов,
проверки диапазона и использование игровыми callers из них не выводятся.

| Смещение в parent prefix | Label издателя | Raw DWORD default |
| --- | --- | --- |
| `0x124` | Goal Time (ms) (determines exceptional performance) | `120000` |
| `0x128` | Starting Time (ms) on timer | `15000` |
| `0x12C` | Number of Checkpoints (for races) | `16` |
| `0x130` | Base Score (for completing challenge) | `0` |
| `0x134` | Score Goal Bonus (number of points per second better than Goal Time) | `20` |
| `0x138` | Score Failure Penalty (number points deducted per checkpoint missed) | `10` |
| `0x13C` | Stars Value(%) for Med. Performance (fairy stars challenge) | `40` |
| `0x140` | Stars Value(%) for High Performance (fairy stars challenge) | `75` |
| `0x144` | Bonus Given at 10% | `10` |
| `0x148` | Bonus Given at 20% | `20` |
| `0x14C` | Bonus Given at 30% | `40` |
| `0x150` | Bonus Given at 40% | `60` |
| `0x154` | Bonus Given at 50% | `100` |
| `0x158` | Bonus Given at 60% | `150` |
| `0x15C` | Bonus Given at 70% | `200` |
| `0x160` | Bonus Given at 80% | `250` |
| `0x164` | Bonus Given at 90% | `300` |
| `0x168` | Bonus Given at 100% | `350` |
| `0x16C` | Total Waves (Battle Only) | `10` |

Аналитический API требует явно выбранный PC-профиль и captured primary
`0x00701290`. Он стадирует полный prefix `0x170`, меняет только блок
`+0x124..+0x16F` и сохраняет все292предшествующих opaque bytes. Input/output
могут совпадать; повтор даёт тот же представленный результат. Отказ профиля
или чужой primary сохраняет output. Эти проверки принадлежат анализу:
оригинальный helper сам не читает primary table и получает interior pointer.

API не исполняет constructor, publisher или factory, не создаёт native
объект и не возвращает фиктивный pointer. У оригинального вызова EAX после
возврата содержит полученный interior pointer, ECX равен10, EDX равен40;
это наблюдаемый машинный контракт, а не восстановленная C++ сигнатура.
Стадированные значения не воспроизводят порядок native write-attempts.

Минимальный затронутый parent prefix равен368байтам. Историческая factory
allocation измерена тем же числом; текущий `sizeof` и физический layout
из этого не устанавливаются. Остальные методы, lifetime, production
маршруты, PS2, gameplay и целый класс остаются открытыми.
