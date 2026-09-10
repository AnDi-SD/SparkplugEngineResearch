# `wxGameFlowState`: базовое состояние PC и PS2

Первый behavioral block цикла 10 сентября до 19:00. Это частичное исследование
класса, не готовая реализация всего game flow. Registration/name не включены
в behavioral score. Оригинальные названия методов и исходного файла неизвестны;
имена операций ниже аналитические. Приложения `tools/` не изменялись.

## Identity и границы

| Факт | PC | PS2 |
|---|---|---|
| Class/base ID | `11521AFA /415352A1` | те же |
| Base | `spBaseObject` | `spBaseObject` |
| Registration object | `7681F0` | `4C4090` |
| Factory / constructor | `5D6DC0 /5D6B20` | `33C570 /33C370` |
| Allocation / vtable | `3C /70F188` | `3C /493F10` |
| Clone | `5D6E20` | `33C4B0` |
| Deleting destructor | `5D6BC0`, body `5D67F0` | `33C2E0` |
| Set three fields | `5D6850` | `33C230` |
| Get three fields | отдельный адрес пока не установлен | `33C260` |
| Reset global table | `5D6880` | `33B590` |
| Следующий большой метод | `5D6E70` | `33B910` |

PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PS2 SHA256 `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.
Адреса относятся только к этим pristine файлам. Сопоставление сделано по
независимым registration, коду и данным обеих платформ.

## Конструктор, владение и clone

После base constructor обе версии устанавливают `10=0`, byte`14=0`,
words`18/1C/20=FFFFFFFF`, bytes`24/25=1`, words`2C/30/34=0`, byte`38=1`.
PC не записывает padding `15..17`, `26..2B`, `39..3B`; probe с `CC` fill это
сохраняет. Размер или похожие offsets не разрешают обнулять весь объект.

PS2 затем заменяет поля `18/1C/20` текущими словами по адресам
`49C64C /49C650 /49C648` (`GP=4A4170`). В ELF стоят `14000 /1900 /4000`,
но конструктор читает globals, а не hard-coded constants. Перед каждым чтением
проверяет singleton `GP-44FC` и при необходимости вызывает `20E870` — factory
зарегистрированного `wxMemoryManagerConfigPS2`. Семантика трёх кодов пока
не присвоена. PC оставляет `FFFFFFFF`.

Обе версии получают singleton `wxUserInput`: PC global`7552A0`, factory`401230`;
PS2 global`GP-443C`, factory`3F8260`. Заимствуют его member`18` в собственный
member`30`. Далее создают отдельный `spTaskTimer`, сохраняют в`2C`, вызывают
Reset, затем Start, затем сбрасывают глобальную таблицу. Существующее
[исследование таймера](native-class-sp-task-timer.md) переиспользуется;
вторая реализация таймера не создаётся.

Destructor удаляет owned timer`2C` через его virtual deleting destructor,
обнуляет`2C`, вызывает base cleanup. Borrowed`30` не освобождается. PC wrapper
освобождает память состояния при `flags &1`. PS2 освобождает её при
**положительном signed16 значении аргумента** и отдельно допускает null this.
Разница native ABI сохранена; это не общий boolean-контракт.

Clone создаёт fresh `3C` состояние и fresh timer, регистрирует пару и вызывает
унаследованный base copy. GameFlowState payload не копируется: изменённые
`18/1C/20` исходника не переходят в clone. На PC это исполнено; на PS2 проверена
структура кода, а весь lifecycle ещё не исполнен.

## Листовые операции

Setter записывает каждый из трёх аргументов в`18/1C/20`, только если он не равен
`FFFFFFFF`. Проверены четыре одинаковые последовательности на PC и PS2,
включая zero, `80000000`, все sentinel и частичную запись. Сравниваются все
`3C` bytes, чтобы исключить незаявленное изменение соседних полей.

PS2 getter последовательно записывает три слова в переданные output pointers.
Проверены различные валидные output buffers; произвольное aliasing не заявлено.
PS2 ABI стенда sign-extends 32-bit integer arguments в 64-bit GPR, как требуется
сравнением с native `ADDIU -1`. Нулевое расширение sentinel дало бы иной контракт.

Шесть собственных virtual hooks базового класса — действительные пустые методы:
два возвращают true, четыре выполняют return без значения. На PC это slots
`1C..30`, addresses `5A7DB0 /5B7A00`; на PS2 slots`24..38`, addresses
`33C2D0 /33C2C0 /33C2B0 /33C2A0 /33C290 /33C280`. PS2 имеет два header words
перед первым function pointer. Все шесть PS2 leaves исполнены. Для void hooks
случайное содержимое return register не получает семантического значения.

## Общая таблица 56 STX имён

PC reset переносит 56 DWORD pointers из`742DE4` в`768110..7681EF`, переставляя
порядок: `0..25,39,26..38,40..55`. Затем очищает 224 bytes`768250..76832F`.
Directed probe подставляет 56 различных synthetic IDs, поэтому перестановка
подтверждается независимо от повторяющихся строк в pristine image.

PS2 reset имеет другую раскладку исходных globals и stack spills, но 56 записей
в`4C40F0..4C41CF`. Строгая straight-line provenance прослеживает каждый global
load до store и аргументы единственного вызова `4076A8(4C41E0,0,E0)`.
Это статический анализ, **не запуск PS2 reset**. Callee использует EE/MMI;
generic MIPS decoder не выдаётся за исполнение этого пути. Его full arbitrary
input behavior отдельно не закрыт.

Все 56 соответствующих строк независимо прочитаны из PC и PS2 и совпадают,
включая повторы `default.stx`, `guard.stx`, `group.stx` и положение `lucy.stx`.
Имена и адреса находятся в
[`game-flow-state-data-2026-09-10.json`](../../research/game-flow-state-data-2026-09-10.json).
Назначение таблицы подтверждено пока до списка имён STX; фактическая загрузка,
кеширование и отображение этих ресурсов этим блоком не доказаны.

## Проверки и сохранённый отказ

- PC `original-run1`: fresh image с нулевым singleton остановился при чтении
  `1C` внутри цепочки создания внешнего ввода/renderer (`4BE693`). Это
  недостающее состояние стенда; не ошибка игры и не исчерпание instruction cap.
  Guest не продолжался после остановки. Failure и точная версия probe сохранены.
- PC `original-run2`: явно задан existing external input — opaque storage
  `24` bytes с borrowed pointer по`18`. Это вход класса, не поддельный
  `wxUserInput` constructor или полноценный запуск игры. Original GameFlowState
  factory/constructor, timer Reset/Start, setter, reset, clone и teardown
  исполнились до обычного return: **9 операций, 0,785 с, 4/4 owners freed**.
  Clock ticks`1200`, divisor`1`, refresh disabled. Существующие allocator/CRT/
  SEH и clone-map recording seams перечислены в fixture; весь CloneManager
  и UI startup не считаются исполненными.
- PS2 `ps2-leaves-run1`: **11 случаев, 0,484 с**; original integer leaf bytes
  в bounded Unicorn MIPS64 R4000. По 1000 instructions/100 ms, 30 s child cap,
  без imports, callees, interrupts, EE/MMI и game startup.
- Generic bounded capture проверен тремя regression tests: R5900 LQ/SQ и raw
  COP2/MMI, PC file-backed section edges, PS2 alignment/BSS/truncation.
  Static reset provenance первоначально отказала на неописанном `MOVE a1,zero`;
  после явного добавления единственной инструкции разобраны все 56 stores.
  Пропуск неизвестных инструкций запрещён; это исправление tooling, не игры.

Артефакты: `local-data/results/native-cycle-20260910-1900/game-flow/`:
`window1..5/capture.json`, `original-run1/2.json`, `ps2-leaves-run1.json`,
`ps2-reset-provenance.json`, `paired-reset-strings.json` и неизменяемые copies
использованных scripts. В captures есть raw bytes, SHA, file offsets и адреса.
Linear windows не означают full-function/CFG proof для соседних методов.

## Что осталось

Большой метод `5D6E70 /33B910`, helper paths вокруг PS2`33B270 /33B430`,
потребители flags и кодов, transitions, resource lifetime, exception/null
ветви зависимостей, полная подготовка `wxUserInput`, PS2 timer lifecycle и
соответствие неразобранных PC/PS2 методов. Portable runtime-класс ещё не
добавлен: lifecycle boundary должен оставаться явным и не становиться
выдуманным успешным factory. Восстановленные самостоятельные контракты и
таблица сохранены; следующий шаг — активные методы и семейство наследников.

Первая отдельная экспертная оценка знаний: **PC35 [20,50], PS230 [15,45]**,
оба `partial`, `new_research`. Это не доля перенесённого кода, число инструкций
или готовность tools. Класс не закрыт.
