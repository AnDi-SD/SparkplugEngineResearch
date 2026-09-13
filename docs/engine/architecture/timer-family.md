# Таймеры PC и PS2: идентичность, состояния и границы часов

## Классы и независимые доказательства

| Класс | Роль | Размер PC/PS2 |
| --- | --- | ---: |
| spTimer | Накопитель отсчётов платформенных часов | 36/36 |
| spMasterTimer | spTimer с singleton-интерфейсом | 40/40 |
| spTaskTimer | Время задач, source clock и дочерние таймеры | 60/60 |
| wxGameTimer | spTaskTimer с отдельным флагом игровой паузы | 136/136 |

Три новых PC factory/getter/Clone/два удаления прошли, всего15 class operations.
Каждый Clone создаёт отдельный объект с начальными runtime-значениями;
виртуальный Copy у этих классов — базовый no-op. spTaskTimer PC ранее имел83/100:
[прежний подробный реверс](../../reference/classes/sp-task-timer.md) повторно не засчитывается.

Общие primary tables имеют семь slots у Timer/Master и одиннадцать у Task/Game.
В Game только Start/Pause отличаются от таблицы Task; Update и Reset наследуются.
PC соседние функции critical section после таблицы Timer не являются её slots.
Singleton PS2 Master имеет secondary interface+24; Game —+3C, объект+84 указывает
на сам GameTimer. Это не доказательство полной глобальной lifetime policy.

## spTimer: два отсчёта и строгий лимит

PS2 каждый раз запускает новый scalar prefix: до оригинального clock001E7980,
затем отдельный prefix с явно объявленным возвращённым отсчётом. Wrapper часов,
ядро ОС и аппаратные таймеры PS2 не подменяются успешным callback. Каждая граница,
входной регистр и весь136-байтовый guard сохранены.

Layout обеих платформ: active byte10, accumulated word14, last-start word18,
clamp byte1C, limit word20. Аналитические имена методов:

## Различие арифметики и пределы вывода

PC использует unsigned difference, умноженный на float32-константу3A83126F,
с последующей записью float. PS2 явно конвертирует unsigned difference в float
и выполняет DIV.S на1000; отрицательный signed intermediate переводит через
shift/or, conversion и удвоение. Это разные последовательности операций.

Семь новых assessments: PC Timer60/Master25/Game45; PS2 Timer50/Master20/Task55/
Game40. Ни один класс не объявляется закрытым. Открыты полная платформа часов,
PS2 lifetimes, FPU rounding, native child ownership и встроенный GameTimer+44.
