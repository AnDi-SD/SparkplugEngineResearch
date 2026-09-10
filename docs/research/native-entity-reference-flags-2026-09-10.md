# Entity: ссылки, общие флаги и переход между менеджерами

Проверены десять PC/PS2 случаев назначения ссылки, 24 PC случая общих
методов wxEntity и два original PC factory/teardown сценария с наблюдением
списков менеджеров. Здесь нет новой реализации игровых классов: исполняются
оригинальные методы, а подготовка входов явно отделена от игровой инициализации.

## spEntity и привязка +18

Размер spEntity — **28 hex / 40 байт** на обеих платформах. PC factory
`00419D40`, vtable `006DBCCC`, getter `00419BF0`, clone `00419DA0`, deleting
destructor `00419DF0`: все пять операций прошли за 0,424 с. PS2 factory
`0014E560` содержит собственную inline construction: вызывает spNamedObject,
пишет vtable `0048DF60`, обнуляет `14/18/1C/24`, ставит `20=3`, добавляет объект
в engine manager. Это не вызов расположенного отдельно ctor `0014E390`.
Getter `0014E140` возвращает record `004A88E0`.

Назначение поля `+18` выполняют PC **00419D00** и PS2 **0014E150**. Это
собственный virtual slot9 PC / entry+2 ABI words PS2; его наследуют wxEntity
и базовая CharacterStateMachine. Для старого и нового указателей:

1. При равенстве указателей метод сразу завершает работу, включая null/null.
2. У прежнего nonnull объекта уменьшает **16-битный** счётчик `+8`.
3. Если результат равен нулю, вызывает его deleting destructor с аргументом1.
4. У нового nonnull объекта увеличивает счётчик `+8`, затем сохраняет указатель.

PC probe использует реальные spNode factories и deleting destructors;
receiver имеет явно заданный 28-byte spEntity layout. Проверены одинаковый
указатель, замена, обнуление, исходные счётчики0/1/2/5/FFFF. Локальная арифметика
счётчика оборачивается: increment FFFF→0, decrement0→FFFF; дополнительные
защиты или игровая допустимость таких входов не приписываются методу.
При удалении последней ссылки receiver всё ещё содержит старый указатель
на входе destructor. Восемь PS2 prefixes завершают все эффекты setter до
restore frame; два останавливаются у реального spNode destructor и совпадают
с PC в этой точке. PS2 destructor не подменяется успешным возвратом.
Все десять пар прошли за **3,127 с**, guard всего receiver подтверждает,
что setter меняет только четыре байта `+18`.

## Общие флаги wxEntity

PC virtual10 `004DA930`, PS2 `00286460` записывают одно слово в
**`[[entity+24]+0C]`**. Это поле затем читает ранее исследованный
CharacterStateMachine filter `004F5000 / 002B47F0` с маской18.

| Условие | Эффект |
|---|---|
| Начало расчёта | Результат `FFFFFF00` |
| byte38 nonzero и все cached predicates успешны | OR2; пустой список также успешен |
| byte39 nonzero и original getter global timer byte40 nonzero | OR8 |
| Квадрат расстояния больше порога word3C | OR10 |
| word3C содержит `7F7FFFFF` | Дистанционная часть пропускается |

Проверены восемь форм cached списка, отключённый byte38, состояние timer
byte40=0/1/2/FF и отключённый byte39. Обход прекращается после первого false.
PC вызывает predicate через встроенный интерфейс child+B4, slot10; PS2 —
через основную таблицу child, slot6C. Во входах используются literal interface
records с существующими оригинальными true/false leaves, а не новая игровая
реализация predicate. Getter timer **005968F0 / 0028F020** реально читает byte40.
Нормальное построение cached списка здесь ещё не исполнялось: byte28=1.

15 PC и PS2 сценариев дают одинаковые эффекты, порядок predicates и guard
записи только record+0C. Sentinel path проверен с null record+24: он не читает
координаты. PC дополнительно полностью вычисляет дистанцию для (3,4,0) и
порогов24/25/26, нулевой дистанции с порогом−1/0 и quiet-NaN порога.
При равенстве порогу bit10 не ставится. У PC unordered comparison с NaN
также не ставит bit10; это **только PC результат**.

