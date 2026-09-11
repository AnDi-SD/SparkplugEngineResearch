# AIAction: случайные параметры Minotaur и Mosquito

54 исполнения PC/PS2 прошли с первого раза. PC подтвердил полные методы
при найденной цели и реальные selector boundaries при её отсутствии.
PS2 отдельно подтвердил доступные prefixes; деление/общая ACC-арифметика
за указанными ниже границами не выдаются за выполненные операции.

## Подтверждённое поведение PC

Методы v10: MinotaurAttack `5B8F50`, MinotaurDefend `5B8A40`,
MosquitoAttack `5B4B40`. Все используют настоящий nearest perception query.
Начальный borrowed граф и оригинальные vtables такие же, как в
[пакете входа атак](native-ai-attack-entry-2026-09-11.md).
В таблице смещения PC шестнадцатеричные, время и формулы десятичные.

| Класс | Подготовка и случайные значения |
|---|---|
| MinotaurAttack | command1D=1,target3A8,state3AC=0,word3B0=0,byte3BC=0,command pointer3C0. При цели один RNG result r;float3B4=float32(250000+r×240000/2³²) |
| MinotaurDefend | target3A8,state3AC=0;один RNG result r;deadline3B0=uint32(now+3000+r%1501);command pointer3B4 |
| MosquitoAttack | command1D=1,target3A8,state3AC=4;deadline3B0=uint32(now+1000);первый RNG r1 задаёт deadline3B4=uint32(now+500+r1%501);float3B8 меняет знак;второй RNG r2 задаёт move254=float32(100+r2×150/2³²);word3BC=47AFC800(90000f),byte3C4=0 |

При отсутствующей цели вызывается owner selector key1,parameter0.
MinotaurAttack делает это **до RNG**; его случайное значение за этой
границей не вычислялось. MinotaurDefend и Mosquito сначала выполняют
указанные RNG/записи, затем доходят до selector. Возврат selector не подменён.

При найденной цели Mosquito берёт Node из существующего camera manager
765AF0→28→18 и сохраняет в move1E0; null view даёт0. Это Node камеры,
не найденной perception цели. Move берётся из character12C.
Сравнение с ранее проверенным ShadowBeast показывает разные источники
movement target, которые нельзя объединять по внешнему сходству действий.

Входы: сохранённый RNG state с явно выбранными индексами1,2,3,15,
target/null target,now1200/FFFFFFF0,два заполнения и аргументы0/FFFFFFFF.
Для Mosquito дополнительно null view и amplitude0,−4,0.5 наряду с3.5;
битовая проверка включает смену знака нуля в PC. Произвольные float,
нечисловые значения и новые seed состояния не исследованы.

## RNG и проверочный эталон

Использован **полный624-word state**, полученный ранее настоящей default
инициализацией и twist на обеих платформах. Он загружен в свежие гости
из [сохранённого original RNG опыта](native-ps2-pc-rng-transactions-2026-09-11.md).
Выбор индекса указан явно; это не повторный serial replay всех предыдущих
выходов и не синтетическая подстановка возвращаемого случайного числа.

PC вызывает4132B0,PS2 —108350 с original warm body108E30. В каждом опыте
сохранены фактические outputs на исходных caller return addresses,
проверены прирост индекса и неизменность всего массива с крайними guards.
Числовой oracle новых игровых полей использует эти **наблюдённые original
outputs**, а контракт самого генератора опирается на прежнее независимое
исследование. Никакой новый RNG или успешный callback не внедрён.

Для Mosquito PC четыре пары outputs:
581869302/3890346734,3890346734/3586334585,
3586334585/545404204,4264392720/4112460519.
Высокий unsigned bit присутствует; PC не интерпретирует его как отрицательный
в итоговой формуле. Статические single-precision коэффициенты240000/2³²
и150/2³² подтверждены оригинальными words386A6000 и33160000.

## PS2: отдельно установленная граница

PS2 entries: MinotaurAttack25F280,MinotaurDefend25FCE0,Mosquito2605A0.
Perception исполняется полностью с оригинальным stack frame; own offsets
из таблицы в этой области больше PC на4. Вход содержит существующий timer
49FC80 и original RNG state,а не подменённые результаты этих методов.

- MinotaurAttack без цели доходит до selector225DF0 без RNG. При цели
  записывает target/state/command pointer,исполняет RNG и подготовку FPR,
  останавливается **перед ADDA.S25F358**,word46010018. Итоговый float3B8
  ещё не записан; общая ADDA/MADD формула не проверена динамически.
- MinotaurDefend записывает target3AC,state3B0=0,считывает timer и получает
  RNG output,затем останавливается **перед DIVU25FD28**. Deadline,
  command pointer и возможный selector находятся после границы.
- Mosquito записывает command1D=1,target3AC,state3B0=4,deadline3B4=now+1000,
  получает первый RNG output и останавливается **перед DIVU26061C**.
  Второй RNG,смена знака,общая ACC-арифметика и camera target ещё не выполнены.

DIVU не разрешён текущим opt-in stack scalar allowlist; это ограничение
квалифицированного стенда,не утверждение об отсутствии инструкции у PS2
или всего Unicorn. ADDA/общий MADD тоже выходят за существующий профиль
точных целых квадратов. Ничего за этими границами не заменялось собственным
игровым алгоритмом. Следующий вариант — отдельно квалифицировать необходимый
CPU-поднабор или проверить независимые original suffix-компоненты с явно
связанными входами. Полный PS2 возврат в этом пакете не заявляется.

## Итог проверки

Пилот6 за0,614s,общий пакет48 за3,864s;все с первого раза.
PC:15 whole returns/12 selector boundaries;PS2:4 selector/23 arithmetic
boundaries. Итого57 actual RNG outputs,316 SQ/LQ и45 ACC operations
на разрешённых участках. Guard24KiB плюс полный RNG array;original code
bytes неизменны. Повторов успешных тестов и изменений CPU wrapper нет.

6 platform updates по5 за новые v10 контракты трёх классов,с разными
объёмами PC/PS2 evidence. Старый RNG и perception повторно не засчитываются.
Новых C++ классов,полностью закрытых классов или готового игрового startup нет.

Контракт: [ai-random-entry-contracts](../../research/ai-random-entry-contracts-2026-09-11.json).
Probe: [probe_ai_random_entry.py](../../research/probe_ai_random_entry.py).
Локальные данные: `local-data/results/native-cycle-20260911-0730/ai-random-entry/`.
