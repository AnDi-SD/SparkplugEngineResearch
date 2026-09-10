# Таймеры PC и PS2: идентичность, состояния и границы часов

10 сентября 2026. [Контракт четырёх классов](../../research/timer-family-contracts-2026-09-10.json).
Контрольные PC/PS2 SHA256 записаны в контракте и каждом capture; используются
прежние pristine WinxClub.exe и SLES_532.19.

## Классы и независимые доказательства

| Класс | Роль | Размер PC/PS2 | PC / PS2 factory |
|---|---|---:|---|
| spTimer | Накопитель отсчётов платформенных часов | 36/36 | 00481950 /00115810 |
| spMasterTimer | spTimer с singleton-интерфейсом | 40/40 | 004505C0 /00115190 |
| spTaskTimer | Время задач, source clock и дочерние таймеры | 60/60 | 00450880 /00115670 |
| wxGameTimer | spTaskTimer с отдельным флагом игровой паузы | 136/136 | 00596A90 /0028F250 |

Три новых PC factory/getter/Clone/два удаления прошли, всего15 class operations.
Каждый Clone создаёт отдельный объект с начальными runtime-значениями;
виртуальный Copy у этих классов — базовый no-op. spTaskTimer PC ранее имел83/100:
[прежний подробный реверс](native-class-sp-task-timer.md) повторно не засчитывается.

PS2 отдельно проверена по factory allocation, записи vtable и getter, возвращающему
точную запись регистрации. У spTimer свой ctor001E7900 вызывает spBaseObject
00102BF0. Master factory встраивает собственные таблицы после этого Timer ctor;
Task factory встраивает значения после BaseObject и helpers списка. Game factory
вызывает Task ctor00115530 и constructor встроенного объекта по+44 через0010DE00.
Полное назначение этого встроенного объекта здесь не заявляется.

Общие primary tables имеют семь slots у Timer/Master и одиннадцать у Task/Game.
В Game только Start/Pause отличаются от таблицы Task; Update и Reset наследуются.
PC соседние функции critical section после таблицы Timer не являются её slots.
Singleton PS2 Master имеет secondary interface+24; Game —+3C, объект+84 указывает
на сам GameTimer. Это не доказательство полной глобальной lifetime policy.

## spTimer: два отсчёта и строгий лимит

[Probe](../../research/probe_timer_family_common.py):24 парных случаев,4,433 с.
PC выполняет оригинальные методы целиком. Верифицирован импорт WINMM.timeGetTime
по IAT006D9454; только этот платформенный вызов получает заранее заданные отсчёты.
Оригинальные методы006BE2D0/006BE2F0 не заменяются прежними refresh seams.

PS2 каждый раз запускает новый scalar prefix: до оригинального clock001E7980,
затем отдельный prefix с явно объявленным возвращённым отсчётом. Wrapper часов,
ядро ОС и аппаратные таймеры PS2 не подменяются успешным callback. Каждая граница,
входной регистр и весь136-байтовый guard сохранены.

Layout обеих платформ: active byte10, accumulated word14, last-start word18,
clamp byte1C, limit word20. Аналитические имена методов:

- Start PC006BE2D0 /PS2001E7830 получает часы, пишет18 и active1. Accumulated
  не сбрасывает. Reset PC006BE3C0 /PS2001E7860 сначала обнуляет14, затем делает
  такой же запуск. Прежние active/clamp не препятствуют операции.
- Stop PC006BE2F0 /PS2001E7790 не проверяет active. Без clamp добавляет
  unsigned modulo2³² `(now-start)` к accumulated и ставит active0.
- При clamp сначала получает отсчёт для сравнения. Только `elapsed > limit`
  выбирает прибавление limit. При равенстве или меньшем elapsed читает часы
  **второй раз** и прибавляет уже второй elapsed без повторной проверки лимита.
  Проверены изменения между отсчётами, равенство, нулевой лимит, wrap и high bit.

## Task/Game: пауза, целочисленное время и дети

Task Pause PC004506D0 /PS200115260 копирует1C→24 и обнуляет active18.
Reset PC004506E0 /PS200115240 обнуляет18/1C/20/24. Оба сохраняют delta28.
Game Pause PC00596900 /PS20028F010 сначала пишет pause40=1, затем вызывает
Task Pause. Game Start PC00596910 /PS20028F000 пишет40=0 перед Task Start.
Унаследованный Game Reset **не меняет pause40**. Полные guard проверки включают
NaN bits старого delta, active255, currentFFFFFFFF и неизменные соседние поля.

[Update probe](../../research/probe_task_timer_ps2_prefix.py):13 PS2 случаев,
1,519 с; десять новых PC leaf сравнений, три новых PS2 child routes. Прежний PC
обход детей не повторяется и не считается новой парной проверкой.

Paused Update обнуляет delta и всё равно переходит к детям. Active с borrowed
source2C копирует source1C и float word28 без обновления source/часов. Без source
integer current получается из unsigned rawTicks/divisor: relative mode использует
`now-start+paused`, absolute —now. Разность с прежним current также modulo2³².
Проверены ноль, FFFFFFFF,80000000 и wrap. PS2 останавливается перед конвертацией
этой разности во float; проверяются регистры нового current и unsigned difference.
Перед actual virtual child Update PS2 копирует active/relative bytes без bool
нормализации и передаёт настоящий child receiver. Полный рекурсивный PS2 обход
и native detach/reparent этим не закрываются.

## Различие арифметики и пределы вывода

PC использует unsigned difference, умноженный на float32-константу3A83126F,
с последующей записью float. PS2 явно конвертирует unsigned difference в float
и выполняет DIV.S на1000; отрицательный signed intermediate переводит через
shift/or, conversion и удвоение. Это разные последовательности операций.

**Побитное равенство дробного delta PC/PS2 не утверждается.** R4000 prefix runner
не является моделью всех особенностей R5900 FPU; в новых арифметических случаях
исполнение остановлено до float-конвертации. LWС1/SWС1 source delta используются
только для переноса битов, не вычисления. Для закрытия дробной PS2 арифметики
нужен отдельно проверенный R5900 FPU путь или трасса настоящей PS2/подходящего
эмулятора. Это открытая исследовательская граница, не причина менять PC код.

Семь новых assessments: PC Timer60/Master25/Game45; PS2 Timer50/Master20/Task55/
Game40. Ни один класс не объявляется закрытым. Открыты полная платформа часов,
PS2 lifetimes, FPU rounding, native child ownership и встроенный GameTimer+44.
