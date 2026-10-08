# spPS2Mouse

PS2-класс `spPS2Mouse` зарегистрирован с ID `0x5E6722F3` и базой
`spPS2InputDevice`. Factory выделяет `0xE0` байт, вызывает общий constructor
и заменяет primary/secondary tables. Secondary input interface расположен
в `+14`. Собственные bytes `+48..DF` factory не инициализирует.

Clone заново проходит этот constructor, регистрирует source/destination и
вызывает физический Named copy. Имя сохраняется; input map, flag `+44` и
собственный payload заново получают состояние constructor. Tail остаётся
неопределённым. Destructor проходит общий InputDevice lifetime; собственного
RPC shutdown или удаления глобального semaphore в этом теле нет.

## Инициализация и состояния

При нулевом глобальном gate метод инициализации сразу возвращает `true`,
сохраняя старые flag и tail. При ненулевом gate он создаёт semaphore, сохраняет
его результат в глобальную cell и инициализирует RPC. Binding повторяется до
ненулевого server pointer в client. Ошибка создания или отрицательный bind
возвращают `false`; собственные поля устройства ещё не изменяются.

Успешная инициализация пишет `+44 = 1`, word `+48 = 2` и обнуляет три
диапазона по `0x30` байт: `+50..7F`, `+80..AF`, `+B0..DF`. Word `+4C`
остаётся нетронутым. Семантическое имя word `+48` не установлено.

| Offset | Использование |
| --- | --- |
| `+50..6C` | восемь current button words |
| `+70` | current button mask byte |
| `+74/+78/+7C` | current X/Y/wheel float values |
| `+80..9C` | предыдущие button words |
| `+A0` | предыдущий mask byte |
| `+A4/+A8/+AC` | предыдущие X/Y/wheel bits |
| `+B0..CC` | восемь changed button words |

## Poll

При нулевом `+44` poll возвращает `true` без чтения payload. Активный путь
вызывает RPC function `1` для device count. Ошибка этого вызова возвращает
`false`; flag и packet caches сохраняются. Для доступных устройств
вызывается function `3`, затем function `2` до нулевого event type.
Ошибка `3` или `2` сообщается logger и переводит цикл к следующему устройству.

После успешного function `3` X/Y/wheel обнуляются. Каждое ненулевое event
сначала обнуляет changed block. Если current mask отличается от previous,
все восемь button words заменяются bits нового mask, а changed words
становятся `1` для изменившихся bits. При равных masks current words
не переписываются даже при несогласованном содержимом этих words.

Signed bytes event прибавляются к float X/Y. Signed event type не меньше
`4` дополнительно заменяет wheel на `120 × signed wheel byte`; меньший type
сохраняет текущий wheel. X ограничивается интервалом `[-320, 320]`,
Y — `[-224, 224]`. Current button/axis values затем копируются в previous
snapshot. Последнее event определяет changed block; отсутствие устройств
или events сохраняет старый packet.

Semaphore identifier, device count и reply bytes перечитываются в исходных
точках. Completion callback также заново читает глобальный semaphore.
Binding/event loops не имеют придуманного retry count.

## Queries и граница backend

Queries gated ненулевым `+44`. Codes `100..107` проверяют точное равенство
current или changed word значению `1`. Codes `108/109` преобразуют X/Y в
unsigned word: при значении не меньше `2³¹` сначала вычитается `2³¹`,
затем результат signed conversion объединяется с `0x80000000`.
Signed slot использует X/Y/wheel для codes `108..110`. Другие codes дают
исходный zero/false. Command leaf — пустой метод, float leaf возвращает `+0`.

IOP RPC и kernel calls принадлежат явному borrowed host boundary. Optional
byte cells отражают установленные bytes без чтения неинициализированной C++
памяти. Неразрешённая зависимость даёт analytical `nullopt`, отдельно от
исходного Boolean результата; уже выполненный подтверждённый prefix не
откатывается. Это контракт инструмента, не native ABI.

Общий math helper представляет точные integral singles обычной initialized
polling последовательности и normal finite conversion/comparison. EE
фиксирует округление к нулю; преобразование за пределами signed word
насыщается. Signed-zero правила отдельно определены в
[EE Core User's Manual, главы 8.6–8.8](https://docs.alexrp.com/mips/ee.pdf).
Другие EE float domains требуют установленной реализации backend; обычный
IEEE host add не используется как универсальная замена PS2 arithmetic.

Связанные сведения: [cached input state](input-cached-state.md).
