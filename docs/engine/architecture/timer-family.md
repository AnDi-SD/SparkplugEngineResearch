# Таймеры PC и PS2: идентичность, состояния и границы часов

## Четыре разные роли

| Класс | Роль | Размер PC/PS2 |
| --- | --- | ---: |
| [spTimer](../../reference/classes/sp-timer.md) | Накопитель отсчётов платформенных часов | 36/36 |
| [spMasterTimer](../../reference/classes/sp-master-timer.md) | spTimer с singleton-интерфейсом | 40/40 |
| [spTaskTimer](../../reference/classes/sp-task-timer.md) | Время задач, source clock и дочерние таймеры | 60/60 |
| wxGameTimer | spTaskTimer с отдельным флагом игровой паузы | 136/136 |

Каждый Clone создаёт отдельный объект с начальными runtime-значениями;
виртуальный Copy у этих классов — базовый no-op. `spTaskTimer` физически
наследует `spBaseObject`, а не `spTimer`: сходство названия не задаёт наследование.

Общие primary tables имеют семь slots у Timer/Master и одиннадцать у Task/Game.
В Game только Start/Pause отличаются от таблицы Task; Update и Reset наследуются.
PC соседние функции critical section после таблицы Timer не являются её slots.
Singleton PS2 Master имеет secondary interface+24; Game —+3C, объект+84 указывает
на сам GameTimer. Это не доказательство полной глобальной lifetime policy.

## spTimer: два отсчёта и строгий лимит

PC получает отсчёт через `WINMM.timeGetTime`; PS2 вызывает wrapper `001E7980`.
Общие исходники принимают отдельный unsigned отсчёт для каждого вызова. Wrapper,
ОС и аппаратура остаются внешними службами, а значения не берутся из host clock
автоматически.

Layout обеих платформ: active byte10, accumulated word14, last-start word18,
clamp byte1C, limit word20. Start сохраняет один отсчёт и ставит active1;
Reset перед этим обнуляет accumulated. Stop не проверяет active и не меняет
last-start, поэтому повторный вызов снова прибавляет время от прежнего запуска.

При clamp0 Stop использует один отсчёт. При clamp≠0 первый unsigned difference
сравнивается с limit: строгое превышение прибавляет limit, иначе часы вызываются
второй раз. Разность второго отсчёта не ограничивается повторно. Сложение и
вычитание выполняются modulo2³².

Master публикует адрес полного объекта в singleton global после инициализации
таймера. Clone заменяет global своим новым объектом. Destructor очищает global
безусловно, даже если уничтожаемый объект уже перестал быть текущим instance.

## Различие арифметики и пределы вывода

`spTaskTimer` PC использует unsigned difference, умноженный на float32-константу3A83126F,
с последующей записью float. PS2 явно конвертирует unsigned difference в float
и выполняет DIV.S на1000; отрицательный signed intermediate переводит через
shift/or, conversion и удвоение. Это разные последовательности операций.

Исходники Timer и Master сохраняют собственные операции и явную границу часов.
Открыты полная платформа часов, полные PS2 lifetimes, FPU rounding Task/Game,
глобальная политика singleton ownership, native child ownership и встроенный
GameTimer+44. Наличие этих компонентов не означает полной интеграции игрового
цикла или всей платформы.