PS2 читает координаты через record+24 с offsets70/74/78, PC —74/78/7C.
PS2 слова `00286640/48/4C` относятся к не реализованной этим стендом COP1
accumulator части. Она не исполняется как обычный MIPS. Пять отдельных PS2
comparison tails `00286650..00286690` с явно заданным конечным квадратом
расстояния подтверждают сравнение и запись bit10; это не полный расчёт
дистанции PS2. Для NaN/Inf/denormal не заявляется эквивалентность R4000 и EE.

## Собственная copy-часть wxEntity

PC `004D9700` сначала вызывает spNamedObject copy; его собственная часть
копирует **только bytes28/38/39 и word3C**, после чего обращается к clone
manager с class hash `796A1869`. Пointers14/18/24, cached список, остальные
поля и padding этот метод не переносит. Три изменённых source payload,
включая ненормализованные bytes и raw float word `7FC12345`, прошли с guard
всего source и destination.

PS2 собственный prefix `00286AD4` после inherited success переносит те же
байты/слово и достигает настоящего clone-manager consumer `00104F00` с тем
же class hash. До этого consumer prefix останавливается. Сохраняется отличие
от проверки всего inherited copy и произвольных shared strings.
В сумме flags/copy/distance batch: **24 PC cases за3,722 с**, 18 сравнений
cached/payload effects, пять отдельных PS2 comparison tails; один PC NaN case.

## Менеджеры и исправление предыдущей формулировки

Два PC factory/teardown сценария за **0,584 с** наблюдают реальные entry
`004200D0` (engine add), `00420070` (engine remove), `00574890` (game add),
списки после factory и после удаления. Подтверждён следующий порядок:

| Создаваемый тип | Original factory calls | Списки после возврата |
|---|---|---|
| spEntity | engine add | engine содержит объект |
| wxEntity | engine add → engine remove → game add | engine пуст, game содержит объект |

Удаление очищает соответствующие списки. PS2 independently: spEntity ctor
вызывает engine table+30=`0014EB70`, wxEntity ctor затем table+34=`0014EA90`
и game add `00289600`. Последний добавляет указатель в список manager+20.
Поля entity18/24 остаются null после factory: присутствие в списке само по
себе ещё не завершает привязку сцены или вспомогательной записи.

Это **исправляет фразу «регистрируют entity в двух менеджерах»** в предыдущем
[construction dossier](native-entity-direct-family-2026-09-10.md): объект
переводится из engine-списка в game-список и одновременно в обоих не остаётся.
Хешированный исторический документ сохранён; актуальный вывод — здесь.

Также уточнён старый [spEntityManager dossier](native-class-sp-entity-manager.md):
original PC factory действительно выделяет **20 hex /32 байта**; записанный
vtable pointer — **006DC460**, а не соседний адрес006DC458. Его slots1C/20
дают add/remove. PS2 primary/support ABI layout сохраняется отдельным.
Это ошибки/пробелы прежней аннотации, не повод менять игру.

Игровой менеджер определён по собственным factory/vtable/getter identities:
**wxEntityManager**, PC factory00407400, vtable0070069C, getter00574610,
record00754608; PS2 factory003E7C60, ctor002897A0, vtable00495AC0,
getter003F88A0, record004CB3F0. Размеры **PC5DF8 /24056, PS23E4C /15948**.
Они установлены независимо, одинаковую раскладку массивов предполагать нельзя.
Его самостоятельное clone/полный lifetime и обработка накопленного списка
остаются открыты; singleton не удаляется ради удобного результата проверки.

## Воспроизводимость и границы

Данные: `local-data/results/native-cycle-20260910-1900/entity-protocol/`,
отдельный spEntity lifecycle — соседний `entity-core/`. Точные версии новых
probe и общего `ps2_scalar_prefix.py` сохранены в `source-snapshot/`.
Новая обвязка PS2 переиспользует pristine image и явные ranges, но каждый
guest исполняется только один раз. Проверки original leaves, stop boundaries,
исключённых SQ и COP1 accumulator, uncaptured delay slot —5 tests за0,103 с.
Она не является новым PS2 CPU emulator и не снимает ограничений R5900.
В одном scouting capture неверно подписан лишний getter-range; он не
использован. Исправление подписи и настоящий getter из vtable сохранены отдельно.

Учёт: первые spEntity35/30, wxEntity20→40 /15→35, spEntityManager PC65→70,
первые wxEntityManager15/15. Восстановленный C++ и интерфейсы не изменялись.
Нормальное заполнение entity+24, cache building, работа обоих менеджеров
с живой сценой, все derived copy и полная PS2 арифметика остаются открыты.
